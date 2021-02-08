
#ifndef GBAPU_HPP
#define GBAPU_HPP

#include <array>
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

// container for blip_buf_t. Forward-declared so we can keep blip_buf out of the public API
struct BlipBuf;

// mix flags
constexpr int MIX_LEFT = 2;
constexpr int MIX_RIGHT = 1;
constexpr int MIX_QUALITY = 4;

enum class MixMode {
    lowQualityMute      = 0,
    lowQualityLeft      = MIX_LEFT,
    lowQualityRight     = MIX_RIGHT,
    lowQualityMiddle    = MIX_LEFT | MIX_RIGHT,
    highQualityMute     = MIX_QUALITY,
    highQualityLeft     = MIX_QUALITY | MIX_LEFT,
    highQualityRight    = MIX_QUALITY | MIX_RIGHT,
    highQualityMiddle   = MIX_QUALITY | MIX_LEFT | MIX_RIGHT
};

constexpr int operator+(MixMode mode) {
    return static_cast<int>(mode);
}

constexpr bool modeIsHighQuality(MixMode mode) {
    return !!(+mode & MIX_QUALITY);
}

constexpr bool modePansLeft(MixMode mode) {
    return !!(+mode & MIX_LEFT);
}

constexpr bool modePansRight(MixMode mode) {
    return !!(+mode & MIX_RIGHT);
}

constexpr MixMode modeSetQuality(MixMode mode, bool quality) {
    if (quality) {
        return static_cast<MixMode>(+mode | MIX_QUALITY);
    } else {
        return static_cast<MixMode>(+mode & ~MIX_QUALITY);
    }
}

constexpr MixMode modeSetPanning(MixMode mode, int panning) {
    return static_cast<MixMode>(
        (+mode & ~(MIX_LEFT | MIX_RIGHT)) | (panning & 1) | (panning >> 3)
        );
}

class Mixer {

public:
    Mixer(BlipBuf &blip);

    void addDelta(MixMode mode, int term, uint32_t cycletime, int16_t delta);

    void mix(MixMode mode, int8_t sample, uint32_t cycletime);

    template <MixMode mode>
    void mixfast(int8_t sample, uint32_t cycletime);

    void setVolume(int32_t leftVolume, int32_t rightVolume);

    int32_t leftVolume() const noexcept;

    int32_t rightVolume() const noexcept;

    int32_t dcLeft() const noexcept;

    int32_t dcRight() const noexcept;

private:

    BlipBuf &mBlip;
    int32_t mVolumeStepLeft;
    int32_t mVolumeStepRight;

    int32_t mDcLeft;
    int32_t mDcRight;


};


//
// Base class for all internal components in the APU.
//
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

    int8_t lastOutput() const noexcept;

    bool lengthEnabled() const noexcept;

    virtual void reset() noexcept;

    virtual void restart() noexcept;

    void setDacEnable(bool enabled) noexcept;
    
    void step(Mixer &mixer, MixMode mode, uint32_t cycletime, uint32_t cycles);
    
    void stepLengthCounter() noexcept;

    void writeFrequencyLsb(uint8_t value);

    void writeFrequencyMsb(uint8_t value);
    
    void writeLengthCounter(uint8_t value) noexcept;

protected:

    ChannelBase(uint32_t defaultPeriod, uint32_t minPeriod, unsigned lengthCounterMax) noexcept;

    void disable() noexcept;

    void setLengthCounterEnable(bool enable);

    uint16_t frequency() const noexcept;

    virtual void setPeriod() = 0;


    virtual void stepOscillator() noexcept = 0;

    uint16_t mFrequency; // 0-2047 (for noise channel only 8 bits are used)

    // PCM value going into the DAC (0 to F)
    int8_t mOutput;

    
    bool mDacOn;

private:

    template <MixMode mode>
    void stepImpl(Mixer &mixer, uint32_t cycletime, uint32_t cycles);

    // previous PCM value
    int8_t mLastOutput;
    

    unsigned mLengthCounter;
    bool mLengthEnabled;
    bool mDisabled;

    unsigned const mLengthCounterMax;
    uint32_t const mDefaultPeriod;
    // minimum period that mixing will occur
    uint32_t const mMinPeriod;

};

// adds a volume envelope to the base class
class EnvChannelBase : public ChannelBase {
public:
    uint8_t readEnvelope() const noexcept;

    void writeEnvelope(uint8_t value) noexcept;

    void stepEnvelope() noexcept;

    virtual void restart() noexcept;

    virtual void reset() noexcept;


protected:
    EnvChannelBase(uint32_t defaultPeriod, uint32_t minPeriod, unsigned lengthCounterMax) noexcept;

