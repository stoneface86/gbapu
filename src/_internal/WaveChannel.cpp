
#include "gbapu.hpp"

#include <algorithm>

// some obscure behavior is not implemented
// 1. wave RAM is not corrupted on retrigger if the DAC is still enabled, this
//    behavior occurs on DMG models and was corrected for the CGB.

namespace {

// multiplier for frequency calculation
// 32 Hz - 65.536 KHz
static constexpr unsigned WAVE_MULTIPLIER = 2;

static constexpr uint32_t DEFAULT_PERIOD = (2048 - 0) * WAVE_MULTIPLIER;

// Obscure behavior on the DMG - this is the number of cycles after the channel is
// clocked by the frequency timer that we can access waveram while the channel is enabled
// note that this threshold is a guess - further research is needed to find the limit
static constexpr int RAM_ACCESS_THRESHOLD = 4;

}

namespace gbapu::_internal {

WaveChannel::WaveChannel() noexcept :
    ChannelBase(DEFAULT_PERIOD, 0),
    mLastRamAccess(0),
    mVolumeShift(0),
    mWaveIndex(0),
    mSampleBuffer(0),
    mWaveram{0}
{
    mVolume = 15;
}

uint8_t* WaveChannel::waveram() noexcept {
    return mWaveram;
}

bool WaveChannel::canAccessRam(uint32_t timestamp) const noexcept {
    if (mDacOn) {
        int64_t diff = timestamp - mLastRamAccess;
        return (diff >= 0 && diff < RAM_ACCESS_THRESHOLD);
    } else {
        // can always access ram when the DAC is off
        return true;
    }
}

uint8_t WaveChannel::readVolume() const noexcept {
    static uint8_t const shiftToNr32[] = {
        0x20, // mVolumeShift = 0
        0x40, // mVolumeShift = 1
        0x60, // mVolumeShift = 2
        0x00, // mVolumeShift = 3 (NOT USABLE)
        0x00  // mVolumeShift = 4
    };
    return shiftToNr32[mVolumeShift];
}

void WaveChannel::reset() noexcept {
    ChannelBase::reset();
    mLastRamAccess = 0;
    mVolumeShift = 0;
    mVolume = 15;
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
    static uint8_t const nr32ToShift[] = {
        4, // nr32 = 0x00 (Mute)
        0, // nr32 = 0x20 (100%)
        1, // nr32 = 0x40 ( 50%)
        2, // nr32 = 0x60 ( 25%)
    };

    // convert nr32 register to a shift amount
    // shift = 0 : sample / 1  = 100%
    // shift = 1 : sample / 2  =  50%
    // shift = 2 : sample / 4  =  25%
    // shift = 4 : sample / 16 =   0%
    auto volumeIndex = (volume >> 5) & 3;
    mVolumeShift = nr32ToShift[volumeIndex];
    mVolume = 15 >> mVolumeShift;
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
