
#include "gbapu/NoiseGen.hpp"

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

NoiseGen::NoiseGen() noexcept :
    Generator(calcCounterMax(Gbs::DEFAULT_DRF, Gbs::DEFAULT_SCF), 0),
    mRegister(Gbs::DEFAULT_NOISE_REGISTER),
    mStepSelection(Gbs::DEFAULT_STEP_COUNT),
    mLfsr(LFSR_INIT)
{
}

void NoiseGen::reset() noexcept {
    mRegister = Gbs::DEFAULT_NOISE_REGISTER;
    restart();
}

void NoiseGen::restart() noexcept {
    Generator::restart();
    mLfsr = LFSR_INIT;
    mOutput = 0;
}

void NoiseGen::step(uint32_t cycles) noexcept {
    mFreqCounter += cycles;
    while (mFreqCounter >= mPeriod) {
        mFreqCounter -= mPeriod;

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
    }

    // output is bit 0 inverted, so if bit 0 == 1, output MIN
    // V = (~mLfsr) & 0x1 gives us 0 for no sample, 1 for envelope value
    // ~V + 1 = 0 for no sample, 0xFF for envelope value (mask)
    // using de morgan's laws
    // ~((~mLfsr) & 0x1) + 1 -> (mLfsr | (~1)) + 1
    // so we get 0xFF when bit 0 is 0, and 0x00 when bit 0 is 1
    //mOutput = static_cast<uint8_t>(mLfsr | static_cast<uint8_t>(~1)) + 1;
    mOutput = (~mLfsr) & 1;
}

uint8_t NoiseGen::readRegister() const noexcept {
    return mRegister;
}

void NoiseGen::writeRegister(uint8_t reg) noexcept {
    mRegister = reg;
    uint8_t drf = mRegister & 0x7;
    mStepSelection = static_cast<Gbs::NoiseSteps>((mRegister >> 3) & 1);
    uint8_t scf = mRegister >> 4;
    mPeriod = calcCounterMax(drf, scf);
}



}
