
#include "gbapu.hpp"

#include <cassert>
#include <cmath>

// the audio buffer is stereo interleaved, always remember to * 2 when indexing

namespace gbapu::_internal {

constexpr size_t PHASES = 32;
constexpr size_t STEP_WIDTH = 16;

//
// pre-computed step table for bandlimited-synthesis
// table was taken from blargg's bandlimited synthesis example - https://web.archive.org/web/20200216121030/http://www.slack.net/~ant/bl-synth/bandlimited.zip
//
static float const STEP_TABLE[PHASES][STEP_WIDTH] = {
    { -0.0184025131f, 0.0297560841f, -0.0347368829f, 0.0430234447f, -0.054382734f, 0.0781219453f, -0.126568645f, 0.556177258f, 0.616528034f, -0.125728071f, 0.0780046582f, -0.0542643666f, 0.0429624915f, -0.0346751809f, 0.0297103524f, -0.0255258679f },
    { -0.0182203092f, 0.0293509755f, -0.034261778f, 0.0424350984f, -0.0536419787f, 0.0770451277f, -0.124944702f, 0.524641275f, 0.64503324f, -0.123118758f, 0.0767701864f, -0.0533851981f, 0.0423021913f, -0.0341323614f, 0.0292553902f, -0.0251284242f },
    { -0.0179097578f, 0.028668616f, -0.0334597901f, 0.0414481312f, -0.0523885638f, 0.0752485842f, -0.122065261f, 0.492412478f, 0.672228336f, -0.118967295f, 0.0747364759f, -0.0519556403f, 0.0412208438f, -0.033246994f, 0.028512001f, -0.024482131f },
    { -0.0174750537f, 0.0277181193f, -0.0323421732f, 0.0400773101f, -0.0506436229f, 0.0727676675f, -0.118018836f, 0.459652603f, 0.697973013f, -0.113223016f, 0.0719011426f, -0.0499790907f, 0.0397235155f, -0.0320256352f, 0.0274868608f, -0.0235928297f },
    { -0.016921429f, 0.0265110806f, -0.0309231691f, 0.0383410454f, -0.0484328642f, 0.0696431845f, -0.112898827f, 0.426523656f, 0.722130775f, -0.105842829f, 0.0682692528f, -0.0474632978f, 0.0378174782f, -0.0304745436f, 0.0261851549f, -0.0224646926f },
    { -0.0162552856f, 0.025061544f, -0.0292197652f, 0.0362607613f, -0.045785509f, 0.0659207106f, -0.106803283f, 0.393188834f, 0.744574666f, -0.0967923999f, 0.0638523698f, -0.0444225669f, 0.0355164409f, -0.0286056399f, 0.0246168971f, -0.0211077929f },
    { -0.0154839708f, 0.0233855471f, -0.027251482f, 0.0338611081f, -0.0427346863f, 0.0616504177f, -0.0998336449f, 0.359809756f, 0.765187979f, -0.0860523582f, 0.0586722493f, -0.0408763885f, 0.0328372717f, -0.0264332891f, 0.0227954984f, -0.0195339918f },
    { -0.0146158561f, 0.0215013288f, -0.0250400752f, 0.0311692543f, -0.0393164903f, 0.0568859503f, -0.0920944363f, 0.326546222f, 0.783860505f, -0.0736090541f, 0.0527556539f, -0.0368480086f, 0.0297988057f, -0.0239750743f, 0.0207372308f, -0.0177559853f },
    { -0.0136601292f, 0.0194288231f, -0.0226093698f, 0.028214816f, -0.0355700813f, 0.0516845137f, -0.0836918578f, 0.293555021f, 0.800494075f, -0.0594609976f, 0.0461380482f, -0.0323675275f, 0.0264263749f, -0.0212499499f, 0.0184568763f, -0.0157886147f },
    { -0.012626702f, 0.0171896145f, -0.0199851114f, 0.0250295848f, -0.0315368362f, 0.0461055897f, -0.0747332945f, 0.260989368f, 0.815000176f, -0.0436173677f, 0.0388631821f, -0.0274685621f, 0.022746563f, -0.0182828903f, 0.0159758329f, -0.0136491656f },
    { -0.0115261469f, 0.0148066608f, -0.0171944685f, 0.0216468126f, -0.0272601768f, 0.0402112901f, -0.0653266534f, 0.228997305f, 0.827303648f, -0.0260998011f, 0.0309807062f, -0.0221898556f, 0.0187914371f, -0.015098989f, 0.0133170485f, -0.0113587976f },
    { -0.010369706f, 0.012304144f, -0.0142660402f, 0.0181016065f, -0.0227851011f, 0.0340647064f, -0.0555791482f, 0.197720706f, 0.837339401f, -0.00694000721f, 0.0225496292f, -0.0165759325f, 0.0145953894f, -0.0117259026f, 0.0105016232f, -0.0089353323f },
    { -0.00916891824f, 0.00970709603f, -0.0112293446f, 0.0144299483f, -0.0181577504f, 0.0277300254f, -0.0455968976f, 0.167296529f, 0.845054924f, 0.0138187408f, 0.0136340261f, -0.0106725693f, 0.0101940632f, -0.00819301605f, 0.00755703449f, -0.00640392303f },
    { -0.00793569256f, 0.0070412471f, -0.00811463594f, 0.0106684603f, -0.0134248435f, 0.0212716758f, -0.0354838073f, 0.13785255f, 0.850411236f, 0.0361213088f, 0.00430715084f, -0.00453209877f, 0.00562775135f, -0.00453460217f, 0.0045106411f, -0.00378632545f },
    { -0.00668218592f, 0.00433279388f, -0.00495265331f, 0.00685440423f, -0.00863349903f, 0.0147537803f, -0.0253411792f, 0.109509036f, 0.853381097f, 0.0599033237f, -0.00535464287f, 0.00179171562f, 0.000937819481f, -0.000783324242f, 0.00139021873f, -0.00110673904f },
    { -0.00542057864f, 0.00160801457f, -0.0017740489f, 0.0030246831f, -0.00383035722f, 0.00823958404f, -0.0152667481f, 0.0823783427f, 0.853951871f, 0.0850877166f, -0.01526618f, 0.00823962688f, -0.00383019447f, 0.00302445889f, -0.00177431107f, 0.00160813332f },
    { -0.00416310737f, -0.00110684754f, 0.00139042479f, -0.000783733791f, 0.000938409241f, 0.00179099489f, -0.00535423495f, 0.0565620288f, 0.85212177f, 0.111590981f, -0.025341928f, 0.0147541165f, -0.00863349438f, 0.00685453415f, -0.00495266914f, 0.00433278084f },
    { -0.00292180176f, -0.00378582976f, 0.00451062154f, -0.0045346655f, 0.00562749943f, -0.00453220075f, 0.00430752523f, 0.0321532041f, 0.847904086f, 0.139313638f, -0.0354838371f, 0.0212708712f, -0.0134245157f, 0.0106688738f, -0.00811517239f, 0.00704169273f },
    { -0.00170851371f, -0.00640359381f, 0.00755708013f, -0.00819308124f, 0.0101936068f, -0.010672478f, 0.0136346892f, 0.0092337057f, 0.841324449f, 0.168151081f, -0.045597136f, 0.0277299285f, -0.0181577802f, 0.0144301057f, -0.0112297535f, 0.00970768929f },
    { -0.000534632243f, -0.00893560331f, 0.0105013559f, -0.0117250402f, 0.0145946834f, -0.0165753327f, 0.0225495547f, -0.0121249752f, 0.832421362f, 0.197988629f, -0.0555792451f, 0.0340644717f, -0.0227851272f, 0.0181017518f, -0.0142659545f, 0.0123041272f },
    { 0.000588860945f, -0.01135829f, 0.0133162411f, -0.0150984451f, 0.0187913291f, -0.022189673f, 0.0309804603f, -0.0318628103f, 0.821246088f, 0.228703082f, -0.0653269887f, 0.0402111411f, -0.0272604227f, 0.0216475725f, -0.0171952844f, 0.0148071647f },
    { 0.00165153958f, -0.0136494851f, 0.0159761459f, -0.018282894f, 0.0227464996f, -0.0274681114f, 0.038862586f, -0.0499304309f, 0.807860911f, 0.260163486f, -0.0747331977f, 0.0461054444f, -0.0315366387f, 0.0250294805f, -0.0199849606f, 0.0171896219f },
    { 0.00264370302f, -0.0157884024f, 0.0184571557f, -0.0212502703f, 0.0264259353f, -0.0323671028f, 0.0461377576f, -0.0662903339f, 0.792342007f, 0.292233229f, -0.083691895f, 0.0516842008f, -0.0355698466f, 0.0282148719f, -0.0226093531f, 0.0194283724f },
    { 0.0035563542f, -0.0177557357f, 0.0207371712f, -0.0239748619f, 0.0297987182f, -0.0368479788f, 0.0527554452f, -0.0809167027f, 0.774774253f, 0.324768126f, -0.0920943618f, 0.0568858981f, -0.0393167138f, 0.0311697125f, -0.0250405073f, 0.0215011835f },
    { 0.00438133301f, -0.0195342079f, 0.0227964502f, -0.0264334846f, 0.032837037f, -0.0408763699f, 0.0586723015f, -0.0937943608f, 0.755255342f, 0.357619047f, -0.0998337865f, 0.061650455f, -0.0427343249f, 0.0338606238f, -0.0272508264f, 0.0233847499f },
    { 0.00511138467f, -0.0211083442f, 0.0246173907f, -0.0286058243f, 0.035516873f, -0.0444230549f, 0.0638529509f, -0.104920737f, 0.733891547f, 0.390633225f, -0.10680306f, 0.0659203529f, -0.0457853079f, 0.0362608433f, -0.0292196274f, 0.0250613689f },
    { 0.00574027514f, -0.0224648789f, 0.026184978f, -0.0304745324f, 0.0378177464f, -0.0474637225f, 0.068269372f, -0.114303052f, 0.710799873f, 0.423653722f, -0.112898886f, 0.0696431994f, -0.0484325886f, 0.038340807f, -0.0309232473f, 0.0265109539f },
    { 0.00626263442f, -0.0235925168f, 0.0274865702f, -0.0320253223f, 0.0397232175f, -0.0499792323f, 0.0719013438f, -0.121960349f, 0.686104119f, 0.456520557f, -0.11801827f, 0.0727676153f, -0.0506439209f, 0.0400774479f, -0.0323421359f, 0.027718246f },
    { 0.00667436002f, -0.024482578f, 0.0285124332f, -0.0332471281f, 0.0412208512f, -0.0519559644f, 0.074736543f, -0.127922416f, 0.659936488f, 0.48907572f, -0.122065425f, 0.0752484798f, -0.0523886085f, 0.0414481759f, -0.0334597826f, 0.0286688805f },
    { 0.00697226496f, -0.0251283683f, 0.02925523f, -0.0341320634f, 0.0423020683f, -0.0533852503f, 0.0767699927f, -0.132228523f, 0.632436574f, 0.521155357f, -0.124944508f, 0.0770446658f, -0.0536416173f, 0.0424352288f, -0.0342622399f, 0.0293511748f },
    { 0.00715441722f, -0.0255259201f, 0.0297107063f, -0.0346755832f, 0.0429625139f, -0.0542639568f, 0.0780044124f, -0.134928733f, 0.603748918f, 0.552600026f, -0.126569152f, 0.0781222582f, -0.0543830395f, 0.0430234671f, -0.0347364545f, 0.0297561288f },
    { 0.00721982634f, -0.0256733205f, 0.0298772901f, -0.0348765142f, 0.0432017297f, -0.0545940921f, 0.0784494877f, -0.136081576f, 0.574023604f, 0.583250523f, -0.126854479f, 0.0784491897f, -0.0545944571f, 0.043201983f, -0.034876883f, 0.0298777223f }
};


Mixer::Mixer() :
    mVolumeStepLeft(0.0f),
    mVolumeStepRight(0.0f),
    mFactor(0.0f),
    mBuffer(),
    mBuffersize(0),
    mSumLeft(0.0f),
    mSumRight(0.0f),
    mSampleOffset(0.0f),
    mWriteIndex(0)
{
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

    // using macros to DRY, non-macro version is a little messier

#define LOOP_STEP() for (auto i = STEP_WIDTH; i--; )


    if constexpr (mode == MixMode::middle) {

        float const deltaLeft = delta * mVolumeStepLeft;
        float const deltaRight = delta * mVolumeStepRight;
        LOOP_STEP() {
            auto const s = *param.stepset++;
            *param.dest++ += s * deltaLeft;
            *param.dest++ += s * deltaRight;
        }

    } else {

        float const deltaf = [&](){
            if constexpr (mode == MixMode::left) {
                return delta * mVolumeStepLeft;
            } else {
                ++param.dest; // adjust destination pointer to right channel
                return delta * mVolumeStepRight;
            }
        }();
        LOOP_STEP() {
            *param.dest += *param.stepset++ * deltaf;
            param.dest += 2;
        }

    }

#undef LOOP_STEP

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
    mFactor = rate / constants::CLOCK_SPEED<float>;
}

void Mixer::clear() {
    mSampleOffset = 0.0f;
    mWriteIndex = 0;
    mSumLeft = 0.0f;
    mSumRight = 0.0f;
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

size_t Mixer::readSamples(float *buf, size_t samples) {
    samples = std::min(samples, mWriteIndex);
    if (samples) {

        float const* in = mBuffer.get();
        for (size_t i = samples; i--; ) {
            auto sample = *in++;
            mSumLeft += sample;
            *buf++ = mSumLeft;
            // TODO: highpass filter

            sample = *in++;
            mSumRight += sample;
            *buf++ = mSumRight;
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


template void Mixer::mixfast<MixMode::left>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::right>(int8_t delta, uint32_t cycletime);
template void Mixer::mixfast<MixMode::middle>(int8_t delta, uint32_t cycletime);

}

