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

// The cave simulation.
//
// No psyqo, no platform headers, no floating point. This compiles for the host
// and for the R3000 out of one source, and the differential test runs the same
// input tape through both and compares a hash of the grid every tick. That is
// the only way I know to be sure the console and the desk agree, and the rules
// below are the kind where being subtly wrong looks completely fine.
//
// Element codes are the C64 originals rather than a tidy enum of my own. That
// is not reverence - the low bit of the code IS the moved-this-scan mark, which
// makes the whole scheme legible only if the numbers stay put.
//
//   $00 space   $01 dirt   $02 wall   $03 magic wall
//   $04 outbox (hidden)    $05 outbox (open)        $07 steel
//   $08-$0B firefly           $0C-$0F firefly scanned
//   $10 boulder   $11 scanned    $12 boulder falling   $13 scanned
//   $14 diamond   $15 scanned    $16 diamond falling   $17 scanned
//   $1B-$1F explode to space     $20-$24 explode to diamond
//   $30-$33 butterfly         $34-$37 butterfly scanned
//   $38 player  $39 scanned   $3A amoeba  $3B scanned

#pragma once

#include <stdint.h>

namespace bd {

constexpr unsigned kCaveWidth = 40;
constexpr unsigned kCaveHeight = 22;
constexpr unsigned kCaveCells = kCaveWidth * kCaveHeight;

namespace El {
enum : uint8_t {
    Space = 0x00,
    Dirt = 0x01,
    Wall = 0x02,
    MagicWall = 0x03,
    OutboxHidden = 0x04,
    OutboxOpen = 0x05,
    Steel = 0x07,

    FireflyBase = 0x08,      // +0 up, +1 left, +2 down, +3 right
    FireflyScanned = 0x0C,
    Boulder = 0x10,
    BoulderScanned = 0x11,
    BoulderFalling = 0x12,
    BoulderFallingScanned = 0x13,
    Diamond = 0x14,
    DiamondScanned = 0x15,
    DiamondFalling = 0x16,
    DiamondFallingScanned = 0x17,

    ExplodeToSpace = 0x1B,   // five stages, $1B..$1F, then Space
    ExplodeToDiamond = 0x20, // five stages, $20..$24, then Diamond

    ButterflyBase = 0x30,
    ButterflyScanned = 0x34,
    Player = 0x38,
    PlayerScanned = 0x39,
    Amoeba = 0x3A,
    AmoebaScanned = 0x3B,
};
}  // namespace El

// Facing, in the order the creature codes encode it.
enum class Dir : uint8_t { Up = 0, Left = 1, Down = 2, Right = 3 };

struct Input {
    int8_t dx = 0;  // -1, 0, +1
    int8_t dy = 0;
    bool grab = false;  // fire: act in that direction without vacating the cell
};

enum class Status : uint8_t {
    Playing,
    Dead,
    Escaped,
};

// The cave header fields the simulation actually reads. The C64 packs these
// into a 32-byte block; nothing here needs that layout, only the values.
struct CaveSpec {
    uint8_t randomSeed = 0;         // seed2; seed1 is always zero
    uint8_t fillObject[4] = {El::Dirt, El::Dirt, El::Dirt, El::Dirt};
    uint8_t fillProbability[4] = {0, 0, 0, 0};
    uint16_t diamondsNeeded = 10;
    uint16_t magicWallMillingTime = 0;
    uint16_t timeLimit = 150;
    uint8_t amoebaSlowGrowthTime = 0;  // shared with magic wall milling time
};

class Cave {
  public:
    // Generates the random fill exactly the way the 1984 decoder does, then
    // applies `instructions` (a PLOT/LINE/RECT stream, terminated by 0xFF),
    // then stamps the steel border. See cave.cpp for why that order is not the
    // order most descriptions of it give.
    void generate(const CaveSpec& spec, const uint8_t* instructions);

    // One scan. `in` is the player's intent for this tick.
    void tick(const Input& in);

    uint8_t at(unsigned x, unsigned y) const { return m_grid[y * kCaveWidth + x]; }
    void set(unsigned x, unsigned y, uint8_t e) { m_grid[y * kCaveWidth + x] = e; }

    Status status() const { return m_status; }
    uint16_t diamonds() const { return m_diamonds; }
    uint32_t ticks() const { return m_ticks; }
    bool exitOpen() const { return m_exitOpen; }
    uint16_t amoebaCount() const { return m_amoebaCount; }
    uint8_t magicWallState() const { return m_magicWallState; }

    // FNV-1a over the whole grid plus the scalar state. This is what the host
    // and the console compare, so it has to cover everything a divergence could
    // hide in - a grid-only hash would miss a desynchronised diamond count.
    uint32_t hash() const;

    // Deterministic PRNG, two 8-bit registers and the constant 19, exactly as
    // the C64 used it for cave fill. Exposed because the fill is worth testing
    // on its own.
    static uint8_t nextRandom(uint8_t& seed1, uint8_t& seed2);

  private:
    void scanCell(unsigned x, unsigned y, const Input& in);
    void processBoulderish(unsigned x, unsigned y, bool falling, bool isDiamond);
    void processCreature(unsigned x, unsigned y, bool butterfly);
    void processPlayer(unsigned x, unsigned y, const Input& in);
    void processAmoeba(unsigned x, unsigned y);
    void explode(unsigned x, unsigned y, bool toDiamonds);
    bool slippery(uint8_t e) const;
    void clearScannedFlags();

    uint8_t m_grid[kCaveCells] = {};
    Status m_status = Status::Playing;
    uint16_t m_diamonds = 0;
    uint16_t m_diamondsNeeded = 0;
    uint32_t m_ticks = 0;
    bool m_exitOpen = false;
    // The push is probabilistic, so the simulation carries its own generator
    // and it must be part of the hashed state or the two arms desynchronise on
    // the first push and the hash says "grid differs" instead of "RNG differs".
    uint8_t m_rngA = 0;
    uint8_t m_rngB = 0;

    // The amoeba cannot decide its own fate one cell at a time: whether it
    // turns to diamonds or to boulders is a property of the colony. So the scan
    // accumulates, and the verdict is applied on the FOLLOWING scan - which is
    // the only reason a growth rule that looks local is not.
    uint16_t m_amoebaCount = 0;
    bool m_amoebaCanGrow = false;
    uint8_t m_amoebaVerdict = 0;  // 0 none, El::Diamond, or El::Boulder

    // One global timer for every magic wall in the cave, not one per wall.
    uint8_t m_magicWallState = 0;  // 0 dormant, 1 milling, 2 expired
    uint16_t m_magicWallTimer = 0;
    uint16_t m_magicWallMillingTime = 0;
    uint8_t m_amoebaSlowGrowthTime = 0;
};

}  // namespace bd
