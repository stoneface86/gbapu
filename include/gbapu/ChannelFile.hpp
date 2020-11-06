
#pragma once

#include "gbapu/SweepPulseChannel.hpp"
#include "gbapu/PulseChannel.hpp"
#include "gbapu/NoiseChannel.hpp"
#include "gbapu/WaveChannel.hpp"

namespace gbapu {

//
// POD struct for the individual hardware components of the synthesizer.
// A number in the field name indicates the channel it belongs to.
//
struct ChannelFile {

    Channel<SweepPulseChannel> ch1;
    Channel<PulseChannel> ch2;
    Channel<WaveChannel> ch3;
    Channel<NoiseChannel> ch4;

    ChannelFile() noexcept :
        ch1(),
        ch2(),
        ch3(),
        ch4()
    {
    }

};


}