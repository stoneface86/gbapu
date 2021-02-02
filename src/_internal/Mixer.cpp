
#include "gbapu.hpp"

#include "BlipBuf.hpp"

#include <cassert>


namespace gbapu::_internal {


Mixer::Mixer(BlipBuf &blip) :
    mBlip(blip),
    mVolumeStepLeft(0),
    mVolumeStepRight(0),
    mPanning(0),
    mHighQuality(true),
    mLastOutput(0)
{

}

void Mixer::reset() {
    mLastOutput = 0;
    mPanning = 0;
}

void Mixer::setQuality(bool quality) {
    mHighQuality = quality;
}

void Mixer::setOutput(int8_t sample, uint32_t cycletime) {
    if (sample != mLastOutput) {
        auto deltaSample = sample - mLastOutput;
        if (!!(mPanning & PAN_LEFT)) {
            auto delta = (int16_t)((deltaSample * mVolumeStepLeft + 0x8000) >> 16);
            addDelta(0, cycletime, delta);
        }

        if (!!(mPanning & PAN_RIGHT)) {
            auto delta = (int16_t)((deltaSample * mVolumeStepRight + 0x8000) >> 16);
            addDelta(1, cycletime, delta);
        }
        mLastOutput = sample;
    }
}

void Mixer::setPanning(int panning, uint32_t cycletime) {
    if (mLastOutput) {
        auto changes = mPanning ^ panning;
        if (!!(changes & PAN_LEFT)) {
            auto ampl = (int16_t)((mLastOutput * mVolumeStepLeft + 0x8000) >> 16);
            addDelta(0, cycletime, !!(panning & PAN_LEFT) ? ampl : -ampl);
        }
        if (!!(changes & PAN_RIGHT)) {
            auto ampl = (int16_t)((mLastOutput * mVolumeStepRight + 0x8000) >> 16);
            addDelta(1, cycletime, !!(panning & PAN_RIGHT) ? ampl : -ampl);
        }
    }
    mPanning = panning;
}

void Mixer::setVolume(int32_t volumeLeft, int32_t volumeRight, uint32_t cycletime) {
    // a change in volume requires a transition
    if (mLastOutput) {
        if (!!(mPanning & PAN_LEFT)) {
            auto delta = (int16_t)((mLastOutput * (volumeLeft - mVolumeStepLeft) + 0x8000) >> 16);
            addDelta(0, cycletime, delta);
        }
        if (!!(mPanning & PAN_RIGHT)) {
            auto delta = (int16_t)((mLastOutput * (volumeRight - mVolumeStepRight) + 0x8000) >> 16);
            addDelta(1, cycletime, delta);
        }
    }

    mVolumeStepLeft = volumeLeft;
    mVolumeStepRight = volumeRight;
}

void Mixer::addDelta(int term, uint32_t cycletime, int16_t delta) {
    if (mHighQuality) {
        blip_add_delta(mBlip.bbuf[term], cycletime, delta);
    } else {
        blip_add_delta_fast(mBlip.bbuf[term], cycletime, delta);
    }
}

}

