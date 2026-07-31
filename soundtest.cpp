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

// Does the sound actually PLAY?
//
// Everything the host tests prove stops at the bytes: the waveform is audible,
// the encoder round-trips, the flags are the ones I meant. None of that is
// evidence that the SPU accepted the upload and walked through it. `dmaWrite`
// returning is not playback, and I cannot hear this machine.
//
// The first oracle here was the hardware's ENDX status register at 1F801D9C,
// which is the right instrument on real silicon: bit N clears on key-on and
// sets when voice N reaches a block flagged loop-end. It reported that not one
// effect played. It was wrong, and the way it was wrong is the reason this
// comment is long.
//
// pcsx-redux has no read case for 1F801D9C. The read falls through to the
// shadow register array and returns whatever was last written there, so the bit
// can never move no matter what the SPU does. The `ENDX:` that turns up in its
// source is a goto label meaning "done with this channel", not the register.
//
// What saved it was the negative control reading ZERO as well. Subject and
// control agreeing is not corroboration when they can only produce one value -
// it is the instrument being incapable of an answer, and had I only run the
// positive half I would have spent the afternoon debugging working audio.
//
// So the oracle is now a register this emulator actually implements: the
// per-voice ADSR envelope volume at 1F801C0C + voice*16. It is non-zero while a
// voice is live and zero when it is not, which is a property of the SPU's own
// state machine rather than of my code. The control is the same read on a voice
// that was never keyed on.

#include "psyqo/spu.hh"

#include "common/syscalls/syscalls.h"

#include "sound.hh"

using namespace bd;

namespace {

// Voice registers are sixteen bytes apart from 1F801C00; offset 0x0C is the
// current ADSR envelope volume.
volatile uint16_t* voiceEnvelope(unsigned voice) {
    return reinterpret_cast<volatile uint16_t*>(0x1f801c0c + voice * 16);
}

uint16_t g_address[kSfxCount];
unsigned g_bytes[kSfxCount];
unsigned g_failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        ramsyscall_printf("  FAIL  %s\n", what);
        g_failures++;
    }
}

// Sample the envelope repeatedly rather than once. A single read lands at an
// arbitrary point and can easily catch a live voice at a zero crossing of its
// envelope or after it has finished; the peak over a window is what says
// whether the voice was ever live at all.
uint16_t peakEnvelope(unsigned voice, unsigned samples) {
    uint16_t peak = 0;
    for (unsigned i = 0; i < samples; i++) {
        const uint16_t v = *voiceEnvelope(voice);
        if (v > peak) peak = v;
        for (volatile unsigned d = 0; d < 400; d++) {
        }
    }
    return peak;
}

}  // namespace

int main() {
    ramsyscall_printf("BOULDERDASH soundtest\n");
    psyqo::SPU::initialize();

    static int16_t pcm[kMaxSamples];
    static uint8_t adpcm[kMaxBlocks * kAdpcmBlockBytes];

    uint16_t addr = psyqo::SPU::BASE_ALLOC_ADDR;
    for (unsigned i = 0; i < kSfxCount; i++) {
        const unsigned count = renderSfx(static_cast<Sfx>(i), pcm);
        g_bytes[i] = encodeAdpcm(pcm, count, adpcm);
        g_address[i] = addr;
        psyqo::SPU::dmaWrite(addr, adpcm, static_cast<uint16_t>(g_bytes[i]), 16);
        addr = static_cast<uint16_t>(addr + g_bytes[i]);
    }

    // A region with no end flag anywhere in it, for the negative control. Zero
    // bytes decode as silence and, critically, carry flags of zero - so a voice
    // pointed here has nothing that can ever set its ENDX bit.
    static uint8_t silence[kAdpcmBlockBytes * 8] = {};
    const uint16_t silenceAddr = addr;
    psyqo::SPU::dmaWrite(silenceAddr, silence, sizeof(silence), 16);

    psyqo::SPU::ChannelPlaybackConfig config;
    config.sampleRate = psyqo::FixedPoint<12, uint16_t>(0, 2048);
    config.volumeLeft = 0x2000;
    config.volumeRight = 0x2000;
    config.adsr = 0x1fc080ff;

    // Control FIRST, before any voice has been keyed on. If a never-started
    // voice already reports a live envelope then the register means something
    // other than what I think and nothing below is worth reading.
    const uint16_t idle = peakEnvelope(1, 200);
    ramsyscall_printf("  control (voice never keyed on): envelope peak %u\n", idle);
    expect(idle == 0, "an unstarted voice reports a zero envelope");

    for (unsigned i = 0; i < kSfxCount; i++) {
        psyqo::SPU::playADPCM(0, g_address[i], config, true);
        const uint16_t peak = peakEnvelope(0, 400);
        ramsyscall_printf("  sfx %u: %u bytes at 0x%04x, envelope peak %u\n", i, g_bytes[i],
                          g_address[i], peak);
        expect(peak > 0, "the voice actually became live");
    }

    (void)silenceAddr;

    ramsyscall_printf("BOULDERDASH soundtest %s\n", g_failures == 0 ? "PASSED" : "FAILED");
    return g_failures == 0 ? 0 : 1;
}
