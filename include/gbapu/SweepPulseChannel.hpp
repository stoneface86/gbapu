
#pragma once

#include "gbapu/PulseChannel.hpp"

namespace gbapu {

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

}
