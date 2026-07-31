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

#include "tiles.hh"

namespace bd {

// 0 transparent, 1 black, 2-5 earth, 6-9 stone/metal, a-c gem, d green,
// e hot, f bright.
const Rgb kPalette[16] = {
    {0x00, 0x00, 0x00},  // 0 transparent key
    {0x00, 0x00, 0x00},  // 1 black
    {0x3a, 0x24, 0x10},  // 2 dark brown
    {0x7a, 0x4c, 0x1c},  // 3 mid brown
    {0xb8, 0x82, 0x3c},  // 4 light brown
    {0xe0, 0xb8, 0x78},  // 5 sand
    {0x28, 0x28, 0x28},  // 6 dark grey
    {0x58, 0x58, 0x58},  // 7 mid grey
    {0xa8, 0xa8, 0xa8},  // 8 light grey
    {0xf8, 0xf8, 0xf8},  // 9 white
    {0x10, 0x28, 0x50},  // a dark blue
    {0x28, 0x70, 0xc0},  // b blue
    {0x60, 0xd8, 0xf8},  // c cyan
    {0x38, 0xb0, 0x30},  // d green
    {0xe8, 0x50, 0x18},  // e orange red
    {0xf8, 0xd0, 0x38},  // f yellow
};

const char kTileArt[kTileCount][kTileSize][kTileSize + 1] = {
    // Space. Entirely transparent - the background colour shows through, which
    // is cheaper than drawing a sprite for every empty cell in the cave.
    {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    },
    // Dirt. A hash scatter of three earth tones, weighted dark. This covers
    // most of the screen, so it has to read as grain at a glance and then get
    // out of the way. Two earlier passes failed differently: a sparse speck
    // pattern read as flat orange with dots, and a period-3 stipple put a
    // phase break at every tile seam that lined up into vertical columns down
    // the whole cave. A hash has no period to break.
    {
        "2332333222323333",
        "2333333232332333",
        "3334333333232333",
        "3232222333233242",
        "2333222323232333",
        "3233322333233333",
        "2333322322323223",
        "3343233232323323",
        "2323233222332334",
        "3332343323233323",
        "3333322233323323",
        "3232332233223333",
        "3323333323323243",
        "3243333334222332",
        "4232223333233233",
        "3322333332332323",
    },
    // Wall. Running-bond brick, black mortar, each brick lit along its bottom
    // edge so the courses separate without a second grey.
    {
        "1111111111111111",
        "1777777817777771",
        "1777777817777771",
        "1777777817777771",
        "1888888818888881",
        "1111111111111111",
        "7778177777781777",
        "7778177777781777",
        "7778177777781777",
        "8881888888818888",
        "1111111111111111",
        "1777777817777771",
        "1777777817777771",
        "1777777817777771",
        "1888888818888881",
        "1111111111111111",
    },
    // Steel. The indestructible border. Black grout and one top highlight: a
    // four-sided bevel made neighbouring tiles share a bright seam and the
    // whole frame read as a pile of loose blocks.
    {
        "1111111111111111",
        "1999999999999991",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1988888888888871",
        "1877777777777771",
        "1111111111111111",
    },
    // Boulder. Sphere, lit top left, outlined so it separates from dirt.
    {
        "......6666......",
        "....66999966....",
        "..669999998866..",
        ".66999999988876.",
        "6699999988888876",
        "6999998888888876",
        "6999988888888776",
        "6999888888887776",
        "6998888888877776",
        "6988888888777776",
        "6888888887777776",
        "6688888777777766",
        ".66887777777766.",
        "..667777777766..",
        "....66777766....",
        "......6666......",
    },
    // Diamond. Bright facet over a darker one, so it holds a shape against the
    // dirt instead of reading as a cyan blob.
    {
        ".......cc.......",
        "......cccc......",
        ".....acccca.....",
        "....accc9cca....",
        "...acccc99cca...",
        "..acccccc9ccca..",
        ".acccccccccccca.",
        "acccccccccccccca",
        ".abbbbbbbbbbbba.",
        "..abbbbbbbbbba..",
        "...abbbbbbbba...",
        "....abbbbbba....",
        ".....abbbba.....",
        "......abba......",
        ".......aa.......",
        "................",
    },
    // Player. Outlined in black, because at 16x16 on a brown field a
    // light-grey figure has no silhouette and simply dissolves.
    {
        "................",
        "....11111111....",
        "...199999991....",
        "..19999999991...",
        "..1ee99999ee1...",
        "..1ee99999ee1...",
        "..19999999991...",
        "..11999999911...",
        "...199999991....",
        "....1999991.....",
        "...19999991.....",
        "..1999999991....",
        "..1999999991....",
        "..11111111111...",
        "...1991..1991...",
        "...1991..1991...",
    },
};

namespace {

// '.' is index 0. Anything else is a hex digit. Anything else again is a bug in
// the art, and it becomes index 0 so it shows up as a hole rather than as a
// plausible colour.
inline uint8_t indexOf(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

}  // namespace

unsigned packTexture(uint16_t* out) {
    unsigned written = 0;
    // Tiles sit side by side in one strip, so a row of the strip interleaves
    // every tile's row. Walking rows outermost keeps the writes sequential.
    for (unsigned row = 0; row < kTileSize; row++) {
        for (unsigned tile = 0; tile < kTileCount; tile++) {
            const char* line = kTileArt[tile][row];
            for (unsigned word = 0; word < kWordsPerRow; word++) {
                const unsigned x = word * 4;
                // Low nibble first: pixel x is the low nibble of byte x/2.
                const uint16_t packed = static_cast<uint16_t>(indexOf(line[x + 0])) |
                                        static_cast<uint16_t>(indexOf(line[x + 1])) << 4 |
                                        static_cast<uint16_t>(indexOf(line[x + 2])) << 8 |
                                        static_cast<uint16_t>(indexOf(line[x + 3])) << 12;
                out[written++] = packed;
            }
        }
    }
    return written;
}

void packClut(uint16_t* out) {
    for (unsigned i = 0; i < 16; i++) {
        if (i == 0) {
            out[i] = 0;
            continue;
        }
        const Rgb& c = kPalette[i];
        const uint16_t r = c.r >> 3;
        const uint16_t g = c.g >> 3;
        const uint16_t b = c.b >> 3;
        out[i] = static_cast<uint16_t>(r | (g << 5) | (b << 10));
    }
}

}  // namespace bd
