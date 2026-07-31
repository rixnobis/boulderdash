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

// Sound effects, synthesised and encoded on the console at boot.
//
// No sample files. The waveforms are generated from a handful of parameters and
// then run through an ADPCM encoder written here, which means the whole audio
// path is testable on the host: synthesise, encode, decode with an independent
// implementation of the documented decoder, and measure the error. That is a
// real oracle rather than "it made a noise, sounds about right" - my ears are
// not attached to this machine and the emulator's audio does not come back
// through a screenshot.
//
// Nothing in here includes psyqo. main.cpp does the DMA and the channel setup.

#pragma once

#include <stdint.h>

namespace bd {

enum class Sfx : uint8_t {
    Dig,
    Collect,
    Thud,
    Explode,
    Count,
};

constexpr unsigned kSfxCount = static_cast<unsigned>(Sfx::Count);

// The SPU consumes ADPCM in 16-byte blocks: two header bytes then 28 samples
// packed as nibbles. Every buffer here is sized in blocks so nothing has to
// deal with a partial one.
constexpr unsigned kAdpcmBlockBytes = 16;
constexpr unsigned kSamplesPerBlock = 28;

// Longest effect, in blocks. 96 blocks is 2688 samples, about 0.12s at 22kHz.
constexpr unsigned kMaxBlocks = 96;
constexpr unsigned kMaxSamples = kMaxBlocks * kSamplesPerBlock;

// Synthesises one effect into `out` (at least kMaxSamples entries). Returns the
// sample count, always a multiple of kSamplesPerBlock.
unsigned renderSfx(Sfx sfx, int16_t* out);

// Encodes PCM to SPU ADPCM. `count` must be a multiple of kSamplesPerBlock.
// Returns bytes written, which is (count / 28) * 16.
//
// Filter 0 only, with a per-block shift chosen from the block's peak. The four
// prediction filters would buy better fidelity on tonal material and cost a
// search per block; these are short, noisy, deliberately crunchy effects on a
// 1984 tribute and the difference is inaudible under the crunch that is already
// there on purpose.
unsigned encodeAdpcm(const int16_t* pcm, unsigned count, uint8_t* out);

// The documented SPU decoder, implemented independently of the encoder above
// and used only by the tests. It exists to be the other half of a round trip:
// an encoder graded by its own inverse proves nothing at all.
unsigned decodeAdpcm(const uint8_t* adpcm, unsigned bytes, int16_t* out);

}  // namespace bd
