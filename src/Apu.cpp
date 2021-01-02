
#include "gbapu.hpp"

#include <algorithm>

namespace gbapu {

Apu::Apu(Buffer &buffer, Model model) :
    mBuffer(buffer),
    mModel(model),
    mCf(),
    mSequencer(mCf),
    mRegs{ 0 },
    mCycletime(0),
    mLeftVolume(1),
    mRightVolume(1),
    mOutputStat(0),
    mLastAmpLeft(0),
    mLastAmpRight(0),
    mEnabled(false)
{
}

Registers const& Apu::registers() const {
    return mRegs;
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
    mLastAmpLeft = 0;
    mLastAmpRight = 0;
    mEnabled = false;

}

void Apu::reset(Model model) noexcept {
    mModel = model;
    reset();
}

uint8_t Apu::readRegister(uint8_t reg, uint32_t autostep) {

    step(autostep);

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
            if (mModel == Model::cgb || mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                return waveram[reg - REG_WAVERAM];
            }
            return 0xFF;
        default:
            return 0xFF;
    }
}

void Apu::writeRegister(uint8_t reg, uint8_t value, uint32_t autostep) {
    step(autostep);

    // TODO: length counters can still be accessed on DMG when powered off
    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this write
        return;
    }


    switch (reg) {
        case REG_NR10:
            mCf.ch1.writeSweep(value);
            mRegs.nr10 = value & 0x7F;
            break;
        case REG_NR11:
            mCf.ch1.writeDuty(value >> 6);
            mCf.ch1.writeLengthCounter(value & 0x3F);
            mRegs.nr11 = value;
            break;
        case REG_NR12:
            mCf.ch1.writeEnvelope(value);
            mRegs.nr12 = value;
            break;
        case REG_NR13:
            mCf.ch1.writeFrequencyLsb(value);
            mRegs.nr13 = value;
            break;
        case REG_NR14:
            mCf.ch1.writeFrequencyMsb(value);
            mRegs.nr14 = value & 0xC7;
            break;
        case REG_NR21:
            mCf.ch2.writeDuty(value >> 6);
            mCf.ch2.writeLengthCounter(value & 0x3F);
            mRegs.nr21 = value;
            break;
        case REG_NR22:
            mCf.ch2.writeEnvelope(value);
            mRegs.nr22 = value;
            break;
        case REG_NR23:
            mCf.ch2.writeFrequencyLsb(value);
            mRegs.nr23 = value;
            break;
        case REG_NR24:
            mCf.ch2.writeFrequencyMsb(value);
            mRegs.nr24 = value & 0xC7;
            break;
        case REG_NR30:
            mCf.ch3.setDacEnable(!!(value & 0x80));
            mRegs.nr30 = value & 0x80;
            break;
        case REG_NR31:
            mCf.ch3.writeLengthCounter(value);
            mRegs.nr31 = value;
            break;
        case REG_NR32:
            mCf.ch3.writeVolume(value);
            mRegs.nr32 = value & 0x60;
            break;
        case REG_NR33:
            mCf.ch3.writeFrequencyLsb(value);
            mRegs.nr33 = value;
            break;
        case REG_NR34:
            mCf.ch3.writeFrequencyMsb(value);
            mRegs.nr34 = value & 0xC7;
            break;
        case REG_NR41:
            mCf.ch4.writeLengthCounter(value & 0x3F);
            mRegs.nr41 = value & 0x3F;
            break;
        case REG_NR42:
            mCf.ch4.writeEnvelope(value);
            mRegs.nr42 = value;
            break;
        case REG_NR43:
            mCf.ch4.writeFrequencyLsb(value);
            mRegs.nr43 = value;
            break;
        case REG_NR44:
            mCf.ch4.writeFrequencyMsb(value);
            mRegs.nr44 = value & 0xC0;
            break;
        case REG_NR50:
            // do nothing with the Vin bits
            // Vin will not be emulated since no cartridge in history ever made use of it
            mLeftVolume = ((value >> 4) & 0x7) + 1;
            mRightVolume = (value & 0x7) + 1;
            mRegs.nr50 = value & 0x77;
            break;
        case REG_NR51:
            mOutputStat = value;
            mRegs.nr51 = value;
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
                    mRegs.nr52 &= ~0x80;
                } else {
                    // startup
                    mEnabled = true;
                    //mHf.gen1.softReset();
                    //mHf.gen2.softReset();
                    //mHf.gen3.softReset();
                    mSequencer.reset();
                    mRegs.nr52 |= 0x80;
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
            // this behavior was fixed for the CGB, so we can access waveram whenever
            if (mModel == Model::cgb || mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                waveram[reg - REG_WAVERAM] = value;
            }
            // ignore write if enabled
            break;
        default:
            return;
    }
}

void Apu::step(uint32_t cycles) {
    while (cycles) {

        int16_t ampLeft = 0;
        int16_t ampRight = 0;

        // figure out the largest step we can take without missing any
        // changes in output.
        uint32_t cyclesToStep = std::min(cycles, mSequencer.timer());
        getOutput<0>(ampLeft, ampRight, cyclesToStep);
        getOutput<1>(ampLeft, ampRight, cyclesToStep);
        getOutput<2>(ampLeft, ampRight, cyclesToStep);
        getOutput<3>(ampLeft, ampRight, cyclesToStep);

        // volume scale
        ampLeft *= mLeftVolume;
        ampRight *= mRightVolume;

        // calculate deltas, a nonzero value indicates a transition
        int16_t deltaLeft = ampLeft - mLastAmpLeft;
        int16_t deltaRight = ampRight - mLastAmpRight;

        if (deltaLeft) {
            mBuffer.addDelta(0, deltaLeft, mCycletime);
            mLastAmpLeft = ampLeft;
        }

        if (deltaRight) {
            mBuffer.addDelta(1, deltaRight, mCycletime);
            mLastAmpRight = ampRight;
        }

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

void Apu::stepTo(uint32_t time) {
    if (time <= mCycletime) {
        return;
    }

    step(time - mCycletime);
}

void Apu::endFrame() {
    mBuffer.endFrame(mCycletime);
    mCycletime = 0;
}

template <int ch>
void Apu::getOutput(int16_t &leftamp, int16_t &rightamp, uint32_t &timer) {

    _internal::ChannelBase *baseChannel;
    if constexpr (ch == 0) {
        baseChannel = &mCf.ch1;
    } else if constexpr (ch == 1) {
        baseChannel = &mCf.ch2;
    } else if constexpr (ch == 2) {
        baseChannel = &mCf.ch3;
    } else {
        baseChannel = &mCf.ch4;
    }

    if (baseChannel->dacOn()) {
        int8_t output = baseChannel->output();

        if constexpr (ch == 2) {
            //  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F | sample
            // -F -D -B -9 -7 -5 -3 -1 +1 +3 +5 +7 +9 +B +D +F | amplitude
            output = 2 * output - 15;
        } else {
            // oscillates between envelope and -envelope
            
            // 0, 1 => -1, 1
            output = (output << 1) - 1;
            output *= static_cast<_internal::EnvChannelBase*>(baseChannel)->volume();
           
        }

        if (!!((mOutputStat >> (ch + 4)) & 1)) {
            leftamp += output;
        }
        if (!!((mOutputStat >> ch) & 1)) {
            rightamp += output;
        }

        timer = std::min(timer, baseChannel->timer());
    }

    

    

}


}