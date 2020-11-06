
#include <algorithm>

#include "gbapu/WaveChannel.hpp"

// some obscure behavior is not implemented
// 1. wave RAM is not corrupted on retrigger if the DAC is still enabled, this
//    behavior occurs on DMG models and was corrected for the CGB.

namespace {

// multiplier for frequency calculation
// 32 Hz - 65.536 KHz
static constexpr unsigned WAVE_MULTIPLIER = 2;

static constexpr uint32_t DEFAULT_PERIOD = (2048 - 0) * WAVE_MULTIPLIER;

}

namespace gbapu {

WaveChannel::WaveChannel() noexcept :
    ChannelBase(DEFAULT_PERIOD, 0),
    mVolumeShift(0),
    mWaveIndex(0),
    mSampleBuffer(0),
    mWaveram{0}
{
}

uint8_t* WaveChannel::waveram() noexcept {
    return mWaveram;
}

void WaveChannel::reset() noexcept {
    ChannelBase::reset();
    mVolumeShift = 0;
    std::fill_n(mWaveram, constants::WAVE_RAMSIZE, static_cast<uint8_t>(0));
    mSampleBuffer = 0;
    restart();
}

void WaveChannel::restart() noexcept {
    ChannelBase::restart();
    // wave position is reset to 0, but the sample buffer remains unchanged
    mWaveIndex = 0;
}

void WaveChannel::writeVolume(uint8_t volume) noexcept {
    // convert nr32 register to a shift amount
    // shift = 0 : sample / 1  = 100%
    // shift = 1 : sample / 2  =  50%
    // shift = 2 : sample / 4  =  25%
    // shift = 4 : sample / 16 =   0%
    volume = (volume >> 5) & 0x3;
    if (volume) {
        mVolumeShift = volume - 1;
    } else {
        mVolumeShift = 4; // shifting by 4 results in 0 (mute)
    }
    setOutput();
}

void WaveChannel::stepOscillator() noexcept {

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

void WaveChannel::setPeriod() noexcept {
    mPeriod = (2048 - mFrequency) * WAVE_MULTIPLIER;
}

//Gbs::WaveVolume WaveChannel::volume() const noexcept {
//    return mVolume;
//}

void WaveChannel::setOutput() {
    mOutput = mSampleBuffer >> mVolumeShift;
}


}
