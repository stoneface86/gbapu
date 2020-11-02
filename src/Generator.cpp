
#include "gbapu/Generator.hpp"

#include <cassert>

namespace gbapu {

Generator::Generator(uint32_t defaultPeriod, uint8_t defaultOutput) noexcept :
    Timer(defaultPeriod),
    mOutput(defaultOutput),
    mDisableMask(ENABLED)
{
}

void Generator::disable() noexcept {
    mDisableMask = DISABLED;
}

bool Generator::disabled() const noexcept {
    return mDisableMask == DISABLED;
}

void Generator::restart() noexcept {
    mTimer = mPeriod;
    mDisableMask = ENABLED;
}


}
