
#pragma once

#include "Envelope.hpp"
#include "LengthCounter.hpp"
#include "NoiseGen.hpp"
#include "PulseGen.hpp"
#include "Sweep.hpp"
#include "WaveGen.hpp"

namespace gbapu {

//
// POD struct for the individual hardware components of the synthesizer.
// A number in the field name indicates the channel it belongs to.
//
struct HardwareFile {

    Envelope env1, env2, env4;
    Sweep sweep1;
    PulseGen gen1, gen2;
    WaveGen gen3;
    NoiseGen gen4;
    LengthCounter lc1, lc2, lc3, lc4;

    HardwareFile() noexcept :
        env1(),
        env2(),
        env4(),
        sweep1(gen1),
        gen1(),
        gen2(),
        gen3(),
        gen4(),
        lc1(gen1),
        lc2(gen2),
        lc3(gen3),
        lc4(gen4)
    {
    }

};


}