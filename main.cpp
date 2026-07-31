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

#include "cave.hh"
#include "levels.hh"
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

// The viewport. The cave is 40x22 and the screen holds 20x15 tiles, so it
// scrolls, clamped at the edges.
constexpr int kViewCols = kCols;
constexpr int kViewRows = kRows;

TileId tileFor(uint8_t element, uint8_t magicWallState) {
    switch (element) {
        case El::Dirt: return TileId::Dirt;
        case El::Wall: return TileId::Wall;
        case El::MagicWall: return magicWallState == 1 ? TileId::MagicWall : TileId::Wall;
        case El::Steel: return TileId::Steel;
        // The hidden outbox is indistinguishable from steel on purpose. That is
        // the whole point of it being hidden.
        case El::OutboxHidden: return TileId::Steel;
        case El::OutboxOpen: return TileId::Exit;
        case El::Amoeba:
        case El::AmoebaScanned: return TileId::Amoeba;
        case El::Boulder:
        case El::BoulderScanned:
        case El::BoulderFalling:
        case El::BoulderFallingScanned: return TileId::Boulder;
        case El::Diamond:
        case El::DiamondScanned:
        case El::DiamondFalling:
        case El::DiamondFallingScanned: return TileId::Diamond;
        case El::Player:
        case El::PlayerScanned: return TileId::Player;
        default: break;
    }
    if (element >= El::FireflyBase && element <= El::FireflyScanned + 3) return TileId::Firefly;
    if (element >= El::ButterflyBase && element <= El::ButterflyScanned + 3) return TileId::Butterfly;
    if (element >= El::ExplodeToSpace && element <= El::ExplodeToDiamond + 4) return TileId::Explosion;
    return TileId::Space;
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

    Cave m_cave;
    unsigned m_tickDivider = 0;
    int m_camX = 0;
    int m_camY = 0;
    unsigned m_level = 0;
    unsigned m_lives = 3;
    unsigned m_secondsLeft = 0;
    unsigned m_secondTimer = 0;
    unsigned m_holdFrames = 0;
    void buildRows(unsigned buffer);
    void loadLevel(unsigned index);
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

void SheetScene::loadLevel(unsigned index) {
    const Level& level = kLevels[index];
    m_cave.generate(level.spec, level.instructions);
    m_cave.set(level.playerX, level.playerY, El::Player);
    m_secondsLeft = level.spec.timeLimit;
    m_secondTimer = 0;
    m_holdFrames = 0;
    m_camX = 0;
    m_camY = 0;
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
            frag.count = kCols;
            for (int col = 0; col < kCols; col++) {
                auto& sprite = frag.primitives[col];
                sprite.setColor({{.r = 128, .g = 128, .b = 128}});
                sprite.setOpaque();
                sprite.position = {{.x = static_cast<int16_t>(col * 16),
                                    .y = static_cast<int16_t>(row * 16)}};
                sprite.texInfo.v = 0;
                sprite.texInfo.clut =
                    psyqo::PrimPieces::ClutIndex(psyqo::Vertex{{.x = kClutVramX, .y = kClutVramY}});
            }
        }
    }

    loadLevel(0);
}

void SheetScene::buildRows(unsigned buffer) {
    for (int row = 0; row < kViewRows; row++) {
        auto& frag = m_rows[buffer][row];
        for (int col = 0; col < kViewCols; col++) {
            const int cx = m_camX + col;
            const int cy = m_camY + row;
            const TileId id =
                (cx >= 0 && cy >= 0 && cx < (int)kCaveWidth && cy < (int)kCaveHeight)
                    ? tileFor(m_cave.at(cx, cy), m_cave.magicWallState())
                    : TileId::Space;
            frag.primitives[col].texInfo.u =
                static_cast<uint8_t>(static_cast<unsigned>(id) * kTileSize);
        }
    }
}

