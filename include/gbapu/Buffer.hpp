
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

namespace gbapu {

class Buffer {

    friend class Apu;

public:

    enum Quality {
        QUALITY_LOW,    // low quality transitions on all channels
        QUALITY_MED,    // high quality transitions on CH1+CH2, low quality on CH3+CH4
        QUALITY_HIGH    // high quality transitions on all channels
    };

    Buffer(unsigned samplerate, unsigned buffersize = 100);
    ~Buffer();

    // access

    size_t available();

    size_t read(int16_t *dest, size_t samples);

    void clear();

    
    // settings

    void setQuality(Quality quality);

    void setVolume(unsigned percent);

    void setSamplerate(unsigned samplerate);

    void setBuffersize(unsigned milliseconds);

    void resize();


private:

    void addDelta12(int term, int16_t delta, uint32_t clocktime);

    void addDelta34(int term, int16_t delta, uint32_t clocktime);

    void endFrame(uint32_t clocktime);

    struct Internal;
    std::unique_ptr<Internal> mInternal;

    Quality mQuality;
    unsigned mVolumeStep;
    unsigned mSamplerate;
    unsigned mBuffersize;
    bool mResizeRequired;

};


}