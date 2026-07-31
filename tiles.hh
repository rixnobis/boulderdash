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

// Tile art, authored as text and packed at runtime.
//
// The tiles are 16x16, 4bpp, one shared 16-entry palette. They are written as
// sixteen strings of sixteen characters, one character per pixel, the character
// being the palette index in hex. '.' is an alias for index 0, which the GPU
// treats as transparent in a paletted texture.
//
// This costs a few hundred bytes of ROM over shipping the packed words, and it
// buys the only thing that matters while the art is still being argued with: a
// diff that shows a changed pixel as a changed pixel. A blob of hex literals is
// not reviewable, and a typo in one is indistinguishable from a decision.
//
// Nothing in this header knows about psyqo. The packer is compiled for the host
// as well, so the preview tool and the console are packing from one source.

#pragma once

#include <stdint.h>

namespace bd {

constexpr unsigned kTileSize = 16;
// 4bpp: two pixels per byte, four per 16-bit word, so one row is four words.
constexpr unsigned kWordsPerRow = kTileSize / 4;
constexpr unsigned kWordsPerTile = kWordsPerRow * kTileSize;

enum class TileId : uint8_t {
    Space,
    Dirt,
    Wall,
    Steel,
    Boulder,
    Diamond,
    Player,
    Firefly,
    Butterfly,
    Amoeba,
    Explosion,
    Exit,
    MagicWall,
    Count,
};

constexpr unsigned kTileCount = static_cast<unsigned>(TileId::Count);

// Sixteen colours, ordered dark-to-light within each family so the art reads as
// art when you look at the source. Entries are 8-bit RGB; packTexture converts.
// Index 0 is never drawn - it is the transparency key.
struct Rgb {
    uint8_t r, g, b;
};
extern const Rgb kPalette[16];

// The art. kTileArt[tile][row] is a 16-character string.
// A row is exactly kTileSize characters plus the terminator, declared as an
// array rather than a pointer on purpose: a miscounted row is then a compile
// error instead of a read past the end of a string literal, which is how a
// typo in art turns into a garbage pixel nobody can find.
extern const char kTileArt[kTileCount][kTileSize][kTileSize + 1];

// Packs every tile into `out`, which must hold kTileCount * kWordsPerTile
// words, laid out as one horizontal strip of tiles. Returns the number of words
// written. Low nibble is the left pixel of each byte, which is what the GPU
// wants and is exactly the convention a preview tool must not simply inherit.
unsigned packTexture(uint16_t* out);

// Packs the palette into 16 words of 1555 BGR, the CLUT format. Index 0 is
// emitted as all-zero, which is the transparent-black encoding.
void packClut(uint16_t* out);

}  // namespace bd
