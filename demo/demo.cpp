
#include "gbapu.hpp"
#include "wave_writer.h"

constexpr unsigned SAMPLERATE = 48000;
constexpr double CYCLES_PER_SAMPLE = 4194304.0 / SAMPLERATE;
constexpr double CYCLES_PER_FRAME = 4194304.0 / 59.7;

using namespace gbapu;


int main() {


    Buffer buffer(SAMPLERATE);
    buffer.setQuality(Buffer::QUALITY_MED);
    Apu apu(buffer);

    // power on
    apu.writeRegister(Apu::REG_NR52, 0x80);
    // enable terminals
    apu.writeRegister(Apu::REG_NR51, 0xFF);
    apu.writeRegister(Apu::REG_NR50, 0x77);


    apu.writeRegister(Apu::REG_NR10, 0x3f);
    apu.writeRegister(Apu::REG_NR11, 0xC0);
    // CH1 Envelope = $f0 (constant volume 15)
    apu.writeRegister(Apu::REG_NR12, 0xF0);
    apu.writeRegister(Apu::REG_NR14, 0x87);

    Wave_Writer wav(SAMPLERATE);
    wav.enable_stereo();

    constexpr size_t samplesPerFrame = (CYCLES_PER_FRAME / CYCLES_PER_SAMPLE) + 1;
    std::unique_ptr<int16_t[]> frameBuf(new int16_t[samplesPerFrame * 2]);

    for (int i = 0; i != 120; ++i) {
        apu.step(CYCLES_PER_FRAME);
        apu.endFrame();
        size_t samples = buffer.available();
        buffer.read(frameBuf.get(), samples);
        wav.write(frameBuf.get(), samples * 2);
    }

    return 0;
}