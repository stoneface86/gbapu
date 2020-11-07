
#include "gbapu.hpp"

#include <algorithm>

namespace gbapu {

Apu::Apu(Buffer &buffer) :
    mBuffer(buffer),
    mCf(),
    mSequencer(mCf),
    mLeftVolume(1),
    mRightVolume(1),
    mOutputStat(0),
    mLastAmps{ 0 },
    mEnabled(false)
{
}

void Apu::reset() noexcept {
    endFrame();

    mSequencer.reset();
    mCf.ch1.reset();
    mCf.ch2.reset();
    mCf.ch3.reset();
    mCf.ch4.reset();
    mOutputStat = 0;
    mLeftVolume = 1;
    mRightVolume = 1;
    std::fill_n(mLastAmps, 8, static_cast<uint8_t>(0));
    
    mEnabled = false;

}

uint8_t Apu::readRegister(uint16_t addr) {
    return readRegister(static_cast<Reg>(addr & 0xFF));
}

uint8_t Apu::readRegister(Reg reg) {

    /*
    * Read masks
    *      NRx0 NRx1 NRx2 NRx3 NRx4
    *     ---------------------------
    * NR1x  $80  $3F $00  $FF  $BF 
    * NR2x  $FF  $3F $00  $FF  $BF 
    * NR3x  $7F  $FF $9F  $FF  $BF
    * NR4x  $FF  $FF $00  $00  $BF
    * NR5x  $00  $00 $70
    *
    * $FF27-$FF2F always read back as $FF
    */


    // TODO: length counters can still be accessed on DMG when powered off
    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this read
        return 0xFF;
    }

    switch (reg) {
        // ===== CH1 =====

        case REG_NR10:
            return mCf.ch1.readSweep();
        case REG_NR11:
            return 0x3F | mCf.ch1.readDuty();
        case REG_NR12:
            return mCf.ch1.readEnvelope();
        case REG_NR13:
            return 0xFF;
        case REG_NR14:
            return mCf.ch1.lengthEnabled() ? 0xFF : 0xBF;
        
        // ===== CH2 =====

        case REG_NR21:
            return 0x3F | mCf.ch2.readDuty();
        case REG_NR22:
            return mCf.ch2.readEnvelope();
        case REG_NR23:
            return 0xFF;
        case REG_NR24:
            return mCf.ch2.lengthEnabled() ? 0xFF : 0xBF;

        // ===== CH3 =====

        case REG_NR30:
            return mCf.ch3.dacOn() ? 0xFF : 0x7F;
        case REG_NR31:
            return 0xFF;
        case REG_NR32:
            return 0x9F | mCf.ch3.readVolume();
        case REG_NR33:
            return 0xFF;
        case REG_NR34:
            return mCf.ch3.lengthEnabled() ? 0xFF : 0xBF;

        // ===== CH4 =====

        case REG_NR41:
            return 0xFF;
        case REG_NR42:
            return mCf.ch4.readEnvelope();
        case REG_NR43:
            return mCf.ch4.readNoise();
        case REG_NR44:
            return mCf.ch4.lengthEnabled() ? 0xFF : 0xBF;

       // ===== Sound control ======

        case REG_NR50:
            // Not implemented: Vin, always read back as 0
            return ((mLeftVolume - 1) << 4) | (mRightVolume - 1);
        case REG_NR51:
            return mOutputStat;
        case REG_NR52:
        {
            uint8_t nr52 = mEnabled ? 0xF0 : 70;
            if (mCf.ch1.dacOn()) {
                nr52 |= 0x1;
            }
            if (mCf.ch2.dacOn()) {
                nr52 |= 0x2;
            }
            if (mCf.ch3.dacOn()) {
                nr52 |= 0x4;
            }
            if (mCf.ch4.dacOn()) {
                nr52 |= 0x8;
            }
            return nr52;
        }

        case REG_WAVERAM:
        case REG_WAVERAM + 1:
        case REG_WAVERAM + 2:
        case REG_WAVERAM + 3:
        case REG_WAVERAM + 4:
        case REG_WAVERAM + 5:
        case REG_WAVERAM + 6:
        case REG_WAVERAM + 7:
        case REG_WAVERAM + 8:
        case REG_WAVERAM + 9:
        case REG_WAVERAM + 10:
        case REG_WAVERAM + 11:
        case REG_WAVERAM + 12:
        case REG_WAVERAM + 13:
        case REG_WAVERAM + 14:
        case REG_WAVERAM + 15:
            if (mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                return waveram[reg - REG_WAVERAM];
            }
            return 0xFF;
        default:
            return 0xFF;
    }
}

