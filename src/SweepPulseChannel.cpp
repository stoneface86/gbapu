
#include "gbapu/SweepPulseChannel.hpp"

namespace gbapu {

SweepPulseChannel::SweepPulseChannel() noexcept :
    PulseChannel(),
    mSweepMode(Gbs::DEFAULT_SWEEP_MODE),
    mSweepTime(Gbs::DEFAULT_SWEEP_TIME),
    mSweepShift(Gbs::DEFAULT_SWEEP_SHIFT),
    mSweepCounter(0),
    mSweepRegister(Gbs::DEFAULT_SWEEP_REGISTER),
    mShadow(0)
{
}

uint8_t SweepPulseChannel::readSweep() const noexcept {
    return mSweepRegister;
}

void SweepPulseChannel::reset() noexcept {
    mSweepRegister = Gbs::DEFAULT_SWEEP_REGISTER;
    restart();
}

void SweepPulseChannel::restart() noexcept {
    PulseChannel::restart();
    mSweepCounter = 0;
    mSweepShift = mSweepRegister & 0x7;
    mSweepMode = static_cast<Gbs::SweepMode>((mSweepRegister >> 3) & 1);
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
                if (mSweepMode == Gbs::SWEEP_SUBTRACTION) {
                    sweepfreq = mShadow - sweepfreq;
                    if (sweepfreq < 0) {
                        return; // no change
                    }
                } else {
                    sweepfreq = mShadow + sweepfreq;
                    if (sweepfreq > Gbs::MAX_FREQUENCY) {
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
