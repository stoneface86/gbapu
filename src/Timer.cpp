
#include "gbapu.hpp"

#include <cassert>

namespace gbapu::_internal {

Timer::Timer(uint32_t period) :
    mTimer(period),
    mPeriod(period)
{
}

bool Timer::stepTimer(uint32_t cycles) {
    // if this assertion fails then we have missed a clock from the frequency timer!
    assert(mTimer >= cycles);

    mTimer -= cycles;
    if (mTimer == 0) {
        mTimer = mPeriod;
        return true;
    } else {
        return false;
    }
}

}
