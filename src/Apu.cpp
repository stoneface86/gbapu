
#include "gbapu/Apu.hpp"

#include <algorithm>

namespace gbapu {

Apu::Apu(Buffer &buffer) :
    mBuffer(buffer),
    mHf(),
    mSequencer(mHf),
    mLeftVolume(Gbs::MAX_TERM_VOLUME + 1),
    mRightVolume(Gbs::DEFAULT_TERM_VOLUME + 1),
    mOutputStat(0),
    mLastAmps{ 0 },
    mEnabled(false)
{
}

void Apu::writeRegister(uint16_t addr, uint8_t value) {
    writeRegister(static_cast<Reg>(addr & 0xFF), value);
}

void Apu::writeRegister(Reg reg, uint8_t value) {
    #define writeDuty(gen) gen.setDuty(static_cast<Gbs::Duty>(value >> 6))
    #define writeFreqLSB(gen) gen.setFrequency((gen.frequency() & 0xFF00) | value)
    #define writeFreqMSB(gen) gen.setFrequency((gen.frequency() & 0x00FF) | ((value & 0x7) << 8))
    #define onTrigger() if (!!(value & 0x80))

    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this write
        return;
    }

    switch (reg) {
        case REG_NR10:
            mHf.sweep1.writeRegister(value);
            break;
        case REG_NR11:
            writeDuty(mHf.gen1);
            // length counters aren't implemented so ignore the other 6 bits
            break;
        case REG_NR12:
            mHf.env1.writeRegister(value);
            break;
        case REG_NR13:
            writeFreqLSB(mHf.gen1);
            break;
        case REG_NR14:
            writeFreqMSB(mHf.gen1);
            onTrigger() {
                mHf.env1.restart();
                mHf.sweep1.restart();
                mHf.gen1.restart();
            }
            break;
        case REG_NR21:
            writeDuty(mHf.gen2);
            break;
        case REG_NR22:
            mHf.env2.writeRegister(value);
            break;
        case REG_NR23:
            writeFreqLSB(mHf.gen2);
            break;
        case REG_NR24:
            writeFreqMSB(mHf.gen2);
            onTrigger() {
                mHf.env2.restart();
                mHf.gen2.restart();
            }
            break;
        case REG_NR30:
            // TODO: implement functionality in WaveGen
            break;
        case REG_NR31:
            // TODO
            break;
        case REG_NR32:
            mHf.gen3.setVolume(static_cast<Gbs::WaveVolume>((value >> 5) & 0x3));
            break;
        case REG_NR33:
            writeFreqLSB(mHf.gen3);
            break;
        case REG_NR34:
            writeFreqMSB(mHf.gen3);
            onTrigger() {
                mHf.gen3.restart();
            }
            break;
        case REG_NR41:
            // TODO
            break;
        case REG_NR42:
            mHf.env4.writeRegister(value);
            break;
        case REG_NR43:
            mHf.gen4.writeRegister(value);
            break;
        case REG_NR44:
            onTrigger() {
                mHf.env4.restart();
                mHf.gen4.restart();
            }
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
            if (mHf.gen3.disabled()) {
                auto waveram = mHf.gen3.waveram();
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
            mBuffer.addDelta12(1, rightdelta * mLeftVolume, mCycletime);
            rightdelta = 0;
        }

        getOutput<2>(leftdelta, rightdelta);
        getOutput<3>(leftdelta, rightdelta);

        if (leftdelta) {
            mBuffer.addDelta34(0, leftdelta * mRightVolume, mCycletime);
        }
        if (rightdelta) {
            mBuffer.addDelta34(1, rightdelta * mRightVolume, mCycletime);
        }

        // get the smallest timer and we will step to it
        uint32_t cyclesToStep = std::min({
            cycles,
            mSequencer.timer(),
            mHf.gen1.timer(),
            mHf.gen2.timer(),
            mHf.gen3.timer(),
            mHf.gen4.timer()
        });

        // step hardware components
        mSequencer.step(cyclesToStep);
        mHf.gen1.step(cyclesToStep);
        mHf.gen2.step(cyclesToStep);
        mHf.gen3.step(cyclesToStep);
        mHf.gen4.step(cyclesToStep);

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
        output = mHf.gen1.output();
    } else if constexpr (ch == 1) {
        output = mHf.gen2.output();
    } else if constexpr (ch == 2) {
        output = mHf.gen3.output();
    } else {
        output = mHf.gen4.output();
    }

    if constexpr (ch != 2) {
        if (output) {
            if constexpr (ch == 0) {
                output = mHf.env1.value();
            } else if constexpr (ch == 1) {
                output = mHf.env2.value();
            } else {
                output = mHf.env4.value();
            }
        }
    }

    // get the previous volumes
    uint8_t &prevL = mLastAmps[ch];
    uint8_t &prevR = mLastAmps[ch + 4];
    
    // convert output stat (NR51) to a mask
    uint8_t maskL = ~((mOutputStat >> ch) & 1) + 1;
    uint8_t maskR = ~((mOutputStat >> (ch + 4)) & 1) + 1;

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