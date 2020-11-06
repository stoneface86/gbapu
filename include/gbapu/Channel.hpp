
#pragma once

#include "gbapu/Timer.hpp"

#include <cstdint>

namespace gbapu {

// Note: do not use dynamic dispatch with Channel subclasses

class ChannelBase : public Timer {

public:

    bool dacOn() const noexcept;

    void setDacEnable(bool enabled) noexcept;

    void writeLengthCounter(uint8_t value) noexcept;

    uint8_t output() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void restartLengthCounter() noexcept;

    void stepLengthCounter() noexcept;

protected:

    ChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept;

    void disable() noexcept;

    uint16_t frequency() const noexcept;

    uint16_t mFrequency; // 0-2047 (for noise channel only 8 bits are used)

    uint8_t mOutput;

private:
    static constexpr uint8_t ENABLED = 0xFF;
    static constexpr uint8_t DISABLED = 0x00;

    uint8_t mDisableMask;
    bool mDacOn;

    unsigned mLengthCounter;
    bool mLengthEnabled;

    unsigned const mLengthCounterMax;
    uint32_t const mDefaultPeriod;

};

// adds a volume envelope to the base class
class EnvChannelBase : public ChannelBase {
public:
    uint8_t readEnvelope() const noexcept;

    void writeEnvelope(uint8_t value) noexcept;

    void stepEnvelope() noexcept;

    void restart() noexcept;

    void reset() noexcept;

    uint8_t volume() const noexcept;

protected:
    EnvChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept;

    // current volume level of the envelope
    uint8_t mEnvelopeVolume;

    // contents of the envelope register (NRx2)
    uint8_t mEnvelopeRegister;

    uint8_t mEnvelopeCounter;
    uint8_t mEnvelopePeriod;
    bool mEnvelopeAmplify;
};

// virtual functions without virtual
//

template<class Base>
class Channel : public Base {

public:
    void step(uint32_t cycles) noexcept {
        if (stepTimer(cycles)) {
            Base::stepOscillator();
        }
    }

    void writeFrequencyLsb(uint8_t value) {
        mFrequency = (mFrequency & 0xFF00) | value;
        Base::setPeriod();
    }

    void writeFrequencyMsb(uint8_t value) {
        mFrequency = (mFrequency & 0x00FF) | ((value & 0x7) << 8);
        Base::setPeriod();
        if (!!(value & 0x80)) {
            Base::restart();
            if (!!(value & 0x40)) {
                restartLengthCounter();
            }
        }
    }

protected:
    /*Channel(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept :
        Base(defaultPeriod, lengthCounterMax)
    {
    }*/



    

};

//template <bool envelope>
//using ChannelSuperclass = std::conditional<envelope, EnvChannelBase, ChannelBase>::type;
//
//
//// static polymorphism via crtp + thin template idiom
//template <
//    class Derived,
//    //class Base,
//    bool tHasEnvelope,
//    unsigned tLengthCounterMax,
//    uint32_t tDefaultPeriod
//>
//class Channel : public ChannelSuperclass<tHasEnvelope> {
//
//public:
//
//    void checkReinit(uint8_t value) noexcept {
//        if (!!(value & 0x80)) {
//            // restart channel
//            
//            static_cast<Derived*>(this)->onRestart();
//            restart();
//
//            mTimer = mPeriod;
//
//            if (!dacOn()) {
//                disable();
//            }
//
//            if (!!(value & 0x40)) {
//                // enable length counter if DAC is enabled
//                if (mDacOn) {
//                    mLengthEnabled = mDacOn;
//                    if (mLengthCounter == 0) {
//                        mLengthCounter = tLengthCounterMax;
//                    }
//                } else {
//                    mLengthEnabled = false;
//                }
//            }
//
//            
//        }
//    }
//
//    void step(uint32_t cycles) noexcept {
//        if (stepTimer(cycles)) {
//            static_cast<Derived*>(this)->onStep();
//        }
//    }
//
//    void reset() noexcept {
//        reset();
//        mTimer = 0;
//        mPeriod = tDefaultPeriod;
//        static_cast<Derived*>(this)->onReset();
//    }
//
//protected:
//    Channel() noexcept :
//        ChannelSuperclass<tHasEnvelope>()
//    {
//    }
//
//};

}
