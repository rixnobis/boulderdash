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

#include "agent.hh"

namespace bd {

namespace {

constexpr int kDx[4] = {0, 0, -1, 1};
constexpr int kDy[4] = {-1, 1, 0, 0};

bool findPlayer(const Cave& cave, int& px, int& py) {
    for (unsigned y = 0; y < kCaveHeight; y++) {
        for (unsigned x = 0; x < kCaveWidth; x++) {
            const uint8_t e = cave.at(x, y);
            if (e == El::Player || e == El::PlayerScanned) {
                px = static_cast<int>(x);
                py = static_cast<int>(y);
                return true;
            }
        }
    }
    return false;
}

// What the planner is willing to move into, from a given direction. Direction
// matters because a boulder can be pushed HORIZONTALLY and only into an empty
// cell - so passability is a property of the move, not of the cell, which is
// why this is not a simple predicate on (x, y).
//
// The first version refused boulders entirely on the grounds that a push is a
// one-in-four roll and a plan resting on a coin flip is not a plan. That was
// wrong for a re-planning agent: it retries every tick, so a one-in-four move
// costs four ticks on average rather than failing. Refusing it made six of ten
// caves unwinnable for reasons that were about the planner, not the game.
//
// Brick wall is deliberately absent. It is not diggable, which is exactly the
// distinction that turned up two caves with their exits sealed inside one.
bool passable(const Cave& cave, int fromX, int fromY, int dx, int dy) {
    const int x = fromX + dx, y = fromY + dy;
    if (x <= 0 || y <= 0 || x >= static_cast<int>(kCaveWidth) - 1 ||
        y >= static_cast<int>(kCaveHeight) - 1) {
        return false;
    }
    const uint8_t e = cave.at(x, y);
    if (e == El::Space || e == El::Dirt || e == El::Diamond || e == El::DiamondScanned ||
        e == El::OutboxOpen) {
        return true;
    }
    if ((e == El::Boulder || e == El::BoulderScanned) && dy == 0) {
        const int beyond = x + dx;
        if (beyond > 0 && beyond < static_cast<int>(kCaveWidth) - 1) {
            return cave.at(beyond, y) == El::Space;
        }
    }
    return false;
}

bool dangerous(const Cave& cave, int x, int y) {
    for (int d = 0; d < 4; d++) {
        const int cx = x + kDx[d], cy = y + kDy[d];
        if (cx < 0 || cy < 0 || cx >= static_cast<int>(kCaveWidth) ||
            cy >= static_cast<int>(kCaveHeight)) {
            continue;
        }
        const uint8_t e = cave.at(cx, cy);
        if (e >= El::FireflyBase && e <= El::FireflyScanned + 3) return true;
        if (e >= El::ButterflyBase && e <= El::ButterflyScanned + 3) return true;
        if (e == El::Amoeba || e == El::AmoebaScanned) return true;
    }
    // A falling object directly overhead lands on this cell next tick.
    if (y > 0) {
        const uint8_t above = cave.at(x, y - 1);
        if (above == El::BoulderFalling || above == El::BoulderFallingScanned ||
            above == El::DiamondFalling || above == El::DiamondFallingScanned) {
            return true;
        }
    }
    return false;
}

// Breadth first from the player. Returns the first step of the shortest route
// to any cell the predicate accepts, or -1.
template <typename Want>
int firstStepToward(const Cave& cave, int px, int py, Want want) {
    static int8_t firstStep[kCaveCells];
    static uint8_t seen[kCaveCells];
    static int queue[kCaveCells];
    for (unsigned i = 0; i < kCaveCells; i++) {
        seen[i] = 0;
        firstStep[i] = -1;
    }

    int head = 0, tail = 0;
    seen[py * kCaveWidth + px] = 1;
    queue[tail++] = py * kCaveWidth + px;

    while (head < tail) {
        const int cell = queue[head++];
        const int cx = cell % kCaveWidth, cy = cell / kCaveWidth;
        for (int d = 0; d < 4; d++) {
            const int nx = cx + kDx[d], ny = cy + kDy[d];
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(kCaveWidth) ||
                ny >= static_cast<int>(kCaveHeight)) {
                continue;
            }
            const int ncell = ny * kCaveWidth + nx;
            if (seen[ncell]) continue;
            if (!passable(cave, cx, cy, kDx[d], kDy[d])) continue;
            if (dangerous(cave, nx, ny)) continue;
            seen[ncell] = 1;
            const int start = py * static_cast<int>(kCaveWidth) + px;
            firstStep[ncell] = (cell == start) ? static_cast<int8_t>(d) : firstStep[cell];
            if (want(nx, ny)) return firstStep[ncell];
            queue[tail++] = ncell;
        }
    }
    return -1;
}

}  // namespace

PlayResult playCave(const Level& level, TapeStep* tape, unsigned tapeCapacity, unsigned maxTicks) {
    PlayResult result;
    Cave cave;
    cave.generate(level.spec, level.instructions);
    cave.placePlayer(level.playerX, level.playerY);

    for (unsigned t = 0; t < maxTicks; t++) {
        if (cave.status() == Status::Escaped) {
            result.escaped = true;
            break;
        }
        if (cave.status() == Status::Dead) {
            result.failure = "died";
            break;
        }
        if (result.steps >= tapeCapacity) {
            result.failure = "tape full";
            break;
        }

        int px = 0, py = 0;
        if (!findPlayer(cave, px, py)) {
            result.failure = "no player in the cave";
            break;
        }

        int step;
        if (cave.exitOpen()) {
            step = firstStepToward(cave, px, py, [&](int x, int y) {
                return cave.at(x, y) == El::OutboxOpen;
            });
        } else {
            step = firstStepToward(cave, px, py, [&](int x, int y) {
                const uint8_t e = cave.at(x, y);
                return e == El::Diamond || e == El::DiamondScanned;
            });
        }

        Input in;
        if (step >= 0) {
            in.dx = static_cast<int8_t>(kDx[step]);
            in.dy = static_cast<int8_t>(kDy[step]);
        }
        // No route: stand still and let the cave move. Rocks settle, creatures
        // wander off, and a route that did not exist this tick often exists a
        // few ticks later. Standing still is a legitimate move in this game.
        tape[result.steps].dx = in.dx;
        tape[result.steps].dy = in.dy;
        result.steps++;
        cave.tick(in);
        result.ticks = cave.ticks();
        result.diamonds = cave.diamonds();
    }

    if (!result.escaped && result.failure[0] == '\0') result.failure = "ran out of ticks";
    return result;
}

}  // namespace bd
