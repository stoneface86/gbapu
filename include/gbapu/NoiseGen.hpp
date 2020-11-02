
#pragma once

#include "gbapu/Generator.hpp"
#include "gbapu/constants.hpp"

namespace gbapu {

class NoiseGen : public Generator {

public:

    NoiseGen() noexcept;

    void reset() noexcept override;

    //
    // channel retrigger. LFSR is re-initialized, counters are reset
    // and the period is reloaded from the set register
    //
    void restart() noexcept override;

    //
    // Step the generator for the given number of cycles, returning the
    // current output.
    //
    void step(uint32_t cycles) noexcept;

    //
    // Write the given value to this generator's register, NR43
    //
    void writeRegister(uint8_t reg) noexcept;

    //
    // Returns the contents of this generator's register
    //
    uint8_t readRegister() const noexcept;

private:

    // NR43 register contents
    uint8_t mRegister;
    bool mValidScf;

    // width of the LFSR (15-bit or 7-bit)
    Gbs::NoiseSteps mStepSelection;
    // lfsr: linear feedback shift register
    uint16_t mLfsr;


};



}
