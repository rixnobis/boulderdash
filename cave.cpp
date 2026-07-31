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

#include "cave.hh"

namespace bd {

// ---------------------------------------------------------------- generation

uint8_t Cave::nextRandom(uint8_t& seed1, uint8_t& seed2) {
    // Two 8-bit registers, one constant, carry chained through three adds. The
    // 6502 does the two bit-7 moves with a pair of RORs, which rotate through
    // carry; every reimplementation models that as (x & 1) << 7 on the grounds
    // that the incoming carry contributes nothing. That reproduces the original
    // caves, so it is right in practice - but it is an interpretation of the
    // instruction, not a transcription of it, and worth knowing if a generated
    // cave ever comes out one cell wrong.
    const uint8_t tmp1 = static_cast<uint8_t>((seed1 & 0x01) << 7);
    const uint8_t tmp2 = static_cast<uint8_t>((seed2 >> 1) & 0x7F);

    unsigned r = seed2 + static_cast<unsigned>((seed2 & 0x01) << 7);
    unsigned carry = r > 0xFF ? 1u : 0u;
    r = (r & 0xFF) + carry + 0x13;
    carry = r > 0xFF ? 1u : 0u;
    seed2 = static_cast<uint8_t>(r & 0xFF);

    r = seed1 + carry + tmp1;
    carry = r > 0xFF ? 1u : 0u;
    r = (r & 0xFF) + carry + tmp2;
    seed1 = static_cast<uint8_t>(r & 0xFF);

    return seed1;
}

void Cave::generate(const CaveSpec& spec, const uint8_t* instructions) {
    // The 1984 decoder's actual order, which is not the order most write-ups
    // give. Checked against decodecaves.c rather than taken on description:
    //
    //   1. the whole array is pre-filled with steel
    //   2. the random fill overwrites TWENTY-ONE rows, not twenty
    //   3. the designer's instruction stream runs
    //   4. the steel border rectangle is stamped LAST, over the top of it
    //
    // Two of those are commonly documented the other way round. The row count
    // matters most: the fill consumes one PRNG value per cell across all forty
    // columns, so 21 rows is 840 draws and 20 rows is 800, and getting it wrong
    // desynchronises the sequence for the whole cave. Every cell after the
    // first row would then be plausible and wrong, which is the worst kind.
    for (unsigned i = 0; i < kCaveCells; i++) m_grid[i] = El::Steel;

    uint8_t seed1 = 0;
    uint8_t seed2 = spec.randomSeed;
    for (unsigned y = 1; y < kCaveHeight; y++) {
        for (unsigned x = 0; x < kCaveWidth; x++) {
            uint8_t object = El::Dirt;
            const uint8_t value = nextRandom(seed1, seed2);
            // All four pairs are tested, ascending, with no early exit, so the
            // LAST match wins and overrides the earlier ones. Stopping at the
            // first match is the single most commonly mis-implemented line in
            // this whole format.
            for (unsigned n = 0; n < 4; n++) {
                if (value < spec.fillProbability[n]) object = spec.fillObject[n];
            }
            set(x, y, object);
        }
    }

    if (instructions) {
        for (unsigned i = 0; instructions[i] != 0xFF;) {
            const uint8_t code = instructions[i++];
            const uint8_t object = static_cast<uint8_t>(code & 0x3F);
            switch ((code >> 6) & 3) {
                case 0: {  // PLOT
                    const uint8_t x = instructions[i++];
                    const uint8_t y = instructions[i++];
                    if (x < kCaveWidth && y < kCaveHeight) set(x, y, object);
                    break;
                }
                case 1: {  // LINE
                    const uint8_t x = instructions[i++];
                    const uint8_t y = instructions[i++];
                    const uint8_t length = instructions[i++];
                    const uint8_t dir = instructions[i++];
                    static const int8_t kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
                    static const int8_t kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
                    int cx = x, cy = y;
                    for (unsigned n = 0; n < length; n++) {
                        if (cx >= 0 && cy >= 0 && cx < static_cast<int>(kCaveWidth) &&
                            cy < static_cast<int>(kCaveHeight)) {
                            set(cx, cy, object);
                        }
                        cx += kDx[dir & 7];
                        cy += kDy[dir & 7];
                    }
                    break;
                }
                case 2: {  // FILLED RECT
                    const uint8_t x = instructions[i++];
                    const uint8_t y = instructions[i++];
                    const uint8_t w = instructions[i++];
                    const uint8_t h = instructions[i++];
                    const uint8_t fill = instructions[i++];
                    for (unsigned oy = 0; oy < h; oy++) {
                        for (unsigned ox = 0; ox < w; ox++) {
                            if (x + ox >= kCaveWidth || y + oy >= kCaveHeight) continue;
                            const bool edge = ox == 0 || oy == 0 || ox == w - 1u || oy == h - 1u;
                            set(x + ox, y + oy, edge ? object : fill);
                        }
                    }
                    break;
                }
                default: {  // OPEN RECT
                    const uint8_t x = instructions[i++];
                    const uint8_t y = instructions[i++];
                    const uint8_t w = instructions[i++];
                    const uint8_t h = instructions[i++];
                    for (unsigned n = 0; n < w; n++) {
                        if (x + n >= kCaveWidth) break;
                        if (y < kCaveHeight) set(x + n, y, object);
                        if (y + h - 1u < kCaveHeight) set(x + n, y + h - 1u, object);
                    }
                    for (unsigned n = 0; n < h; n++) {
                        if (y + n >= kCaveHeight) break;
                        if (x < kCaveWidth) set(x, y + n, object);
                        if (x + w - 1u < kCaveWidth) set(x + w - 1u, y + n, object);
                    }
                    break;
                }
            }
        }
    }

    for (unsigned x = 0; x < kCaveWidth; x++) {
        set(x, 0, El::Steel);
        set(x, kCaveHeight - 1, El::Steel);
    }
    for (unsigned y = 0; y < kCaveHeight; y++) {
        set(0, y, El::Steel);
        set(kCaveWidth - 1, y, El::Steel);
    }

    m_status = Status::Playing;
    m_diamonds = 0;
    m_diamondsNeeded = spec.diamondsNeeded;
    m_ticks = 0;
    m_exitOpen = false;
    m_rngA = spec.randomSeed;
    m_rngB = 0x13;
    m_amoebaCount = 0;
    m_amoebaCanGrow = false;
    m_amoebaVerdict = 0;
    m_amoebaSlowGrowthTime = spec.amoebaSlowGrowthTime;
    m_magicWallState = 0;
    m_magicWallTimer = 0;
    m_magicWallMillingTime = spec.magicWallMillingTime;
    m_events = 0;
}

void Cave::placePlayer(unsigned x, unsigned y) {
    set(x, y, El::Player);
    // Dirt overhead, not space: dirt supports whatever is above it, so this
    // stops the column falling rather than merely delaying it by a tick.
    if (y > 0) set(x, y - 1, El::Dirt);
}

// ---------------------------------------------------------------------- rules

bool Cave::slippery(uint8_t e) const {
    // What a boulder can roll off. Not "anything round" - the original tests
    // three specific codes, and a magic wall is deliberately not one of them.
    return e == El::Wall || e == El::Boulder || e == El::BoulderScanned ||
           e == El::Diamond || e == El::DiamondScanned;
}

void Cave::explode(unsigned x, unsigned y, bool toDiamonds) {
    m_events |= Ev::Exploded;
    const uint8_t stage0 = toDiamonds ? El::ExplodeToDiamond : El::ExplodeToSpace;
    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            const int cx = static_cast<int>(x) + ox;
            const int cy = static_cast<int>(y) + oy;
            if (cx < 0 || cy < 0 || cx >= static_cast<int>(kCaveWidth) ||
                cy >= static_cast<int>(kCaveHeight)) {
                continue;
            }
            // Steel is the only thing a blast does not touch. Everything else,
            // including other creatures, is consumed rather than detonated -
            // there is no chain reaction in this game and adding one is the
            // classic way a clone stops feeling like the original.
            if (at(cx, cy) == El::Steel) continue;
            set(cx, cy, stage0);
        }
    }
}