void SheetScene::frame() {
    // The cave runs slower than the display. Boulder Dash is a turn-based game
    // wearing an action game's clothes, and a scan per vblank is far too fast
    // to read.
    // A death or an escape holds the screen for a moment before the next cave.
    // Without it the transition is a single frame and reads as a glitch rather
    // than as an outcome.
    if (m_holdFrames > 0) {
        if (--m_holdFrames == 0) {
            if (m_cave.status() == Status::Escaped) {
                m_level = (m_level + 1) % kLevelCount;
                loadLevel(m_level);
            } else if (m_lives > 0) {
                m_lives--;
                loadLevel(m_level);
            } else {
                m_lives = 3;
                m_level = 0;
                loadLevel(0);
            }
        }
    } else if (m_cave.status() != Status::Playing) {
        m_holdFrames = 90;
    } else if (++m_tickDivider >= 8) {
        m_tickDivider = 0;

        // One second per fifty ticks at this divider, near enough. Running out
        // of time is a death, which is why it goes through the same path.
        if (++m_secondTimer >= 8) {
            m_secondTimer = 0;
            if (m_secondsLeft > 0) {
                m_secondsLeft--;
            } else {
                m_holdFrames = 90;
            }
        }

        Input in;
        const auto pad = psyqo::SimplePad::Pad1;
        auto& padState = g_viewer.m_pad;
        if (padState.isButtonPressed(pad, psyqo::SimplePad::Button::Left)) in.dx = -1;
        else if (padState.isButtonPressed(pad, psyqo::SimplePad::Button::Right)) in.dx = 1;
        else if (padState.isButtonPressed(pad, psyqo::SimplePad::Button::Up)) in.dy = -1;
        else if (padState.isButtonPressed(pad, psyqo::SimplePad::Button::Down)) in.dy = 1;
        in.grab = padState.isButtonPressed(pad, psyqo::SimplePad::Button::Cross);
        m_cave.tick(in);

        // Follow the player, clamped. Searching the grid for him each tick is
        // wasteful and completely invisible at this scale.
        for (unsigned y = 0; y < kCaveHeight; y++) {
            for (unsigned x = 0; x < kCaveWidth; x++) {
                const uint8_t e = m_cave.at(x, y);
                if (e != El::Player && e != El::PlayerScanned) continue;
                m_camX = (int)x - kViewCols / 2;
                m_camY = (int)y - kViewRows / 2;
            }
        }
        if (m_camX < 0) m_camX = 0;
        if (m_camY < 0) m_camY = 0;
        if (m_camX > (int)kCaveWidth - kViewCols) m_camX = (int)kCaveWidth - kViewCols;
        if (m_camY > (int)kCaveHeight - kViewRows) m_camY = (int)kCaveHeight - kViewRows;
    }

    const unsigned parity = gpu().getParity();
    buildRows(parity);

    auto& clear = m_clear[parity];
    gpu().getNextClear(clear.primitive, kBackground);
    gpu().chain(clear);
    for (int row = 0; row < kRows; row++) gpu().chain(m_rows[parity][row]);

    const char* state = m_cave.status() == Status::Dead      ? " DEAD"
                        : m_cave.status() == Status::Escaped ? " OUT"
                        : m_secondsLeft == 0                 ? " TIME"
                                                             : "";
    // The quota goes bold once it is met, because "the exit is open now" is the
    // single most important thing the HUD ever has to say.
    const psyqo::Color quota = m_cave.exitOpen() ? psyqo::Color{{.r = 90, .g = 240, .b = 130}}
                                                 : psyqo::Color{{.r = 220, .g = 220, .b = 230}};
    g_viewer.m_font.chainprintf(gpu(), {{.x = 4, .y = 226}}, quota,
                                "%s  %u/%u  %us  x%u%s", kLevels[m_level].name, m_cave.diamonds(),
                                kLevels[m_level].spec.diamondsNeeded, m_secondsLeft, m_lives,
                                state);
}

}  // namespace

int main() { return g_viewer.run(); }
