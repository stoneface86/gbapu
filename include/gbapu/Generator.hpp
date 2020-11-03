
#pragma once

#include "gbapu/Timer.hpp"

#include <cstdint>

namespace gbapu {


class Generator : public Timer {

public:

    //
    // Disables the generator. A disabled generator always outputs 0
    // Generators can only be re-enabled by restarting.
    //
    void disable() noexcept;

    //
    // Returns true if the generator is disabled, false otherwise.
    //
    bool disabled() const noexcept;

    //
    // Returns true if the DAC is enabled, false otherwise.
    //
    bool dacOn() const noexcept;

    //
    // Hardware reset the generator.
    //
    virtual void reset() noexcept = 0;

    //
    // Restart (retrigger) the generator.
    //  * Channel is enabled
    //  * The timer is reloaded with the period
    //
    //
    virtual void restart() noexcept;

    //
    // Return the current output of the generator. Channels with an envelope
    // return 1 for the current envelope value and 0 for off.
    //
    inline uint8_t output() const noexcept {
        return mDisableMask & mOutput;
    }

    //
    // Enable/Disable the DAC. Disabling the DAC also disables the generator
    // For CH3, the DAC is controlled by bit 7 in NR30
    // For other channels, it is disabled when the upper 5 bits of NRx2 is cleared,
    // and enabled otherwise. Disabling the DAC also disables the generator, but enabling
    // it does not enable the generator.
    //
    void setDacEnable(bool enabled) noexcept;


protected:

    Generator(uint32_t defaultPeriod, uint8_t defaultOutput) noexcept;

    uint8_t mOutput;

private:

    static constexpr uint8_t ENABLED  = 0xFF;
    static constexpr uint8_t DISABLED = 0x00;

    uint8_t mDisableMask;
    bool mDacOn;

};



}
