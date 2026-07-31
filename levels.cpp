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

// 4. Butterflies over a rock shelf. Drop a rock on one and it pays out nine
//    diamonds, which is the only way to make the quota.
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
    BD_FILLED(El::Steel, 13, 13, 14, 6, El::Space),
    BD_LINE(El::Space, 14, 4, 12, LineDir::Right),
    BD_LINE(El::Space, 14, 5, 12, LineDir::Right),
    BD_LINE(El::Boulder, 14, 3, 12, LineDir::Right),
    BD_LINE(El::Steel, 13, 3, 9, LineDir::Down),
    BD_LINE(El::Steel, 26, 3, 9, LineDir::Down),
    BD_PLOT(El::OutboxHidden, 37, 20),
    BD_END,
};

// 6. The amoeba, in a room with one narrow vent. Leave it sealed and it becomes
//    diamonds; let it out and it becomes your problem.
const uint8_t kCave6[] = {
    BD_RECT(El::Steel, 10, 6, 14, 10),
    BD_FILLED(El::Steel, 11, 7, 12, 8, El::Space),
    BD_PLOT(El::Amoeba, 16, 11),
    BD_PLOT(El::Space, 23, 10),
    BD_PLOT(El::Space, 24, 10),
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
    BD_FILLED(El::Steel, 5, 10, 10, 6, El::Space),
    BD_FILLED(El::Steel, 23, 10, 10, 6, El::Space),
    BD_LINE(El::Space, 6, 4, 8, LineDir::Right),
    BD_LINE(El::Space, 24, 4, 8, LineDir::Right),
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
    BD_FILLED(El::Steel, 20, 6, 16, 3, El::Space),
    BD_LINE(El::Boulder, 21, 5, 14, LineDir::Right),
    BD_PLOT(El::ButterflyBase + 1, 23, 7),
    BD_PLOT(El::ButterflyBase + 3, 28, 7),
    BD_PLOT(El::ButterflyBase + 1, 33, 7),
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
const uint8_t kCave10[] = {
    BD_FILLED(El::Steel, 2, 2, 36, 18, El::Space),
    BD_LINE(El::Boulder, 4, 6, 32, LineDir::Right),
    BD_LINE(El::Boulder, 4, 11, 32, LineDir::Right),
    BD_LINE(El::Boulder, 4, 16, 32, LineDir::Right),
    BD_LINE(El::Diamond, 6, 5, 6, LineDir::Right),
    BD_LINE(El::Diamond, 24, 10, 8, LineDir::Right),
    BD_LINE(El::Diamond, 10, 15, 8, LineDir::Right),
    BD_PLOT(El::OutboxHidden, 36, 18),
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
    {"SEALED ROOM",
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
      .diamondsNeeded = 15,
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
