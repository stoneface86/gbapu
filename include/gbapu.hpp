
#ifndef GBAPU_HPP
#define GBAPU_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <tuple>
#include <optional>

namespace gbapu {

namespace constants {

constexpr uint16_t MAX_FREQUENCY = 2047;

constexpr size_t WAVE_RAMSIZE = 16;

constexpr uint8_t MAX_TERM_VOLUME = 7;

template <typename T>
constexpr T CLOCK_SPEED = T(4194304);

} // constants

namespace _internal {


// mix flags
constexpr int MIX_LEFT = 2;
constexpr int MIX_RIGHT = 1;

enum class MixMode {
    mute      = 0,
    left      = MIX_LEFT,
    right     = MIX_RIGHT,
    middle    = MIX_LEFT | MIX_RIGHT
};

constexpr int operator+(MixMode mode) {
    return static_cast<int>(mode);
}

constexpr bool modePansLeft(MixMode mode) {
    return !!(+mode & MIX_LEFT);
}

constexpr bool modePansRight(MixMode mode) {
    return !!(+mode & MIX_RIGHT);
}

using ChannelMix = std::array<MixMode, 4>;

class Mixer {

public:
    Mixer();

    void mix(MixMode mode, int8_t sample, uint32_t cycletime);

    void mixDc(float dcLeft, float dcRight, uint32_t cycletime);

    template <MixMode mode>
    void mixfast(int8_t sample, uint32_t cycletime);

    void setVolume(float leftVolume, float rightVolume);

    float leftVolume() const noexcept;

    float rightVolume() const noexcept;

    // buffer management

    void setBuffer(size_t samples);

    void setSamplerate(unsigned rate);

    void endFrame(uint32_t cycletime);

    size_t availableSamples() const noexcept;

    size_t readSamples(float buf[], size_t samples);

    void removeSamples(size_t samples);

    float sampletime(uint32_t cycletime) const noexcept;

    void clear();

private:

    struct MixParam {
        // the stepset to use
        float const* stepset;
        // the destination in the buffer to mix the step
        float *dest;

    };

    //
    // Sample accumulator, used for integrating + high pass filtering
    // the sample buffer.
    //
    struct Accum {
        float sum = 0.0f;
        float highpass = 0.0f;

        void process(float *dest, float in, float highPassRate);

        void reset();
    };

    MixParam getMixParameters(uint32_t cycletime);

    void calculateHighpass();
    void calculateFactor();

    float mVolumeStepLeft;
    float mVolumeStepRight;

    unsigned mSamplerate;
    float mFactor;                      // samples per cycle (multiply cycletime by this to get sampletime)

    std::unique_ptr<float[]> mBuffer;   // sample buffer
    size_t mBuffersize;                 // total size of the buffer
    std::array<Accum, 2> mAccumulators; // running sum state for each terminal
    float mSampleOffset;                // fractional carry-over from previous frame
    size_t mWriteIndex;                 // index to start mixing samples (samples before this index can be read)
    float mHighpassRate;                // rate of the highpass filter


};


class Hardware;
class Envelope;

class Timer {

public:
    Timer(uint32_t initPeriod);

    uint32_t counter() const noexcept;

    uint32_t period() const noexcept;

    bool run(uint32_t cycles) noexcept;

    void restart() noexcept;

    void setPeriod(uint32_t period) noexcept;

private:

    uint32_t mCounter;
    uint32_t mPeriod;
};


class Channel {

public:
    Channel(uint32_t initPeriod);

    bool isDacOn() const noexcept;

    bool isEnabled() const noexcept;

    void disable() noexcept;

    void setDacEnabled(bool enabled) noexcept;

    uint16_t frequency() const noexcept;

    uint8_t output() const noexcept;

    Timer& timer() noexcept;

    void reset() noexcept;

    void restart() noexcept;

protected:
    uint16_t mFrequency;
    uint8_t mOutput;

private:
    bool mDacOn;
    bool mEnabled;
    Timer mTimer;


};

class NoiseChannel : public Channel {

public:
    NoiseChannel(Envelope const& env);

    void setNoise(uint8_t noisereg) noexcept;

