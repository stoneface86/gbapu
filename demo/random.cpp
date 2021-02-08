#include "gbapu.hpp"
#include "wave_writer.h"

#include <cstdlib>

constexpr unsigned SAMPLERATE = 48000;
constexpr double CYCLES_PER_SAMPLE = 4194304.0 / SAMPLERATE;
constexpr double CYCLES_PER_FRAME = 4194304.0 / 59.7;

using namespace gbapu;

constexpr size_t FRAMES = 60 * 4;


int main() {

    Apu apu(SAMPLERATE, SAMPLERATE / 10);
    Wave_Writer wav(SAMPLERATE, "random.wav");
    wav.enable_stereo();

    constexpr size_t samplesPerFrame = (CYCLES_PER_FRAME / CYCLES_PER_SAMPLE) + 1;
    std::unique_ptr<int16_t[]> frameBuf(new int16_t[samplesPerFrame * 2]);

    // demo taken from blargg's gb_apu library
    int delay = 12;
    for (size_t i = 0; i < FRAMES; ++i) {
        if (--delay == 0) {
            delay = 12;
            int terms = rand() & 0x11;
            if (terms == 0) {
                terms = 0x11;
            }
            apu.writeRegister(Apu::REG_NR52, 0x80);
            apu.writeRegister(Apu::REG_NR50, 0x77);
            apu.writeRegister(Apu::REG_NR51, terms);
            apu.writeRegister(Apu::REG_NR10, 0x70 + (rand() & 0xF));
            apu.writeRegister(Apu::REG_NR11, 0x80);
            apu.writeRegister(Apu::REG_NR12, 0xF1);
            int16_t freq = (rand() & 0x4FF) + 0x300;
            apu.writeRegister(Apu::REG_NR13, freq & 0xFF);
            apu.writeRegister(Apu::REG_NR14, (freq >> 8) | 0x80);
        }

        apu.stepTo(CYCLES_PER_FRAME);
        apu.endFrame();

        size_t samples = apu.availableSamples();
        apu.readSamples(frameBuf.get(), samples);
        wav.write(frameBuf.get(), samples * 2);
    }

    return 0;
}