void Cave::processBoulderish(unsigned x, unsigned y, bool falling, bool isDiamond) {
    const uint8_t fallingScanned = isDiamond ? El::DiamondFallingScanned : El::BoulderFallingScanned;
    const uint8_t restingScanned = isDiamond ? El::DiamondScanned : El::BoulderScanned;
    const uint8_t below = at(x, y + 1);

    if (below == El::Space) {
        set(x, y, El::Space);
        set(x, y + 1, fallingScanned);
        return;
    }

    // The magic wall. Only a FALLING object activates it - a boulder that comes
    // to rest on top of one sits there inert forever - and activation is global:
    // every magic wall in the cave starts milling at once, off one timer.
    // The object never occupies the wall cell. One tick it is above, the next it
    // is two cells below, transmuted and already falling.
    if (falling && below == El::MagicWall) {
        if (m_magicWallState == 0) {
            m_magicWallState = 1;
            m_magicWallTimer = m_magicWallMillingTime;
        }
        set(x, y, El::Space);
        if (m_magicWallState == 1 && y + 2 < kCaveHeight && at(x, y + 2) == El::Space) {
            set(x, y + 2, isDiamond ? El::BoulderFallingScanned : El::DiamondFallingScanned);
        }
        // Expired, or nowhere to emerge: the object is simply gone. That is the
        // documented behaviour and not a dropped case.
        return;
    }

    if (falling) {
        // Impact, and only from directly above. There is no check anywhere for
        // something moving sideways into a cell a boulder is passing through,
        // which is why standing under a rock and holding it on your head is
        // safe, and why that is a mechanic rather than a bug.
        if (below == El::Player || below == El::PlayerScanned) {
            m_status = Status::Dead;
            explode(x, y + 1, false);
            return;
        }
        if (below >= El::ButterflyBase && below <= El::ButterflyScanned + 3) {
            explode(x, y + 1, true);  // butterflies pay out in diamonds
            return;
        }
        if (below >= El::FireflyBase && below <= El::FireflyScanned + 3) {
            explode(x, y + 1, false);
            return;
        }
    }

    // Roll. Left is tested before right, unconditionally - not nearest-first,
    // not alternating. Both the side cell and the one diagonally below it must
    // be clear, and the result is the FALLING code even though the movement is
    // sideways, so a rolling rock kills on the way down.
    if (slippery(below)) {
        if (x > 0 && at(x - 1, y) == El::Space && at(x - 1, y + 1) == El::Space) {
            set(x, y, El::Space);
            set(x - 1, y, fallingScanned);
            return;
        }
        if (x + 1 < kCaveWidth && at(x + 1, y) == El::Space && at(x + 1, y + 1) == El::Space) {
            set(x, y, El::Space);
            set(x + 1, y, fallingScanned);
            return;
        }
    }

    // A falling object coming to rest is the thud. A resting one that stays
    // resting is not, or the cave would clatter continuously.
    if (falling) m_events |= Ev::Landed;
    set(x, y, restingScanned);
}

