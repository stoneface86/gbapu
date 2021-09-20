
#include "gbapu.hpp"

#include <cassert>
#include <cmath>

// the audio buffer is stereo interleaved, always remember to * 2 when indexing

namespace gbapu::_internal {

constexpr size_t PHASES = 32;
constexpr size_t STEP_WIDTH = 16;

//
// pre-computed step table for bandlimited-synthesis
// table is from the blip_buf library, converted to float by multiplying all values by (1/32768.0f)
//
static float const STEP_TABLE[PHASES][STEP_WIDTH] = {
    { 0.001312256f, -0.003509521f,  0.010681152f, -0.014892578f,  0.034667969f, -0.027893066f,  0.178863525f,  0.641540527f,  0.178863525f, -0.027893066f,  0.034667969f, -0.014892578f,  0.010681152f, -0.003509521f,  0.001312256f,  0.000000000f },
    { 0.001342773f, -0.003601074f,  0.010620117f, -0.014434814f,  0.032836914f, -0.024383545f,  0.160949707f,  0.640899658f,  0.197265625f, -0.031158447f,  0.036315918f, -0.015228271f,  0.010681152f, -0.003356934f,  0.001220703f,  0.000030518f },
    { 0.001373291f, -0.003692627f,  0.010498047f, -0.013854980f,  0.030853271f, -0.020660400f,  0.143615723f,  0.638916016f,  0.216125488f, -0.034149170f,  0.037780762f, -0.015441895f,  0.010589600f, -0.003112793f,  0.001068115f,  0.000091553f },
    { 0.001403809f, -0.003723145f,  0.010253906f, -0.013153076f,  0.028747559f, -0.016754150f,  0.126831055f,  0.635650635f,  0.235382080f, -0.036773682f,  0.039001465f, -0.015472412f,  0.010406494f, -0.002868652f,  0.000946045f,  0.000122070f },
    { 0.001434326f, -0.003753662f,  0.009979248f, -0.012329102f,  0.026489258f, -0.012756348f,  0.110748291f,  0.631072998f,  0.254974365f, -0.039062500f,  0.040039063f, -0.015380859f,  0.010162354f, -0.002593994f,  0.000793457f,  0.000183105f },
    { 0.001434326f, -0.003723145f,  0.009643555f, -0.011444092f,  0.024169922f, -0.008697510f,  0.095336914f,  0.625244141f,  0.274810791f, -0.040863037f,  0.040802002f, -0.015136719f,  0.009826660f, -0.002288818f,  0.000671387f,  0.000213623f },
    { 0.001434326f, -0.003662109f,  0.009246826f, -0.010498047f,  0.021789551f, -0.004608154f,  0.080688477f,  0.618164063f,  0.294799805f, -0.042205811f,  0.041320801f, -0.014739990f,  0.009429932f, -0.001922607f,  0.000488281f,  0.000274658f },
    { 0.001403809f, -0.003570557f,  0.008819580f, -0.009460449f,  0.019348145f, -0.000518799f,  0.066772461f,  0.609893799f,  0.314910889f, -0.043029785f,  0.041564941f, -0.014160156f,  0.008911133f, -0.001495361f,  0.000274658f,  0.000335693f },
    { 0.001403809f, -0.003479004f,  0.008331299f, -0.008392334f,  0.016876221f,  0.003570557f,  0.053649902f,  0.600433350f,  0.335052490f, -0.043304443f,  0.041534424f, -0.013397217f,  0.008300781f, -0.001068115f,  0.000091553f,  0.000396729f },
    { 0.001342773f, -0.003295898f,  0.007781982f, -0.007232666f,  0.014373779f,  0.007537842f,  0.041381836f,  0.589813232f,  0.355163574f, -0.042968750f,  0.041229248f, -0.012512207f,  0.007629395f, -0.000579834f, -0.000122070f,  0.000457764f },
    { 0.001312256f, -0.003143311f,  0.007232666f, -0.006072998f,  0.011901855f,  0.011383057f,  0.029937744f,  0.578125000f,  0.375152588f, -0.041992188f,  0.040618896f, -0.011444092f,  0.006896973f, -0.000091553f, -0.000366211f,  0.000549316f },
    { 0.001281738f, -0.002990723f,  0.006652832f, -0.004882813f,  0.009460449f,  0.015106201f,  0.019317627f,  0.565399170f,  0.394958496f, -0.040344238f,  0.039703369f, -0.010223389f,  0.006072998f,  0.000488281f, -0.000610352f,  0.000610352f },
    { 0.001220703f, -0.002777100f,  0.006042480f, -0.003692627f,  0.007049561f,  0.018646240f,  0.009582520f,  0.551696777f,  0.414489746f, -0.037963867f,  0.038482666f, -0.008850098f,  0.005187988f,  0.001037598f, -0.000823975f,  0.000671387f },
    { 0.001159668f, -0.002563477f,  0.005432129f, -0.002471924f,  0.004669189f,  0.022033691f,  0.000671387f,  0.537078857f,  0.433654785f, -0.034851074f,  0.036956787f, -0.007293701f,  0.004241943f,  0.001617432f, -0.001098633f,  0.000762939f },
    { 0.001098633f, -0.002319336f,  0.004791260f, -0.001312256f,  0.002441406f,  0.025146484f, -0.007354736f,  0.521606445f,  0.452392578f, -0.030975342f,  0.035156250f, -0.005615234f,  0.003234863f,  0.002227783f, -0.001342773f,  0.000823975f },
    { 0.001037598f, -0.002075195f,  0.004119873f, -0.000091553f,  0.000244141f,  0.028045654f, -0.014526367f,  0.505310059f,  0.470642090f, -0.026306152f,  0.033050537f, -0.003753662f,  0.002136230f,  0.002868652f, -0.001586914f,  0.000885010f },
    { 0.000976563f, -0.001861572f,  0.003509521f,  0.001037598f, -0.001831055f,  0.030700684f, -0.020843506f,  0.488311768f,  0.488311768f, -0.020843506f,  0.030700684f, -0.001831055f,  0.001037598f,  0.003509521f, -0.001861572f,  0.000976563f },
    { 0.000885010f, -0.001586914f,  0.002868652f,  0.002136230f, -0.003753662f,  0.033050537f, -0.026306152f,  0.470642090f,  0.505310059f, -0.014526367f,  0.028045654f,  0.000244141f, -0.000091553f,  0.004119873f, -0.002075195f,  0.001037598f },
    { 0.000823975f, -0.001342773f,  0.002227783f,  0.003234863f, -0.005615234f,  0.035156250f, -0.030975342f,  0.452392578f,  0.521606445f, -0.007354736f,  0.025146484f,  0.002441406f, -0.001312256f,  0.004791260f, -0.002319336f,  0.001098633f },
    { 0.000762939f, -0.001098633f,  0.001617432f,  0.004241943f, -0.007293701f,  0.036956787f, -0.034851074f,  0.433654785f,  0.537078857f,  0.000671387f,  0.022033691f,  0.004669189f, -0.002471924f,  0.005432129f, -0.002563477f,  0.001159668f },
    { 0.000671387f, -0.000823975f,  0.001037598f,  0.005187988f, -0.008850098f,  0.038482666f, -0.037963867f,  0.414489746f,  0.551696777f,  0.009582520f,  0.018646240f,  0.007049561f, -0.003692627f,  0.006042480f, -0.002777100f,  0.001220703f },
    { 0.000610352f, -0.000610352f,  0.000488281f,  0.006072998f, -0.010223389f,  0.039703369f, -0.040344238f,  0.394958496f,  0.565399170f,  0.019317627f,  0.015106201f,  0.009460449f, -0.004882813f,  0.006652832f, -0.002990723f,  0.001281738f },
    { 0.000549316f, -0.000366211f, -0.000091553f,  0.006896973f, -0.011444092f,  0.040618896f, -0.041992188f,  0.375152588f,  0.578125000f,  0.029937744f,  0.011383057f,  0.011901855f, -0.006072998f,  0.007232666f, -0.003143311f,  0.001312256f },
    { 0.000457764f, -0.000122070f, -0.000579834f,  0.007629395f, -0.012512207f,  0.041229248f, -0.042968750f,  0.355163574f,  0.589813232f,  0.041381836f,  0.007537842f,  0.014373779f, -0.007232666f,  0.007781982f, -0.003295898f,  0.001342773f },
    { 0.000396729f,  0.000091553f, -0.001068115f,  0.008300781f, -0.013397217f,  0.041534424f, -0.043304443f,  0.335052490f,  0.600433350f,  0.053649902f,  0.003570557f,  0.016876221f, -0.008392334f,  0.008331299f, -0.003479004f,  0.001403809f },
    { 0.000335693f,  0.000274658f, -0.001495361f,  0.008911133f, -0.014160156f,  0.041564941f, -0.043029785f,  0.314910889f,  0.609893799f,  0.066772461f, -0.000518799f,  0.019348145f, -0.009460449f,  0.008819580f, -0.003570557f,  0.001403809f },
    { 0.000274658f,  0.000488281f, -0.001922607f,  0.009429932f, -0.014739990f,  0.041320801f, -0.042205811f,  0.294799805f,  0.618164063f,  0.080688477f, -0.004608154f,  0.021789551f, -0.010498047f,  0.009246826f, -0.003662109f,  0.001434326f },
    { 0.000213623f,  0.000671387f, -0.002288818f,  0.009826660f, -0.015136719f,  0.040802002f, -0.040863037f,  0.274810791f,  0.625244141f,  0.095336914f, -0.008697510f,  0.024169922f, -0.011444092f,  0.009643555f, -0.003723145f,  0.001434326f },
    { 0.000183105f,  0.000793457f, -0.002593994f,  0.010162354f, -0.015380859f,  0.040039063f, -0.039062500f,  0.254974365f,  0.631072998f,  0.110748291f, -0.012756348f,  0.026489258f, -0.012329102f,  0.009979248f, -0.003753662f,  0.001434326f },
    { 0.000122070f,  0.000946045f, -0.002868652f,  0.010406494f, -0.015472412f,  0.039001465f, -0.036773682f,  0.235382080f,  0.635650635f,  0.126831055f, -0.016754150f,  0.028747559f, -0.013153076f,  0.010253906f, -0.003723145f,  0.001403809f },
    { 0.000091553f,  0.001068115f, -0.003112793f,  0.010589600f, -0.015441895f,  0.037780762f, -0.034149170f,  0.216125488f,  0.638916016f,  0.143615723f, -0.020660400f,  0.030853271f, -0.013854980f,  0.010498047f, -0.003692627f,  0.001373291f },
    { 0.000030518f,  0.001220703f, -0.003356934f,  0.010681152f, -0.015228271f,  0.036315918f, -0.031158447f,  0.197265625f,  0.640899658f,  0.160949707f, -0.024383545f,  0.032836914f, -0.014434814f,  0.010620117f, -0.003601074f,  0.001342773f }
};


Mixer::Mixer() :
    mVolumeStepLeft(0.0f),
    mVolumeStepRight(0.0f),
    mSamplerate(44100),
    mFactor(0.0f),
    mBuffer(),
    mBuffersize(0),
    mAccumulators(),
    mSampleOffset(0.0f),
    mWriteIndex(0),
    mHighpassRate(0.0f)

{
    calculateFactor();
    calculateHighpass();
}


void Mixer::mix(MixMode mode, int8_t sample, uint32_t cycletime) {
    switch (mode) {
        case MixMode::mute:
            break;
        case MixMode::left:
            mixfast<MixMode::left>(sample, cycletime);
            break;
        case MixMode::right:
            mixfast<MixMode::right>(sample, cycletime);
            break;
        case MixMode::middle:
            mixfast<MixMode::middle>(sample, cycletime);
            break;
        default:
            break;
    }
}

float Mixer::sampletime(uint32_t cycletime) const noexcept {
    return (cycletime * mFactor) + mSampleOffset + mWriteIndex;
}

void Mixer::mixDc(float dcLeft, float dcRight, uint32_t cycletime) {
    auto buf = mBuffer.get() + ((size_t)sampletime(cycletime) * 2);
    *buf++ += dcLeft;
    *buf += dcRight;
}

Mixer::MixParam Mixer::getMixParameters(uint32_t cycletime) {
    // convert cycle time to sample time, separating the
    // integral and fraction components
    float index;
    float phase = modff(sampletime(cycletime), &index);

    return {
        STEP_TABLE[(size_t)(phase * PHASES)],
        mBuffer.get() + ((size_t)index * 2)
    };
}


template <MixMode mode>
void Mixer::mixfast(int8_t delta, uint32_t cycletime) {
    // a muted mode results in an empty function body, so make sure we do not implement one!
    static_assert(mode != MixMode::mute, "cannot mix a muted mode!");

    auto param = getMixParameters(cycletime);

    float deltaLeft;
    float deltaRight;

    if constexpr (modePansLeft(mode)) {
        deltaLeft = delta * mVolumeStepLeft;
    } else {
        (void)deltaLeft;
    }

    if constexpr (modePansRight(mode)) {
        deltaRight = delta * mVolumeStepRight;
    } else {
        (void)deltaRight;
    }

    if constexpr (mode == MixMode::right) {
        ++param.dest;
    }


    for (auto i = STEP_WIDTH; i--; ) {
        auto const s = *param.stepset++;

        if constexpr (modePansLeft(mode)) {
            *param.dest++ += deltaLeft * s;
        }

        if constexpr (modePansRight(mode)) {
            *param.dest++ += deltaRight * s;
        }

        if constexpr (mode != MixMode::middle) {
            ++param.dest;
        }
    }

}

void Mixer::setVolume(float leftVolume, float rightVolume) {
    mVolumeStepLeft = leftVolume;
    mVolumeStepRight = rightVolume;
}

float Mixer::leftVolume() const noexcept {
    return mVolumeStepLeft;
}

float Mixer::rightVolume() const noexcept {
    return mVolumeStepRight;
}

void Mixer::setBuffer(size_t samples) {
    auto size = (samples + STEP_WIDTH) * 2;
    if (size != mBuffersize) {
        mBuffer = std::make_unique<float[]>(size);
        mBuffersize = size;
    }
    clear();
}

void Mixer::setSamplerate(unsigned rate) {
    if (mSamplerate != rate) {
        mSamplerate = rate;
        calculateFactor();
        calculateHighpass(); // highpass rate dependent on mFactor
    }
}

void Mixer::clear() {
    mSampleOffset = 0.0f;
    mWriteIndex = 0;
    for (auto &accum : mAccumulators) {
        accum.reset();
    }
    std::fill_n(mBuffer.get(), mBuffersize, 0.0f);
}

void Mixer::endFrame(uint32_t cycletime) {
    float index;
    mSampleOffset = modff(sampletime(cycletime), &index);
    mWriteIndex = (size_t)index;
}

size_t Mixer::availableSamples() const noexcept {
    return mWriteIndex;
}

void Mixer::Accum::reset() {
    sum = highpass = 0.0f;
}

void Mixer::Accum::process(float *dest, float in, float highPassRate) {
    sum += in;
    in = sum - highpass;
    highpass = sum - (in * highPassRate);
    *dest = in;
}


size_t Mixer::readSamples(float *buf, size_t samples) {
    samples = std::min(samples, mWriteIndex);
    if (samples) {

        float const* in = mBuffer.get();
        for (size_t i = samples; i--; ) {
            mAccumulators[0].process(buf++, *in++, mHighpassRate);
            mAccumulators[1].process(buf++, *in++, mHighpassRate);
        }
        removeSamples(samples);
    }

    return samples;
}

void Mixer::removeSamples(size_t samples) {
    auto amountInFrames = samples * 2;
    std::copy(mBuffer.get() + amountInFrames, mBuffer.get() + mBuffersize, mBuffer.get());
    std::fill_n(mBuffer.get() + (mBuffersize - amountInFrames), amountInFrames, 0.0f);
    mWriteIndex -= samples;
}


void Mixer::calculateFactor() {
    mFactor = mSamplerate / constants::CLOCK_SPEED<float>;
}

void Mixer::calculateHighpass() {
    // using SameBoy's HPF (GB_HIGHPASS_ACCURATE)
    mHighpassRate = powf(0.999958f, 1.0f / mFactor);
}


template void Mixer::mixfast<MixMode::left>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::right>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::middle>(int8_t delta, uint32_t cycletime);

}

