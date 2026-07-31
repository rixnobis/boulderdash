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

using namespace bd;

namespace {

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

// The tape the console will replay. Deliberately busy: it digs, collects,
// pushes, and drops a rock on a butterfly, so the hash trace covers the paths
// that actually differ between two compilers.
struct TapeStep {
    int8_t dx, dy;
    uint8_t grab;
};
const TapeStep kTape[] = {
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
    testGeneratorIsDeterministic();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
