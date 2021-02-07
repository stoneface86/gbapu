
#include "gbapu.hpp"


#include <cassert>

namespace gbapu::_internal {

ChannelBase::ChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept :
    Timer(defaultPeriod),
    mFrequency(0),
    mOutput(0),
    mVolume(0),
    mDisableMask(DISABLED),
    mLastDacOutput(0),
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

int8_t ChannelBase::dacOutput() const noexcept {
    return mLastDacOutput;
}

bool ChannelBase::lengthEnabled() const noexcept {
    return mLengthEnabled;
}

void ChannelBase::disable() noexcept {
    mDisableMask = DISABLED;
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
    switch (mode) {
        case MixMode::lowQualityMute:
        case MixMode::highQualityMute:
            stepImpl<MixMode::lowQualityMute>(mixer, cycletime, cycles);
            break;
        case MixMode::lowQualityLeft:
            stepImpl<MixMode::lowQualityLeft>(mixer, cycletime, cycles);
            break;
        case MixMode::lowQualityRight:
            stepImpl<MixMode::lowQualityRight>(mixer, cycletime, cycles);
            break;
        case MixMode::lowQualityMiddle:
            stepImpl<MixMode::lowQualityMiddle>(mixer, cycletime, cycles);
            break;
        case MixMode::highQualityLeft:
            stepImpl<MixMode::highQualityLeft>(mixer, cycletime, cycles);
            break;
        case MixMode::highQualityRight:
            stepImpl<MixMode::highQualityRight>(mixer, cycletime, cycles);
            break;
        case MixMode::highQualityMiddle:
            stepImpl<MixMode::highQualityMiddle>(mixer, cycletime, cycles);
            break;
        default:
            break;
    }

    //if (mDacOn) {
    //    while (cycles) {
    //        mixer.setOutput((int8_t)mOutput * 2 - (int8_t)mVolume, cycletime);
    //        auto toStep = std::min(mTimer, cycles);
    //        if (stepTimer(toStep)) {
    //            stepOscillator();
    //        }
    //        cycles -= toStep;
    //        cycletime += toStep;
    //    }

    //} else {
    //    mixer.setOutput(0, cycletime);
    //}
}

template <MixMode mode>
void ChannelBase::stepImpl(Mixer &mixer, uint32_t cycletime, uint32_t cycles) {
    if (mDacOn) {
        while (cycles) {
            int8_t sample = (int8_t)mOutput * 2 - (int8_t)mVolume;
            if (mLastDacOutput != sample) {
                if constexpr (mode != MixMode::lowQualityMute) {
                    mixer.mixfast<mode>(sample - mLastDacOutput, cycletime);
                }
                mLastDacOutput = sample;
            }
            auto toStep = std::min(mTimer, cycles);
            if (stepTimer(toStep)) {
                stepOscillator();
            }
            cycles -= toStep;
            cycletime += toStep;
        }
    } /*else {
        if (mLastOutput) {
            mixer.mixfast<mode>((int8_t)mVolume - (int8_t)(mOutput * 2), cycletime);
            mLastOutput = 0;
        }
    }*/
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

uint8_t ChannelBase::output() const noexcept {
    return mOutput & mDisableMask;
}

uint8_t ChannelBase::volume() const noexcept {
    return mVolume;
}

void ChannelBase::reset() noexcept {
    mDacOn = false;
    mDisableMask = DISABLED;
    mFrequency = 0;
    mLengthCounter = 0;
    mLengthEnabled = false;
    mPeriod = mDefaultPeriod;
    mTimer = mPeriod;
    mVolume = 0;
    mLastDacOutput = 0;
}

void ChannelBase::restart() noexcept {
    mTimer = mPeriod; // reload frequency timer with period
    if (mLengthCounter == 0) {
        mLengthCounter = mLengthCounterMax;
    }
    mDisableMask = mDacOn ? ENABLED : DISABLED;
}

void ChannelBase::setLengthCounterEnable(bool enable) {
    mLengthEnabled = enable;
}

EnvChannelBase::EnvChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept :
    ChannelBase(defaultPeriod, lengthCounterMax),
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
}





}