    void clock() noexcept;

    void reset() noexcept;

    void restart() noexcept;

private:

    Envelope const& mEnvelope;
    bool mValidScf;
    bool mHalfWidth;
    uint16_t mLfsr;

};

class PulseChannel : public Channel {

public:

    enum Duty {
        Duty125 = 0,
        Duty25 = 1,
        Duty50 = 2,
        Duty75 = 3
    };

    PulseChannel(Envelope const& env);

    uint8_t duty() const noexcept;

    void setDuty(uint8_t duty) noexcept;

    void setFrequency(uint16_t freq) noexcept;

    void clock() noexcept;

    void reset() noexcept;

private:

    Envelope const& mEnvelope;
    uint8_t mDuty;
    uint8_t mDutyWaveform;

    unsigned mDutyCounter;

};



class WaveChannel : public Channel {

public:

    enum Volume {
        VolumeMute = 0,
        VolumeFull = 1,
        VolumeHalf = 2,
        VolumeQuarter = 3
    };

    WaveChannel();

    uint8_t* waveram() noexcept;

    uint8_t volume() const noexcept;

    void setVolume(uint8_t volume) noexcept;

    void setFrequency(uint16_t frequency) noexcept;

    void clock() noexcept;

    void reset() noexcept;

    void restart() noexcept;

private:

    void updateOutput() noexcept;

    uint8_t mWaveVolume;
    uint8_t mVolumeShift;
    uint8_t mWaveIndex;
    uint8_t mSampleBuffer;
    std::array<uint8_t, constants::WAVE_RAMSIZE> mWaveram;


};

class LengthCounter {

public:
    LengthCounter(unsigned max);

    unsigned counter() const noexcept;

    bool isEnabled() const noexcept;

    void setCounter(unsigned value) noexcept;

    void setEnable(bool enabled) noexcept;

    void clock(Channel &channel) noexcept;

    void reset() noexcept;

    void restart() noexcept;

private:
    bool mEnabled;
    unsigned mCounter;
    unsigned const mCounterMax;

};


class Envelope {

public:

    Envelope();

    uint8_t readRegister() const noexcept;

    void writeRegister(Channel &channel, uint8_t val) noexcept;

    void clock() noexcept;

    void restart() noexcept;

    void reset() noexcept;

    uint8_t volume() const noexcept;

private:

    // contents of the envelope register (NRx2)
    uint8_t mRegister;

    uint8_t mCounter;
    uint8_t mPeriod;
    bool mAmplify;
    int8_t mVolume;
};


class Sweep {

public:
    Sweep();

    uint8_t readRegister() const noexcept;

    void writeRegister(uint8_t val) noexcept;

    void clock(PulseChannel &ch1) noexcept;

    void reset() noexcept;

    void restart(PulseChannel &ch1) noexcept;

private:

    bool mSubtraction;
    uint8_t mTime;
    uint8_t mShift;
    uint8_t mCounter;

    // Sweep register, NR10
    // Bits 0-2: Shift amount
    // Bit    3: Sweep mode (1 = subtraction)
    // Bits 4-6: Period
    uint8_t mRegister;

    // shadow register, CH1's frequency gets copied here on restart (initalization)
    uint16_t mShadow;

};


class Sequencer {

public:
    Sequencer();

    void reset() noexcept;

    void run(Hardware &hw, uint32_t cycles) noexcept;

    uint32_t cyclesToNextTrigger() const noexcept;

private:
    enum class TriggerType {
        lcSweep,
        lc,
        env
    };

    struct Trigger {
        uint32_t nextIndex;     // next index in the sequence
        uint32_t nextPeriod;    // timer period for the next trigger
        TriggerType type;       // trigger to do
    };

    static Trigger const TRIGGER_SEQUENCE[];

    Timer mTimer;
    uint32_t mTriggerIndex;

};



class Hardware {

    using ChannelTuple = std::tuple<PulseChannel, PulseChannel, WaveChannel, NoiseChannel>;

public:
    Hardware();

    void reset();

    void clockEnvelopes() noexcept;

    void clockLengthCounters() noexcept;

    void clockSweep() noexcept;

