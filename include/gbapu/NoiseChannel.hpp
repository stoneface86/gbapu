
#pragma once

#include "gbapu/Channel.hpp"

namespace gbapu {

class NoiseChannel : public EnvChannelBase {

public:

    NoiseChannel() noexcept;

    void restart() noexcept;

    void reset() noexcept;

protected:
    void stepOscillator() noexcept;

    void setPeriod() noexcept;

private:

    bool mValidScf;

    // width of the LFSR (7-bit = true, 15-bit = false)
    bool mHalfWidth;
    // lfsr: linear feedback shift register
    uint16_t mLfsr;


};



}
