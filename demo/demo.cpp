
#include "gbapu.hpp"
#include "wave_writer.h"

#include <string>

constexpr unsigned SAMPLERATE = 48000;
constexpr double CYCLES_PER_SAMPLE = 4194304.0 / SAMPLERATE;
constexpr double CYCLES_PER_FRAME = 4194304.0 / 59.7;

using namespace gbapu;

static constexpr uint8_t HOLD = 0x00;

#pragma pack(push)
struct DemoCommand {

    uint8_t reg;
    uint8_t value;
};
#pragma pack(pop)

static DemoCommand const DEMO_DUTY[] = {
    // frame 0, setup control regs and retrigger CH1 with duty = 0 (12.5%)
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0x11}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR12, 0xF0}, {Apu::REG_NR13, 0x00}, {Apu::REG_NR14, 0x87},
    {Apu::REG_NR11, 0x00}, {HOLD, 60}, // duty = 12.5%

    {Apu::REG_NR11, 0x40}, {HOLD, 60}, // duty = 25%
    {Apu::REG_NR11, 0x80}, {HOLD, 60}, // duty = 50%
    {Apu::REG_NR11, 0xC0}, {HOLD, 60}  // duty = 75%

};

static DemoCommand const DEMO_MASTER_VOLUME[] = {
    // frame 0, setup control regs and retrigger CH1 with duty = 0 (12.5%)
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0x11}, {Apu::REG_NR50, 0x07},
    {Apu::REG_NR11, 0x80}, { Apu::REG_NR12, 0xF0 }, {Apu::REG_NR13, 0x00}, {Apu::REG_NR14, 0x87},
    {HOLD, 2},
    {Apu::REG_NR50, 0x16}, {HOLD, 2},
    {Apu::REG_NR50, 0x25}, {HOLD, 2},
    {Apu::REG_NR50, 0x34}, {HOLD, 2},
    {Apu::REG_NR50, 0x43}, {HOLD, 2},
    {Apu::REG_NR50, 0x52}, {HOLD, 2},
    {Apu::REG_NR50, 0x61}, {HOLD, 2},
    {Apu::REG_NR50, 0x70}, {HOLD, 60},
    {Apu::REG_NR50, 0x61}, {HOLD, 2},
    {Apu::REG_NR50, 0x52}, {HOLD, 2},
    {Apu::REG_NR50, 0x43}, {HOLD, 2},
    {Apu::REG_NR50, 0x34}, {HOLD, 2},
    {Apu::REG_NR50, 0x25}, {HOLD, 2},
    {Apu::REG_NR50, 0x16}, {HOLD, 2},
    {Apu::REG_NR50, 0x07}, {HOLD, 60}

};

struct Demo {
    const char *name;
    DemoCommand const *sequence;
    size_t sequenceLen;
};

#define demoStruct(name, seq) { name, seq, sizeof(seq) / sizeof(DemoCommand) }

static Demo const DEMO_TABLE[] = {
    demoStruct("duty", DEMO_DUTY),
    demoStruct("master_volume", DEMO_MASTER_VOLUME)
};

constexpr size_t DEMO_COUNT = sizeof(DEMO_TABLE) / sizeof(Demo);


int main() {

    Buffer buffer(SAMPLERATE);
    Apu apu(buffer);

    constexpr size_t samplesPerFrame = (CYCLES_PER_FRAME / CYCLES_PER_SAMPLE) + 1;
    std::unique_ptr<int16_t[]> frameBuf(new int16_t[samplesPerFrame * 2]);

    for (size_t i = 0; i != DEMO_COUNT; ++i) {
        auto &demo = DEMO_TABLE[i];

        std::string filename = "demo_";
        filename.append(demo.name);
        filename.append(".wav");
        Wave_Writer wav(SAMPLERATE, filename.c_str());
        wav.enable_stereo();

        apu.reset();
        buffer.clear();

        uint32_t cycles = 0;
        for (size_t j = 0; j != demo.sequenceLen; ++j) {
            auto cmd = demo.sequence[j];
            if (cmd.reg == HOLD) {
                for (size_t frames = cmd.value; frames--; ) {
                    apu.step(CYCLES_PER_FRAME - cycles);
                    apu.endFrame();
                    size_t samples = buffer.available();
                    buffer.read(frameBuf.get(), samples);
                    wav.write(frameBuf.get(), samples * 2);

                    cycles = 0;
                }
                
            } else {
                cycles += 3;
                apu.writeRegister(static_cast<Apu::Reg>(cmd.reg), cmd.value);
            }
        }
    }

    return 0;
}