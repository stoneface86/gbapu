
#include "gbapu.hpp"


#include <cassert>

namespace gbapu::_internal {

ChannelBase::ChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept :
    Timer(defaultPeriod),
    mFrequency(0),
    mOutput(0),
    mDisableMask(DISABLED),
    mDacOn(false),
    mLengthCounter(0),
    mLengthEnabled(false),
    mLengthCounterMax(lengthCounterMax),
    mDefaultPeriod(defaultPeriod)
{
}

bool ChannelBase::dacOn() const noexcept {
    return mDacOn;
}

void ChannelBase::disable() noexcept {
    mDisableMask = DISABLED;
    mLengthEnabled = false;
}

uint16_t ChannelBase::frequency() const noexcept {
    return mFrequency;
}

void ChannelBase::setDacEnable(bool enabled) noexcept {
    mDacOn = enabled;
    if (!enabled) {
        disable();
    }
}

void ChannelBase::stepLengthCounter() noexcept {
    if (mLengthEnabled) {
        if (--mLengthCounter == 0) {
            disable();
        }
    }
}

void ChannelBase::writeLengthCounter(uint8_t value) noexcept {
    mLengthCounter = value;
}

uint8_t ChannelBase::output() const noexcept {
    return mOutput & mDisableMask;
}

void ChannelBase::reset() noexcept {
    mDacOn = false;
    mDisableMask = DISABLED;
    mFrequency = 0;
    mLengthCounter = 0;
    mLengthEnabled = false;
    // move this to Timer::reset();
    mPeriod = mDefaultPeriod;
    mTimer = 0;
}

void ChannelBase::restart() noexcept {
    mTimer = mPeriod; // reload frequency timer with period
    if (mDacOn) {
        mDisableMask = ENABLED;
    } else {
        disable(); // channel is immediately disabled if the DAC is off
    } 
}

void ChannelBase::restartLengthCounter() noexcept {
    mLengthEnabled = mDacOn;
    if (mLengthEnabled && mLengthCounter == 0) {
        mLengthCounter = mLengthCounterMax;
    }
}

EnvChannelBase::EnvChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept :
    ChannelBase(defaultPeriod, lengthCounterMax),
    mEnvelopeVolume(0),
    mEnvelopeRegister(0),
    mEnvelopeCounter(0),
    mEnvelopePeriod(0),
    mEnvelopeAmplify(false)
{
}

uint8_t EnvChannelBase::readEnvelope() const noexcept {
    return mEnvelopeRegister;
}

void EnvChannelBase::writeEnvelope(uint8_t value) noexcept {
    mEnvelopeRegister = value;
}

void EnvChannelBase::restart() noexcept {
    
    setDacEnable(!!(mEnvelopeRegister & 0xF8));
    mEnvelopeCounter = 0;
    mEnvelopePeriod = mEnvelopeRegister & 0x7;
    mEnvelopeAmplify = !!(mEnvelopeRegister & 0x8);
    mEnvelopeVolume = mEnvelopeRegister >> 4;
    ChannelBase::restart();
}

void EnvChannelBase::stepEnvelope() noexcept {
    if (mEnvelopePeriod) {
        // do nothing if period == 0
        if (++mEnvelopeCounter == mEnvelopePeriod) {
            mEnvelopeCounter = 0;
            if (mEnvelopeAmplify) {
                if (mEnvelopeVolume < 0xF) {
                    ++mEnvelopeVolume;
                }
            } else {
                if (mEnvelopeVolume > 0x0) {
                    --mEnvelopeVolume;
                }
            }
        }
    }
}

void EnvChannelBase::reset() noexcept {
    ChannelBase::reset();
    mEnvelopeVolume = 0;
    mEnvelopeRegister = 0;
    mEnvelopeCounter = 0;
    mEnvelopePeriod = 0;
    mEnvelopeAmplify = false;
}

uint8_t EnvChannelBase::volume() const noexcept {
    return mEnvelopeVolume;
}


}
