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

// An agent that plays a cave, so that "this is a game" stops being an assertion.
//
// Every picture of this project so far has been a cave settling under its own
// physics with nobody at the controls. The simulation is verified tick for tick
// against an R3000, the audio is verified live at the SPU, the tiles go through
// the real GPU path - and not one input had ever been exercised end to end, so
// the single claim the whole thing rests on, that a cave can be COMPLETED, was
// the one thing never measured.
//
// This is not an AI and is not trying to be. It is a greedy planner: breadth
// first to the nearest thing it wants, one step per tick, re-planned every tick
// because the cave moves underneath it. It plays badly. That is fine - the
// question is whether the game can be finished at all, and a bad player that
// finishes answers it.
//
// Host only. The console never runs this; it replays the tape the agent found.

#pragma once

#include "cave.hh"
#include "levels.hh"

namespace bd {

struct TapeStep {
    int8_t dx, dy;
};

struct PlayResult {
    bool escaped = false;
    unsigned ticks = 0;
    unsigned diamonds = 0;
    unsigned steps = 0;
    const char* failure = "";
};

// Plays `level` from its start, writing the inputs it used into `tape` (at most
// `tapeCapacity` entries). Returns what happened.
PlayResult playCave(const Level& level, TapeStep* tape, unsigned tapeCapacity,
                    unsigned maxTicks = 4000);

}  // namespace bd