void Cave::processCreature(unsigned x, unsigned y, bool butterfly) {
    const uint8_t base = butterfly ? El::ButterflyBase : El::FireflyBase;
    const uint8_t scannedBase = butterfly ? El::ButterflyScanned : El::FireflyScanned;
    const uint8_t facing = static_cast<uint8_t>(at(x, y) - base);

    static const int8_t kDx[4] = {0, -1, 0, 1};  // up, left, down, right
    static const int8_t kDy[4] = {-1, 0, 1, 0};

    // Adjacency is tested BEFORE the creature moves, which is the whole reason
    // a firefly can be walked past: approach so that it is scanned first, it
    // sees an empty cell, and you have vacated before it looks again.
    for (unsigned d = 0; d < 4; d++) {
        const int cx = static_cast<int>(x) + kDx[d];
        const int cy = static_cast<int>(y) + kDy[d];
        if (cx < 0 || cy < 0 || cx >= static_cast<int>(kCaveWidth) ||
            cy >= static_cast<int>(kCaveHeight)) {
            continue;
        }
        const uint8_t n = at(cx, cy);
        if (n == El::Player || n == El::PlayerScanned || n == El::Amoeba || n == El::AmoebaScanned) {
            m_status = (n == El::Player || n == El::PlayerScanned) ? Status::Dead : m_status;
            explode(x, y, butterfly);
            return;
        }
    }

    // Turn toward the preferred side; if that is free, take it. Otherwise go
    // straight. Otherwise turn the other way and stand still this frame, which
    // is why a full reversal costs two ticks. Firefly prefers left, butterfly
    // prefers right - the clockwise/anticlockwise labels in the literature
    // contradict each other because they describe different reference frames,
    // so this is written as the turn preference and nothing else.
    const uint8_t preferred =
        butterfly ? static_cast<uint8_t>((facing + 3) & 3) : static_cast<uint8_t>((facing + 1) & 3);
    const uint8_t other =
        butterfly ? static_cast<uint8_t>((facing + 1) & 3) : static_cast<uint8_t>((facing + 3) & 3);

    const int px = static_cast<int>(x) + kDx[preferred];
    const int py = static_cast<int>(y) + kDy[preferred];
    if (px >= 0 && py >= 0 && px < static_cast<int>(kCaveWidth) &&
        py < static_cast<int>(kCaveHeight) && at(px, py) == El::Space) {
        set(x, y, El::Space);
        set(px, py, static_cast<uint8_t>(scannedBase + preferred));
        return;
    }

    const int fx = static_cast<int>(x) + kDx[facing];
    const int fy = static_cast<int>(y) + kDy[facing];
    if (fx >= 0 && fy >= 0 && fx < static_cast<int>(kCaveWidth) &&
        fy < static_cast<int>(kCaveHeight) && at(fx, fy) == El::Space) {
        set(x, y, El::Space);
        set(fx, fy, static_cast<uint8_t>(scannedBase + facing));
        return;
    }

    set(x, y, static_cast<uint8_t>(scannedBase + other));
}

