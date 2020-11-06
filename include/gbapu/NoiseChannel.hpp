
#pragma once

#include "gbapu/Channel.hpp"
#include "gbapu/constants.hpp"

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

    // width of the LFSR (15-bit or 7-bit)
    Gbs::NoiseSteps mStepSelection;
    // lfsr: linear feedback shift register
    uint16_t mLfsr;


};



}
