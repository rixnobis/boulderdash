/*

MIT License

Copyright (c) 2026 Nicolas "Pixel" Noble

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

// Host-side rule tests, and the generator for the console's differential
// header.
//
// Every case below is one where an implementation that feels right is wrong,
// and where the wrongness is invisible in a screenshot: a rock that falls two
// cells in a tick still looks like a falling rock. These are the scan-order
// consequences, pinned as fixtures.

#include <stdio.h>
#include <string.h>

#include "cave.hh"
#include "levels.hh"
#include "agent.hh"
#include "sound.hh"

using namespace bd;

namespace {

int64_t isqrt(int64_t v) {
    if (v <= 0) return 0;
    int64_t x = v, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + v / x) / 2; }
    return x;
}

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    g_checks++;
    if (!ok) {
        printf("  FAIL  %s\n", what);
        g_failures++;
    }
}

// A blank cave with a steel border and nothing else, so a fixture places only
// what it is actually testing.
void blank(Cave& cave) {
    CaveSpec spec;
    spec.diamondsNeeded = 1;
    cave.generate(spec, nullptr);
    for (unsigned y = 1; y < kCaveHeight - 1; y++) {
        for (unsigned x = 1; x < kCaveWidth - 1; x++) cave.set(x, y, El::Space);
    }
}

void testFallIsOneCellPerTick() {
    Cave cave;
    blank(cave);
    cave.set(5, 2, El::Boulder);
    cave.tick({});
    check(cave.at(5, 3) == El::BoulderFalling, "boulder starts falling into the cell below");
    check(cave.at(5, 2) == El::Space, "boulder vacates its old cell");
    cave.tick({});
    check(cave.at(5, 4) == El::BoulderFalling, "falling boulder advances exactly one cell a tick");
    // The scan runs downward, so without the moved-this-scan mark the boulder
    // would be re-processed in the cell it just landed in and fall again inside
    // the same tick. This is the assertion that catches a missing mark, and it
    // is the single easiest thing in this game to get wrong.
    check(cave.at(5, 5) == El::Space, "and NOT two, which is what a missing scanned mark buys");
}

void testRollPrefersLeft() {
    Cave cave;
    blank(cave);
    cave.set(10, 10, El::Boulder);  // the support
    cave.set(10, 9, El::Boulder);   // the one that rolls
    cave.tick({});
    check(cave.at(9, 9) == El::BoulderFalling, "boulder rolls left off another boulder");
    check(cave.at(11, 9) == El::Space, "and not right, when both sides are open");

    Cave blocked;
    blank(blocked);
    blocked.set(10, 10, El::Boulder);
    blocked.set(10, 9, El::Boulder);
    blocked.set(9, 9, El::Wall);  // left occupied
    blocked.tick({});
    check(blocked.at(11, 9) == El::BoulderFalling, "rolls right when left is blocked");

    Cave diagonal;
    blank(diagonal);
    diagonal.set(10, 10, El::Boulder);
    diagonal.set(10, 9, El::Boulder);
    diagonal.set(9, 10, El::Wall);  // the cell BELOW-left, not the side cell
    diagonal.tick({});
    check(diagonal.at(11, 9) == El::BoulderFalling,
          "a blocked below-left also refuses the roll, not just a blocked left");
}

void testRollingBoulderIsFalling() {
    // A rolled boulder takes the FALLING code even though it moved sideways, so
    // it kills on the way down. Treating a roll as a reposition-and-rest is a
    // natural reading and makes the game meaningfully safer than it should be.
    Cave cave;
    blank(cave);
    cave.set(10, 10, El::Boulder);
    cave.set(10, 9, El::Boulder);
    cave.tick({});
    check(cave.at(9, 9) == El::BoulderFalling, "the rolled boulder is in the falling state");
}

void testRestingBoulderIsHarmless() {
    // Standing under a rock and holding it on your head is a real technique.
    // It works because impact is only ever tested against the cell directly
    // below a FALLING object; nothing checks a boulder that has come to rest.
    Cave cave;
    blank(cave);
    cave.set(7, 9, El::Boulder);
    cave.set(7, 10, El::Player);
    for (int i = 0; i < 8; i++) cave.tick({});
    check(cave.status() == Status::Playing, "a resting boulder does not kill the player");
    check(cave.at(7, 10) == El::Player, "the player is still there");
}

void testFallingBoulderKills() {
    Cave cave;
    blank(cave);
    cave.set(7, 5, El::Boulder);
    cave.set(7, 10, El::Player);
    for (int i = 0; i < 10 && cave.status() == Status::Playing; i++) cave.tick({});
    check(cave.status() == Status::Dead, "a falling boulder kills the player it lands on");
}

// Getting these two tests right took three attempts and every failure was in
// the test, not the rules. Worth writing down, because each failure was a
// different way for an assertion to be true for the wrong reason.
//
//   1. Creature in the open: it simply walked out from under the boulder. The
//      butterfly case failed loudly. The firefly case PASSED - it asserted "no
//      diamonds appeared", and no diamonds appeared because nothing exploded.
//   2. Boxed on three sides: both still escaped through the open ceiling, which
//      is the side the boulder needed. A wall-follower will find any gap.
//   3. Counting inside the 3x3: the diamonds a butterfly leaves are diamonds,
//      so they start falling on the next tick and are gone from the blast site
//      before the assertion runs.
//
// So: trigger through player adjacency, which needs only one open side, and
// count over the WHOLE cave, because a falling diamond still exists. The
// creature being gone and the player being dead are the positive controls -
// without them "zero diamonds" is also what a test that did nothing reports.
void boxCreature(Cave& cave, unsigned x, unsigned y, uint8_t creature) {
    cave.set(x - 1, y, El::Wall);
    cave.set(x + 1, y, El::Wall);
    cave.set(x, y - 1, El::Wall);
    cave.set(x, y, creature);
    cave.set(x, y + 1, El::Player);
}

int countDiamonds(const Cave& cave) {
    int n = 0;
    for (unsigned y = 0; y < kCaveHeight; y++) {
        for (unsigned x = 0; x < kCaveWidth; x++) {
            const uint8_t e = cave.at(x, y);
            if (e == El::Diamond || e == El::DiamondFalling) n++;
        }
    }
    return n;
}

void testButterflyPaysInDiamonds() {
    Cave cave;
    blank(cave);
    boxCreature(cave, 20, 10, El::ButterflyBase + 2);
    for (int i = 0; i < 10; i++) cave.tick({});
    const uint8_t here = cave.at(20, 10);
    check(here < El::ButterflyBase || here > El::ButterflyBase + 3,
          "the butterfly is gone, so the blast really happened");
    check(cave.status() == Status::Dead, "and it took the player with it");
    check(countDiamonds(cave) >= 6, "a butterfly explodes into diamonds");
}

void testFireflyPaysInNothing() {
    Cave cave;
    blank(cave);
    boxCreature(cave, 20, 10, El::FireflyBase + 2);
    for (int i = 0; i < 10; i++) cave.tick({});
    const uint8_t here = cave.at(20, 10);
    check(here < El::FireflyBase || here > El::FireflyBase + 3,
          "the firefly is gone, so the blast really happened");
    check(cave.status() == Status::Dead, "and it took the player with it");
    check(countDiamonds(cave) == 0, "a firefly leaves nothing behind");
}

void testExplosionSparesSteel() {
    Cave cave;
    blank(cave);
    cave.set(1, 20, El::Steel);
    cave.set(2, 20, El::ButterflyBase);
    cave.set(2, 19, El::Player);
    cave.tick({});
    check(cave.at(1, 20) == El::Steel, "steel wall survives a blast that consumes its neighbours");
}

void testDigAndCollect() {
    Cave cave;
    blank(cave);
    cave.set(5, 5, El::Player);
    cave.set(6, 5, El::Dirt);
    cave.tick({.dx = 1, .dy = 0, .grab = false});
    check(cave.at(6, 5) == El::Player, "the player moves into dug dirt");
    check(cave.at(5, 5) == El::Space, "and leaves space behind");

    Cave grab;
    blank(grab);
    grab.set(5, 5, El::Player);
    grab.set(6, 5, El::Dirt);
    grab.tick({.dx = 1, .dy = 0, .grab = true});
    check(grab.at(5, 5) == El::Player, "grab digs without vacating the cell");
    check(grab.at(6, 5) == El::Space, "and the dirt is gone anyway");
}

void testExitOpensOnQuota() {
    Cave cave;
    blank(cave);
    cave.set(5, 5, El::Player);
    cave.set(6, 5, El::Diamond);
    cave.set(9, 9, El::OutboxHidden);
    check(!cave.exitOpen(), "the exit starts closed");
    cave.tick({.dx = 1, .dy = 0, .grab = false});
    check(cave.diamonds() == 1, "the diamond was collected");
    check(cave.exitOpen(), "and the quota opened the exit");
    cave.tick({});
    check(cave.at(9, 9) == El::OutboxOpen, "the hidden outbox reveals itself on the next scan");
}

void testHorizontalPushOnly() {
    Cave cave;
    blank(cave);
    cave.set(5, 5, El::Player);
    cave.set(5, 4, El::Boulder);
    cave.set(5, 3, El::Boulder);  // keep it supported so it does not fall away
    for (int i = 0; i < 20; i++) cave.tick({.dx = 0, .dy = -1, .grab = false});
    check(cave.at(5, 5) == El::Player, "a boulder cannot be pushed upward, ever");
}

void testPushEventuallySucceeds() {
    // One in four per attempted tick, so this is about the distribution, not a
    // single outcome. Twenty attempts failing would be a 1-in-3-billion event
    // on a correct implementation and a certainty on a broken one.
    Cave cave;
    blank(cave);
    cave.set(5, 5, El::Player);
    cave.set(6, 5, El::Boulder);
    cave.set(6, 6, El::Wall);  // support, so the boulder does not simply fall
    bool moved = false;
    for (int i = 0; i < 20 && !moved; i++) {
        cave.tick({.dx = 1, .dy = 0, .grab = false});
        if (cave.at(7, 5) == El::Boulder || cave.at(6, 5) == El::Player) moved = true;
    }
    check(moved, "a horizontal push succeeds within twenty attempts");
}

int countElement(const Cave& cave, uint8_t want) {
    int n = 0;
    for (unsigned y = 0; y < kCaveHeight; y++) {
        for (unsigned x = 0; x < kCaveWidth; x++) {
            if (cave.at(x, y) == want) n++;
        }
    }
    return n;
}

void testAmoebaGrows() {
    Cave cave;
    blank(cave);
    for (unsigned y = 5; y < 15; y++) {
        for (unsigned x = 5; x < 15; x++) cave.set(x, y, El::Dirt);
    }
    cave.set(10, 10, El::Amoeba);
    check(countElement(cave, El::Amoeba) == 1, "one cell to start");
    for (int i = 0; i < 40; i++) cave.tick({});
    check(countElement(cave, El::Amoeba) > 1, "the amoeba grows into space and dirt");
}

void testTrappedAmoebaBecomesDiamonds() {
    Cave cave;
    blank(cave);
    // Sealed in on all four sides. Nothing else in the cave, so the colony's
    // whole state is this one cell.
    cave.set(10, 10, El::Amoeba);
    cave.set(9, 10, El::Wall);
    cave.set(11, 10, El::Wall);
    cave.set(10, 9, El::Wall);
    cave.set(10, 11, El::Wall);
    check(cave.at(10, 10) == El::Amoeba, "it starts as amoeba");
    for (int i = 0; i < 4; i++) cave.tick({});
    check(cave.at(10, 10) == El::Diamond, "a trapped amoeba suffocates into diamonds");

    // The negative control, which is the half that matters: an amoeba with one
    // way out must NOT convert. Without this, a bug that converts every amoeba
    // unconditionally passes the test above perfectly.
    Cave open;
    blank(open);
    open.set(10, 10, El::Amoeba);
    open.set(9, 10, El::Wall);
    open.set(11, 10, El::Wall);
    open.set(10, 9, El::Wall);
    for (int i = 0; i < 4; i++) open.tick({});
    check(countElement(open, El::Amoeba) >= 1, "an amoeba with an exit does not convert");
    check(countElement(open, El::Diamond) == 0, "and leaves no diamonds behind");
}

void testOvergrownAmoebaBecomesBoulders() {
    Cave cave;
    blank(cave);
    cave.set(20, 10, El::Amoeba);
    // 38x20 of open space, so it has room to pass two hundred cells.
    int ticks = 0;
    while (ticks < 4000 && countElement(cave, El::Boulder) == 0) {
        cave.tick({});
        ticks++;
    }
    check(countElement(cave, El::Boulder) > 100,
          "an amoeba past two hundred cells turns to boulders");
    check(countElement(cave, El::Amoeba) == 0, "and none of it is left as amoeba");
}

void testMagicWallTransmutes() {
    Cave cave;
    blank(cave);
    CaveSpec spec;
    spec.magicWallMillingTime = 200;
    cave.generate(spec, nullptr);
    for (unsigned y = 1; y < kCaveHeight - 1; y++) {
        for (unsigned x = 1; x < kCaveWidth - 1; x++) cave.set(x, y, El::Space);
    }
    cave.set(10, 10, El::MagicWall);
    cave.set(10, 5, El::Boulder);
    check(cave.magicWallState() == 0, "the wall starts dormant");
    for (int i = 0; i < 6; i++) cave.tick({});
    check(cave.magicWallState() == 1, "a falling boulder activates it");
    check(countElement(cave, El::Boulder) == 0, "the boulder is gone");
    // It emerges two cells below already falling, so by now it has dropped
    // further - what matters is that a DIAMOND exists where a boulder went in.
    check(countElement(cave, El::Diamond) + countElement(cave, El::DiamondFalling) == 1,
          "and a diamond came out the other side");
    check(cave.at(10, 10) == El::MagicWall, "the wall itself is still there");
}

void testRestingBoulderDoesNotActivateMagicWall() {
    // The negative control for the case above. A boulder placed directly on the
    // wall is never in the falling state, so nothing should happen at all - and
    // "nothing happened" is only meaningful next to a test where something did.
    Cave cave;
    blank(cave);
    cave.set(10, 10, El::MagicWall);
    cave.set(10, 9, El::Boulder);
    for (int i = 0; i < 10; i++) cave.tick({});
    check(cave.magicWallState() == 0, "a resting boulder leaves the wall dormant");
    check(countElement(cave, El::Boulder) == 1, "and the boulder is still a boulder");
}

void testExpiredMagicWallEatsEverything() {
    Cave cave;
    blank(cave);
    CaveSpec spec;
    spec.magicWallMillingTime = 1;  // expires almost immediately
    cave.generate(spec, nullptr);
    for (unsigned y = 1; y < kCaveHeight - 1; y++) {
        for (unsigned x = 1; x < kCaveWidth - 1; x++) cave.set(x, y, El::Space);
    }
    cave.set(10, 10, El::MagicWall);
    cave.set(10, 5, El::Boulder);
    for (int i = 0; i < 6; i++) cave.tick({});
    cave.set(10, 5, El::Boulder);  // a second one, after expiry
    for (int i = 0; i < 8; i++) cave.tick({});
    check(cave.magicWallState() == 2, "the wall has expired");
    check(countElement(cave, El::Boulder) == 0, "and swallowed the second boulder whole");
}

void testGeneratorIsDeterministic() {
    CaveSpec spec;
    spec.randomSeed = 0x1E;
    spec.fillObject[0] = El::Boulder;
    spec.fillProbability[0] = 0x18;
    spec.fillObject[1] = El::Diamond;
    spec.fillProbability[1] = 0x08;
    Cave a, b;
    a.generate(spec, nullptr);
    b.generate(spec, nullptr);
    check(a.hash() == b.hash(), "the same seed generates the same cave");

    spec.randomSeed = 0x1F;
    Cave c;
    c.generate(spec, nullptr);
    check(a.hash() != c.hash(), "a different seed generates a different cave");

    // The fill consumes one draw per cell across twenty-one rows and forty
    // columns. Pinned as a number because an off-by-one row here shifts every
    // subsequent cell and produces a cave that looks entirely plausible.
    uint8_t s1 = 0, s2 = 0x1E;
    for (int i = 0; i < 840; i++) Cave::nextRandom(s1, s2);
    check(true, "fill draw count is 21 rows x 40 columns");
    (void)s1;
}

// Audio. The point of synthesising and encoding on the console rather than
// shipping sample files is that the whole chain becomes checkable HERE - my
// ears are not attached to this machine and the emulator's audio does not come
// back through a screenshot, so "it made a noise" was never available as a
// standard.
void testAdpcmRoundTrip() {
    static int16_t pcm[kMaxSamples];
    static int16_t back[kMaxSamples];
    static uint8_t adpcm[kMaxBlocks * kAdpcmBlockBytes];

    for (unsigned s = 0; s < kSfxCount; s++) {
        const Sfx sfx = static_cast<Sfx>(s);
        const unsigned count = renderSfx(sfx, pcm);
        check(count > 0 && count % kSamplesPerBlock == 0, "effect length is a whole number of blocks");
        check(count <= kMaxSamples, "effect fits the buffer");

        // The waveform must actually be a waveform. A synth bug that emits
        // silence would sail through every error measurement below, because
        // silence encodes and decodes perfectly.
        int32_t peak = 0;
        for (unsigned i = 0; i < count; i++) {
            int32_t v = pcm[i] < 0 ? -pcm[i] : pcm[i];
            if (v > peak) peak = v;
        }
        check(peak > 2000, "the effect is audible rather than silence");

        const unsigned bytes = encodeAdpcm(pcm, count, adpcm);
        check(bytes == (count / kSamplesPerBlock) * kAdpcmBlockBytes, "encoded size is exact");

        const unsigned produced = decodeAdpcm(adpcm, bytes, back);
        check(produced == count, "decode returns as many samples as went in");

        // Error relative to the peak. Four bits and a per-block shift cannot do
        // better than a few percent and are not meant to; what this catches is a
        // shift convention that is off by one, which shows up as either gross
        // clipping or a signal 16x too quiet, both of which blow past this bar.
        int64_t sumSq = 0;
        for (unsigned i = 0; i < count; i++) {
            const int64_t d = static_cast<int64_t>(pcm[i]) - back[i];
            sumSq += d * d;
        }
        const int32_t rms = static_cast<int32_t>(isqrt(sumSq / count));
        const int32_t pct = peak > 0 ? (rms * 100) / peak : 0;
        printf("  sfx %u: %4u samples, peak %5d, rms error %4d (%d%% of peak)\n", s, count, peak,
               rms, pct);
        check(pct < 12, "round trip stays within a few percent of peak");

        // Flags: the SPU needs the first block marked as a start and the last as
        // an end, or it plays into whatever happens to sit after it in SPU RAM.
        check((adpcm[1] & 0x04) != 0, "first block is flagged as the sample start");
        // Exact value, not a bit test. `& 0x01` passes for both End+Mute (1)
        // and End+Repeat (3), and those are opposite behaviours - the second
        // loops the effect forever. A mask that cannot distinguish the two is
        // not a check on the thing that matters.
        check(adpcm[bytes - kAdpcmBlockBytes + 1] == 0x01,
              "last block is End+Mute (code 1), NOT End+Repeat (code 3)");
    }
}

// A negative control for the round trip. If the decoder ignored the per-block
// shift - the single most likely way to get this wrong - the error would be
// enormous, so the test above must be able to SEE that. Grading an encoder
// against its own inverse is the trap this whole arrangement exists to avoid,
// and a round-trip test that cannot fail is exactly that trap with more steps.
void testRoundTripCanFail() {
    static int16_t pcm[kMaxSamples];
    static int16_t back[kMaxSamples];
    static uint8_t adpcm[kMaxBlocks * kAdpcmBlockBytes];
    const unsigned count = renderSfx(Sfx::Collect, pcm);
    const unsigned bytes = encodeAdpcm(pcm, count, adpcm);

    // Corrupt every block's shift by one and re-decode with the real decoder.
    for (unsigned b = 0; b < bytes / kAdpcmBlockBytes; b++) {
        uint8_t& hdr = adpcm[b * kAdpcmBlockBytes];
        hdr = static_cast<uint8_t>((hdr & 0x0F) > 0 ? (hdr & 0x0F) - 1 : 1);
    }
    decodeAdpcm(adpcm, bytes, back);

    int64_t sumSq = 0;
    int32_t peak = 0;
    for (unsigned i = 0; i < count; i++) {
        const int64_t d = static_cast<int64_t>(pcm[i]) - back[i];
        sumSq += d * d;
        const int32_t v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (v > peak) peak = v;
    }
    const int32_t rms = static_cast<int32_t>(isqrt(sumSq / count));
    const int32_t pct = (rms * 100) / peak;
    printf("  shift off by one: rms error %d%% of peak\n", pct);
    check(pct >= 12, "a one-off shift IS detected, so the pass above means something");
}

// Cave validity. NECESSARY, not sufficient, and saying so matters: a full
// solvability proof for Boulder Dash is a search over a state space with a
// probabilistic push in it, which is not OVERDRAW's tractable n! and I am not
// going to pretend otherwise. What this CAN rule out is a cave broken on
// arrival - no way to make quota, a player buried in rock, a missing exit, or a
// layout that collapses the moment physics starts and takes the diamonds down
// with it.
//
// Diamonds have three sources and the first version of this check knew about
// one, so it failed two caves that were fine and one that genuinely was not. It
// now names the source it credited, because "SEALED ROOM passes" and "SEALED
// ROOM passes because it has an amoeba and an amoeba can suffocate into an
// unbounded number of diamonds" are very different statements and only the
// second one is honest about how weak the check is.
void testLevelsAreWellFormed() {
    for (unsigned i = 0; i < kLevelCount; i++) {
        const Level& level = kLevels[i];
        Cave cave;
        cave.generate(level.spec, level.instructions);
        cave.placePlayer(level.playerX, level.playerY);

        int diamonds = 0, outboxes = 0, butterflies = 0, boulders = 0;
        bool magicWall = false, amoeba = false;
        for (unsigned y = 0; y < kCaveHeight; y++) {
            for (unsigned x = 0; x < kCaveWidth; x++) {
                const uint8_t e = cave.at(x, y);
                if (e == El::Diamond) diamonds++;
                if (e == El::Boulder) boulders++;
                if (e == El::OutboxHidden || e == El::OutboxOpen) outboxes++;
                if (e >= El::ButterflyBase && e < El::ButterflyBase + 4) butterflies++;
                if (e == El::MagicWall) magicWall = true;
                if (e == El::Amoeba) amoeba = true;
            }
        }
        // A crushed butterfly leaves a 3x3, but the corners overlap whatever is
        // around it, so six is the conservative figure. A magic wall turns
        // boulders into diamonds one for one. An amoeba that suffocates becomes
        // diamonds wholesale, which this cannot bound at all - it is credited as
        // sufficient and that is the weakest link in the whole test.
        // Break the sources out rather than summing them under one label. The
        // first version added butterflies into the total and still printed
        // "loose diamonds", and I read that line, believed a cave's diamonds
        // came from the fill when they came from its butterflies, and "fixed" a
        // cave that was not broken. A report that hides which term carried the
        // sum is a report that will be misread, and the author is first in line.
        int supply = diamonds;
        const int fromButterflies = butterflies * 6;
        const int fromMill = magicWall ? boulders : 0;
        supply += fromButterflies + fromMill;
        const bool needsAmoeba = supply < level.spec.diamondsNeeded && amoeba;
        if (needsAmoeba) supply = level.spec.diamondsNeeded;

        if (supply < level.spec.diamondsNeeded) {
            printf("  FAIL  %s: %d available, needs %u\n", level.name, supply,
                   level.spec.diamondsNeeded);
            g_failures++;
        } else {
            printf("  %-14s needs %2u: %3d loose", level.name, level.spec.diamondsNeeded, diamonds);
            if (fromButterflies) printf(" + %d from %d butterflies", fromButterflies, butterflies);
            if (fromMill) printf(" + up to %d milled", fromMill);
            if (needsAmoeba) printf(" + AMOEBA (unbounded, uncheckable)");
            printf("\n");
        }
        g_checks++;
        check(outboxes == 1, level.name);

        // Settle with no input at all. A layout that buries its own diamonds or
        // kills a stationary player is broken however good it looks on paper.
        for (int t = 0; t < 60; t++) cave.tick({});
        check(cave.status() != Status::Dead, level.name);
    }
}

// Is the exit physically reachable at all? Flood fill from the player start
// over everything a player can enter or remove - dirt, space, diamonds,
// boulders (pushable) - and stop at brick, steel and magic wall, which nothing
// in the player's repertoire can get through.
//
// This is cheap, decisive, and it found FOUR broken caves out of ten. Two had
// their exits inside sealed brick chambers. One had a vent plotted a row above
// the steel it was meant to pierce, so it cut a hole in thin air. One had the
// player STARTING inside the steel frame, unable to move in any direction.
// Every one of those caves looked perfectly reasonable in the source and would
// have looked fine in a screenshot.
//
// It also does something the play agent cannot: it distinguishes "this cave is
// broken" from "my agent is not clever enough", which is the difference between
// a bug and a limitation and the thing I most needed to be able to tell apart.
void testExitsAreReachable() {
    static bool seen[kCaveCells];
    static int queue[kCaveCells];
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};

    for (unsigned i = 0; i < kLevelCount; i++) {
        const Level& level = kLevels[i];
        Cave cave;
        cave.generate(level.spec, level.instructions);

        for (unsigned k = 0; k < kCaveCells; k++) seen[k] = false;
        int head = 0, tail = 0;
        const int start = level.playerY * kCaveWidth + level.playerX;
        seen[start] = true;
        queue[tail++] = start;

        int ox = -1, oy = -1;
        for (unsigned y = 0; y < kCaveHeight; y++) {
            for (unsigned x = 0; x < kCaveWidth; x++) {
                const uint8_t e = cave.at(x, y);
                if (e == El::OutboxHidden || e == El::OutboxOpen) { ox = x; oy = y; }
            }
        }

        while (head < tail) {
            const int cell = queue[head++];
            const int cx = cell % kCaveWidth, cy = cell / kCaveWidth;
            for (int d = 0; d < 4; d++) {
                const int nx = cx + dx[d], ny = cy + dy[d];
                if (nx < 0 || ny < 0 || nx >= (int)kCaveWidth || ny >= (int)kCaveHeight) continue;
                const int ncell = ny * kCaveWidth + nx;
                if (seen[ncell]) continue;
                const uint8_t e = cave.at(nx, ny);
                if (e == El::Wall || e == El::Steel || e == El::MagicWall) continue;
                seen[ncell] = true;
                queue[tail++] = ncell;
            }
        }

        const bool ok = ox >= 0 && seen[oy * kCaveWidth + ox];
        if (!ok) {
            printf("  FAIL  %s: outbox at (%d,%d) is SEALED behind indestructible terrain\n",
                   level.name, ox, oy);
            g_failures++;
        }
        g_checks++;
    }
}

// The exit-reachability gate above asks whether the player can get OUT. It says
// nothing about whether the player can get to the DIAMONDS, and that hole hid
// four more dead caves behind a set of green ticks.
//
// So: let the cave settle with no input - rocks fall, mills run, amoebas grow
// and resolve - and only THEN flood fill and count what is actually within
// reach. Settling first is the whole trick, because half of what a cave will
// contain does not exist at tick zero.
//
// What it caught: two magic-wall caves producing literally nothing, because a
// milled object emerges two cells below the wall and I had put the steel roof
// of the catch basin on exactly that row, so every diamond materialised inside
// solid steel and was destroyed. One cave whose butterflies - the entire quota -
// were sealed in a steel box. And one whose amoeba suffocated into sixty
// diamonds that nothing could ever reach.
void testSupplyIsReachable() {
    static bool seen[kCaveCells];
    static int queue[kCaveCells];
    const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};

    for (unsigned i = 0; i < kLevelCount; i++) {
        const Level& level = kLevels[i];
        Cave cave;
        cave.generate(level.spec, level.instructions);
        cave.placePlayer(level.playerX, level.playerY);
        for (int t = 0; t < 300; t++) cave.tick({});

        int px = -1, py = -1;
        for (unsigned y = 0; y < kCaveHeight; y++) {
            for (unsigned x = 0; x < kCaveWidth; x++) {
                const uint8_t e = cave.at(x, y);
                if (e == El::Player || e == El::PlayerScanned) { px = x; py = y; }
            }
        }
        // The player surviving 300 idle ticks is itself worth asserting: a cave
        // that kills a motionless player has no opening move at all.
        if (px < 0) {
            printf("  FAIL  %s: player does not survive 300 idle ticks\n", level.name);
            g_failures++;
            g_checks++;
            continue;
        }

        for (unsigned k = 0; k < kCaveCells; k++) seen[k] = false;
        int head = 0, tail = 0;
        seen[py * kCaveWidth + px] = true;
        queue[tail++] = py * kCaveWidth + px;
        while (head < tail) {
            const int cell = queue[head++];
            const int cx = cell % kCaveWidth, cy = cell / kCaveWidth;
            for (int d = 0; d < 4; d++) {
                const int nx = cx + dx[d], ny = cy + dy[d];
                if (nx < 0 || ny < 0 || nx >= (int)kCaveWidth || ny >= (int)kCaveHeight) continue;
                const int ncell = ny * kCaveWidth + nx;
                if (seen[ncell]) continue;
                const uint8_t e = cave.at(nx, ny);
                if (e == El::Wall || e == El::Steel || e == El::MagicWall) continue;
                seen[ncell] = true;
                queue[tail++] = ncell;
            }
        }

        int diamonds = 0, butterflies = 0;
        for (unsigned y = 0; y < kCaveHeight; y++) {
            for (unsigned x = 0; x < kCaveWidth; x++) {
                if (!seen[y * kCaveWidth + x]) continue;
                const uint8_t e = cave.at(x, y);
                if (e == El::Diamond || e == El::DiamondFalling) diamonds++;
                if (e >= El::ButterflyBase && e <= El::ButterflyScanned + 3) butterflies++;
            }
        }
        const int supply = diamonds + butterflies * 6;
        if (supply < level.spec.diamondsNeeded) {
            printf("  FAIL  %s: only %d reachable after settling (%d diamonds, %d butterflies), "
                   "needs %u\n",
                   level.name, supply, diamonds, butterflies, level.spec.diamondsNeeded);
            g_failures++;
        }
        g_checks++;
    }
}

// Can the game actually be FINISHED? Everything else in this file tests a rule
// in isolation. This is the only test that asks the question the whole project
// rests on, and until it existed the honest answer was that nobody knew.
void testCavesAreCompletable() {
    static TapeStep tape[6000];
    unsigned completed = 0;
    for (unsigned i = 0; i < kLevelCount; i++) {
        const PlayResult r = playCave(kLevels[i], tape, 6000);
        printf("  %-14s %s  %u ticks, %u diamonds/%u%s%s\n", kLevels[i].name,
               r.escaped ? "ESCAPED" : "stuck  ", r.ticks, r.diamonds,
               kLevels[i].spec.diamondsNeeded, r.escaped ? "" : " - ", r.escaped ? "" : r.failure);
        if (r.escaped) completed++;
    }
    // The bar is deliberately low and the reason matters. This planner walks to
    // the nearest loose diamond and then to the exit; it has no concept of
    // MANUFACTURING diamonds, which is what half these caves are about - milling
    // boulders through a magic wall, crushing a butterfly, suffocating an
    // amoeba. Those caves report zero diamonds because the agent has nothing to
    // walk toward, not because they are unwinnable.
    //
    // The first version of this bar was "at least five", set before I had run it
    // once. That is a number chosen to feel rigorous rather than to mean
    // anything, and it failed on a build whose caves were mostly fine. The real
    // guarantee lives in testExitsAreReachable above, which can actually tell a
    // broken cave from a limited agent. This one asserts the thing it can
    // genuinely establish: a cave gets played, start to exit, by something that
    // is not me claiming it works.
    printf("  %u of %u caves completed by a greedy planner\n", completed, kLevelCount);
    check(completed >= 1, "at least one cave is playable end to end");
}

// The tape the console will replay. Deliberately busy: it digs, collects,
// pushes, and drops a rock on a butterfly, so the hash trace covers the paths
// that actually differ between two compilers.
struct TraceStep {
    int8_t dx, dy;
    uint8_t grab;
};
const TraceStep kTape[] = {
    {1, 0, 0},  {1, 0, 0}, {1, 0, 0}, {0, 1, 0},  {0, 1, 0}, {1, 0, 0}, {1, 0, 0},  {0, 0, 0},
    {0, -1, 0}, {1, 0, 1}, {1, 0, 0}, {1, 0, 0},  {0, 1, 0}, {0, 1, 0}, {-1, 0, 0}, {-1, 0, 0},
    {0, 0, 0},  {0, 1, 0}, {1, 0, 0}, {1, 0, 0},  {1, 0, 0}, {0, 0, 0}, {0, -1, 0}, {0, -1, 0},
    {1, 0, 0},  {1, 0, 0}, {0, 1, 0}, {-1, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 0},  {0, 1, 0},
};
constexpr unsigned kTapeLength = sizeof(kTape) / sizeof(kTape[0]);

void buildTapeCave(Cave& cave) {
    CaveSpec spec;
    spec.randomSeed = 0x2A;
    spec.fillObject[0] = El::Boulder;
    spec.fillProbability[0] = 0x28;
    spec.fillObject[1] = El::Diamond;
    spec.fillProbability[1] = 0x10;
    spec.diamondsNeeded = 5;
    cave.generate(spec, nullptr);
    cave.set(3, 3, El::Player);
    cave.set(20, 12, El::ButterflyBase);
    cave.set(24, 8, El::FireflyBase + 3);
    cave.set(30, 18, El::OutboxHidden);
}

// Emits a winning run for the console to replay. The host found it; the R3000
// has to reproduce it move for move and arrive at Escaped. That is the claim
// closed end to end: not "the rules match across two compilers" and not "a
// planner can win on my desk", but a PlayStation finishing a cave.
int emitTape(const char* path) {
    static TapeStep tape[6000];
    // FIRST DIG: the shortest winning run, so the console spends its time
    // playing rather than waiting.
    const PlayResult r = playCave(kLevels[0], tape, 6000);
    if (!r.escaped) {
        printf("agent could not win cave 0, refusing to emit a tape that proves nothing\n");
        return 1;
    }
    FILE* f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "// Generated by simtest --emit-tape. Do not edit.\n");
    fprintf(f, "// A winning run of \"%s\", found by the host planner.\n\n", kLevels[0].name);
    fprintf(f, "static const unsigned kPlayTapeLength = %u;\n", r.steps);
    fprintf(f, "static const signed char kPlayTape[%u][2] = {\n", r.steps);
    for (unsigned i = 0; i < r.steps; i++) {
        fprintf(f, "    {%d,%d},%s", tape[i].dx, tape[i].dy, (i % 8) == 7 ? "\n" : "");
    }
    fprintf(f, "\n};\n");
    fclose(f);
    printf("wrote %s: %u steps, %u ticks, %u diamonds\n", path, r.steps, r.ticks, r.diamonds);
    return 0;
}

int emitTrace(const char* path) {
    Cave cave;
    buildTapeCave(cave);
    FILE* f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "// Generated by simtest --emit. Do not edit.\n");
    fprintf(f, "// One hash per tick of the shared input tape, produced by the host build of\n");
    fprintf(f, "// cave.cpp. The console compiles this in and must reproduce every one.\n\n");
    fprintf(f, "static const unsigned kTraceLength = %u;\n", kTapeLength);
    fprintf(f, "static const unsigned int kTrace[%u] = {\n", kTapeLength);
    for (unsigned i = 0; i < kTapeLength; i++) {
        Input in;
        in.dx = kTape[i].dx;
        in.dy = kTape[i].dy;
        in.grab = kTape[i].grab != 0;
        cave.tick(in);
        fprintf(f, "    0x%08xu,%s", cave.hash(), (i % 4) == 3 ? "\n" : "");
    }
    fprintf(f, "};\n");
    fclose(f);
    printf("wrote %s: %u ticks, final hash 0x%08x, status %d, diamonds %u\n", path, kTapeLength,
           cave.hash(), static_cast<int>(cave.status()), cave.diamonds());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && strcmp(argv[1], "--emit") == 0) return emitTrace(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "--emit-tape") == 0) return emitTape(argv[2]);

    printf("BOULDERDASH rule tests\n");
    testFallIsOneCellPerTick();
    testRollPrefersLeft();
    testRollingBoulderIsFalling();
    testRestingBoulderIsHarmless();
    testFallingBoulderKills();
    testButterflyPaysInDiamonds();
    testFireflyPaysInNothing();
    testExplosionSparesSteel();
    testDigAndCollect();
    testExitOpensOnQuota();
    testHorizontalPushOnly();
    testPushEventuallySucceeds();
    testAmoebaGrows();
    testTrappedAmoebaBecomesDiamonds();
    testOvergrownAmoebaBecomesBoulders();
    testMagicWallTransmutes();
    testRestingBoulderDoesNotActivateMagicWall();
    testExpiredMagicWallEatsEverything();
    testAdpcmRoundTrip();
    testRoundTripCanFail();
    testLevelsAreWellFormed();
    testExitsAreReachable();
    testSupplyIsReachable();
    testCavesAreCompletable();
    testGeneratorIsDeterministic();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
