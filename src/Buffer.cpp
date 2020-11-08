
#include "gbapu.hpp"

#include "blip_buf.h"

#include <algorithm>

namespace gbapu {

struct Buffer::Internal {
    // index 0 is the left terminal, 1 is the right
    blip_buffer_t *bbuf[2];

    Internal() :
        bbuf{ nullptr }
    {
    }

    ~Internal() {
        blip_delete(bbuf[0]);
        blip_delete(bbuf[1]);
    }

};


Buffer::Buffer(unsigned samplerate, size_t buffersizeInSamples) :
    mInternal(new Internal()),
    mIsHighQuality(true),
    mSamplerate(samplerate),
    mBuffersize(buffersizeInSamples),
    mResizeRequired(true)
{
    setVolume(100);
    resize();
}

Buffer::~Buffer() = default;

size_t Buffer::available() {
    return blip_samples_avail(mInternal->bbuf[0]);
}

size_t Buffer::read(int16_t *dest, size_t samples) {
    size_t toRead = std::min(samples, available());
    blip_read_samples(mInternal->bbuf[0], dest, toRead, 1);
    blip_read_samples(mInternal->bbuf[1], dest + 1, toRead, 1);
    return toRead;
}

void Buffer::clear() {
    blip_clear(mInternal->bbuf[0]);
    blip_clear(mInternal->bbuf[1]);
}

void Buffer::setQuality(bool quality) {
    mIsHighQuality = quality;
}

void Buffer::setVolume(unsigned percent) {
    double limit = INT16_MAX * pow(percent / 100.0, 2);
    // max amp on each channel is 15 so max amp is 60
    // 8 master volume levels so 60 * 8 = 480
    mVolumeStep = static_cast<unsigned>(limit / 480.0);
}

void Buffer::setSamplerate(unsigned samplerate) {
    if (mSamplerate != samplerate) {
        mSamplerate = samplerate;
        mResizeRequired = true;
    }
}

void Buffer::setBuffersize(size_t samples) {
    if (mBuffersize != samples) {
        mBuffersize = samples;
        mResizeRequired = true;
    }
}

void Buffer::resize() {

    if (mResizeRequired) {

        for (int i = 0; i != 2; ++i) {
            auto &bbuf = mInternal->bbuf[i];
            blip_delete(bbuf);
            bbuf = blip_new(mBuffersize);
            blip_set_rates(bbuf, constants::CLOCK_SPEED<double>, mSamplerate);
        }

        mResizeRequired = false;
    }
}

void Buffer::addDelta(int term, int16_t delta, uint32_t clocktime) {
    if (mIsHighQuality) {
        blip_add_delta(mInternal->bbuf[term], clocktime, delta * mVolumeStep);
    } else {
        blip_add_delta_fast(mInternal->bbuf[term], clocktime, delta * mVolumeStep);
    }
}

void Buffer::endFrame(uint32_t clocktime) {
    blip_end_frame(mInternal->bbuf[0], clocktime);
    blip_end_frame(mInternal->bbuf[1], clocktime);

}



}
