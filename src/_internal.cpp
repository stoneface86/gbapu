
#include "gbapu.hpp"

#include <algorithm>
#include <functional>
#include <cmath>
#include <cassert>

#include <utility>

namespace gbapu {
namespace _internal {

// =========================================================== LengthCounter ===

LengthCounter::LengthCounter(unsigned max) :
    mEnabled(false),
    mCounter(0),
    mCounterMax(max)
{
}

unsigned LengthCounter::counter() const noexcept {
    return mCounter;
}

bool LengthCounter::isEnabled() const noexcept {
    return mEnabled;
}

void LengthCounter::setCounter(unsigned value) noexcept {
    mCounter = value;
}

void LengthCounter::setEnable(bool enabled) noexcept {
    mEnabled = enabled;
}

void LengthCounter::clock(Channel &channel) noexcept {
    if (mEnabled) {
        if (mCounter == 0) {
            channel.disable();
        } else {
            --mCounter;
        }
    }
}

void LengthCounter::reset() noexcept {
    mEnabled = false;
    mCounter = 0;
}

void LengthCounter::restart() noexcept {
    if (mCounter == 0) {
        mCounter = mCounterMax;
    }
}

// ================================================================ Envelope ===

Envelope::Envelope() :
    mRegister(0),
    mCounter(0),
    mPeriod(0),
    mAmplify(false),
    mVolume(0)
{
}

uint8_t Envelope::readRegister() const noexcept {
    return mRegister;
}

void Envelope::writeRegister(Channel &channel, uint8_t val) noexcept {
    mRegister = val;
    channel.setDacEnabled(!!(val & 0xF8));
}

void Envelope::clock() noexcept {
    if (mPeriod) {
        // do nothing if period == 0
        if (++mCounter == mPeriod) {
            mCounter = 0;
            if (mAmplify) {
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

void Envelope::reset() noexcept {
    mRegister = 0;
    mCounter = 0;
    mPeriod = 0;
    mAmplify = false;
    mVolume = 0;
}

void Envelope::restart() noexcept {
    mCounter = 0;
    mPeriod = mRegister & 0x7;
    mAmplify = !!(mRegister & 0x8);
    mVolume = mRegister >> 4;
}

uint8_t Envelope::volume() const noexcept {
    return mVolume;
}

// =================================================================== Sweep ===

Sweep::Sweep() :
    mSubtraction(false),
    mTime(0),
    mShift(0),
    mCounter(0),
    mRegister(0),
    mShadow(0)
{
}

uint8_t Sweep::readRegister() const noexcept {
    return mRegister;
}

void Sweep::writeRegister(uint8_t val) noexcept {
    mRegister = val & 0x7F;
}

void Sweep::clock(PulseChannel &ch1) noexcept {
    if (mTime) {
        if (++mCounter >= mTime) {
            mCounter = 0;
            if (mShift) {
                int16_t sweepfreq = mShadow >> mShift;
                if (mSubtraction) {
                    sweepfreq = mShadow - sweepfreq;
                    if (sweepfreq < 0) {
                        return; // no change
                    }
                } else {
                    sweepfreq = mShadow + sweepfreq;
                    if (sweepfreq > constants::MAX_FREQUENCY) {
                        // sweep will overflow, disable the channel
                        ch1.disable();
                        return;
                    }
                }
                // no overflow/underflow
                // write-back the shadow register to CH1's frequency register
                ch1.setFrequency((uint16_t)sweepfreq);
                mShadow = sweepfreq;
            }
        }
    }
}

void Sweep::reset() noexcept {
    mSubtraction = false;
    mTime = 0;
    mShift = 0;
    mCounter = 0;
    mRegister = 0;
    mShadow = 0;
}

void Sweep::restart(PulseChannel &ch1) noexcept {
    mCounter = 0;
    mShift = mRegister & 0x7;
    mSubtraction = !!((mRegister >> 3) & 1);
    mTime = (mRegister >> 4) & 0x7;
    mShadow = ch1.frequency();
}

// =================================================================== Timer ===

Timer::Timer(uint32_t initPeriod) :
    mCounter(initPeriod),
    mPeriod(initPeriod)
{
}

uint32_t Timer::counter() const noexcept {
    return mCounter;
}

uint32_t Timer::period() const noexcept {
    return mPeriod;
}

bool Timer::run(uint32_t cycles) noexcept {
    assert(mCounter >= cycles);
    mCounter -= cycles;
    if (mCounter == 0) {
        mCounter = mPeriod;
        return true;
    } else {
        return false;
    }
}

uint32_t Timer::fastforward(uint32_t cycles) noexcept {
    if (cycles < mCounter) {
        mCounter -= cycles;
        return 0;
    } else {
        cycles -= mCounter;
        auto clocks = (cycles / mPeriod) + 1;
        mCounter = mPeriod - (cycles % mPeriod);
        return clocks;
    }
}

void Timer::setPeriod(uint32_t period) noexcept {
    mPeriod = period;
}

void Timer::restart() noexcept {
    mCounter = mPeriod;
}

// ================================================================= Channel ===

Channel::Channel(uint32_t initPeriod) :
    mFrequency(0),
    mOutput(0),
    mDacOn(false),
    mEnabled(false),
    mTimer(initPeriod)
{
}

bool Channel::isDacOn() const noexcept {
    return mDacOn;
}

bool Channel::isEnabled() const noexcept {
    return mEnabled;
}

void Channel::disable() noexcept {
    mEnabled = false;
}

void Channel::setDacEnabled(bool enabled) noexcept {
    mDacOn = enabled;
    if (!enabled) {
        disable();
    }
}

uint16_t Channel::frequency() const noexcept {
    return mFrequency;
}

uint8_t Channel::output() const noexcept {
    return mOutput;
}

Timer& Channel::timer() noexcept {
    return mTimer;
}

void Channel::reset() noexcept {
    mDacOn = false;
    mEnabled = false;
    mFrequency = 0;
    mOutput = 0;
}

void Channel::restart() noexcept {
    mTimer.restart();
    mEnabled = mDacOn;
}

// ============================================================ NoiseChannel ===

namespace {

constexpr uint16_t LFSR_INIT = 0x7FFF;

constexpr uint32_t NOISE_DEFAULT_PERIOD = 8;

}

NoiseChannel::NoiseChannel(Envelope const& envelope) :
    Channel(NOISE_DEFAULT_PERIOD),
    mEnvelope(envelope),
    mValidScf(true),
    mHalfWidth(false),
    mLfsr(LFSR_INIT)
{
}

void NoiseChannel::setNoise(uint8_t noisereg) noexcept {
    mFrequency = noisereg;
    // drf = "dividing ratio frequency", divisor, etc
    uint8_t drf = mFrequency & 0x7;
    if (drf == 0) {
        drf = 8;
    } else {
        drf *= 16;
    }
    mHalfWidth = !!((mFrequency >> 3) & 1);
    // scf = "shift clock frequency"
    auto scf = mFrequency >> 4;
    mValidScf = scf < 0xE; // obscure behavior: a scf of 14 or 15 results in the channel receiving no clocks
    timer().setPeriod(drf << scf);
}

void NoiseChannel::clock() noexcept {
    if (mValidScf) {
        clockLfsr();
        updateOutput();
    }
}

void NoiseChannel::reset() noexcept {
    Channel::reset();

    mValidScf = true;
    mHalfWidth = false;
    mLfsr = LFSR_INIT;

    timer().setPeriod(NOISE_DEFAULT_PERIOD);
}

void NoiseChannel::restart() noexcept {
    Channel::restart();
    mLfsr = LFSR_INIT;
    mOutput = 0;
}

void NoiseChannel::fastforward(uint32_t cycles) noexcept {

    auto clocks = timer().fastforward(cycles);
    if (mValidScf) {
        while (clocks) {
            clockLfsr();
            --clocks;
        }
        updateOutput();
    }
}

void NoiseChannel::clockLfsr() noexcept {
    // xor bits 1 and 0 of the lfsr
    uint8_t result = (mLfsr & 0x1) ^ ((mLfsr >> 1) & 0x1);
    // shift the register
    mLfsr >>= 1;
    // set the resulting xor to bit 15 (feedback)
    mLfsr |= result << 14;
    if (mHalfWidth) {
        // 7-bit lfsr, set bit 7 with the result
        mLfsr &= ~0x40; // reset bit 7
        mLfsr |= result << 6; // set bit 7 result
    }
}

void NoiseChannel::updateOutput() noexcept {
    mOutput = -((~mLfsr) & 1) & mEnvelope.volume();
}

// ============================================================ PulseChannel ===

namespace {

// multiplier for frequency calculation
// 64 Hz - 131.072 KHz
static constexpr unsigned PULSE_MULTIPLIER = 4;

//                    STEP: 76543210
// Bits 24-31 - 75%   Duty: 01111110 (0x7E) _------_
// Bits 16-23 - 50%   Duty: 11100001 (0xE1) -____---
// Bits  8-15 - 25%   Duty: 10000001 (0x81) -______-
// Bits  0-7  - 12.5% Duty: 10000000 (0x80) _______-

static constexpr uint32_t DUTY_MASK = 0x7EE18180;


static constexpr uint32_t PULSE_DEFAULT_PERIOD = (2048 - 0) * PULSE_MULTIPLIER;

// frequency 2042 is about 21845 Hz
//constexpr uint32_t MIN_PERIOD = (2048 - 2042) * PULSE_MULTIPLIER;

//#define setOutput() mOutput = ((DUTY_MASK >> ((mDuty << 3) + mDutyCounter)) & 1)
#define setOutput() mOutput = (mDutyWaveform >> mDutyCounter) & 1
#define dutyWaveform(duty) ((DUTY_MASK >> (duty << 3)) & 0xFF)

}

PulseChannel::PulseChannel(Envelope const& envelope) :
    Channel(PULSE_DEFAULT_PERIOD),
    mEnvelope(envelope),
    mDuty(Duty75),
    mDutyWaveform(dutyWaveform(mDuty)),
    mDutyCounter(0)
{
}

uint8_t PulseChannel::duty() const noexcept {
    return mDuty;
}

void PulseChannel::setDuty(uint8_t duty) noexcept {
    mDuty = duty;
    mDutyWaveform = dutyWaveform(duty);
}

void PulseChannel::setFrequency(uint16_t freq) noexcept {
    mFrequency = freq;
    timer().setPeriod((2048 - freq) * PULSE_MULTIPLIER);
}

void PulseChannel::clock() noexcept {
    // this implementation uses bit shifting instead of a lookup table

    // increment duty counter
    mDutyCounter = (mDutyCounter + 1) & 0x7;
    updateOutput();
}

void PulseChannel::reset() noexcept {
    timer().setPeriod(PULSE_DEFAULT_PERIOD);
    mDutyCounter = 0;
    setDuty(Duty75);
    Channel::reset();
}

void PulseChannel::fastforward(uint32_t cycles) noexcept {
    auto clocks = timer().fastforward(cycles);
    mDutyCounter = (mDutyCounter + clocks) & 0x7;
    updateOutput();
}

void PulseChannel::updateOutput() noexcept {
    mOutput = -((mDutyWaveform >> mDutyCounter) & 1) & mEnvelope.volume();
}


// ============================================================= WaveChannel ===

namespace {

// multiplier for frequency calculation
// 32 Hz - 65.536 KHz
static constexpr unsigned WAVE_MULTIPLIER = 2;

static constexpr uint32_t WAVE_DEFAULT_PERIOD = (2048 - 0) * WAVE_MULTIPLIER;

}

WaveChannel::WaveChannel() :
    Channel(WAVE_DEFAULT_PERIOD),
    mWaveVolume(VolumeMute),
    mVolumeShift(0),
    mWaveIndex(0),
    mSampleBuffer(0),
    mWaveram()
{
    mWaveram.fill((uint8_t)0);
}

uint8_t* WaveChannel::waveram() noexcept {
    return mWaveram.data();
}

uint8_t WaveChannel::volume() const noexcept {
    return mWaveVolume;
}

void WaveChannel::setVolume(uint8_t volume) noexcept {
    mWaveVolume = volume;
    switch (volume) {
        case VolumeMute:
            mVolumeShift = 4;
            break;
        case VolumeFull:
            mVolumeShift = 0;
            break;
        case VolumeHalf:
            mVolumeShift = 1;
            break;
        default:
            mVolumeShift = 2;
            break;
    }
    updateOutput();
}

void WaveChannel::setFrequency(uint16_t frequency) noexcept {
    mFrequency = frequency;
    timer().setPeriod((2048 - mFrequency) * WAVE_MULTIPLIER);
}

void WaveChannel::clock() noexcept {
    mWaveIndex = (mWaveIndex + 1) & 0x1F;
    updateSampleBuffer();
}

void WaveChannel::reset() noexcept {
    timer().setPeriod(WAVE_DEFAULT_PERIOD);
    mVolumeShift = 0;
    mWaveVolume = VolumeMute;
    mWaveram.fill((uint8_t)0);
    mSampleBuffer = 0;
    mWaveIndex = 0;
    Channel::reset();
}

void WaveChannel::restart() noexcept {
    Channel::restart();
    mWaveIndex = 0;
}

void WaveChannel::fastforward(uint32_t cycles) noexcept {
    auto clocks = timer().fastforward(cycles);
    mWaveIndex = (mWaveIndex + clocks) & 0x1F;
    updateSampleBuffer();
}

void WaveChannel::updateSampleBuffer() noexcept {
    mSampleBuffer = mWaveram[mWaveIndex >> 1];
    if (mWaveIndex & 1) {
        // odd number, low nibble
        mSampleBuffer &= 0xF;
    } else {
        // even number, high nibble
        mSampleBuffer >>= 4;
    }
    updateOutput();
}

void WaveChannel::updateOutput() noexcept {
    mOutput = mSampleBuffer >> mVolumeShift;
}


// =============================================================== Sequencer ===

// A step occurs every 8192 cycles (4194304 Hz / 8192 = 512 Hz)
//
// Step:                 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
// --------------------------+---+---+---+---+---+---+---+-------------------
// Len. counter (256 Hz) | x       x       x       x
// Sweep        (128 Hz) |         x               x
// envelope     ( 64 Hz) |                             x
//

namespace {

constexpr uint32_t CYCLES_PER_STEP = 8192;

constexpr uint32_t SEQUENCER_DEFAULT_PERIOD = CYCLES_PER_STEP * 2;

}

Sequencer::Trigger const Sequencer::TRIGGER_SEQUENCE[] = {

    {1,     CYCLES_PER_STEP * 2,    TriggerType::lc},

    {2,     CYCLES_PER_STEP * 2,    TriggerType::lcSweep},

    {3,     CYCLES_PER_STEP,        TriggerType::lc},

    // step 6 trigger, next trigger: 7
    {4,     CYCLES_PER_STEP,        TriggerType::lcSweep},

    // step 7 trigger, next trigger: 0
    {0,     CYCLES_PER_STEP * 2,    TriggerType::env}
};

Sequencer::Sequencer() :
    mTimer(SEQUENCER_DEFAULT_PERIOD),
    mTriggerIndex(0)
{
}

void Sequencer::reset() noexcept {
    mTimer.setPeriod(SEQUENCER_DEFAULT_PERIOD);
    mTimer.restart();
    mTriggerIndex = 0;
}

void Sequencer::run(Hardware &hw, uint32_t cycles) noexcept {
    if (mTimer.run(cycles)) {
        Trigger const &trigger = TRIGGER_SEQUENCE[mTriggerIndex];
        switch (trigger.type) {
            case TriggerType::lcSweep:
                hw.clockSweep();
                [[fallthrough]];
            case TriggerType::lc:
                hw.clockLengthCounters();
                break;
            case TriggerType::env:
                hw.clockEnvelopes();
                break;
        }
        mTimer.setPeriod(trigger.nextPeriod);
        mTriggerIndex = trigger.nextIndex;
    }
}

uint32_t Sequencer::cyclesToNextTrigger() const noexcept {
    return mTimer.counter();
}

// ================================================================ Hardware ===

Hardware::Hardware() :
    mLengthCounters{
        LengthCounter(64),
        LengthCounter(64),
        LengthCounter(256),
        LengthCounter(64)
    },
    mEnvelopes(),
    mSweep(),
    mSequencer(),
    mChannels{
        PulseChannel(mEnvelopes[0]),
        PulseChannel(mEnvelopes[1]),
        WaveChannel(),
        NoiseChannel(mEnvelopes[2])
    },
    mMix(),
    mLastOutputs()
{
}

void Hardware::reset() {
    std::for_each(mLengthCounters.begin(), mLengthCounters.end(), std::mem_fn(&LengthCounter::reset));
    std::for_each(mEnvelopes.begin(), mEnvelopes.end(), std::mem_fn(&Envelope::reset));
    mSweep.reset();
    mSequencer.reset();
    std::get<0>(mChannels).reset();
    std::get<1>(mChannels).reset();
    std::get<2>(mChannels).reset();
    std::get<3>(mChannels).reset();

    mMix.fill(MixMode::mute);
    mLastOutputs.fill((uint8_t)0);
}

void Hardware::clockEnvelopes() noexcept {
    std::for_each(mEnvelopes.begin(), mEnvelopes.end(), std::mem_fn(&Envelope::clock));
}

void Hardware::clockLengthCounters() noexcept {
    mLengthCounters[0].clock(std::get<0>(mChannels));
    mLengthCounters[1].clock(std::get<1>(mChannels));
    mLengthCounters[2].clock(std::get<2>(mChannels));
    mLengthCounters[3].clock(std::get<3>(mChannels));
}

void Hardware::clockSweep() noexcept {
    mSweep.clock(std::get<0>(mChannels));
}

Sweep& Hardware::sweep() noexcept {
    return mSweep;
}

void Hardware::setMix(const ChannelMix &mix, Mixer &mixer, uint32_t cycletime) noexcept {

    // check for changes in the mix
    for (size_t i = 0; i < mMix.size(); ++i) {
        auto const next = mix[i];
        auto &last = mMix[i];

        if (next != last) {
            auto changes = +last ^ +next;

            float dcLeft = 0.0f;
            float dcRight = 0.0f;
            auto const level = 7.5f - mLastOutputs[i];
            if (changes & MIX_LEFT) {
                dcLeft = mixer.leftVolume() * level;
                if (modePansLeft(next)) {
                    dcLeft = -dcLeft;
                }
            }

            if (changes & MIX_RIGHT) {
                dcRight = mixer.rightVolume() * level;
                if (modePansRight(next)) {
                    dcRight = -dcRight;
                }
            }

            mixer.mixDc(dcLeft, dcRight, cycletime);
        }
    }


    mMix = mix;
}

ChannelMix const& Hardware::mix() const noexcept {
    return mMix;
}

uint8_t Hardware::lastOutput(size_t channel) const noexcept {
    return mLastOutputs[channel];
}

void Hardware::run(Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept {
    while (cycles) {
        // step components to the beat of the sequencer
        auto toStep = std::min(cycles, mSequencer.cyclesToNextTrigger());
        runChannel(0, std::get<0>(mChannels), mixer, cycletime, toStep);
        runChannel(1, std::get<1>(mChannels), mixer, cycletime, toStep);
        runChannel(2, std::get<2>(mChannels), mixer, cycletime, toStep);
        runChannel(3, std::get<3>(mChannels), mixer, cycletime, toStep);
        mSequencer.run(*this, toStep);

        cycletime += toStep;
        cycles -= toStep;
    }
}

template <class Channel>
void Hardware::runChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept {
    auto mix = preRunChannel(index, ch, mixer, cycletime);
    switch (mix) {
        case MixMode::mute:
            runAndMixChannel<Channel, MixMode::mute>(index, ch, mixer, cycletime, cycles);
            break;
        case MixMode::left:
            runAndMixChannel<Channel, MixMode::left>(index, ch, mixer, cycletime, cycles);
            break;
        case MixMode::right:
            runAndMixChannel<Channel, MixMode::right>(index, ch, mixer, cycletime, cycles);
            break;
        case MixMode::middle:
            runAndMixChannel<Channel, MixMode::middle>(index, ch, mixer, cycletime, cycles);
            break;
        default:
            break;
    }

}

template <class Channel, MixMode mode>
void Hardware::runAndMixChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime, uint32_t cycles) noexcept {

    if constexpr (mode == MixMode::mute) {

        // optimization, since the channel is muted, we don't need to mix any
        // changes in the output, just run the channel for the needed amount of cycles
        ch.fastforward(cycles);

    } else {

        auto &last = mLastOutputs[index];

        auto mixChanges = [&]() {
            // mix any change in output
            if (auto out = ch.output(); out != last) {
                mixer.mixfast<mode>(out - last, cycletime);
                last = out;
            }
        };


        auto &timer = ch.timer();

        mixChanges();
        cycletime += timer.counter();

        // determine the number of clocks we are stepping
        auto clocks = timer.fastforward(cycles);
        auto const period = timer.period();

        // iterate each clock and mix any change in output
        while (clocks) {
            ch.clock();
            --clocks;
            mixChanges();
            cycletime += period;
        }
    }
}

MixMode Hardware::preRunChannel(size_t index, Channel &ch, Mixer &mixer, uint32_t cycletime) noexcept {
    if (ch.isDacOn() && ch.isEnabled()) {
        return mMix[index];
    }

    // no mixing required, either the channel's DAC is off or
    // the length counter disabled the channel
    silence(index, mixer, cycletime);
    return MixMode::mute;

}

void Hardware::silence(size_t channel, Mixer &mixer, uint32_t cycletime) noexcept {
    auto &output = mLastOutputs[channel];
    if (output) {
        mixer.mix(mMix[channel], -output, cycletime);
        output = 0;
    }
}

// ================================================================== Mixer ===

namespace {

constexpr size_t PHASES = 32;       // number of step sets
constexpr size_t STEP_WIDTH = 16;   // width, in samples, of a step (MUST BE EVEN)

// note that the STEP_TABLE has an extra step set for interpolation purposes

// Compressing the STEP_TABLE
// the STEP_TABLE could be halved in size by taking advantage of symmetry
//
// consider a STEP_TABLE with 4 phases - { A, B, C, D, E} where A,B,C,D,E are step sets
// split each step in half, the table is now { A1 + A2, B1 + B2, C1 + C2, D1 + D2, E1 + E2 }
// and apply symmetry: (rev reverses the step set)
//  A2 = rev(E1)
//  B2 = rev(D1)
//  C2 = rev(C1)
//  D2 = rev(B1)
//  E2 = rev(A1)
//
// so we only need a table with sets A1, B1, C1, D1 and E1
// A2, B2, C2, D2 and E2, can be accessed by reading E1, D1, C1, B1, and A1, in reverse, respectively
//
// Using symmetry reduces the space requirement of the STEP_TABLE by a half
//  sizeof(STEP_TABLE) = sizeof(float) * (PHASES + 1) * (STEP_WIDTH / 2)

//
// pre-computed step table for bandlimited-synthesis
// table is from the blip_buf library, converted to float by multiplying all values by (1/32768.0f)
// the filter kernel the steps are sampled from appears to be some form of windowed-sinc
// this table will eventually be replaced with a custom kernel and may even be generated at runtime
//
static float const STEP_TABLE[PHASES + 1][STEP_WIDTH] = {
    { 0.001312256f, -0.003509521f,  0.010681152f, -0.014892578f,  0.034667969f, -0.027893066f,  0.178863525f,  0.641540527f,  0.178863525f, -0.027893066f,  0.034667969f, -0.014892578f,  0.010681152f, -0.003509521f,  0.001312256f,  0.000000000f },
    { 0.001342773f, -0.003601074f,  0.010620117f, -0.014434814f,  0.032836914f, -0.024383545f,  0.160949707f,  0.640899658f,  0.197265625f, -0.031158447f,  0.036315918f, -0.015228271f,  0.010681152f, -0.003356934f,  0.001220703f,  0.000030518f },
    { 0.001373291f, -0.003692627f,  0.010498047f, -0.013854980f,  0.030853271f, -0.020660400f,  0.143615723f,  0.638916016f,  0.216125488f, -0.034149170f,  0.037780762f, -0.015441895f,  0.010589600f, -0.003112793f,  0.001068115f,  0.000091553f },
    { 0.001403809f, -0.003723145f,  0.010253906f, -0.013153076f,  0.028747559f, -0.016754150f,  0.126831055f,  0.635650635f,  0.235382080f, -0.036773682f,  0.039001465f, -0.015472412f,  0.010406494f, -0.002868652f,  0.000946045f,  0.000122070f },
    { 0.001434326f, -0.003753662f,  0.009979248f, -0.012329102f,  0.026489258f, -0.012756348f,  0.110748291f,  0.631072998f,  0.254974365f, -0.039062500f,  0.040039063f, -0.015380859f,  0.010162354f, -0.002593994f,  0.000793457f,  0.000183105f },
    { 0.001434326f, -0.003723145f,  0.009643555f, -0.011444092f,  0.024169922f, -0.008697510f,  0.095336914f,  0.625244141f,  0.274810791f, -0.040863037f,  0.040802002f, -0.015136719f,  0.009826660f, -0.002288818f,  0.000671387f,  0.000213623f },
    { 0.001434326f, -0.003662109f,  0.009246826f, -0.010498047f,  0.021789551f, -0.004608154f,  0.080688477f,  0.618164063f,  0.294799805f, -0.042205811f,  0.041320801f, -0.014739990f,  0.009429932f, -0.001922607f,  0.000488281f,  0.000274658f },
    { 0.001403809f, -0.003570557f,  0.008819580f, -0.009460449f,  0.019348145f, -0.000518799f,  0.066772461f,  0.609893799f,  0.314910889f, -0.043029785f,  0.041564941f, -0.014160156f,  0.008911133f, -0.001495361f,  0.000274658f,  0.000335693f },
    { 0.001403809f, -0.003479004f,  0.008331299f, -0.008392334f,  0.016876221f,  0.003570557f,  0.053649902f,  0.600433350f,  0.335052490f, -0.043304443f,  0.041534424f, -0.013397217f,  0.008300781f, -0.001068115f,  0.000091553f,  0.000396729f },
    { 0.001342773f, -0.003295898f,  0.007781982f, -0.007232666f,  0.014373779f,  0.007537842f,  0.041381836f,  0.589813232f,  0.355163574f, -0.042968750f,  0.041229248f, -0.012512207f,  0.007629395f, -0.000579834f, -0.000122070f,  0.000457764f },
    { 0.001312256f, -0.003143311f,  0.007232666f, -0.006072998f,  0.011901855f,  0.011383057f,  0.029937744f,  0.578125000f,  0.375152588f, -0.041992188f,  0.040618896f, -0.011444092f,  0.006896973f, -0.000091553f, -0.000366211f,  0.000549316f },
    { 0.001281738f, -0.002990723f,  0.006652832f, -0.004882813f,  0.009460449f,  0.015106201f,  0.019317627f,  0.565399170f,  0.394958496f, -0.040344238f,  0.039703369f, -0.010223389f,  0.006072998f,  0.000488281f, -0.000610352f,  0.000610352f },
    { 0.001220703f, -0.002777100f,  0.006042480f, -0.003692627f,  0.007049561f,  0.018646240f,  0.009582520f,  0.551696777f,  0.414489746f, -0.037963867f,  0.038482666f, -0.008850098f,  0.005187988f,  0.001037598f, -0.000823975f,  0.000671387f },
    { 0.001159668f, -0.002563477f,  0.005432129f, -0.002471924f,  0.004669189f,  0.022033691f,  0.000671387f,  0.537078857f,  0.433654785f, -0.034851074f,  0.036956787f, -0.007293701f,  0.004241943f,  0.001617432f, -0.001098633f,  0.000762939f },
    { 0.001098633f, -0.002319336f,  0.004791260f, -0.001312256f,  0.002441406f,  0.025146484f, -0.007354736f,  0.521606445f,  0.452392578f, -0.030975342f,  0.035156250f, -0.005615234f,  0.003234863f,  0.002227783f, -0.001342773f,  0.000823975f },
    { 0.001037598f, -0.002075195f,  0.004119873f, -0.000091553f,  0.000244141f,  0.028045654f, -0.014526367f,  0.505310059f,  0.470642090f, -0.026306152f,  0.033050537f, -0.003753662f,  0.002136230f,  0.002868652f, -0.001586914f,  0.000885010f },
    { 0.000976563f, -0.001861572f,  0.003509521f,  0.001037598f, -0.001831055f,  0.030700684f, -0.020843506f,  0.488311768f,  0.488311768f, -0.020843506f,  0.030700684f, -0.001831055f,  0.001037598f,  0.003509521f, -0.001861572f,  0.000976563f },
    { 0.000885010f, -0.001586914f,  0.002868652f,  0.002136230f, -0.003753662f,  0.033050537f, -0.026306152f,  0.470642090f,  0.505310059f, -0.014526367f,  0.028045654f,  0.000244141f, -0.000091553f,  0.004119873f, -0.002075195f,  0.001037598f },
    { 0.000823975f, -0.001342773f,  0.002227783f,  0.003234863f, -0.005615234f,  0.035156250f, -0.030975342f,  0.452392578f,  0.521606445f, -0.007354736f,  0.025146484f,  0.002441406f, -0.001312256f,  0.004791260f, -0.002319336f,  0.001098633f },
    { 0.000762939f, -0.001098633f,  0.001617432f,  0.004241943f, -0.007293701f,  0.036956787f, -0.034851074f,  0.433654785f,  0.537078857f,  0.000671387f,  0.022033691f,  0.004669189f, -0.002471924f,  0.005432129f, -0.002563477f,  0.001159668f },
    { 0.000671387f, -0.000823975f,  0.001037598f,  0.005187988f, -0.008850098f,  0.038482666f, -0.037963867f,  0.414489746f,  0.551696777f,  0.009582520f,  0.018646240f,  0.007049561f, -0.003692627f,  0.006042480f, -0.002777100f,  0.001220703f },
    { 0.000610352f, -0.000610352f,  0.000488281f,  0.006072998f, -0.010223389f,  0.039703369f, -0.040344238f,  0.394958496f,  0.565399170f,  0.019317627f,  0.015106201f,  0.009460449f, -0.004882813f,  0.006652832f, -0.002990723f,  0.001281738f },
    { 0.000549316f, -0.000366211f, -0.000091553f,  0.006896973f, -0.011444092f,  0.040618896f, -0.041992188f,  0.375152588f,  0.578125000f,  0.029937744f,  0.011383057f,  0.011901855f, -0.006072998f,  0.007232666f, -0.003143311f,  0.001312256f },
    { 0.000457764f, -0.000122070f, -0.000579834f,  0.007629395f, -0.012512207f,  0.041229248f, -0.042968750f,  0.355163574f,  0.589813232f,  0.041381836f,  0.007537842f,  0.014373779f, -0.007232666f,  0.007781982f, -0.003295898f,  0.001342773f },
    { 0.000396729f,  0.000091553f, -0.001068115f,  0.008300781f, -0.013397217f,  0.041534424f, -0.043304443f,  0.335052490f,  0.600433350f,  0.053649902f,  0.003570557f,  0.016876221f, -0.008392334f,  0.008331299f, -0.003479004f,  0.001403809f },
    { 0.000335693f,  0.000274658f, -0.001495361f,  0.008911133f, -0.014160156f,  0.041564941f, -0.043029785f,  0.314910889f,  0.609893799f,  0.066772461f, -0.000518799f,  0.019348145f, -0.009460449f,  0.008819580f, -0.003570557f,  0.001403809f },
    { 0.000274658f,  0.000488281f, -0.001922607f,  0.009429932f, -0.014739990f,  0.041320801f, -0.042205811f,  0.294799805f,  0.618164063f,  0.080688477f, -0.004608154f,  0.021789551f, -0.010498047f,  0.009246826f, -0.003662109f,  0.001434326f },
    { 0.000213623f,  0.000671387f, -0.002288818f,  0.009826660f, -0.015136719f,  0.040802002f, -0.040863037f,  0.274810791f,  0.625244141f,  0.095336914f, -0.008697510f,  0.024169922f, -0.011444092f,  0.009643555f, -0.003723145f,  0.001434326f },
    { 0.000183105f,  0.000793457f, -0.002593994f,  0.010162354f, -0.015380859f,  0.040039063f, -0.039062500f,  0.254974365f,  0.631072998f,  0.110748291f, -0.012756348f,  0.026489258f, -0.012329102f,  0.009979248f, -0.003753662f,  0.001434326f },
    { 0.000122070f,  0.000946045f, -0.002868652f,  0.010406494f, -0.015472412f,  0.039001465f, -0.036773682f,  0.235382080f,  0.635650635f,  0.126831055f, -0.016754150f,  0.028747559f, -0.013153076f,  0.010253906f, -0.003723145f,  0.001403809f },
    { 0.000091553f,  0.001068115f, -0.003112793f,  0.010589600f, -0.015441895f,  0.037780762f, -0.034149170f,  0.216125488f,  0.638916016f,  0.143615723f, -0.020660400f,  0.030853271f, -0.013854980f,  0.010498047f, -0.003692627f,  0.001373291f },
    { 0.000030518f,  0.001220703f, -0.003356934f,  0.010681152f, -0.015228271f,  0.036315918f, -0.031158447f,  0.197265625f,  0.640899658f,  0.160949707f, -0.024383545f,  0.032836914f, -0.014434814f,  0.010620117f, -0.003601074f,  0.001342773f },
    // extra step! this step is just the first one reversed
    { 0.000000000f,  0.001312256f, -0.003509521f,  0.010681152f, -0.014892578f,  0.034667969f, -0.027893066f,  0.178863525f,  0.641540527f,  0.178863525f, -0.027893066f,  0.034667969f, -0.014892578f,  0.010681152f, -0.003509521f,  0.001312256f }
};

}

Mixer::Mixer() :
    mVolumeStepLeft(0.0f),
    mVolumeStepRight(0.0f),
    mSamplerate(0),
    mFactor(0.0f),
    mBuffer(),
    mBuffersize(0),
    mAccumulators(),
    mSampleOffset(0.0f),
    mWriteIndex(0),
    mHighpassRate(0.0f)

{
    setSamplerate(44100);
}


void Mixer::mix(MixMode mode, int8_t delta, uint32_t cycletime) {
    switch (mode) {
        case MixMode::mute:
            break;
        case MixMode::left:
            mixfast<MixMode::left>(delta, cycletime);
            break;
        case MixMode::right:
            mixfast<MixMode::right>(delta, cycletime);
            break;
        case MixMode::middle:
            mixfast<MixMode::middle>(delta, cycletime);
            break;
        default:
            break;
    }
}

float Mixer::sampletime(uint32_t cycletime) const noexcept {
    return (cycletime * mFactor) + mSampleOffset;
}

void Mixer::mixDc(float dcLeft, float dcRight, uint32_t cycletime) {
    auto buf = mBuffer.get() + (((size_t)sampletime(cycletime) + mWriteIndex) * 2);
    *buf++ += dcLeft;
    *buf += dcRight;
}

Mixer::MixParam Mixer::getMixParameters(uint32_t cycletime) {
    // convert cycle time to sample time, separating the
    // integral and fraction components

    // modff was too slow
    float time = sampletime(cycletime);
    float phase = (time - (int)time) * PHASES;

    return {
        STEP_TABLE[(int)(phase)],
        mBuffer.get() + (((size_t)time + mWriteIndex) * 2),
        phase - (int)phase
    };
}

//
// Returns a pair of deltas both scaled by the given scale and linearly interpolated by
// the given fraction
//
static inline std::pair<float, float> deltaScale(float delta, float scale, float interp) {
    delta *= scale;
    float deltaInterp = delta * interp;
    return std::make_pair(delta - deltaInterp, deltaInterp);
}


template <MixMode mode>
void Mixer::mixfast(int8_t delta, uint32_t cycletime) {
    // muted mixing is a no-op, so don't bother instantiating a template
    // for this mode.
    static_assert(mode != MixMode::mute, "cannot mix a muted mode!");

    auto param = getMixParameters(cycletime);

    if constexpr (mode == MixMode::right) {
        ++param.dest;
    }

    std::pair<float, float> deltaLeft, deltaRight;

    if constexpr (modePansLeft(mode)) {
        deltaLeft = deltaScale(delta, mVolumeStepLeft, param.timeFract);
    }

    if constexpr (modePansRight(mode)) {
        deltaRight = deltaScale(delta, mVolumeStepRight, param.timeFract);
    }

    // interpolate with the next step set
    auto nextset = param.stepset + STEP_WIDTH;
    for (auto i = STEP_WIDTH; i--; ) {
        auto const s0 = *param.stepset++;
        auto const s1 = *nextset++;


        if constexpr (modePansLeft(mode)) {
            *param.dest++ += deltaLeft.first * s0 + deltaLeft.second * s1;
        }

        if constexpr (modePansRight(mode)) {
            *param.dest++ += deltaRight.first * s0 + deltaRight.second * s1;
        }

        if constexpr (mode != MixMode::middle) {
            ++param.dest;
        }
    }

}

void Mixer::setVolume(float leftVolume, float rightVolume) {
    mVolumeStepLeft = leftVolume;
    mVolumeStepRight = rightVolume;
}

float Mixer::leftVolume() const noexcept {
    return mVolumeStepLeft;
}

float Mixer::rightVolume() const noexcept {
    return mVolumeStepRight;
}

void Mixer::setBuffer(size_t samples) {
    auto size = (samples + STEP_WIDTH) * 2;
    if (size != mBuffersize) {
        mBuffer = std::make_unique<float[]>(size);
        mBuffersize = size;
    }
    clear();
}

void Mixer::setSamplerate(unsigned rate) {
    if (mSamplerate != rate) {
        mSamplerate = rate;
        mFactor = mSamplerate / constants::CLOCK_SPEED<float>;
        // using SameBoy's HPF (GB_HIGHPASS_ACCURATE)
        mHighpassRate = powf(0.999958f, 1.0f / mFactor);
    }
}

void Mixer::clear() {
    mSampleOffset = 0.0f;
    mWriteIndex = 0;
    for (auto &accum : mAccumulators) {
        accum.reset();
    }
    std::fill_n(mBuffer.get(), mBuffersize, 0.0f);
}

void Mixer::endFrame(uint32_t cycletime) {
    float index;
    mSampleOffset = modff(sampletime(cycletime), &index);
    mWriteIndex += (size_t)index;
}

size_t Mixer::availableSamples() const noexcept {
    return mWriteIndex;
}

void Mixer::Accum::reset() {
    sum = highpass = 0.0f;
}

void Mixer::Accum::process(float *dest, float in, float highPassRate) {
    sum += in;
    in = sum - highpass;
    highpass = sum - (in * highPassRate);
    *dest = in;
}


size_t Mixer::readSamples(float *buf, size_t samples) {
    samples = std::min(samples, mWriteIndex);
    if (samples) {

        float const* in = mBuffer.get();
        for (size_t i = samples; i--; ) {
            mAccumulators[0].process(buf++, *in++, mHighpassRate);
            mAccumulators[1].process(buf++, *in++, mHighpassRate);
        }
        removeSamples(samples);
    }

    return samples;
}

void Mixer::removeSamples(size_t samples) {
    auto amountInFrames = samples * 2;
    std::copy(mBuffer.get() + amountInFrames, mBuffer.get() + mBuffersize, mBuffer.get());
    std::fill_n(mBuffer.get() + (mBuffersize - amountInFrames), amountInFrames, 0.0f);
    mWriteIndex -= samples;
}


template void Mixer::mixfast<MixMode::left>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::right>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::middle>(int8_t delta, uint32_t cycletime);


}
}
