
#include "gbapu.hpp"

#include "BlipBuf.hpp"

#include <cassert>


namespace gbapu::_internal {

#if 0
Mixer::Mixer(BlipBuf &blip) :
    mBlip(blip),
    mVolumeStepLeft(0),
    mVolumeStepRight(0),
    mPanning(0),
    mHighQuality(true),
    mLastOutput(0),
    mMode(MixMode::lowQualityMute)
{

}

void Mixer::reset() {
    mLastOutput = 0;
    mPanning = 0;
}

void Mixer::setQuality(bool quality) {
    mHighQuality = quality;
    if (quality) {
        mMode = static_cast<MixMode>(static_cast<int>(mMode) | 4);
    } else {
        mMode = static_cast<MixMode>(static_cast<int>(mMode) & ~4);
    }
}

void Mixer::setOutput(int8_t sample, uint32_t cycletime) {
    if (sample != mLastOutput) {
        mix(sample - mLastOutput, cycletime);
        mLastOutput = sample;
    }
}

void Mixer::setPanning(int panning, uint32_t cycletime) {
    if (mLastOutput) {
        // if a terminal goes from enabled -> disabled, we need to drop the amplitude to 0
        // and if it goes from disabled -> enabled, we need to go to the last amplitude from setOutput
        
        // XOR'ing the new value with the current one tells us what changed
        auto changes = mPanning ^ panning;
        if (!!(changes & PAN_LEFT)) {
            // the left terminal output status changed, determine amplitude
            auto ampl = (int16_t)((mLastOutput * mVolumeStepLeft + 0x8000) >> 16);
            // if the new value is ON, go to this amplitude, otherwise drop down to 0
            addDelta(0, cycletime, !!(panning & PAN_LEFT) ? ampl : -ampl);
        }
        if (!!(changes & PAN_RIGHT)) {
            // same as above but for the right terminal
            auto ampl = (int16_t)((mLastOutput * mVolumeStepRight + 0x8000) >> 16);
            addDelta(1, cycletime, !!(panning & PAN_RIGHT) ? ampl : -ampl);
        }
    }
    int mode = static_cast<int>(mMode) & ~3;
    if (panning & PAN_LEFT) {
        mode |= 1;
    }

    if (panning & PAN_RIGHT) {
        mode |= 2;
    }

    mMode = static_cast<MixMode>(mode);
    mPanning = panning;
}

void Mixer::setVolume(int32_t volumeLeft, int32_t volumeRight, uint32_t cycletime) {
    // a change in volume requires a transition to the new volume for each terminal
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

// mixing




void Mixer::mix(int8_t delta, uint32_t cycletime) {
    switch (mMode) {
        case MixMode::lowQualityMute:
        case MixMode::highQualityMute:
            break;
        case MixMode::lowQualityLeft:
            mixfast<MixMode::lowQualityLeft>(delta, cycletime);
            break;
        case MixMode::lowQualityRight:
            mixfast<MixMode::lowQualityRight>(delta, cycletime);
            break;
        case MixMode::lowQualityMiddle:
            mixfast<MixMode::lowQualityMiddle>(delta, cycletime);
            break;
        case MixMode::highQualityLeft:
            mixfast<MixMode::highQualityLeft>(delta, cycletime);
            break;
        case MixMode::highQualityRight:
            mixfast<MixMode::highQualityRight>(delta, cycletime);
            break;
        case MixMode::highQualityMiddle:
            mixfast<MixMode::highQualityMiddle>(delta, cycletime);
            break;
        default:
            break;
    }
}

template <MixMode mode>
void Mixer::mixfast(int8_t delta, uint32_t cycletime) {
    // a muted mode results in an empty function body, so make sure we do not implement one!
    static_assert(mode != MixMode::highQualityMute && mode != MixMode::lowQualityMute, "cannot mix a muted mode!");

    constexpr auto deltaFn = modeIsHighQuality(mode) ? blip_add_delta : blip_add_delta_fast;

    if constexpr (modePansLeft(mode)) {
        deltaFn(mBlip.bbuf[0], cycletime, (int16_t)((delta * mVolumeStepLeft + 0x8000) >> 16));
    }
    if constexpr (modePansRight(mode)) {
        deltaFn(mBlip.bbuf[1], cycletime, (int16_t)((delta * mVolumeStepRight + 0x8000) >> 16));
    }
}

#endif

Mixer::Mixer(BlipBuf &buf) :
    mBlip(buf),
    mVolumeStepLeft(0),
    mVolumeStepRight(0)
{
}

void Mixer::addDelta(MixMode mode, int term, uint32_t cycletime, int16_t delta) {
    if (modeIsHighQuality(mode)) {
        blip_add_delta(mBlip.bbuf[term], cycletime, delta);
    } else {
        blip_add_delta_fast(mBlip.bbuf[term], cycletime, delta);
    }
}

void Mixer::mix(MixMode mode, int8_t sample, uint32_t cycletime) {
    switch (mode) {
        case MixMode::lowQualityMute:
        case MixMode::highQualityMute:
            break;
        case MixMode::lowQualityLeft:
            mixfast<MixMode::lowQualityLeft>(sample, cycletime);
            break;
        case MixMode::lowQualityRight:
            mixfast<MixMode::lowQualityRight>(sample, cycletime);
            break;
        case MixMode::lowQualityMiddle:
            mixfast<MixMode::lowQualityMiddle>(sample, cycletime);
            break;
        case MixMode::highQualityLeft:
            mixfast<MixMode::highQualityLeft>(sample, cycletime);
            break;
        case MixMode::highQualityRight:
            mixfast<MixMode::highQualityRight>(sample, cycletime);
            break;
        case MixMode::highQualityMiddle:
            mixfast<MixMode::highQualityMiddle>(sample, cycletime);
            break;
        default:
            break;
    }
}

template <MixMode mode>
void Mixer::mixfast(int8_t delta, uint32_t cycletime) {
    // a muted mode results in an empty function body, so make sure we do not implement one!
    static_assert(mode != MixMode::highQualityMute && mode != MixMode::lowQualityMute, "cannot mix a muted mode!");

    constexpr auto deltaFn = modeIsHighQuality(mode) ? blip_add_delta : blip_add_delta_fast;

    if constexpr (modePansLeft(mode)) {
        deltaFn(mBlip.bbuf[0], cycletime, (int16_t)((delta * mVolumeStepLeft + 0x8000) >> 16));
    }
    if constexpr (modePansRight(mode)) {
        deltaFn(mBlip.bbuf[1], cycletime, (int16_t)((delta * mVolumeStepRight + 0x8000) >> 16));
    }
}

void Mixer::setVolume(int32_t leftVolume, int32_t rightVolume) {
    mVolumeStepLeft = leftVolume;
    mVolumeStepRight = rightVolume;
}

int32_t Mixer::leftVolume() const noexcept {
    return mVolumeStepLeft;
}

int32_t Mixer::rightVolume() const noexcept {
    return mVolumeStepRight;
}

}

