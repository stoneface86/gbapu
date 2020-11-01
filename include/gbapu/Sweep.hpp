
#pragma once

#include "gbapu/PulseGen.hpp"
#include "gbapu/constants.hpp"

#include <cstdint>


namespace gbapu {


class Sweep {

public:

    Sweep(PulseGen &gen) noexcept;

    uint8_t readRegister() const noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void writeRegister(uint8_t reg) noexcept;

    void trigger() noexcept;

private:

    PulseGen &mGen;

    Gbs::SweepMode mSweepMode;
    uint8_t mSweepTime;
    uint8_t mSweepShift;

    uint8_t mSweepCounter;

    // Sweep register, NR10
    // Bits 0-2: Shift amount
    // Bit    3: Sweep mode (1 = subtraction)
    // Bits 4-6: Period
    uint8_t mRegister;

    // shadow register, CH1's frequency gets copied here on reset (initalization)
    int16_t mShadow;

};


}
