
#include "gbapu.hpp"
#include "wave_writer.h"

#include <chrono>
#include <string>
#include <iostream>

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

static DemoCommand const DEMO_NOISE[] = {
    // frame 0, setup control regs and retrigger CH4
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0xFF}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR42, 0xF0}, {Apu::REG_NR43, 0x77}, {Apu::REG_NR44, 0x80}, {HOLD, 5},
    {Apu::REG_NR43, 0x76}, {HOLD, 5},
    {Apu::REG_NR43, 0x75}, {HOLD, 5},
    {Apu::REG_NR43, 0x74}, {HOLD, 5},
    {Apu::REG_NR43, 0x67}, {HOLD, 5},
    {Apu::REG_NR43, 0x66}, {HOLD, 5},
    {Apu::REG_NR43, 0x65}, {HOLD, 5},
    {Apu::REG_NR43, 0x64}, {HOLD, 5},
    {Apu::REG_NR43, 0x57}, {HOLD, 5},
    {Apu::REG_NR43, 0x56}, {HOLD, 5},
    {Apu::REG_NR43, 0x55}, {HOLD, 5},
    {Apu::REG_NR43, 0x54}, {HOLD, 5},
    {Apu::REG_NR43, 0x47}, {HOLD, 5},
    {Apu::REG_NR43, 0x46}, {HOLD, 5},
    {Apu::REG_NR43, 0x45}, {HOLD, 5},
    {Apu::REG_NR43, 0x44}, {HOLD, 5},
    {Apu::REG_NR43, 0x37}, {HOLD, 5},
    {Apu::REG_NR43, 0x36}, {HOLD, 5},
    {Apu::REG_NR43, 0x35}, {HOLD, 5},
    {Apu::REG_NR43, 0x34}, {HOLD, 5},
    {Apu::REG_NR43, 0x27}, {HOLD, 5},
    {Apu::REG_NR43, 0x26}, {HOLD, 5},
    {Apu::REG_NR43, 0x25}, {HOLD, 5},
    {Apu::REG_NR43, 0x24}, {HOLD, 5},
    {Apu::REG_NR43, 0x17}, {HOLD, 5},
    {Apu::REG_NR43, 0x16}, {HOLD, 5},
    {Apu::REG_NR43, 0x15}, {HOLD, 5},
    {Apu::REG_NR43, 0x14}, {HOLD, 5},
    {Apu::REG_NR43, 0x07}, {HOLD, 5},
    {Apu::REG_NR43, 0x06}, {HOLD, 5},
    {Apu::REG_NR43, 0x05}, {HOLD, 5},
    {Apu::REG_NR43, 0x04}, {HOLD, 5},
    {Apu::REG_NR43, 0x03}, {HOLD, 5},
    {Apu::REG_NR43, 0x02}, {HOLD, 5},
    {Apu::REG_NR43, 0x01}, {HOLD, 5},
    {Apu::REG_NR43, 0x00}, {HOLD, 5}
};

static DemoCommand const DEMO_WAVE[] = {
    // frame 0, setup control regs and retrigger CH4
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0x44}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR32, 0x20}, // volume = 100%
    {Apu::REG_WAVERAM,      0x01},
    {Apu::REG_WAVERAM + 1,  0x23},
    {Apu::REG_WAVERAM + 2,  0x45},
    {Apu::REG_WAVERAM + 3,  0x67},
    {Apu::REG_WAVERAM + 4,  0x89},
    {Apu::REG_WAVERAM + 5,  0xAB},
    {Apu::REG_WAVERAM + 6,  0xCD},
    {Apu::REG_WAVERAM + 7,  0xEF},
    {Apu::REG_WAVERAM + 8,  0xFE},
    {Apu::REG_WAVERAM + 9,  0xDC},
    {Apu::REG_WAVERAM + 10, 0xBA},
    {Apu::REG_WAVERAM + 11, 0x98},
    {Apu::REG_WAVERAM + 12, 0x76},
    {Apu::REG_WAVERAM + 13, 0x54},
    {Apu::REG_WAVERAM + 14, 0x32},
    {Apu::REG_WAVERAM + 15, 0x10},
    {Apu::REG_NR30, 0x80}, // DAC on
    {Apu::REG_NR34, 0x80}, // trigger
    {HOLD, 30},
    {Apu::REG_NR34, 0x01}, {HOLD, 30},
    {Apu::REG_NR34, 0x02}, {HOLD, 30},
    {Apu::REG_NR34, 0x03}, {HOLD, 30},
    {Apu::REG_NR34, 0x04}, {HOLD, 30},
    {Apu::REG_NR34, 0x05}, {HOLD, 30},
    {Apu::REG_NR34, 0x06}, {HOLD, 30},
    {Apu::REG_NR34, 0x07}, {HOLD, 60},
    // fade out
    {Apu::REG_NR32, 2 << 5}, {HOLD, 15},
    {Apu::REG_NR32, 3 << 5}, {HOLD, 15},
    {Apu::REG_NR32, 0 << 5}, {HOLD, 15}




};

