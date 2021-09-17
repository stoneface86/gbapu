
#include "gbapu.hpp"


#include <cassert>

namespace gbapu::_internal {

ChannelBase::ChannelBase(uint32_t defaultPeriod, uint32_t minPeriod, unsigned lengthCounterMax) noexcept :
    Timer(defaultPeriod),
    mFrequency(0),
    mOutput(0),
    mLastOutput(0),
    mDacOn(false),
    mLengthCounter(0),
    mLengthEnabled(false),
    mDisabled(true),
    mLengthCounterMax(lengthCounterMax),
    mDefaultPeriod(defaultPeriod),
    mMinPeriod(minPeriod)
{
}

bool ChannelBase::dacOn() const noexcept {
    return mDacOn;
}

int8_t ChannelBase::lastOutput() const noexcept {
    return mLastOutput;
}

bool ChannelBase::lengthEnabled() const noexcept {
    return mLengthEnabled;
}

void ChannelBase::disable() noexcept {
    mDisabled = true;
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
        if (mLengthCounter == 0) {
            disable();
        } else {
            --mLengthCounter;
        }
    }
}

void ChannelBase::step(Mixer &mixer, MixMode mode, uint32_t cycletime, uint32_t cycles) {
    if (mDacOn) {
        if (mDisabled) {
            // when disabled the DAC gets an input of 0
            if (mLastOutput) {
                //
                mixer.mix(mode, -mLastOutput, cycletime);
            }
            stepImpl<MixMode::mute>(mixer, cycletime, cycles);
            mOutput = 0;
            mLastOutput = 0;
        } else if (mPeriod < mMinPeriod) {
            // channel is disabled or is too high of a frequency, don't bother mixing
            // no need to a transition to 0, the high pass filter will do so eventually
            stepImpl<MixMode::mute>(mixer, cycletime, cycles);
            
        } else {

            switch (mode) {
                case MixMode::mute:
                    stepImpl<MixMode::mute>(mixer, cycletime, cycles);
                    break;
                case MixMode::left:
                    stepImpl<MixMode::left>(mixer, cycletime, cycles);
                    break;
                case MixMode::right:
                    stepImpl<MixMode::right>(mixer, cycletime, cycles);
                    break;
                case MixMode::middle:
                    stepImpl<MixMode::middle>(mixer, cycletime, cycles);
                    break;
                default:
                    break;
            }
        }
    }
}

// NOTE: highQualityMute and lowQualityMute are the same function! only use lowQualityMute
template <MixMode mode>
void ChannelBase::stepImpl(Mixer &mixer, uint32_t cycletime, uint32_t cycles) {
    while (cycles) {
        if constexpr (mode != MixMode::mute) {
            if (mLastOutput != mOutput) {
                mixer.mixfast<mode>(mOutput - mLastOutput, cycletime);
                mLastOutput = mOutput;
            }
        }
        auto toStep = std::min(mTimer, cycles);
        if (stepTimer(toStep)) {
            stepOscillator();
        }
        cycles -= toStep;
        cycletime += toStep;
    }

    if constexpr (mode == MixMode::mute) {
        // kept out of the loop for speed
        mLastOutput = mOutput;
    }
    
}

void ChannelBase::writeFrequencyLsb(uint8_t value) {
    mFrequency = (mFrequency & 0xFF00) | value;
    setPeriod();
}

void ChannelBase::writeFrequencyMsb(uint8_t value) {
    mFrequency = (mFrequency & 0x00FF) | ((value & 0x7) << 8);
    setPeriod();
    setLengthCounterEnable(!!(value & 0x40));

    if (!!(value & 0x80)) {
        restart();
    }
}

void ChannelBase::writeLengthCounter(uint8_t value) noexcept {
    mLengthCounter = value;
}

void ChannelBase::reset() noexcept {
    mDacOn = false;
    mDisabled = true;
    mFrequency = 0;
    mLengthCounter = 0;
    mLengthEnabled = false;
    mPeriod = mDefaultPeriod;
    mTimer = mPeriod;
    
    //mLastDacOutput = -15;
    mLastOutput = 0;
}

void ChannelBase::restart() noexcept {
    mTimer = mPeriod; // reload frequency timer with period
    if (mLengthCounter == 0) {
        mLengthCounter = mLengthCounterMax;
    }
    mDisabled = !mDacOn;
}

void ChannelBase::setLengthCounterEnable(bool enable) {
    mLengthEnabled = enable;
}

EnvChannelBase::EnvChannelBase(uint32_t defaultPeriod, uint32_t minPeriod, unsigned lengthCounterMax) noexcept :
    ChannelBase(defaultPeriod, minPeriod, lengthCounterMax),
    mEnvelopeRegister(0),
    mEnvelopeCounter(0),
    mEnvelopePeriod(0),
    mEnvelopeAmplify(false),
    mVolume(0)
{
}

uint8_t EnvChannelBase::readEnvelope() const noexcept {
    return mEnvelopeRegister;
}

void EnvChannelBase::writeEnvelope(uint8_t value) noexcept {
    setDacEnable(!!(value & 0xF8));
    mEnvelopeRegister = value;
}

void EnvChannelBase::restart() noexcept {
    mEnvelopeCounter = 0;
    mEnvelopePeriod = mEnvelopeRegister & 0x7;
    mEnvelopeAmplify = !!(mEnvelopeRegister & 0x8);
    mVolume = mEnvelopeRegister >> 4;
    ChannelBase::restart();
}

void EnvChannelBase::stepEnvelope() noexcept {
    if (mEnvelopePeriod) {
        // do nothing if period == 0
        if (++mEnvelopeCounter == mEnvelopePeriod) {
            mEnvelopeCounter = 0;
            if (mEnvelopeAmplify) {
                if (mVolume < 0xF) {
                    ++mVolume;
                }
            } else {
                if (mVolume > 0x0) {
                    --mVolume;
                }
            }
        }
    }
}

void EnvChannelBase::reset() noexcept {
    ChannelBase::reset();
    
    mEnvelopeRegister = 0;
    mEnvelopeCounter = 0;
    mEnvelopePeriod = 0;
    mEnvelopeAmplify = false;
    mVolume = 0;
}



}
