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

// The console half of the differential.
//
// It links the same cave.cpp the host tests link, replays the same input tape,
// and has to reproduce the host's hash at every single tick. Not just the final
// one: a divergence that self-corrects would pass an end-state comparison, and
// the whole reason to hash per tick is that the failures worth catching here
// are ordering failures, which are transient by nature.
//
// What this actually guards against is the two toolchains disagreeing - integer
// promotion on the uint8_t arithmetic in the PRNG, signed char on the input
// deltas, struct layout, anything where mipsel-none-elf and the host g++ are
// within their rights to differ. Those are exactly the bugs that never show up
// on the desk.

#include "common/syscalls/syscalls.h"

#include "cave.hh"
#include "trace.inc"

using namespace bd;

namespace {

struct TapeStep {
    int8_t dx, dy;
    uint8_t grab;
};
// Must match simtest.cpp. The trace header is generated from that tape, so a
// mismatch here shows up as an immediate hash divergence at tick 1 rather than
// as anything subtle.
const TapeStep kTape[] = {
    {1, 0, 0},  {1, 0, 0}, {1, 0, 0}, {0, 1, 0},  {0, 1, 0}, {1, 0, 0}, {1, 0, 0},  {0, 0, 0},
    {0, -1, 0}, {1, 0, 1}, {1, 0, 0}, {1, 0, 0},  {0, 1, 0}, {0, 1, 0}, {-1, 0, 0}, {-1, 0, 0},
    {0, 0, 0},  {0, 1, 0}, {1, 0, 0}, {1, 0, 0},  {1, 0, 0}, {0, 0, 0}, {0, -1, 0}, {0, -1, 0},
    {1, 0, 0},  {1, 0, 0}, {0, 1, 0}, {-1, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 0},  {0, 1, 0},
};

Cave g_cave;

}  // namespace

int main() {
    ramsyscall_printf("BOULDERDASH selftest\n");

    CaveSpec spec;
    spec.randomSeed = 0x2A;
    spec.fillObject[0] = El::Boulder;
    spec.fillProbability[0] = 0x28;
    spec.fillObject[1] = El::Diamond;
    spec.fillProbability[1] = 0x10;
    spec.diamondsNeeded = 5;
    g_cave.generate(spec, nullptr);
    g_cave.set(3, 3, El::Player);
    g_cave.set(20, 12, El::ButterflyBase);
    g_cave.set(24, 8, El::FireflyBase + 3);
    g_cave.set(30, 18, El::OutboxHidden);

    unsigned failures = 0;
    for (unsigned i = 0; i < kTraceLength; i++) {
        Input in;
        in.dx = kTape[i].dx;
        in.dy = kTape[i].dy;
        in.grab = kTape[i].grab != 0;
        g_cave.tick(in);
        const unsigned got = g_cave.hash();
        if (got != kTrace[i]) {
            ramsyscall_printf("  FAIL tick %u: host 0x%08x, r3000 0x%08x\n", i, kTrace[i], got);
            failures++;
            if (failures >= 4) {
                ramsyscall_printf("  (stopping after four)\n");
                break;
            }
        }
    }

    // A control that costs one line and rules out the whole test having been
    // vacuous: if the tape never actually ran, this prints zero.
    ramsyscall_printf("  ticks simulated: %u, diamonds %u, status %d\n", g_cave.ticks(),
                      g_cave.diamonds(), (int)g_cave.status());

    if (failures == 0) {
        ramsyscall_printf("  %u ticks, every hash matches the host\n", kTraceLength);
    }
    ramsyscall_printf("BOULDERDASH selftest %s\n", failures == 0 ? "PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