static DemoCommand const DEMO_HEADROOM[] = {
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0xFF}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR12, 0xF0}, {Apu::REG_NR14, 0x87}, {HOLD, 60},
    {Apu::REG_NR21, 0x80}, { Apu::REG_NR22, 0xF0 }, {Apu::REG_NR24, 0x87}, {HOLD, 60},
    {Apu::REG_WAVERAM,      0x01},
    {Apu::REG_WAVERAM + 1,  0x23},
    {Apu::REG_WAVERAM + 2,  0x45},
    {Apu::REG_WAVERAM + 3,  0x67},
    {Apu::REG_WAVERAM + 4,  0x89},
    {Apu::REG_WAVERAM + 5,  0xAB},
    {Apu::REG_WAVERAM + 6,  0xCD},
    {Apu::REG_WAVERAM + 7,  0xEF},
    {Apu::REG_WAVERAM + 8,  0xFE},
    {Apu::REG_WAVERAM + 9,  0xDC},
    {Apu::REG_WAVERAM + 10, 0xBA},
    {Apu::REG_WAVERAM + 11, 0x98},
    {Apu::REG_WAVERAM + 12, 0x76},
    {Apu::REG_WAVERAM + 13, 0x54},
    {Apu::REG_WAVERAM + 14, 0x32},
    {Apu::REG_WAVERAM + 15, 0x10},
    {Apu::REG_NR30, 0x80}, // DAC on
    {Apu::REG_NR34, 0x84}, {HOLD, 60},
    {Apu::REG_NR42, 0xF0}, {Apu::REG_NR43, 0x55}, { Apu::REG_NR44, 0x80 },
    {HOLD, 60}

};

static DemoCommand const DEMO_ENVELOPE[] = {
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0x11}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR12, 0xF7}, {Apu::REG_NR14, 0x87}, {HOLD, 120},
    {Apu::REG_NR12, 0xF6}, {Apu::REG_NR14, 0x87}, {HOLD, 90},
    {Apu::REG_NR12, 0xF5}, {Apu::REG_NR14, 0x87}, {HOLD, 70},
    {Apu::REG_NR12, 0xF4}, {Apu::REG_NR14, 0x87}, {HOLD, 60},
    {Apu::REG_NR12, 0xF3}, {Apu::REG_NR14, 0x87}, {HOLD, 50},
    {Apu::REG_NR12, 0xF2}, {Apu::REG_NR14, 0x87}, {HOLD, 40},
    {Apu::REG_NR12, 0xF1}, {Apu::REG_NR14, 0x87}, {HOLD, 30},
    {Apu::REG_NR14, 0x87}, {HOLD, 10},
    {Apu::REG_NR14, 0x87}, {HOLD, 10},
    {Apu::REG_NR14, 0x87}, {HOLD, 10},
    {Apu::REG_NR14, 0x87}, {HOLD, 10}



};

static DemoCommand const DEMO_PANNING[] = {
    {Apu::REG_NR52, 0x80}, {Apu::REG_NR51, 0x00}, {Apu::REG_NR50, 0x77},
    {Apu::REG_NR11, 0x80}, {Apu::REG_NR12, 0xFF}, {Apu::REG_NR14, 0x87},
    {Apu::REG_NR11, 0x80}, {Apu::REG_NR12, 0xFF}, {Apu::REG_NR14, 0x87},
    {Apu::REG_NR21, 0x40}, {Apu::REG_NR22, 0xFF}, {Apu::REG_NR24, 0x87},
    {Apu::REG_WAVERAM,      0x01},
    {Apu::REG_WAVERAM + 1,  0x23},
    {Apu::REG_WAVERAM + 2,  0x45},
    {Apu::REG_WAVERAM + 3,  0x67},
    {Apu::REG_WAVERAM + 4,  0x89},
    {Apu::REG_WAVERAM + 5,  0xAB},
    {Apu::REG_WAVERAM + 6,  0xCD},
    {Apu::REG_WAVERAM + 7,  0xEF},
    {Apu::REG_WAVERAM + 8,  0xFE},
    {Apu::REG_WAVERAM + 9,  0xDC},
    {Apu::REG_WAVERAM + 10, 0xBA},
    {Apu::REG_WAVERAM + 11, 0x98},
    {Apu::REG_WAVERAM + 12, 0x76},
    {Apu::REG_WAVERAM + 13, 0x54},
    {Apu::REG_WAVERAM + 14, 0x32},
    {Apu::REG_WAVERAM + 15, 0x10},
    {Apu::REG_NR30, 0x80}, {Apu::REG_NR34, 0x87},
    {Apu::REG_NR42, 0xFF}, {Apu::REG_NR43, 0x44},  {Apu::REG_NR44, 0x80},
    {HOLD, 2},
    {Apu::REG_NR51, 0x10}, {HOLD, 4},
    {Apu::REG_NR51, 0x01}, {HOLD, 4},
    {Apu::REG_NR51, 0x11}, {HOLD, 4},
    {Apu::REG_NR51, 0x20}, {HOLD, 4},
    {Apu::REG_NR51, 0x02}, {HOLD, 4},
    {Apu::REG_NR51, 0x22}, {HOLD, 4},
    {Apu::REG_NR51, 0x40}, {HOLD, 4},
    {Apu::REG_NR51, 0x04}, {HOLD, 4},
    {Apu::REG_NR51, 0x44}, {HOLD, 4},
    {Apu::REG_NR51, 0x80}, {HOLD, 4},
    {Apu::REG_NR51, 0x08}, {HOLD, 4},
    {Apu::REG_NR51, 0x88}, {HOLD, 4}
};

