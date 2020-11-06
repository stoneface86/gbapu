
#pragma once

#include "gbapu/Buffer.hpp"
#include "gbapu/ChannelFile.hpp"
#include "gbapu/Sequencer.hpp"

#include <memory>

namespace gbapu {


class Apu {

public:

    enum Reg {
        // CH1 - Square 1 --------------------------------------------------------
        REG_NR10 = 0x10, // -PPP NSSS | sweep period, negate, shift
        REG_NR11 = 0x11, // DDLL LLLL | duty, length
        REG_NR12 = 0x12, // VVVV APPP | envelope volume, mode, period
        REG_NR13 = 0x13, // FFFF FFFF | Frequency LSB
        REG_NR14 = 0x14, // TL-- -FFF | Trigger, length enable, freq MSB
        // CH2 - Square 2 --------------------------------------------------------
        REG_UNUSED1 = 0x15,
        REG_NR21 = 0x16, // DDLL LLLL | duty, length
        REG_NR22 = 0x17, // VVVV APPP | envelope volume, mode, period
        REG_NR23 = 0x18, // FFFF FFFF | frequency LSB
        REG_NR24 = 0x19, // TL-- -FFF | Trigger, length enable, freq MSB
        // CH3 - Wave ------------------------------------------------------------
        REG_NR30 = 0x1A, // E--- ---- | DAC Power
        REG_NR31 = 0x1B, // LLLL LLLL | length
        REG_NR32 = 0x1C, // -VV- ---- | wave volume 
        REG_NR33 = 0x1D, // FFFF FFFF | frequency LSB
        REG_NR34 = 0x1E, // TL-- -FFF | Trigger, length enable, freq MSB
        // CH4 - Noise -----------------------------------------------------------
        REG_UNUSED2 = 0x1F,
        REG_NR41 = 0x20, // --LL LLLL | length
        REG_NR42 = 0x21, // VVVV APPP | envelope volume, mode, period 
        REG_NR43 = 0x22, // SSSS WDDD | clock shift, width, divisor mode
        REG_NR44 = 0x23, // TL-- ---- | trigger, length enable
        // Control/Status --------------------------------------------------------
        REG_NR50 = 0x24, // ALLL BRRR | Terminal enable/volume
        REG_NR51 = 0x25, // 4321 4321 | channel terminal enables
        REG_NR52 = 0x26, // P--- 4321 | Power control, channel len. stat
        // Wave RAM
        REG_WAVERAM = 0x30
    };

    Apu(Buffer &buffer);
    ~Apu() = default;

    void reset();

    void step(uint32_t cycles);

    void endFrame();

    uint8_t readRegister(uint16_t addr);
    uint8_t readRegister(uint8_t reg);

    void writeRegister(uint16_t addr, uint8_t value);
    void writeRegister(Reg reg, uint8_t value);


private:

    template <int channel>
    void getOutput(int8_t &leftdelta, int8_t &rightdelta);


    Buffer &mBuffer;

    ChannelFile mCf;
    Sequencer mSequencer;

    uint32_t mCycletime;

    // mixer
    uint8_t mLeftVolume;
    uint8_t mRightVolume;

    uint8_t mOutputStat;

    uint8_t mLastAmps[8];

    bool mEnabled;

};



}