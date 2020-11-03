
#include "gbapu/Generator.hpp"

#include <cassert>

namespace gbapu {

Generator::Generator(uint32_t defaultPeriod, uint8_t defaultOutput) noexcept :
    Timer(defaultPeriod),
    mOutput(defaultOutput),
    mDisableMask(ENABLED),
    mDacOn(false)
{
}

bool Generator::dacOn() const noexcept {
    return mDacOn;
}

void Generator::disable() noexcept {
    mDisableMask = DISABLED;
}

bool Generator::disabled() const noexcept {
    return mDisableMask == DISABLED;
}

void Generator::restart() noexcept {
    mTimer = mPeriod;
    // if the DAC is off the channel is immediately disabled
    mDisableMask = mDacOn ? ENABLED : DISABLED;
}

void Generator::setDacEnable(bool enabled) noexcept {
    mDacOn = enabled;
    if (!enabled) {
        disable();
    }
}


}
