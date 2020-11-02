
#include <algorithm>

#include "gbapu/WaveGen.hpp"

// some obscure behavior is not implemented
// 1. wave RAM is not corrupted on retrigger if the DAC is still enabled, this
//    behavior occurs on DMG models and was corrected for the CGB.

namespace {

// multiplier for frequency calculation
// 32 Hz - 65.536 KHz
static constexpr unsigned WAVE_MULTIPLIER = 2;

static constexpr uint32_t DEFAULT_PERIOD = (2048 - gbapu::Gbs::DEFAULT_FREQUENCY) * WAVE_MULTIPLIER;

}

namespace gbapu {

WaveGen::WaveGen() noexcept :
    Generator(DEFAULT_PERIOD, 0),
    mFrequency(Gbs::DEFAULT_FREQUENCY),
    mVolume(Gbs::DEFAULT_WAVE_LEVEL),
    mWaveIndex(0),
    mSampleBuffer(0),
    mWaveram{0}
{
}

uint8_t* WaveGen::waveram() noexcept {
    return mWaveram;
}

uint16_t WaveGen::frequency() const noexcept {
    return mFrequency;
}

void WaveGen::reset() noexcept {
    mFrequency = Gbs::DEFAULT_FREQUENCY;
    mVolume = Gbs::DEFAULT_WAVE_LEVEL;
    std::fill_n(mWaveram, Gbs::WAVE_RAMSIZE, static_cast<uint8_t>(0));
    mSampleBuffer = 0;
    restart();
}

void WaveGen::restart() noexcept {
    Generator::restart();
    // wave position is reset to 0, but the sample buffer remains unchanged
    mWaveIndex = 0;
}

void WaveGen::setFrequency(uint16_t frequency) noexcept {
    mFrequency = frequency;
    mPeriod = (2048 - frequency) * WAVE_MULTIPLIER;
}

void WaveGen::setVolume(Gbs::WaveVolume volume) noexcept {
    mVolume = volume;
    setOutput();
}

void WaveGen::step(uint32_t cycles) noexcept {

    if (stepTimer(cycles)) {
        mWaveIndex = (mWaveIndex + 1) & 0x1F;
        mSampleBuffer = mWaveram[mWaveIndex >> 1];
        if (mWaveIndex & 1) {
            // odd number, low nibble
            mSampleBuffer &= 0xF;
        } else {
            // even number, high nibble
            mSampleBuffer >>= 4;
        }

        
        setOutput();
    }
}

Gbs::WaveVolume WaveGen::volume() const noexcept {
    return mVolume;
}

void WaveGen::setOutput() {
    switch (mVolume) {
        case Gbs::WAVE_MUTE:
            mOutput = 0;
            break;
        case Gbs::WAVE_FULL:
            mOutput = mSampleBuffer;
            break;
        case Gbs::WAVE_HALF:
            mOutput = mSampleBuffer >> 1;
            break;
        case Gbs::WAVE_QUARTER:
            mOutput = mSampleBuffer >> 2;
            break;
    }
}


}
