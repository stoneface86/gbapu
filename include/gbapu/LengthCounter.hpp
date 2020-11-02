
#pragma once

#include "gbapu/Generator.hpp"

namespace gbapu {


class LengthCounter {

public:

    LengthCounter(Generator &gen);
    ~LengthCounter() = default;

    uint8_t counter() const;
    
    void enable();

    void reset();

    void trigger();

    void setCounter(uint8_t value);

private:

    Generator &mGen;
    uint8_t mCounter;
    bool mEnabled;

};


}