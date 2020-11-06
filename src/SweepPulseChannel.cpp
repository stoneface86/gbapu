
#include "gbapu.hpp"

namespace gbapu::_internal {

SweepPulseChannel::SweepPulseChannel() noexcept :
    PulseChannel(),
    mSweepSubtraction(false),
    mSweepTime(0),
    mSweepShift(0),
    mSweepCounter(0),
    mSweepRegister(0),
    mShadow(0)
{
}

uint8_t SweepPulseChannel::readSweep() const noexcept {
    return mSweepRegister;
}

void SweepPulseChannel::reset() noexcept {
    PulseChannel::reset();
    mSweepRegister = 0;
    restart();
}

void SweepPulseChannel::restart() noexcept {
    PulseChannel::restart();
    mSweepCounter = 0;
    mSweepShift = mSweepRegister & 0x7;
    mSweepSubtraction = !!((mSweepRegister >> 3) & 1);
    mSweepTime = (mSweepRegister >> 4) & 0x7;
    mShadow = mFrequency;
}

void SweepPulseChannel::writeSweep(uint8_t reg) noexcept {
    mSweepRegister = reg & 0x7F;
}

void SweepPulseChannel::stepSweep() noexcept {
    if (mSweepTime) {
        if (++mSweepCounter >= mSweepTime) {
            mSweepCounter = 0;
            if (mSweepShift) {
                int16_t sweepfreq = mShadow >> mSweepShift;
                if (mSweepSubtraction) {
                    sweepfreq = mShadow - sweepfreq;
                    if (sweepfreq < 0) {
                        return; // no change
                    }
                } else {
                    sweepfreq = mShadow + sweepfreq;
                    if (sweepfreq > constants::MAX_FREQUENCY) {
                        // sweep will overflow, disable the channel
                        disable();
                        return;
                    }
                }
                // no overflow/underflow
                // write-back the shadow register to CH1's frequency register
                mFrequency = static_cast<uint16_t>(sweepfreq);
                setPeriod();
                mShadow = sweepfreq;
            }
        }
    }
}

}
