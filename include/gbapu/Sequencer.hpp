
#pragma once

#include "gbapu/HardwareFile.hpp"

namespace gbapu {


class Sequencer : public Timer {

public:

    Sequencer(HardwareFile &hf) noexcept;

    void reset() noexcept;

    void step(uint32_t cycles) noexcept;

private:

    enum class TriggerType {
        lcSweep,
        lc,
        env
    };

    struct Trigger {
        uint32_t nextIndex;     // next index in the sequence
        uint32_t nextPeriod;    // timer period for the next trigger
        TriggerType type;       // trigger to do
    };

    static Trigger const TRIGGER_SEQUENCE[];

    HardwareFile &mHf;
    uint32_t mTriggerIndex;




};



}
