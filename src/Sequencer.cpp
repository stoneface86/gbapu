
#include "gbapu.hpp"

// A step occurs every 8192 cycles (4194304 Hz / 8192 = 512 Hz)
//
// Step:                 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
// --------------------------+---+---+---+---+---+---+---+-------------------
// Len. counter (256 Hz) | x       x       x       x      
// Sweep        (128 Hz) |         x               x       
// envelope     ( 64 Hz) |                             x  
//

namespace {

constexpr uint32_t CYCLES_PER_STEP = 8192;

constexpr uint32_t DEFAULT_PERIOD = CYCLES_PER_STEP * 2;

}


namespace gbapu::_internal {

Sequencer::Trigger const Sequencer::TRIGGER_SEQUENCE[] = {

    {1,     CYCLES_PER_STEP * 2,    TriggerType::lc},

    {2,     CYCLES_PER_STEP * 2,    TriggerType::lcSweep},

    {3,     CYCLES_PER_STEP,        TriggerType::lc},

    // step 6 trigger, next trigger: 7
    {4,     CYCLES_PER_STEP,        TriggerType::lcSweep},

    // step 7 trigger, next trigger: 0
    {0,     CYCLES_PER_STEP * 2,    TriggerType::env}
};

Sequencer::Sequencer(ChannelFile &cf) noexcept :
    Timer(DEFAULT_PERIOD),
    mCf(cf),
    mTriggerIndex(0)
{
}

void Sequencer::reset() noexcept {
    mPeriod = mTimer = DEFAULT_PERIOD;
    mTriggerIndex = 0;
}

void Sequencer::step(uint32_t cycles) noexcept {
    
    if (stepTimer(cycles)) {
        Trigger const &trigger = TRIGGER_SEQUENCE[mTriggerIndex];
        switch (trigger.type) {
            case TriggerType::lcSweep:
                mCf.ch1.stepSweep();
                [[fallthrough]];
            case TriggerType::lc:
                mCf.ch1.stepLengthCounter();
                mCf.ch2.stepLengthCounter();
                mCf.ch3.stepLengthCounter();
                mCf.ch4.stepLengthCounter();
                break;
            case TriggerType::env:
                mCf.ch1.stepEnvelope();
                mCf.ch2.stepEnvelope();
                mCf.ch4.stepEnvelope();
                break;
        }
        mPeriod = trigger.nextPeriod;
        mTriggerIndex = trigger.nextIndex;
    }

}


}
