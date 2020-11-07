
#ifndef GBAPU_HPP
#define GBAPU_HPP

#include <cstdint>
#include <cstddef>
#include <memory>

namespace gbapu {

namespace constants {

constexpr uint16_t MAX_FREQUENCY = 2047;

constexpr size_t WAVE_RAMSIZE = 16;

constexpr uint8_t MAX_TERM_VOLUME = 7;

template <typename T>
constexpr T CLOCK_SPEED = T(4194304);

} // constants

namespace _internal {

// Mixin class
class Timer {

public:

    //
    // Returns the frequency timer, or the number of cycles needed to complete a period.
    //
    inline uint32_t timer() const noexcept {
        return mTimer;
    }

protected:

    Timer(uint32_t period);

    //
    // step the timer the given number of cycles. If the timer is now 0,
    // true is returned and the timer is reloaded with the period
    //
    bool stepTimer(uint32_t cycles);

    uint32_t mTimer;
    uint32_t mPeriod;


};

class ChannelBase : public Timer {

public:

    bool dacOn() const noexcept;

    bool lengthEnabled() const noexcept;

    void setDacEnable(bool enabled) noexcept;

    void writeLengthCounter(uint8_t value) noexcept;

    uint8_t output() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void stepLengthCounter() noexcept;

protected:

    ChannelBase(uint32_t defaultPeriod, unsigned lengthCounterMax) noexcept;

    void disable() noexcept;

    void setLengthCounterEnable(bool enable);

    uint16_t frequency() const noexcept;

    uint16_t mFrequency; // 0-2047 (for noise channel only 8 bits are used)

    uint8_t mOutput;
    bool mDacOn;

private:
    static constexpr uint8_t ENABLED = 0xFF;
    static constexpr uint8_t DISABLED = 0x00;

    uint8_t mDisableMask;
    

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



class PulseChannel : public EnvChannelBase {

public:

    PulseChannel() noexcept;

    //
    // Set the duty of the pulse. Does not require restart.
    //
    void writeDuty(uint8_t duty) noexcept;

    uint8_t readDuty() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

protected:

    void stepOscillator(uint32_t timestamp) noexcept;

    void setPeriod() noexcept;

private:

    uint8_t mDuty;
    uint8_t mDutyWaveform;

    unsigned mDutyCounter;


};

class SweepPulseChannel : public PulseChannel {

public:
    SweepPulseChannel() noexcept;

    uint8_t readSweep() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void writeSweep(uint8_t reg) noexcept;

    void stepSweep() noexcept;

private:

    bool mSweepSubtraction;
    uint8_t mSweepTime;
    uint8_t mSweepShift;

    uint8_t mSweepCounter;

    // Sweep register, NR10
    // Bits 0-2: Shift amount
    // Bit    3: Sweep mode (1 = subtraction)
    // Bits 4-6: Period
    uint8_t mSweepRegister;

    // shadow register, CH1's frequency gets copied here on reset (initalization)
    int16_t mShadow;

};

class WaveChannel : public ChannelBase {

public:

    WaveChannel() noexcept;

    //
    // Returns true if the waveram can be freely access
    // Always true if the DAC is off
    // if the DAC is on then true is returned if timestamp is near the timestamp of the channel's
    // last access
    bool canAccessRam(uint32_t timestamp) const noexcept;

    uint8_t* waveram() noexcept;

    uint8_t readVolume() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void writeVolume(uint8_t volume) noexcept;


protected:

    void stepOscillator(uint32_t timestamp) noexcept;

    void setPeriod() noexcept;

private:

    void setOutput();

    uint32_t mLastRamAccess;

    //Gbs::WaveVolume mVolume;
    uint8_t mVolumeShift;
    uint8_t mWaveIndex;
    uint8_t mSampleBuffer;
    uint8_t mWaveram[constants::WAVE_RAMSIZE];


};

class NoiseChannel : public EnvChannelBase {

public:

