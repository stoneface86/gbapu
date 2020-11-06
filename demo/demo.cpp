
#include "gbapu.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

constexpr unsigned SAMPLERATE = 48000;
constexpr double CYCLES_PER_SAMPLE = 4194304.0 / SAMPLERATE;
constexpr double CYCLES_PER_FRAME = 4194304.0 / 59.7;

using namespace gbapu;


int main() {


    Buffer buffer(SAMPLERATE);
    buffer.setQuality(Buffer::QUALITY_MED);
    Apu apu(buffer);

    // power on
    apu.step(3);
    apu.writeRegister(Apu::REG_NR52, 0x80);
    // enable terminals
    apu.step(3);
    apu.writeRegister(Apu::REG_NR51, 0xFF);
    apu.step(3);
    apu.writeRegister(Apu::REG_NR50, 0x77);


    apu.writeRegister(Apu::REG_NR10, 0x37);
    apu.writeRegister(Apu::REG_NR11, 0x8F);
    // CH1 Envelope = $f0 (constant volume 15)
    apu.step(3);
    apu.writeRegister(Apu::REG_NR12, 0xF4);
    apu.step(3);
    apu.writeRegister(Apu::REG_NR14, 0xC7);

    apu.writeRegister(Apu::REG_NR42, 0xF0);
    apu.writeRegister(Apu::REG_NR43, 0x13);
    apu.writeRegister(Apu::REG_NR44, 0x80);

    

    ma_encoder_config config = ma_encoder_config_init(ma_resource_format_wav, ma_format_s16, 2, SAMPLERATE);
    ma_encoder encoder;
    auto err = ma_encoder_init_file("demo.wav", &config, &encoder);
    if (err != MA_SUCCESS) {
        return 1;
    }

    constexpr size_t samplesPerFrame = (CYCLES_PER_FRAME / CYCLES_PER_SAMPLE) + 1;
    std::unique_ptr<int16_t[]> frameBuf(new int16_t[samplesPerFrame * 2]);

    

    for (int i = 0; i != 120; ++i) {
        apu.step(CYCLES_PER_FRAME);
        apu.endFrame();
        size_t samples = buffer.available();
        buffer.read(frameBuf.get(), samples);
        ma_encoder_write_pcm_frames(&encoder, frameBuf.get(), samples);
    }


    ma_encoder_uninit(&encoder);
    return 0;
}