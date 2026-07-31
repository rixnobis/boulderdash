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

// Cave data.
//
// These caves are original. The 1984 game's cave bytes are First Star's level
// data and are not mine to ship, so what is reproduced here is the FORMAT - the
// header fields, the random fill, the PLOT/LINE/RECT instruction stream - and
// the caves themselves are written against it. That is the part that was worth
// getting right anyway: a cave is a seed and about forty bytes, and the engine
// has to agree with the decoder about what those bytes mean or nothing loads.
//
// Instructions are authored through the macros below rather than as raw hex,
// because a stream of bare bytes is unreviewable and a mistyped one produces a
// cave that loads fine and is subtly wrong.

#pragma once

#include "cave.hh"

namespace bd {

// Direction codes for LINE, in the order the format numbers them: up, then
// clockwise by 45 degrees.
namespace LineDir {
enum : uint8_t { Up = 0, UpRight = 1, Right = 2, DownRight = 3, Down = 4, DownLeft = 5, Left = 6, UpLeft = 7 };
}

#define BD_PLOT(obj, x, y) static_cast<uint8_t>(obj), (x), (y)
#define BD_LINE(obj, x, y, len, dir) static_cast<uint8_t>(0x40 | (obj)), (x), (y), (len), (dir)
#define BD_FILLED(obj, x, y, w, h, fill) \
    static_cast<uint8_t>(0x80 | (obj)), (x), (y), (w), (h), static_cast<uint8_t>(fill)
#define BD_RECT(obj, x, y, w, h) static_cast<uint8_t>(0xC0 | (obj)), (x), (y), (w), (h)
#define BD_END 0xFF

struct Level {
    const char* name;
    CaveSpec spec;
    const uint8_t* instructions;
    uint8_t playerX, playerY;
};

extern const Level kLevels[];
extern const unsigned kLevelCount;

}  // namespace bd