void Apu::writeRegister(uint16_t addr, uint8_t value) {
    writeRegister(static_cast<Reg>(addr & 0xFF), value);
}

void Apu::writeRegister(Reg reg, uint8_t value) {

    // TODO: length counters can still be accessed on DMG when powered off
    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this write
        return;
    }

    switch (reg) {
        case REG_NR10:
            mCf.ch1.writeSweep(value);
            break;
        case REG_NR11:
            mCf.ch1.writeDuty(value >> 6);
            mCf.ch1.writeLengthCounter(value & 0x3F);
            break;
        case REG_NR12:
            mCf.ch1.writeEnvelope(value);
            break;
        case REG_NR13:
            mCf.ch1.writeFrequencyLsb(value);
            break;
        case REG_NR14:
            mCf.ch1.writeFrequencyMsb(value);
            break;
        case REG_NR21:
            mCf.ch2.writeDuty(value >> 6);
            mCf.ch2.writeLengthCounter(value & 0x3F);
            break;
        case REG_NR22:
            mCf.ch2.writeEnvelope(value);
            break;
        case REG_NR23:
            mCf.ch2.writeFrequencyLsb(value);
            break;
        case REG_NR24:
            mCf.ch2.writeFrequencyMsb(value);
            break;
        case REG_NR30:
            mCf.ch3.setDacEnable(!!(value & 0x80));
            break;
        case REG_NR31:
            mCf.ch3.writeLengthCounter(value);
            break;
        case REG_NR32:
            mCf.ch3.writeVolume(value);
            break;
        case REG_NR33:
            mCf.ch3.writeFrequencyLsb(value);
            break;
        case REG_NR34:
            mCf.ch3.writeFrequencyMsb(value);
            break;
        case REG_NR41:
            mCf.ch4.writeLengthCounter(value & 0x3F);
            break;
        case REG_NR42:
            mCf.ch4.writeEnvelope(value);
            break;
        case REG_NR43:
            mCf.ch4.writeFrequencyLsb(value);
            break;
        case REG_NR44:
            mCf.ch4.writeFrequencyMsb(value);
            break;
        case REG_NR50:
            // do nothing with the Vin bits
            // Vin will not be emulated since no cartridge in history ever made use of it
            mLeftVolume = ((value >> 4) & 0x7) + 1;
            mRightVolume = (value & 0x7) + 1;
            break;
        case REG_NR51:
            mOutputStat = value;
            break;
        case REG_NR52:
            if (!!(value & 0x80) != mEnabled) {
                
                if (mEnabled) {
                    // shutdown
                    // zero out all registers
                    for (uint8_t i = REG_NR10; i != REG_NR52; ++i) {
                        writeRegister(static_cast<Reg>(i), 0);
                    }
                    mEnabled = false;
                } else {
                    // startup
                    mEnabled = true;
                    //mHf.gen1.softReset();
                    //mHf.gen2.softReset();
                    //mHf.gen3.softReset();
                    mSequencer.reset();
                }
                
            }
            break;
        case REG_WAVERAM:
        case REG_WAVERAM + 1:
        case REG_WAVERAM + 2:
        case REG_WAVERAM + 3:
        case REG_WAVERAM + 4:
        case REG_WAVERAM + 5:
        case REG_WAVERAM + 6:
        case REG_WAVERAM + 7:
        case REG_WAVERAM + 8:
        case REG_WAVERAM + 9:
        case REG_WAVERAM + 10:
        case REG_WAVERAM + 11:
        case REG_WAVERAM + 12:
        case REG_WAVERAM + 13:
        case REG_WAVERAM + 14:
        case REG_WAVERAM + 15:
            // if CH3's DAC is enabled, then the write goes to the current waveposition
            // this can only be done within a few clocks when CH3 accesses waveram, otherwise the write has no effect
            if (mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                waveram[reg - REG_WAVERAM] = value;
            }
            // ignore write if enabled
            break;
        default:
            break;
    }
}