void Cave::processPlayer(unsigned x, unsigned y, const Input& in) {
    if (in.dx == 0 && in.dy == 0) {
        set(x, y, El::PlayerScanned);
        return;
    }

    // Diagonals resolve horizontally. The player has four directions, and the
    // original picks the horizontal component when both are pressed.
    const int dx = in.dx != 0 ? in.dx : 0;
    const int dy = in.dx != 0 ? 0 : in.dy;

    const int tx = static_cast<int>(x) + dx;
    const int ty = static_cast<int>(y) + dy;
    if (tx < 0 || ty < 0 || tx >= static_cast<int>(kCaveWidth) ||
        ty >= static_cast<int>(kCaveHeight)) {
        set(x, y, El::PlayerScanned);
        return;
    }

    const uint8_t target = at(tx, ty);
    bool move = false;

    switch (target) {
        case El::Space:
            move = true;
            break;
        case El::Dirt:
            set(tx, ty, El::Space);
            m_events |= Ev::Dug;
            move = true;
            break;
        case El::Diamond:
        case El::DiamondScanned:
        case El::DiamondFalling:
        case El::DiamondFallingScanned:
            // Collectable in mid-fall, which is a real technique and not an
            // accident of the code.
            set(tx, ty, El::Space);
            m_events |= Ev::Collected;
            m_diamonds++;
            if (m_diamonds >= m_diamondsNeeded) m_exitOpen = true;
            move = true;
            break;
        case El::OutboxOpen:
            m_status = Status::Escaped;
            set(x, y, El::Space);
            return;
        case El::Boulder:
        case El::BoulderScanned: {
            // Horizontal only, the cell beyond must be empty, and it succeeds
            // on a one-in-four roll per attempted tick. The disassembly masks
            // the random byte with $03; BDCFF's prose says one in eight. They
            // disagree, and I am taking the instruction over the description -
            // it is the C64 original's own code, and BDCFF documents a family
            // of engines rather than this one. Either way it is a per-tick
            // probability, so it reads as "pushing takes a moment", not as a
            // push that fails.
            if (dy != 0) break;
            const int bx = tx + dx;
            if (bx < 0 || bx >= static_cast<int>(kCaveWidth)) break;
            if (at(bx, ty) != El::Space) break;
            const uint8_t roll = nextRandom(m_rngA, m_rngB);
            if ((roll & 3) != 0) break;
            set(bx, ty, El::Boulder);
            move = true;
            break;
        }
        default:
            break;
    }

    if (!move) {
        set(x, y, El::PlayerScanned);
        return;
    }
    // Grab acts on the target cell without vacating this one: everything
    // happens as though the move occurred, except the move.
    if (in.grab) {
        set(x, y, El::PlayerScanned);
        return;
    }
    set(x, y, El::Space);
    set(tx, ty, El::PlayerScanned);
}

