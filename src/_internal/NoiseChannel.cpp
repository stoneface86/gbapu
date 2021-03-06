
#include "gbapu.hpp"

constexpr uint16_t LFSR_INIT = 0x7FFF;

namespace gbapu::_internal {

NoiseChannel::NoiseChannel() noexcept :
    EnvChannelBase(8, 8, 64),
    mValidScf(true),
    mHalfWidth(false),
    mLfsr(LFSR_INIT)
{
}

uint8_t NoiseChannel::readNoise() const noexcept {
    return static_cast<uint8_t>(mFrequency);
}

void NoiseChannel::reset() noexcept {
    EnvChannelBase::reset();
    mValidScf = true;
    mLfsr = LFSR_INIT;
}

void NoiseChannel::restart() noexcept {
    EnvChannelBase::restart();
    mLfsr = LFSR_INIT;
    // bit 0 inverted of LFSR_INIT is 0
    mOutput = 0;
}

void NoiseChannel::stepOscillator() noexcept {

    if (mValidScf) {

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

        mOutput = -((~mLfsr) & 1) & mVolume;
    }
    
}

void NoiseChannel::setPeriod() noexcept {
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
    mPeriod = drf << scf;
}



}
