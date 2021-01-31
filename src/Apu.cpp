
#include "gbapu.hpp"

#include "blip_buf.h"

#include <algorithm>

namespace gbapu {

struct Apu::Internal {
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

Apu::Apu(unsigned samplerate, size_t buffersizeInSamples, Model model) :
    mModel(model),
    mCf(),
    mSequencer(mCf),
    mRegs{ 0 },
    mCycletime(0),
    mLeftVolume(1),
    mRightVolume(1),
    mOutputStat(0),
    mLastAmpLeft(0),
    mLastAmpRight(0),
    mEnabled(false),
    mInternal(new Internal()),
    mIsHighQuality(true),
    mSamplerate(samplerate),
    mBuffersize(buffersizeInSamples),
    mResizeRequired(true)
{
    setVolume(1.0f);
    resizeBuffer();
}

Apu::~Apu() = default;

Registers const& Apu::registers() const {
    return mRegs;
}

void Apu::reset() noexcept {
    endFrame();

    mSequencer.reset();
    mCf.ch1.reset();
    mCf.ch2.reset();
    mCf.ch3.reset();
    mCf.ch4.reset();
    mOutputStat = 0;
    mLeftVolume = 1;
    mRightVolume = 1;
    mLastAmpLeft = 0;
    mLastAmpRight = 0;
    mEnabled = false;

}

void Apu::reset(Model model) noexcept {
    mModel = model;
    reset();
}

uint8_t Apu::readRegister(uint8_t reg, uint32_t autostep) {

    step(autostep);

    /*
    * Read masks
    *      NRx0 NRx1 NRx2 NRx3 NRx4
    *     ---------------------------
    * NR1x  $80  $3F $00  $FF  $BF 
    * NR2x  $FF  $3F $00  $FF  $BF 
    * NR3x  $7F  $FF $9F  $FF  $BF
    * NR4x  $FF  $FF $00  $00  $BF
    * NR5x  $00  $00 $70
    *
    * $FF27-$FF2F always read back as $FF
    */


    // TODO: length counters can still be accessed on DMG when powered off
    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this read
        return 0xFF;
    }