    // contents of the envelope register (NRx2)
    uint8_t mEnvelopeRegister;

    uint8_t mEnvelopeCounter;
    uint8_t mEnvelopePeriod;
    bool mEnvelopeAmplify;
    int8_t mVolume;
};



class PulseChannel : public EnvChannelBase {

public:

    PulseChannel() noexcept;

    //
    // Set the duty of the pulse. Does not require restart.
    //
    void writeDuty(uint8_t duty) noexcept;

    uint8_t readDuty() const noexcept;

    virtual void reset() noexcept;

    virtual void restart() noexcept;

protected:

    void stepOscillator() noexcept override;

    void setPeriod() noexcept override;

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

    void stepOscillator() noexcept override;

    void setPeriod() noexcept override;

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
    void stepOscillator() noexcept override;

    void setPeriod() noexcept override;

private:

    bool mValidScf;

    // width of the LFSR (7-bit = true, 15-bit = false)
    bool mHalfWidth;
    // lfsr: linear feedback shift register
    uint16_t mLfsr;


};

struct ChannelFile {

    SweepPulseChannel ch1;
    PulseChannel ch2;
    WaveChannel ch3;
    NoiseChannel ch4;

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

union Registers {
    struct RegisterStruct {
        // CH1
        uint8_t nr10; // sweep
        uint8_t nr11; // duty + length counter
        uint8_t nr12; // envelope
        uint8_t nr13; // frequency low
        uint8_t nr14; // retrigger + length enable + frequency high
        // CH2
        uint8_t nr20; // unused, always 0
        uint8_t nr21; // duty + length counter
        uint8_t nr22; // envelope
        uint8_t nr23; // frequency low
        uint8_t nr24; // retrigger + length enable + frequency high
        // CH3
        uint8_t nr30; // DAC enable
        uint8_t nr31; // length counter
        uint8_t nr32; // wave volume
        uint8_t nr33; // frequency low
        uint8_t nr34; // retrigger + length enable + frequency high
        // CH4
        uint8_t nr40; // unused
        uint8_t nr41; // length counter
        uint8_t nr42; // envelope
        uint8_t nr43; // noise settings
        uint8_t nr44; // retrigger + length enable
        // Control
        uint8_t nr50; // Vin, terminal volumes
        uint8_t nr51; // channel terminal enables
        uint8_t nr52; // Sound enable, ON flags
    } byName;

    std::array<uint8_t, 23> byArray;
};

class Apu {

public:

    enum class Model {
        dmg,
        cgb
    };

    enum class Quality {
        low,            // low quality on all channels
        medium,         // high quality on 1 + 2, low quality on 3 + 4
        high            // high quality on all channels
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

    Apu(
        unsigned samplerate,
        size_t buffersizeInSamples,
        Quality quality = Quality::medium,
        Model model = Model::dmg
    );
    ~Apu();

    //
    // Gets the current register state. This is for diagnostic purposes only,
    // emulated programs should access registers via the readRegister methods.
    //
    Registers const& registers() const;

    void reset() noexcept;
    void reset(Model model) noexcept;

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

    uint8_t readRegister(uint8_t reg, uint32_t autostep = 3);

    void writeRegister(uint8_t reg, uint8_t value, uint32_t autostep = 3);

    // Output buffer

    // access

    size_t availableSamples();

    size_t readSamples(int16_t *dest, size_t samples);

    void clearSamples();


    // settings

    void setQuality(Quality quality);

    void setVolume(float gain);

    void setSamplerate(unsigned samplerate);

    void setBuffersize(size_t samples);

    void resizeBuffer();

private:

    void updateVolume();
    void updateQuality();

    Model mModel;
    Quality mQuality;
    
    std::unique_ptr<_internal::BlipBuf> mBlip;

    /*_internal::Mixer mMixer1;
    _internal::Mixer mMixer2;
    _internal::Mixer mMixer3;
    _internal::Mixer mMixer4;*/
    _internal::Mixer mMixer;
    std::array<_internal::MixMode, 4> mPannings;


    _internal::ChannelFile mCf;
    _internal::Sequencer mSequencer;

    Registers mRegs;

    uint32_t mCycletime;

    // mixer
    uint8_t mLeftVolume;
    uint8_t mRightVolume;

    bool mEnabled;

    // buffer
    //struct Internal;

    bool mIsHighQuality;
    // Q16.16
    int32_t mVolumeStep;
    unsigned mSamplerate;
    size_t mBuffersize;
    bool mResizeRequired;

};


} // gbapu


#endif // GBAPU_HPP