    NoiseChannel() noexcept;

    uint8_t readNoise() const noexcept;

    void restart() noexcept;

    void reset() noexcept;

protected:
    void stepOscillator(uint32_t timestamp) noexcept;

    void setPeriod() noexcept;

private:

    bool mValidScf;

    // width of the LFSR (7-bit = true, 15-bit = false)
    bool mHalfWidth;
    // lfsr: linear feedback shift register
    uint16_t mLfsr;


};

template<class Base>
class Channel : public Base {

public:
    void step(uint32_t timestamp, uint32_t cycles) noexcept {
        if (mDacOn && stepTimer(cycles)) {
            Base::stepOscillator(timestamp + cycles);
        }
    }

    void writeFrequencyLsb(uint8_t value) {
        mFrequency = (mFrequency & 0xFF00) | value;
        Base::setPeriod();
    }

    void writeFrequencyMsb(uint8_t value) {
        mFrequency = (mFrequency & 0x00FF) | ((value & 0x7) << 8);
        Base::setPeriod();
        setLengthCounterEnable(!!(value & 0x40));

        if (!!(value & 0x80)) {
            Base::restart();
        }
    }
};

struct ChannelFile {

    Channel<SweepPulseChannel> ch1;
    Channel<PulseChannel> ch2;
    Channel<WaveChannel> ch3;
    Channel<NoiseChannel> ch4;

    ChannelFile() noexcept :
        ch1(),
        ch2(),
        ch3(),
        ch4()
    {
    }

};

class Sequencer : public Timer {

public:

    Sequencer(ChannelFile &cf) noexcept;

    void reset() noexcept;

    void step(uint32_t cycles) noexcept;

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

    ChannelFile &mCf;
    uint32_t mTriggerIndex;




};


} // gbapu::_internal


class Buffer {

    friend class Apu;

public:

    enum Quality {
        QUALITY_LOW,    // low quality transitions on all channels
        QUALITY_MED,    // high quality transitions on CH1+CH2, low quality on CH3+CH4
        QUALITY_HIGH    // high quality transitions on all channels
    };

    Buffer(unsigned samplerate, unsigned buffersize = 100);
    ~Buffer();

    // access

    size_t available();

    size_t read(int16_t *dest, size_t samples);

    void clear();


    // settings

    void setQuality(Quality quality);

    void setVolume(unsigned percent);

    void setSamplerate(unsigned samplerate);

    void setBuffersize(unsigned milliseconds);

    void resize();


private:

    void addDelta12(int term, int16_t delta, uint32_t clocktime);

    void addDelta34(int term, int16_t delta, uint32_t clocktime);

    void endFrame(uint32_t clocktime);

    struct Internal;
    std::unique_ptr<Internal> mInternal;

    Quality mQuality;
    unsigned mVolumeStep;
    unsigned mSamplerate;
    unsigned mBuffersize;
    bool mResizeRequired;

};

class Apu {

public:

    enum class Model {
        dmg,
        cgb
    };

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

    Apu(Buffer &buffer, Model model = Model::dmg);
    ~Apu() = default;

    void reset() noexcept;
    void reset(Model model) noexcept;

    void step(uint32_t cycles);

    void endFrame();

    uint8_t readRegister(uint16_t addr);
    uint8_t readRegister(Reg reg);

    void writeRegister(uint16_t addr, uint8_t value);
    void writeRegister(Reg reg, uint8_t value);


private:

    template <int channel>
    void getOutput(int8_t &leftdelta, int8_t &rightdelta);


    Buffer &mBuffer;

    Model mModel;

    _internal::ChannelFile mCf;
    _internal::Sequencer mSequencer;

    uint32_t mCycletime;

    // mixer
    uint8_t mLeftVolume;
    uint8_t mRightVolume;

    uint8_t mOutputStat;

    uint8_t mLastAmps[8];

    bool mEnabled;

};


} // gbapu


#endif // GBAPU_HPP
