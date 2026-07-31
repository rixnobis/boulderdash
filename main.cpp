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

// Tile viewer.
//
// This draws a hand-written scrap of cave through the real upload path and the
// real sprite primitive, because that is the only picture that is evidence. A
// host-side preview of the same packed words would be rendered by code that
// shares my packing convention with the packer, so a swapped nibble order or a
// misaligned CLUT would be invisible in both arms at once - the two would agree
// and the agreement would read as confirmation. The GPU has no such courtesy.

#include "psyqo/application.hh"
#include "psyqo/font.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/primitives/common.hh"
#include "psyqo/primitives/control.hh"
#include "psyqo/primitives/sprites.hh"
#include "psyqo/scene.hh"
#include "psyqo/simplepad.hh"

#include "common/syscalls/syscalls.h"

#include "tiles.hh"

using namespace bd;

namespace {

// The tile strip and its CLUT live to the right of both framebuffers. In 4bpp a
// texture page is 64 VRAM words wide, so 640 is page 10, and the strip is
// kTileCount * 16 pixels across - well inside one page.
constexpr int kSheetVramX = 640;
constexpr int kSheetVramY = 0;
constexpr int kClutVramX = 640;
constexpr int kClutVramY = 256;
constexpr uint8_t kSheetPageX = kSheetVramX / 64;
constexpr uint8_t kSheetPageY = kSheetVramY / 256;

constexpr int kCols = 20;
constexpr int kRows = 15;

constexpr psyqo::Color kBackground = {{.r = 0, .g = 0, .b = 0}};

// A scrap of cave, so the tiles get judged next to the tiles they will actually
// sit next to. A tilesheet laid out in a neat row looks fine and tells you
// nothing about whether a boulder reads against dirt.
const char* const kScene[kRows] = {
    "SSSSSSSSSSSSSSSSSSSS",
    "SdddddbddddDdddddddS",
    "SddPdddddddWWWWdddbS",
    "SddddddbdddWdddddddS",
    "SWWWWdddddddDddddddS",
    "SddddddbddddddbddddS",
    "SddDdddddddWWWdddddS",
    "SddddddddddddddddddS",
    "SddbddddDddddddbdddS",
    "SdddWWWWWddddddddddS",
    "SddddddddddDdddddddS",
    "SdbddddddddddddddbdS",
    "SdddddddWWWWdddddddS",
    "SddddDddddddddddddoS",
    "SSSSSSSSSSSSSSSSSSSS",
};

TileId tileFor(char c) {
    switch (c) {
        case 'S': return TileId::Steel;
        case 'W': return TileId::Wall;
        case 'd': return TileId::Dirt;
        case 'b': return TileId::Boulder;
        case 'D': return TileId::Diamond;
        case 'P': return TileId::Player;
        default: return TileId::Space;
    }
}

class Viewer final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

  public:
    psyqo::Font<> m_font;
    psyqo::SimplePad m_pad;
};

class SheetScene final : public psyqo::Scene {
    void start(StartReason reason) override;
    void frame() override;

    // One fragment per cave row, not one for the whole screen. A DMA linked-list
    // node carries its payload size in eight bits, so a chained fragment tops
    // out at 255 words - the full 300-sprite screen is 901 and psyqo aborts
    // with "Fragment too big to be chained" rather than silently truncating,
    // which is the correct thing for it to do and the reason this was two
    // minutes of debugging instead of an afternoon of wondering why the bottom
    // of the cave was missing. A row is 20 sprites: 61 words.
    // Double buffered, because the GPU is still chewing on the previous frame.
    psyqo::Fragments::FixedFragmentWithPrologue<psyqo::Prim::TPage, psyqo::Prim::Sprite16x16, kCols>
        m_rows[2][kRows];
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];
};

Viewer g_viewer;
SheetScene g_sheetScene;

void Viewer::prepare() {
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
}

void Viewer::createScene() {
    m_font.uploadSystemFont(gpu());
    m_pad.initialize();

    // Pack and upload. The strip is kTileCount tiles wide and 16 rows tall; in
    // 4bpp that is four VRAM words per tile per row.
    static uint16_t sheet[kTileCount * kWordsPerTile];
    const unsigned words = packTexture(sheet);
    const unsigned strideWords = kTileCount * kWordsPerRow;
    // packTexture writes rows outermost, which is exactly the order a VRAM
    // rectangle upload expects. A mismatch here means the layout changed under
    // the upload, so say so and carry on - a scene that never gets pushed is a
    // black screen, which looks identical to every other way this can fail.
    ramsyscall_printf("pack: %u words, expected %u\n", words, strideWords * kTileSize);
    gpu().uploadToVRAM(sheet, {.pos = {{.x = kSheetVramX, .y = kSheetVramY}},
                               .size = {{.w = static_cast<int16_t>(strideWords),
                                         .h = static_cast<int16_t>(kTileSize)}}});

    static uint16_t clut[16];
    packClut(clut);
    gpu().uploadToVRAM(clut,
                       {.pos = {{.x = kClutVramX, .y = kClutVramY}}, .size = {{.w = 16, .h = 1}}});

    ramsyscall_printf("createScene done, pushing\n");
    pushScene(&g_sheetScene);
}

void SheetScene::start(StartReason reason) {
    if (reason != StartReason::Create) return;

    psyqo::PrimPieces::TPageAttr attr;
    attr.setPageX(kSheetPageX)
        .setPageY(kSheetPageY)
        .set(psyqo::Prim::TPageAttr::Tex4Bits)
        .setDithering(false)
        .disableDisplayArea();

    for (unsigned buffer = 0; buffer < 2; buffer++) {
        for (int row = 0; row < kRows; row++) {
            auto& frag = m_rows[buffer][row];
            frag.prologue.attr = attr;
            unsigned n = 0;
            for (int col = 0; col < kCols; col++) {
                const TileId id = tileFor(kScene[row][col]);
                auto& sprite = frag.primitives[n++];
                // Full-brightness neutral: 0x80 per channel is 1.0 in the GPU's
                // texture blend, so the palette comes through unmodulated.
                sprite.setColor({{.r = 128, .g = 128, .b = 128}});
                sprite.setOpaque();
                sprite.position = {{.x = static_cast<int16_t>(col * 16),
                                    .y = static_cast<int16_t>(row * 16)}};
                sprite.texInfo.u = static_cast<uint8_t>(static_cast<unsigned>(id) * kTileSize);
                sprite.texInfo.v = 0;
                sprite.texInfo.clut =
                    psyqo::PrimPieces::ClutIndex(psyqo::Vertex{{.x = kClutVramX, .y = kClutVramY}});
            }
            frag.count = n;
        }
    }
}

void SheetScene::frame() {
    static unsigned s_frames = 0;
    if (s_frames++ == 30) ramsyscall_printf("frame 30 reached\n");
    const unsigned parity = gpu().getParity();
    auto& clear = m_clear[parity];
    gpu().getNextClear(clear.primitive, kBackground);
    gpu().chain(clear);
    for (int row = 0; row < kRows; row++) gpu().chain(m_rows[parity][row]);

    g_viewer.m_font.chainprintf(gpu(), {{.x = 4, .y = 228}},
                                psyqo::Color{{.r = 200, .g = 200, .b = 210}}, "TILES R0");
}

}  // namespace

int main() { return g_viewer.run(); }
