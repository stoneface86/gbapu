
#include "gbapu/LengthCounter.hpp"

namespace gbapu {

LengthCounter::LengthCounter(Generator &gen) :
    mGen(gen),
    mCounter(0),
    mEnabled(false)
{
}

uint8_t LengthCounter::counter() const {
    return mCounter;
}

void LengthCounter::enable() {
    mEnabled = true;
}

void LengthCounter::reset() {
    mCounter = 0;
    mEnabled = false;
}

void LengthCounter::trigger() {
    if (mEnabled) {
        if (--mCounter == 0) {
            mEnabled = false;
            mGen.disable();
        }
    }
}

void LengthCounter::setCounter(uint8_t value) {
    mCounter = value;
}


}
