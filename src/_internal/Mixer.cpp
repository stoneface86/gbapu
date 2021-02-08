
#include "gbapu.hpp"

#include "BlipBuf.hpp"

#include <cassert>


namespace gbapu::_internal {

Mixer::Mixer(BlipBuf &buf) :
    mBlip(buf),
    mVolumeStepLeft(0),
    mVolumeStepRight(0),
    mDcLeft(0),
    mDcRight(0)
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

    // DC offset is -7.5 times the volume step (or is 1/2 the range of a channel's output)
    mDcLeft = (-7 * mVolumeStepLeft) + (mVolumeStepLeft / 2);
    mDcRight = (-7 * mVolumeStepRight) + (mVolumeStepRight / 2);

}

int32_t Mixer::leftVolume() const noexcept {
    return mVolumeStepLeft;
}

int32_t Mixer::rightVolume() const noexcept {
    return mVolumeStepRight;
}

int32_t Mixer::dcLeft() const noexcept {
    return mDcLeft;
}

int32_t Mixer::dcRight() const noexcept {
    return mDcRight;
}

}

