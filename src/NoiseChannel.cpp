
#include "gbapu/NoiseChannel.hpp"

constexpr uint16_t LFSR_INIT = 0x7FFF;
#define calcCounterMax(drf, scf) (DRF_TABLE[drf] << (scf+1))

namespace {

static const uint8_t DRF_TABLE[] = {
    8,
    16,
    32,
    48,
    64,
    80,
    96,
    112
};

}

namespace gbapu {

NoiseChannel::NoiseChannel() noexcept :
    EnvChannelBase(16, 64),
    mValidScf(true),
    mStepSelection(Gbs::DEFAULT_STEP_COUNT),
    mLfsr(LFSR_INIT)
{
}

void NoiseChannel::reset() noexcept {
    EnvChannelBase::reset();
    mValidScf = true;
    mLfsr = LFSR_INIT;
}

void NoiseChannel::restart() noexcept {
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
        if (mStepSelection == Gbs::NOISE_STEPS_7) {
            // 7-bit lfsr, set bit 7 with the result
            mLfsr &= ~0x40; // reset bit 7
            mLfsr |= result << 6; // set bit 7 result
        }

        mOutput = (~mLfsr) & 1;
    }
    
}

void NoiseChannel::setPeriod() noexcept {
    uint8_t drf = mFrequency & 0x7;
    mStepSelection = static_cast<Gbs::NoiseSteps>((mFrequency >> 3) & 1);
    uint8_t scf = mFrequency >> 4;
    mValidScf = scf < 0xD; // obscure behavior: if scf >= 0xD then the channel receives no clocks
    mPeriod = calcCounterMax(drf, scf);
}



}