void Apu::step(uint32_t cycles) {
    while (cycles) {

        // delta is the amount of change from the previous output
        // 0 indicates no change, or no transition
        int8_t leftdelta = 0;
        int8_t rightdelta = 0;

        getOutput<0>(leftdelta, rightdelta);
        getOutput<1>(leftdelta, rightdelta);
        
        // add deltas to the blip bufs if nonzero
        if (leftdelta) {
            mBuffer.addDelta12(0, leftdelta * mLeftVolume, mCycletime);
            leftdelta = 0;
        }
        if (rightdelta) {
            mBuffer.addDelta12(1, rightdelta * mRightVolume, mCycletime);
            rightdelta = 0;
        }

        getOutput<2>(leftdelta, rightdelta);
        getOutput<3>(leftdelta, rightdelta);

        if (leftdelta) {
            mBuffer.addDelta34(0, leftdelta * mLeftVolume, mCycletime);
        }
        if (rightdelta) {
            mBuffer.addDelta34(1, rightdelta * mRightVolume, mCycletime);
        }

        // get the smallest timer and we will step to it
        uint32_t cyclesToStep = std::min({
            cycles,
            mSequencer.timer(),
            mCf.ch1.timer(),
            mCf.ch2.timer(),
            mCf.ch3.timer(),
            mCf.ch4.timer()
        });

        // step hardware components
        mSequencer.step(cyclesToStep);
        mCf.ch1.step(mCycletime, cyclesToStep);
        mCf.ch2.step(mCycletime, cyclesToStep);
        mCf.ch3.step(mCycletime, cyclesToStep);
        mCf.ch4.step(mCycletime, cyclesToStep);

        // update cycle counters
        cycles -= cyclesToStep;
        mCycletime += cyclesToStep;
    }
}

void Apu::endFrame() {
    mBuffer.endFrame(mCycletime);
    mCycletime = 0;
}

template <int ch>
void Apu::getOutput(int8_t &leftdelta, int8_t &rightdelta) {

    uint8_t output;

    if constexpr (ch == 0) {
        output = mCf.ch1.output();
    } else if constexpr (ch == 1) {
        output = mCf.ch2.output();
    } else if constexpr (ch == 2) {
        output = mCf.ch3.output();
    } else {
        output = mCf.ch4.output();
    }

    if constexpr (ch != 2) {
        if (output) {
            if constexpr (ch == 0) {
                output = mCf.ch1.volume();
            } else if constexpr (ch == 1) {
                output = mCf.ch2.volume();
            } else {
                output = mCf.ch4.volume();
            }
        }
    }

    // get the previous volumes
    uint8_t &prevL = mLastAmps[ch];
    uint8_t &prevR = mLastAmps[ch + 4];
    
    // convert output stat (NR51) to a mask
    uint8_t maskR = ~((mOutputStat >> ch) & 1) + 1;
    uint8_t maskL = ~((mOutputStat >> (ch + 4)) & 1) + 1;

    int8_t outputL = output & maskL;
    int8_t outputR = output & maskR;
    // calculate and accumulate deltas
    leftdelta += outputL - static_cast<int8_t>(prevL);
    rightdelta += outputR - static_cast<int8_t>(prevR);

    // save the previous values for next time
    prevL = outputL;
    prevR = outputR;

}


}