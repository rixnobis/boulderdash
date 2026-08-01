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

#include "levels.hh"

namespace bd {

namespace {

// 1. Nothing but rocks and dirt. Teaches that a rock falls when you dig under
//    it, and that standing beneath a resting one is safe.
const uint8_t kCave1[] = {
    BD_FILLED(El::Wall, 4, 4, 14, 6, El::Space),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 2. A rock wall you have to undermine, and the exit behind it. The only route
//    is to make the pile above fall by taking its support out from the side.
const uint8_t kCave2[] = {
    BD_LINE(El::Steel, 20, 2, 16, LineDir::Down),
    BD_LINE(El::Boulder, 21, 2, 16, LineDir::Down),
    BD_FILLED(El::Wall, 24, 6, 12, 8, El::Space),
    // The way in. Brick is not diggable, so without this the chamber - and the
    // exit inside it - is sealed for good.
    BD_PLOT(El::Dirt, 24, 10),
    BD_PLOT(El::OutboxHidden, 34, 11),
    BD_LINE(El::Diamond, 26, 8, 8, LineDir::Right),
    BD_END,
};

// 3. Fireflies in corridors. They wall-follow, so the corridors decide their
//    route entirely and the puzzle is timing, not aim.
const uint8_t kCave3[] = {
    BD_RECT(El::Wall, 6, 4, 12, 8),
    BD_RECT(El::Wall, 22, 10, 14, 9),
    BD_PLOT(El::FireflyBase + 3, 8, 6),
    BD_PLOT(El::FireflyBase + 1, 15, 9),
    BD_PLOT(El::FireflyBase + 0, 30, 13),
    BD_LINE(El::Diamond, 8, 8, 8, LineDir::Right),
    BD_LINE(El::Diamond, 24, 13, 10, LineDir::Right),
    BD_PLOT(El::Dirt, 22, 14),
    BD_PLOT(El::OutboxHidden, 34, 17),
    BD_END,
};

// 4. Butterflies under an unsupported rock shelf, and the introduction to what
//    a butterfly is worth. The rocks have nothing holding them, so they fall on
//    tick one and the payout happens whether the player does anything or not.
//
//    That is deliberate NOW and was an accident before: the comment used to say
//    "drop a rock on one, which is the only way to make the quota", describing
//    a move the player never makes. Kept as-is rather than fixed, because it
//    earns its place as the gentle version - this cave shows you that a crushed
//    butterfly becomes diamonds, and NEST later makes you arrange it yourself
//    with a dirt support to dig and a firefly nest next door. Two caves, one
//    mechanic, taught then tested.
//
//    The comment was the thing that was wrong, not the layout.
const uint8_t kCave4[] = {
    BD_FILLED(El::Steel, 8, 12, 24, 2, El::Space),
    BD_LINE(El::Boulder, 9, 5, 20, LineDir::Right),
    BD_LINE(El::Space, 9, 6, 20, LineDir::Right),
    BD_LINE(El::Space, 9, 7, 20, LineDir::Right),
    BD_PLOT(El::ButterflyBase + 1, 12, 8),
    BD_PLOT(El::ButterflyBase + 3, 20, 8),
    BD_PLOT(El::ButterflyBase + 1, 27, 8),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 5. A magic wall with a rock shaft over it and a hollow under it. Every rock
//    you feed it becomes a diamond, and the timer starts on the FIRST one - so
//    the cave is really about getting the shaft ready before you commit.
const uint8_t kCave5[] = {
    BD_LINE(El::MagicWall, 14, 12, 12, LineDir::Right),
    // The catch basin must be OPEN at the row directly under the wall. A milled
    // object emerges two cells below the boulder that fed it - never inside the
    // wall - so a steel roof one row down is exactly where every diamond
    // materialises and is destroyed. The first version had that roof, and the
    // cave produced zero diamonds while looking like a magic-wall bug.
    BD_FILLED(El::Space, 13, 13, 14, 6, El::Space),
    // The shaft has to reach the wall. The first version cleared two rows and
    // the boulders came to rest on fill dirt seven rows short of the mill, so
    // the cave produced nothing at all and looked like a magic-wall bug.
    BD_FILLED(El::Space, 14, 4, 12, 8, El::Space),
    BD_LINE(El::Boulder, 14, 3, 12, LineDir::Right),
    BD_LINE(El::Steel, 13, 3, 9, LineDir::Down),
    BD_LINE(El::Steel, 26, 3, 9, LineDir::Down),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 6. The amoeba loose in the cave, and a race. The quota is in loose diamonds;
//    the amoeba is purely the clock. It will pass two hundred cells and turn the
//    place to boulders, and you want to be somewhere else when it does.
//
//    This cave began as a sealed room and the premise did not survive contact
//    with the rules. An amoeba grows into dirt as readily as into space, so only
//    indestructible terrain contains it - and terrain that contains the amoeba
//    also contains the diamonds it suffocates into. Any vent large enough to let
//    the player in lets the amoeba out, whereupon it exceeds two hundred cells
//    and becomes boulders instead. There was no version of the original idea
//    that could be won; it was not a layout bug.
const uint8_t kCave6[] = {
    BD_PLOT(El::Amoeba, 20, 4),
    BD_LINE(El::Diamond, 6, 16, 12, LineDir::Right),
    BD_LINE(El::Diamond, 24, 18, 10, LineDir::Right),
    BD_LINE(El::Boulder, 26, 4, 10, LineDir::Right),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 7. Two magic walls fed from one shaft, and the diamonds you need are on the
//    wrong side of a rock wall you have to push through. The timer is shared
//    across both walls, so committing to one commits to both.
const uint8_t kCave7[] = {
    BD_LINE(El::MagicWall, 6, 9, 8, LineDir::Right),
    BD_LINE(El::MagicWall, 24, 9, 8, LineDir::Right),
    BD_FILLED(El::Space, 5, 10, 10, 6, El::Space),
    BD_FILLED(El::Space, 23, 10, 10, 6, El::Space),
    BD_FILLED(El::Space, 6, 4, 8, 5, El::Space),
    BD_FILLED(El::Space, 24, 4, 8, 5, El::Space),
    BD_LINE(El::Boulder, 6, 3, 8, LineDir::Right),
    BD_LINE(El::Boulder, 24, 3, 8, LineDir::Right),
    BD_PLOT(El::OutboxHidden, 19, 20),
    BD_END,
};

// 8. A firefly nest sealed behind brick, and butterflies in the open. The
//    fireflies are worth nothing and will kill you; the butterflies are the
//    entire quota. Everything about this cave is deciding which to disturb.
const uint8_t kCave8[] = {
    BD_RECT(El::Wall, 4, 3, 10, 8),
    BD_PLOT(El::FireflyBase + 3, 6, 5),
    BD_PLOT(El::FireflyBase + 1, 11, 8),
    BD_PLOT(El::FireflyBase + 0, 8, 6),
    // The chamber has to be TALL enough to hold both the butterflies and the
    // rocks that kill them, with diggable material between. The first version
    // was three rows - one interior row - with the boulders sitting on the
    // steel ROOF, so nothing could ever fall on the butterflies and the cave's
    // entire quota was uncollectable. It passed every gate, because the gate
    // credited a reachable butterfly with six diamonds without ever asking
    // whether anything could reach it from above.
    //
    // Interior is dirt, not space, so the boulders rest until the player digs
    // the support out. That IS the cave: choosing which rock to drop and from
    // where, with a firefly nest next door that pays nothing and kills you.
    // Layered on purpose, and the layering is the puzzle. Boulders on row 5,
    // their DIRT support on row 6, open air on row 7, butterflies on row 8.
    // The player digs the support from the side, steps away, and the rock
    // drops two rows onto the butterfly.
    //
    // The gap on row 7 is load-bearing in the literal sense. Fill it with dirt
    // and the only way to drop the rock is to clear that row too - which puts
    // the player orthogonally adjacent to a butterfly, and adjacency is death.
    // The cave would then be crushable, pass every gate, and still kill anyone
    // who tried to win it.
    BD_FILLED(El::Steel, 20, 4, 16, 6, El::Space),
    BD_LINE(El::Boulder, 22, 5, 12, LineDir::Right),
    BD_LINE(El::Dirt, 21, 6, 14, LineDir::Right),
    // The door, and the APPROACH to it. Piercing the steel is not enough: the
    // fill dropped a boulder square in front of the opening, and a boulder is
    // only passable if the cell beyond it is empty, so the door was shut from
    // the outside by one rock the player could not move.
    BD_LINE(El::Dirt, 17, 6, 4, LineDir::Right),
    BD_PLOT(El::ButterflyBase + 1, 23, 8),
    BD_PLOT(El::ButterflyBase + 3, 28, 8),
    BD_PLOT(El::ButterflyBase + 1, 33, 8),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 9. Amoeba above, exit below, and a rock floor between them. The amoeba will
//    reach two hundred cells and turn to boulders long before you can seal it,
//    so the intended answer is to let it - and be somewhere else when it does.
const uint8_t kCave9[] = {
    BD_FILLED(El::Steel, 8, 2, 24, 8, El::Space),
    BD_PLOT(El::Amoeba, 20, 5),
    BD_LINE(El::Boulder, 9, 10, 22, LineDir::Right),
    BD_FILLED(El::Steel, 6, 13, 28, 7, El::Space),
    BD_LINE(El::Diamond, 8, 17, 24, LineDir::Right),
    BD_PLOT(El::OutboxHidden, 32, 18),
    // Pierce the steel roof of the lower chamber. This was at y=12 - one row
    // ABOVE the rectangle it was supposed to open - so it cut a hole in thin
    // air and left the exit sealed behind unbreakable steel.
    BD_PLOT(El::Space, 20, 13),
    BD_END,
};

// 10. No dirt at all. Every cell is rock, diamond or air, so nothing can be
//     tunnelled and the only tool left is which rock you push and when.
// Rewritten. The first version was three unsupported boulder rows over open
// space: everything fell on tick one into a single heap at the bottom, burying
// its own diamonds, and only six of twenty-three were ever reachable again.
// A cave with no dirt needs STEEL to hold its shape, because dirt was the only
// other thing doing that job.
//
// Shelves alternate their gap so the route down zigzags, and the boulders sit
// ON the shelves rather than in mid-air - so they stay put until pushed, which
// is the entire mechanic this cave exists for.
const uint8_t kCave10[] = {
    BD_LINE(El::Steel, 3, 7, 30, LineDir::Right),
    BD_LINE(El::Steel, 7, 12, 30, LineDir::Right),
    BD_LINE(El::Steel, 3, 17, 30, LineDir::Right),
    BD_LINE(El::Boulder, 6, 6, 20, LineDir::Right),
    BD_LINE(El::Boulder, 10, 11, 20, LineDir::Right),
    BD_LINE(El::Boulder, 6, 16, 20, LineDir::Right),
    BD_LINE(El::Diamond, 27, 6, 5, LineDir::Right),
    BD_LINE(El::Diamond, 8, 11, 2, LineDir::Right),
    BD_LINE(El::Diamond, 27, 16, 6, LineDir::Right),
    BD_LINE(El::Diamond, 4, 6, 2, LineDir::Right),
    BD_LINE(El::Diamond, 31, 11, 5, LineDir::Right),
    // Placed where the flood fill says the player can actually stand after the
    // cave settles, rather than where it looked tidy. At (36,18) the planner
    // collected the whole quota, sixteen of sixteen, and then spent four
    // thousand ticks unable to reach an exit five cells away.
    BD_PLOT(El::OutboxHidden, 32, 18),
    BD_END,
};

}  // namespace

const Level kLevels[] = {
    {"FIRST DIG",
     {.randomSeed = 0x14,
      .fillObject = {El::Boulder, El::Diamond, El::Dirt, El::Dirt},
      .fillProbability = {0x18, 0x0C, 0, 0},
      .diamondsNeeded = 6,
      .magicWallMillingTime = 0,
      .timeLimit = 150,
      .amoebaSlowGrowthTime = 0},
     kCave1,
     2,
     2},
    {"UNDERMINE",
     {.randomSeed = 0x27,
      .fillObject = {El::Boulder, El::Diamond, El::Dirt, El::Dirt},
      .fillProbability = {0x30, 0x0A, 0, 0},
      .diamondsNeeded = 10,
      .magicWallMillingTime = 0,
      .timeLimit = 180,
      .amoebaSlowGrowthTime = 0},
     kCave2,
     2,
     2},
    {"WALL FOLLOWERS",
     {.randomSeed = 0x3B,
      .fillObject = {El::Boulder, El::Diamond, El::Dirt, El::Dirt},
      .fillProbability = {0x20, 0x0C, 0, 0},
      .diamondsNeeded = 14,
      .magicWallMillingTime = 0,
      .timeLimit = 200,
      .amoebaSlowGrowthTime = 0},
     kCave3,
     2,
     2},
    {"PAYLOAD",
     {.randomSeed = 0x51,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x28, 0, 0, 0},
      .diamondsNeeded = 12,
      .magicWallMillingTime = 0,
      .timeLimit = 200,
      .amoebaSlowGrowthTime = 0},
     kCave4,
     2,
     2},
    {"THE MILL",
     {.randomSeed = 0x6D,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x22, 0, 0, 0},
      .diamondsNeeded = 10,
      .magicWallMillingTime = 90,
      .timeLimit = 220,
      .amoebaSlowGrowthTime = 0},
     kCave5,
     2,
     2},
    {"THE BLOOM CLOCK",
     {.randomSeed = 0x82,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x1E, 0, 0, 0},
      .diamondsNeeded = 16,
      .magicWallMillingTime = 0,
      .timeLimit = 240,
      .amoebaSlowGrowthTime = 120},
     kCave6,
     2,
     2},
    {"TWIN MILLS",
     {.randomSeed = 0x9A,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x26, 0, 0, 0},
      .diamondsNeeded = 14,
      .magicWallMillingTime = 70,
      .timeLimit = 240,
      .amoebaSlowGrowthTime = 0},
     kCave7,
     2,
     2},
    // No diamonds in the fill: the butterflies really are the entire quota,
    // three of them at roughly six diamonds each. The first version seeded
    // diamonds at 0x06 and the validity check reported the quota as met from
    // loose diamonds alone - which meant the cave's own comment was a lie and
    // the butterflies were decoration. Worth having a check that reads the
    // design rather than only the arithmetic.
    {"NEST",
     {.randomSeed = 0xB3,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x1C, 0, 0, 0},
      // Twelve, not fifteen. The conservative count is six diamonds per
      // butterfly and only two of the three are reliably reachable after the
      // cave settles, so a quota of fifteen demanded the third and the gate was
      // right to refuse it.
      .diamondsNeeded = 12,
      .magicWallMillingTime = 0,
      .timeLimit = 260,
      .amoebaSlowGrowthTime = 0},
     kCave8,
     2,
     2},
    {"BLOOM",
     {.randomSeed = 0xC7,
      .fillObject = {El::Boulder, El::Dirt, El::Dirt, El::Dirt},
      .fillProbability = {0x1A, 0, 0, 0},
      .diamondsNeeded = 20,
      .magicWallMillingTime = 0,
      .timeLimit = 280,
      .amoebaSlowGrowthTime = 200},
     kCave9,
     2,
     2},
    {"NO DIRT",
     {.randomSeed = 0xE1,
      .fillObject = {El::Space, El::Boulder, El::Diamond, El::Dirt},
      .fillProbability = {0xFF, 0x30, 0x08, 0},
      .diamondsNeeded = 16,
      .magicWallMillingTime = 0,
      .timeLimit = 300,
      .amoebaSlowGrowthTime = 0},
     kCave10,
     4,
     4},
};

const unsigned kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

}  // namespace bd