void Cave::processAmoeba(unsigned x, unsigned y) {
    // The verdict from LAST scan lands first. The colony is counted during one
    // scan and judged at the end of it, so the conversion necessarily happens a
    // tick later than the condition that caused it - which is not a rounding
    // error in the design, it is the design.
    if (m_amoebaVerdict != 0) {
        set(x, y, m_amoebaVerdict);
        m_amoebaCount++;
        return;
    }

    m_amoebaCount++;

    static const int8_t kDx[4] = {0, -1, 1, 0};  // up, left, right, down
    static const int8_t kDy[4] = {-1, 0, 0, 1};
    bool canGrow = false;
    for (unsigned d = 0; d < 4; d++) {
        const int cx = static_cast<int>(x) + kDx[d];
        const int cy = static_cast<int>(y) + kDy[d];
        if (cx < 0 || cy < 0 || cx >= static_cast<int>(kCaveWidth) ||
            cy >= static_cast<int>(kCaveHeight)) {
            continue;
        }
        const uint8_t n = at(cx, cy);
        if (n == El::Space || n == El::Dirt) canGrow = true;
    }
    if (canGrow) m_amoebaCanGrow = true;

    // Space and dirt only - it does not eat walls, and it does not eat rock.
    // Roughly 3% per cell per scan, rising to 25% once the slow-growth time has
    // run out, implemented as a mask on a random byte the way the original does
    // it rather than as a percentage, so the distribution is the same shape.
    if (canGrow) {
        const uint8_t mask = m_ticks >= m_amoebaSlowGrowthTime ? 0x0F : 0x7F;
        if ((nextRandom(m_rngA, m_rngB) & mask) < 4) {
            for (unsigned d = 0; d < 4; d++) {
                const int cx = static_cast<int>(x) + kDx[d];
                const int cy = static_cast<int>(y) + kDy[d];
                if (cx < 0 || cy < 0 || cx >= static_cast<int>(kCaveWidth) ||
                    cy >= static_cast<int>(kCaveHeight)) {
                    continue;
                }
                const uint8_t n = at(cx, cy);
                if (n != El::Space && n != El::Dirt) continue;
                // Written as SCANNED so the new cell cannot itself grow in the
                // same scan, which would let the colony race across the cave in
                // one tick in whichever direction the scan happens to run.
                set(cx, cy, El::AmoebaScanned);
                m_amoebaCount++;
                break;
            }
        }
    }

    set(x, y, El::AmoebaScanned);
}

void Cave::scanCell(unsigned x, unsigned y, const Input& in) {
    const uint8_t e = at(x, y);

    if (e >= El::ExplodeToSpace && e <= El::ExplodeToSpace + 4) {
        set(x, y, e == El::ExplodeToSpace + 4 ? El::Space : static_cast<uint8_t>(e + 1));
        return;
    }
    if (e >= El::ExplodeToDiamond && e <= El::ExplodeToDiamond + 4) {
        set(x, y, e == El::ExplodeToDiamond + 4 ? El::Diamond : static_cast<uint8_t>(e + 1));
        return;
    }

    switch (e) {
        case El::Boulder: processBoulderish(x, y, false, false); return;
        case El::BoulderFalling: processBoulderish(x, y, true, false); return;
        case El::Diamond: processBoulderish(x, y, false, true); return;
        case El::DiamondFalling: processBoulderish(x, y, true, true); return;
        case El::Player: processPlayer(x, y, in); return;
        case El::Amoeba: processAmoeba(x, y); return;
        case El::OutboxHidden:
            if (m_exitOpen) set(x, y, El::OutboxOpen);
            return;
        default: break;
    }

    if (e >= El::FireflyBase && e < El::FireflyBase + 4) {
        processCreature(x, y, false);
        return;
    }
    if (e >= El::ButterflyBase && e < El::ButterflyBase + 4) {
        processCreature(x, y, true);
        return;
    }
}