    template <size_t channel>
    void writeFrequencyLsb(uint8_t lsb) noexcept {
        static_assert(channel < 4, "unknown channel");

        auto &ch = std::get<channel>(mChannels);
        if constexpr (channel == 3) {
            // noise channel
            ch.setNoise(lsb);
        } else {
            ch.setFrequency((ch.frequency() & 0xFF00) | lsb);
        }
    }

    template <size_t channel>
    void writeFrequencyMsb(uint8_t msb) noexcept {
        static_assert(channel < 4, "unknown channel");

        auto &ch = std::get<channel>(mChannels);
        if constexpr (channel != 3) {
            ch.setFrequency((ch.frequency() & 0x00FF) | ((msb & 0x7) << 8));
        }

        mLengthCounters[channel].setEnable(!!(msb & 0x40));
        if (!!(msb & 0x80)) {
            ch.restart();
            mLengthCounters[channel].restart();
            if constexpr (channel != 2) {
                envelope<channel>().restart();
            }

            if constexpr (channel == 0) {
                mSweep.restart(std::get<0>(mChannels));
            }
        }
    }

    template <size_t channel>
    Envelope& envelope() noexcept {
        static_assert(channel != 2, "WaveChannel has no envelope");
        static_assert(channel < 4, "unknown channel");

        constexpr auto index = channel > 2 ? 2 : channel;
        return mEnvelopes[index];

    }

    template <size_t channel>
    LengthCounter& lengthCounter() noexcept {
        static_assert(channel < 4, "unknown channel");
        return mLengthCounters[channel];
    }

    Sweep& sweep() noexcept;

    template <size_t index>
    std::tuple_element_t<index, ChannelTuple>& channel() noexcept {
        return std::get<index>(mChannels);
    }

    template <size_t channel>
    void writeEnvelope(uint8_t value) noexcept {
        envelope<channel>().writeRegister(std::get<channel>(mChannels), value);
    }

    void setMix(ChannelMix const& mix, Mixer &mixer, uint32_t cycletime) noexcept;

    ChannelMix const& mix() const noexcept;

    void setChannelMix(Mixer &mixer, size_t channel, MixMode mode) noexcept;

    uint8_t lastOutput(size_t channel) const noexcept;


    void run(Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept;


private:

    template <class Channel>
    void runChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept;

    template <class Channel, MixMode mode>
    void runAndMixChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept;

    MixMode preRunChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime) noexcept;

    void silence(size_t channel, Mixer &mixer, uint32_t cycletime) noexcept;

    std::array<LengthCounter, 4> mLengthCounters;
    std::array<Envelope, 3> mEnvelopes;
    Sweep mSweep;

    Sequencer mSequencer;
    ChannelTuple mChannels;

    ChannelMix mMix;

    std::array<uint8_t, 4> mLastOutputs;

};








} // gbapu::_internal

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

    Apu(
        unsigned samplerate,
        size_t buffersizeInSamples
    );

    void reset() noexcept;

    //
    // Step the emulator for a given number of cycles.
    //
    void step(uint32_t cycles);

    //
    // Step to a given cycle time.
    //
    void stepTo(uint32_t time);

    //
    // Ends the frame and allows for reading audio samples
    //
    void endFrame();

    uint8_t readRegister(uint8_t reg, uint32_t autostep = 12);

    void writeRegister(uint8_t reg, uint8_t value, uint32_t autostep = 12);

    // Output buffer

    // access

    size_t availableSamples();

    size_t readSamples(float *dest, size_t samples);

    void clearSamples();


    // settings

    void setVolume(float gain);

    void setSamplerate(unsigned samplerate);

    void setBuffersize(size_t samples);

private:

    void updateVolume();

    _internal::Mixer mMixer;

    uint8_t mNr51;

    _internal::Hardware mHardware;

    uint32_t mCycletime;

    // mixer
    uint8_t mLeftVolume;
    uint8_t mRightVolume;

    bool mEnabled;

    float mVolumeStep;
    unsigned mSamplerate;
    size_t mBuffersize;

};


} // gbapu


#endif // GBAPU_HPP
