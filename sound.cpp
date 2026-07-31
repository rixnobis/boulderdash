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

#include "sound.hh"

namespace bd {

namespace {

// Deterministic noise. The console and the host must produce byte-identical
// audio or the round-trip test on the desk says nothing about what the console
// will actually upload, so this cannot use anything platform-dependent.
struct Noise {
    uint32_t state;
    explicit Noise(uint32_t seed) : state(seed) {}
    int16_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int16_t>(state & 0xFFFF);
    }
};

inline int16_t clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

// A square wave is the right primitive here: it is what the hardware of the era
// actually produced, it survives a coarse 4-bit encoder far better than a sine,
// and its harmonics are what makes a blip read as a blip on a small speaker.
inline int16_t square(unsigned phase, unsigned period, int16_t amplitude) {
    if (period == 0) return 0;
    return (phase % period) * 2 < period ? amplitude : static_cast<int16_t>(-amplitude);
}

unsigned roundUpToBlock(unsigned samples) {
    const unsigned blocks = (samples + kSamplesPerBlock - 1) / kSamplesPerBlock;
    return blocks * kSamplesPerBlock;
}

}  // namespace

unsigned renderSfx(Sfx sfx, int16_t* out) {
    unsigned count = 0;

    switch (sfx) {
        case Sfx::Dig: {
            // Filtered noise with a fast decay. Digging is a texture, not a
            // pitch - anything tonal here turns into a beep and the player digs
            // several times a second.
            count = roundUpToBlock(560);
            Noise noise(0xD16D16u);
            int32_t lowpass = 0;
            for (unsigned i = 0; i < count; i++) {
                const int32_t raw = noise.next() >> 2;
                lowpass = (lowpass * 3 + raw) / 4;
                const int32_t envelope = static_cast<int32_t>(count - i);
                out[i] = clamp16(lowpass * envelope / static_cast<int32_t>(count));
            }
            break;
        }
        case Sfx::Collect: {
            // Two square tones a fifth apart, the upper one entering halfway.
            // A single rising sweep was the first attempt and read as a UI
            // click; the interval is what makes it read as a reward.
            count = roundUpToBlock(1120);
            for (unsigned i = 0; i < count; i++) {
                const unsigned periodA = 40;
                const unsigned periodB = 27;
                int32_t v = square(i, periodA, 7000);
                if (i > count / 2) v += square(i, periodB, 6000);
                const int32_t envelope = static_cast<int32_t>(count - i);
                out[i] = clamp16(v * envelope / static_cast<int32_t>(count));
            }
            break;
        }
        case Sfx::Thud: {
            // A low square dropping in pitch, plus a noise transient on the
            // front. The transient is the impact; the pitch drop is the mass.
            count = roundUpToBlock(840);
            Noise noise(0x7D00D5u);
            for (unsigned i = 0; i < count; i++) {
                const unsigned period = 90 + (i * 60) / count;
                int32_t v = square(i, period, 9000);
                if (i < 90) v += noise.next() >> 3;
                const int32_t envelope = static_cast<int32_t>(count - i);
                out[i] = clamp16(v * envelope / static_cast<int32_t>(count));
            }
            break;
        }
        case Sfx::Explode: {
            // Full-band noise, long decay, with a slow square underneath for
            // body. Noise alone is a hiss; the low square is what gives it size.
            count = roundUpToBlock(2600);
            Noise noise(0xB005Au);
            for (unsigned i = 0; i < count; i++) {
                int32_t v = noise.next() >> 1;
                v += square(i, 160, 5000);
                // Squared decay, applied as two 32-bit steps. The obvious
                // one-liner needs a 64-bit divide, and the freestanding R3000
                // target has no __divdi3 to call - it links fine on the host and
                // fails only at the console link, which is a good argument for
                // building both arms before trusting either. The intermediate
                // peaks near 32767 * 2604 = 85M, comfortably inside int32.
                const int32_t envelope = static_cast<int32_t>(count - i);
                const int32_t once = (v * envelope) / static_cast<int32_t>(count);
                out[i] = clamp16((once * envelope) / static_cast<int32_t>(count));
            }
            break;
        }
        default:
            count = kSamplesPerBlock;
            for (unsigned i = 0; i < count; i++) out[i] = 0;
            break;
    }

    return count;
}

unsigned encodeAdpcm(const int16_t* pcm, unsigned count, uint8_t* out) {
    const unsigned blocks = count / kSamplesPerBlock;
    unsigned written = 0;

    for (unsigned b = 0; b < blocks; b++) {
        const int16_t* block = pcm + b * kSamplesPerBlock;

        // Pick the smallest shift that keeps every sample in this block inside
        // the four-bit range. The decoder computes sample = nibble << (12 -
        // shift), so a shift that is one too large clips the loudest sample in
        // the block and a shift that is too small throws away resolution.
        int32_t peak = 0;
        for (unsigned i = 0; i < kSamplesPerBlock; i++) {
            int32_t v = block[i];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
        }
        int shift = 12;
        while (shift > 0) {
            const int32_t representable = static_cast<int32_t>(7) << (12 - shift);
            if (representable >= peak) break;
            shift--;
        }

        uint8_t flags = 0;
        if (b == 0) flags |= 0x04;                 // start of the sample
        if (b == blocks - 1) flags |= 0x01 | 0x02;  // end, and stop rather than loop

        out[written++] = static_cast<uint8_t>(shift & 0x0F);  // filter 0 in the high nibble
        out[written++] = flags;

        for (unsigned i = 0; i < kSamplesPerBlock; i += 2) {
            int32_t lo = block[i] >> (12 - shift);
            int32_t hi = block[i + 1] >> (12 - shift);
            if (lo > 7) lo = 7;
            if (lo < -8) lo = -8;
            if (hi > 7) hi = 7;
            if (hi < -8) hi = -8;
            out[written++] = static_cast<uint8_t>((lo & 0x0F) | ((hi & 0x0F) << 4));
        }
    }

    return written;
}

unsigned decodeAdpcm(const uint8_t* adpcm, unsigned bytes, int16_t* out) {
    // Written from the documented decoder, deliberately not by inverting the
    // encoder above. If this shared the encoder's idea of what a shift means,
    // the two would agree about a wrong convention and the round-trip test would
    // pass on a stream the SPU cannot play.
    const unsigned blocks = bytes / kAdpcmBlockBytes;
    unsigned produced = 0;
    for (unsigned b = 0; b < blocks; b++) {
        const uint8_t* block = adpcm + b * kAdpcmBlockBytes;
        const int shift = block[0] & 0x0F;
        for (unsigned i = 0; i < 14; i++) {
            const uint8_t byte = block[2 + i];
            for (unsigned half = 0; half < 2; half++) {
                int32_t nibble = half == 0 ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
                if (nibble > 7) nibble -= 16;  // four-bit two's complement
                out[produced++] = clamp16(nibble << (12 - shift));
            }
        }
    }
    return produced;
}

}  // namespace bd
