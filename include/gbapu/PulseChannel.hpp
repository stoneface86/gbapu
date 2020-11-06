
#pragma once

#include "gbapu/Channel.hpp"

#include <cstdint>

namespace gbapu {

class PulseChannel : public EnvChannelBase {

public:

    PulseChannel() noexcept;

    uint8_t duty() const noexcept;

    //
    // Set the duty of the pulse. Does not require restart.
    //
    void writeDuty(uint8_t duty) noexcept;

    void reset() noexcept;

    void restart() noexcept;

protected:

    void stepOscillator() noexcept;

    void setPeriod() noexcept;

private:

    uint8_t mDuty;
    uint8_t mDutyWaveform;
    
    unsigned mDutyCounter;


};



}
