
#pragma once

#include <cstdint>

namespace gbapu {

// Mixin class
class Timer {

public:

    //
    // Returns the frequency timer, or the number of cycles needed to complete a period.
    //
    inline uint32_t timer() const noexcept {
        return mTimer;
    }

protected:

    Timer(uint32_t period);

    //
    // step the timer the given number of cycles. If the timer is now 0,
    // true is returned and the timer is reloaded with the period
    //
    bool stepTimer(uint32_t cycles);

    uint32_t mTimer;
    uint32_t mPeriod;


};



}