
#pragma once

#include "trackerboy/data/Waveform.hpp"
#include "gbapu/constants.hpp"

namespace gbapu {

class WaveGen : public Generator {

public:

    WaveGen() noexcept;

    void copyWave(Waveform &wave) noexcept;

    uint16_t frequency() const noexcept;

    void reset() noexcept override;

    void restart() noexcept override;

    void setFrequency(uint16_t frequency) noexcept;

    void setVolume(Gbs::WaveVolume volume) noexcept;

    void step(uint32_t cycles) noexcept;

    Gbs::WaveVolume volume() const noexcept;

private:

    uint16_t mFrequency;
    Gbs::WaveVolume mVolume;
    uint8_t mWaveIndex;
    uint8_t mSampleBuffer;
    uint8_t mWaveram[Gbs::WAVE_RAMSIZE];


};

}