struct Demo {
    const char *name;
    DemoCommand const *sequence;
    size_t sequenceLen;
};

#define demoStruct(name, seq) { name, seq, sizeof(seq) / sizeof(DemoCommand) }

static Demo const DEMO_TABLE[] = {
    demoStruct("duty", DEMO_DUTY),
    demoStruct("master_volume", DEMO_MASTER_VOLUME),
    demoStruct("noise", DEMO_NOISE),
    demoStruct("wave", DEMO_WAVE),
    demoStruct("headroom", DEMO_HEADROOM),
    demoStruct("envelope", DEMO_ENVELOPE),
    demoStruct("panning", DEMO_PANNING)
};

constexpr size_t DEMO_COUNT = sizeof(DEMO_TABLE) / sizeof(Demo);

using Clock = std::chrono::steady_clock;

int main() {

    Apu apu(SAMPLERATE, SAMPLERATE / 10);//, Apu::Quality::medium);
    apu.setQuality(Apu::Quality::high);
    apu.setVolume(0.6f);

    constexpr size_t samplesPerFrame = (CYCLES_PER_FRAME / CYCLES_PER_SAMPLE) + 1;
    std::unique_ptr<int16_t[]> frameBuf(new int16_t[samplesPerFrame * 2]);

    //auto timeStart = std::chrono::steady_clock::now();

    Clock::duration minTime(Clock::duration::max()), maxTime(0);
    Clock::duration avgTime(0);
    Clock::duration elapsed(0);
    size_t frameCounter = 0;

    for (size_t i = 0; i != DEMO_COUNT; ++i) {
        auto &demo = DEMO_TABLE[i];

        std::string filename = "demo_";
        filename.append(demo.name);
        filename.append(".wav");
        Wave_Writer wav(SAMPLERATE, filename.c_str());
        wav.enable_stereo();

        apu.reset();
        apu.clearSamples();

        uint32_t cycles = 0;
        for (size_t j = 0; j != demo.sequenceLen; ++j) {
            auto cmd = demo.sequence[j];
            if (cmd.reg == HOLD) {
                for (size_t frames = cmd.value; frames--; ) {
                    auto now = Clock::now();
                    apu.step(CYCLES_PER_FRAME - cycles);
                    auto frameTime = Clock::now() - now;
                    if (frameTime < minTime) {
                        minTime = frameTime;
                    }
                    if (frameTime > maxTime) {
                        maxTime = frameTime;
                    }
                    ++frameCounter;
                    elapsed += frameTime;
                    
                    apu.endFrame();
                    size_t samples = apu.availableSamples();
                    apu.readSamples(frameBuf.get(), samples);
                    wav.write(frameBuf.get(), samples * 2);

                    cycles = 0;
                }
                
            } else {
                cycles += 3;
                apu.writeRegister(static_cast<Apu::Reg>(cmd.reg), cmd.value);
            }
        }
    }

    std::cout << "Time elapsed: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
        << " ms" << std::endl;
    std::cout << "Minimum: "
        << std::chrono::duration_cast<std::chrono::microseconds>(minTime).count()
        << " us" << std::endl;
    std::cout << "Maximum: "
        << std::chrono::duration_cast<std::chrono::microseconds>(maxTime).count()
        << " us" << std::endl;
    std::cout << "Average: "
        << std::chrono::duration_cast<std::chrono::microseconds>(elapsed / frameCounter).count()
        << " us" << std::endl;


    return 0;
}