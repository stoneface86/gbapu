
#pragma once

#include "gbapu/Buffer.hpp"
#include "gbapu/HardwareFile.hpp"
#include "gbapu/Sequencer.hpp"

#include <memory>

namespace gbapu {


class Apu {

public:

    Apu(Buffer &buffer);
    ~Apu();

    void reset();

    void step(uint32_t cycles);

    uint8_t readRegister(uint16_t addr);
    uint8_t readRegister(uint8_t reg);

    void writeRegister(uint16_t addr, uint8_t value);
    void writeRegister(uint8_t reg, uint8_t value);


private:

    Buffer &mBuffer;

    HardwareFile mHf;
    Sequencer mSequencer;

};



}