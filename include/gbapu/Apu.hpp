
#pragma once

#include "Envelope.hpp"
#include "NoiseGen.hpp"
#include "PulseGen.hpp"
#include "Sequencer.hpp"
#include "Sweep.hpp"
#include "WaveGen.hpp"

#include "gbapu/Buffer.hpp"

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

    PulseGen mGen1, mGen2;
    WaveGen mGen3;
    NoiseGen mGen4;

    Envelope mEnv1, mEnv2, mEnv4;
    Sweep mSweep;

    Sequencer mSequencer;

};



}