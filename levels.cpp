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
};

const unsigned kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

}  // namespace bd