    switch (reg) {
        // ===== CH1 =====

        case REG_NR10:
            return mCf.ch1.readSweep();
        case REG_NR11:
            return 0x3F | mCf.ch1.readDuty();
        case REG_NR12:
            return mCf.ch1.readEnvelope();
        case REG_NR13:
            return 0xFF;
        case REG_NR14:
            return mCf.ch1.lengthEnabled() ? 0xFF : 0xBF;
        
        // ===== CH2 =====

        case REG_NR21:
            return 0x3F | mCf.ch2.readDuty();
        case REG_NR22:
            return mCf.ch2.readEnvelope();
        case REG_NR23:
            return 0xFF;
        case REG_NR24:
            return mCf.ch2.lengthEnabled() ? 0xFF : 0xBF;

        // ===== CH3 =====

        case REG_NR30:
            return mCf.ch3.dacOn() ? 0xFF : 0x7F;
        case REG_NR31:
            return 0xFF;
        case REG_NR32:
            return 0x9F | mCf.ch3.readVolume();
        case REG_NR33:
            return 0xFF;
        case REG_NR34:
            return mCf.ch3.lengthEnabled() ? 0xFF : 0xBF;

        // ===== CH4 =====

        case REG_NR41:
            return 0xFF;
        case REG_NR42:
            return mCf.ch4.readEnvelope();
        case REG_NR43:
            return mCf.ch4.readNoise();
        case REG_NR44:
            return mCf.ch4.lengthEnabled() ? 0xFF : 0xBF;

       // ===== Sound control ======

        case REG_NR50:
            // Not implemented: Vin, always read back as 0
            return ((mLeftVolume - 1) << 4) | (mRightVolume - 1);
        case REG_NR51:
            return mOutputStat;
        case REG_NR52:
        {
            uint8_t nr52 = mEnabled ? 0xF0 : 0x70;
            if (mCf.ch1.dacOn()) {
                nr52 |= 0x1;
            }
            if (mCf.ch2.dacOn()) {
                nr52 |= 0x2;
            }
            if (mCf.ch3.dacOn()) {
                nr52 |= 0x4;
            }
            if (mCf.ch4.dacOn()) {
                nr52 |= 0x8;
            }
            return nr52;
        }

        case REG_WAVERAM:
        case REG_WAVERAM + 1:
        case REG_WAVERAM + 2:
        case REG_WAVERAM + 3:
        case REG_WAVERAM + 4:
        case REG_WAVERAM + 5:
        case REG_WAVERAM + 6:
        case REG_WAVERAM + 7:
        case REG_WAVERAM + 8:
        case REG_WAVERAM + 9:
        case REG_WAVERAM + 10:
        case REG_WAVERAM + 11:
        case REG_WAVERAM + 12:
        case REG_WAVERAM + 13:
        case REG_WAVERAM + 14:
        case REG_WAVERAM + 15:
            if (mModel == Model::cgb || mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                return waveram[reg - REG_WAVERAM];
            }
            return 0xFF;
        default:
            return 0xFF;
    }
}

void Apu::writeRegister(uint8_t reg, uint8_t value, uint32_t autostep) {
    step(autostep);

    // TODO: length counters can still be accessed on DMG when powered off
    if (!mEnabled && reg < REG_NR52) {
        // APU is disabled, ignore this write
        return;
    }


    switch (reg) {
        case REG_NR10:
            mCf.ch1.writeSweep(value);
            mRegs.byName.nr10 = value & 0x7F;
            break;
        case REG_NR11:
            mCf.ch1.writeDuty(value >> 6);
            mCf.ch1.writeLengthCounter(value & 0x3F);
            mRegs.byName.nr11 = value;
            break;
        case REG_NR12:
            mCf.ch1.writeEnvelope(value);
            mRegs.byName.nr12 = value;
            break;
        case REG_NR13:
            mCf.ch1.writeFrequencyLsb(value);
            mRegs.byName.nr13 = value;
            break;
        case REG_NR14:
            mCf.ch1.writeFrequencyMsb(value);
            mRegs.byName.nr14 = value & 0xC7;
            break;
        case REG_NR21:
            mCf.ch2.writeDuty(value >> 6);
            mCf.ch2.writeLengthCounter(value & 0x3F);
            mRegs.byName.nr21 = value;
            break;
        case REG_NR22:
            mCf.ch2.writeEnvelope(value);
            mRegs.byName.nr22 = value;
            break;
        case REG_NR23:
            mCf.ch2.writeFrequencyLsb(value);
            mRegs.byName.nr23 = value;
            break;
        case REG_NR24:
            mCf.ch2.writeFrequencyMsb(value);
            mRegs.byName.nr24 = value & 0xC7;
            break;
        case REG_NR30:
            mCf.ch3.setDacEnable(!!(value & 0x80));
            mRegs.byName.nr30 = value & 0x80;
            break;
        case REG_NR31:
            mCf.ch3.writeLengthCounter(value);
            mRegs.byName.nr31 = value;
            break;
        case REG_NR32:
            mCf.ch3.writeVolume(value);
            mRegs.byName.nr32 = value & 0x60;
            break;
        case REG_NR33:
            mCf.ch3.writeFrequencyLsb(value);
            mRegs.byName.nr33 = value;
            break;
        case REG_NR34:
            mCf.ch3.writeFrequencyMsb(value);
            mRegs.byName.nr34 = value & 0xC7;
            break;
        case REG_NR41:
            mCf.ch4.writeLengthCounter(value & 0x3F);
            mRegs.byName.nr41 = value & 0x3F;
            break;
        case REG_NR42:
            mCf.ch4.writeEnvelope(value);
            mRegs.byName.nr42 = value;
            break;
        case REG_NR43:
            mCf.ch4.writeFrequencyLsb(value);
            mRegs.byName.nr43 = value;
            break;
        case REG_NR44:
            mCf.ch4.writeFrequencyMsb(value);
            mRegs.byName.nr44 = value & 0xC0;
            break;
        case REG_NR50:
            // do nothing with the Vin bits
            // Vin will not be emulated since no cartridge in history ever made use of it
            mLeftVolume = ((value >> 4) & 0x7) + 1;
            mRightVolume = (value & 0x7) + 1;
            mRegs.byName.nr50 = value & 0x77;
            break;
        case REG_NR51:
            mOutputStat = value;
            mRegs.byName.nr51 = value;
            break;
        case REG_NR52:
            if (!!(value & 0x80) != mEnabled) {
                
                if (mEnabled) {
                    // shutdown
                    // zero out all registers
                    for (uint8_t i = REG_NR10; i != REG_NR52; ++i) {
                        writeRegister(static_cast<Reg>(i), 0);
                    }
                    mEnabled = false;
                    mRegs.byName.nr52 &= ~0x80;
                } else {
                    // startup
                    mEnabled = true;
                    //mHf.gen1.softReset();
                    //mHf.gen2.softReset();
                    //mHf.gen3.softReset();
                    mSequencer.reset();
                    mRegs.byName.nr52 |= 0x80;
                }
                
            }
            break;
        case REG_WAVERAM:
        case REG_WAVERAM + 1:
        case REG_WAVERAM + 2:
        case REG_WAVERAM + 3:
        case REG_WAVERAM + 4:
        case REG_WAVERAM + 5:
        case REG_WAVERAM + 6:
        case REG_WAVERAM + 7:
        case REG_WAVERAM + 8:
        case REG_WAVERAM + 9:
        case REG_WAVERAM + 10:
        case REG_WAVERAM + 11:
        case REG_WAVERAM + 12:
        case REG_WAVERAM + 13:
        case REG_WAVERAM + 14:
        case REG_WAVERAM + 15:
            // if CH3's DAC is enabled, then the write goes to the current waveposition
            // this can only be done within a few clocks when CH3 accesses waveram, otherwise the write has no effect
            // this behavior was fixed for the CGB, so we can access waveram whenever
            if (mModel == Model::cgb || mCf.ch3.canAccessRam(mCycletime)) {
                auto waveram = mCf.ch3.waveram();
                waveram[reg - REG_WAVERAM] = value;
            }
            // ignore write if enabled
            break;
        default:
            return;
    }
}

void Apu::step(uint32_t cycles) {
    while (cycles) {

        // amplitudes from every channel are summed together at this moment in time
        // each channel ranges from -15 to 15 so the combined sum ranges from -60 to 60
        int16_t ampLeft = 0;
        int16_t ampRight = 0;

        // figure out the largest step we can take without missing any
        // changes in output.
        uint32_t cyclesToStep = std::min(cycles, mSequencer.timer());
        getOutput<0>(ampLeft, ampRight, cyclesToStep);
        getOutput<1>(ampLeft, ampRight, cyclesToStep);
        getOutput<2>(ampLeft, ampRight, cyclesToStep);
        getOutput<3>(ampLeft, ampRight, cyclesToStep);

        // volume scale
        ampLeft *= mLeftVolume;
        ampRight *= mRightVolume;

        // calculate deltas, a nonzero value indicates a transition
        int16_t deltaLeft = ampLeft - mLastAmpLeft;
        int16_t deltaRight = ampRight - mLastAmpRight;

        if (deltaLeft) {
            addDelta(0, deltaLeft, mCycletime);
            mLastAmpLeft = ampLeft;
        }

        if (deltaRight) {
            addDelta(1, deltaRight, mCycletime);
            mLastAmpRight = ampRight;
        }

        // step hardware components
        mSequencer.step(cyclesToStep);
        mCf.ch1.step(mCycletime, cyclesToStep);
        mCf.ch2.step(mCycletime, cyclesToStep);
        mCf.ch3.step(mCycletime, cyclesToStep);
        mCf.ch4.step(mCycletime, cyclesToStep);

        // update cycle counters
        cycles -= cyclesToStep;
        mCycletime += cyclesToStep;
    }
}

void Apu::stepTo(uint32_t time) {
    if (time <= mCycletime) {
        return;
    }

    step(time - mCycletime);
}

void Apu::endFrame() {
    blip_end_frame(mInternal->bbuf[0], mCycletime);
    blip_end_frame(mInternal->bbuf[1], mCycletime);
    mCycletime = 0;
}

template <int ch>
void Apu::getOutput(int16_t &leftamp, int16_t &rightamp, uint32_t &timer) {

    _internal::ChannelBase *baseChannel;
    if constexpr (ch == 0) {
        baseChannel = &mCf.ch1;
    } else if constexpr (ch == 1) {
        baseChannel = &mCf.ch2;
    } else if constexpr (ch == 2) {
        baseChannel = &mCf.ch3;
    } else {
        baseChannel = &mCf.ch4;
    }

    constexpr auto PAN_LEFT = 0x10 << ch;
    constexpr auto PAN_RIGHT = 0x01 << ch;
    constexpr auto PAN_MIDDLE = PAN_LEFT | PAN_RIGHT;

    // channel amplitude is 0 if
    // * the DAC is off
    // * the terminal is disabled for this channel

    if (baseChannel->dacOn()) {
        auto outputStat = mOutputStat & PAN_MIDDLE;
        if (outputStat) {
            int8_t output = baseChannel->output();
            auto volume = baseChannel->volume();

            if constexpr (ch != 2) {
                // NOTE: for envelope channels, output() is either 1 or 0
                // output is the envelope volume when 1, 0 otherwise
                output = -output & volume; // no branching needed >:)
            }

            // DAC conversion (arbitrary units)
            // Input:    0  ...   F
            // Output: -1.0 ... +1.0

            // INPUT  |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
            // OUTPUT | -F -D -B -9 -7 -5 -3 -1 +1 +3 +5 +7 +9 +B +D +F

            //output = (output * 2) - 15;
            // centers the waveform across the zero crossing
            output = (output * 2) - volume;

            if (!!(outputStat & PAN_LEFT)) {
                leftamp += output;
            }
            if (!!(outputStat & PAN_RIGHT)) {
                rightamp += output;
            }
        }
        timer = std::min(timer, baseChannel->timer());
    }
}

// Buffer stuff

size_t Apu::availableSamples() {
    return blip_samples_avail(mInternal->bbuf[0]);
}

size_t Apu::readSamples(int16_t *dest, size_t samples) {
    auto toRead = static_cast<int>(std::min(samples, availableSamples()));
    blip_read_samples(mInternal->bbuf[0], dest, toRead, 1);
    blip_read_samples(mInternal->bbuf[1], dest + 1, toRead, 1);
    return toRead;
}

void Apu::clearSamples() {
    blip_clear(mInternal->bbuf[0]);
    blip_clear(mInternal->bbuf[1]);
}

void Apu::setQuality(bool quality) {
    mIsHighQuality = quality;
}

void Apu::setVolume(float gain) {

    auto limitQ = (int32_t)(gain * (INT16_MAX << 16));

    // max amp on each channel is 15 so max amp is 60
    // 8 master volume levels so 60 * 8 = 480
    mVolumeStep = limitQ / 480;
}

void Apu::setSamplerate(unsigned samplerate) {
    if (mSamplerate != samplerate) {
        mSamplerate = samplerate;
        mResizeRequired = true;
    }
}

void Apu::setBuffersize(size_t samples) {
    if (mBuffersize != samples) {
        mBuffersize = samples;
        mResizeRequired = true;
    }
}

void Apu::resizeBuffer() {

    if (mResizeRequired) {

        for (int i = 0; i != 2; ++i) {
            auto &bbuf = mInternal->bbuf[i];
            blip_delete(bbuf);
            bbuf = blip_new(static_cast<int>(mBuffersize));
            blip_set_rates(bbuf, constants::CLOCK_SPEED<double>, mSamplerate);
        }

        mResizeRequired = false;
    }
}

void Apu::addDelta(int term, int16_t delta, uint32_t clocktime) {
    // multiply delta by volume step and round
    // Q16.16 -> Q16.0
    delta = (int16_t)((delta * mVolumeStep + 0x8000) >> 16);

    if (mIsHighQuality) {
        blip_add_delta(mInternal->bbuf[term], clocktime, delta);
    } else {
        blip_add_delta_fast(mInternal->bbuf[term], clocktime, delta);
    }
}



}