void Cave::clearScannedFlags() {
    // The original has a table mapping scanned codes back to base codes; where
    // it applies that table - a separate pass, or lazily during the following
    // scan - is not documented anywhere I could find, and I did not want to
    // guess quietly. A trailing pass is the reading that makes the table's
    // existence necessary, so that is what this does. Recorded as chosen rather
    // than measured, because the two are indistinguishable from the outside and
    // that is exactly when a note is worth more than a comment saying "correct".
    for (unsigned i = 0; i < kCaveCells; i++) {
        uint8_t& e = m_grid[i];
        if (e >= El::FireflyScanned && e < El::FireflyScanned + 4) {
            e = static_cast<uint8_t>(e - 4);
        } else if (e >= El::ButterflyScanned && e < El::ButterflyScanned + 4) {
            e = static_cast<uint8_t>(e - 4);
        } else if (e == El::BoulderScanned || e == El::BoulderFallingScanned ||
                   e == El::DiamondScanned || e == El::DiamondFallingScanned ||
                   e == El::PlayerScanned || e == El::AmoebaScanned) {
            e = static_cast<uint8_t>(e - 1);
        }
    }
}

void Cave::tick(const Input& in) {
    // The cave does NOT stop when the player dies. Rockford's death is a state
    // of Rockford, not of the world: the explosion that killed him still has
    // five stages to run, the rocks it dislodged still fall, and a butterfly
    // caught in it still pays out. Freezing the scan here was my first version
    // and it hid a whole class of behaviour behind an early return - including,
    // embarrassingly, making a test pass by ensuring nothing happened at all.
    // Only an escape ends the simulation, because at that point the player is
    // no longer in the grid to simulate.
    if (m_status == Status::Escaped) return;
    m_events = 0;

    // Row-major, top to bottom, left to right. Everything about how this game
    // feels comes out of that one line. A rock that falls into a cell the scan
    // has not reached yet would be processed a second time in the same tick and
    // fall two cells; the scanned mark is what stops it, and the mark only
    // makes sense because the scan runs downward.
    for (unsigned y = 0; y < kCaveHeight; y++) {
        for (unsigned x = 0; x < kCaveWidth; x++) {
            scanCell(x, y, in);
        }
    }

    // The colony's verdict, decided now and applied by the next scan. Two
    // conditions, and they can hold at once - engines disagree about which wins
    // and I am taking BD2's answer, overgrown before trapped, because the target
    // here is the 1984 game and not GDash's reading of it.
    if (m_amoebaVerdict == 0 && m_amoebaCount > 0) {
        if (m_amoebaCount >= 200) {
            m_amoebaVerdict = El::Boulder;
        } else if (!m_amoebaCanGrow) {
            m_amoebaVerdict = El::Diamond;
        }
    } else if (m_amoebaCount == 0) {
        m_amoebaVerdict = 0;
    }
    m_amoebaCount = 0;
    m_amoebaCanGrow = false;

    if (m_magicWallState == 1) {
        if (m_magicWallTimer == 0) {
            m_magicWallState = 2;
        } else {
            m_magicWallTimer--;
        }
    }

    clearScannedFlags();
    m_ticks++;
}

uint32_t Cave::hash() const {
    uint32_t h = 2166136261u;
    auto mix = [&h](uint8_t byte) {
        h ^= byte;
        h *= 16777619u;
    };
    for (unsigned i = 0; i < kCaveCells; i++) mix(m_grid[i]);
    mix(static_cast<uint8_t>(m_status));
    mix(static_cast<uint8_t>(m_diamonds));
    mix(static_cast<uint8_t>(m_diamonds >> 8));
    mix(static_cast<uint8_t>(m_exitOpen ? 1 : 0));
    mix(m_rngA);
    mix(m_rngB);
    mix(m_events);
    mix(m_amoebaVerdict);
    mix(m_magicWallState);
    mix(static_cast<uint8_t>(m_magicWallTimer));
    return h;
}

}  // namespace bd
