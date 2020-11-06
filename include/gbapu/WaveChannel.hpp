
#pragma once

#include "gbapu/Channel.hpp"
#include "gbapu/constants.hpp"

namespace gbapu {

class WaveChannel : public ChannelBase {

public:

    WaveChannel() noexcept;

    uint8_t* waveram() noexcept;

    void reset() noexcept;

    void restart() noexcept;

    void writeVolume(uint8_t volume) noexcept;

    Gbs::WaveVolume volume() const noexcept;


protected:

    void stepOscillator() noexcept;

    void setPeriod() noexcept;

private:

    

    void setOutput();

    Gbs::WaveVolume mVolume;
    uint8_t mWaveIndex;
    uint8_t mSampleBuffer;
    uint8_t mWaveram[Gbs::WAVE_RAMSIZE];


};

}
