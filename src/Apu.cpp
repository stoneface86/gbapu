
#include "gbapu.hpp"

#include "_internal/BlipBuf.hpp"

#include <algorithm>

namespace gbapu {

Apu::Apu(unsigned samplerate, size_t buffersizeInSamples, Quality quality, Model model) :
    mModel(model),
    mQuality(quality),
    mBlip(new _internal::BlipBuf()),
    mMixer1(*mBlip.get()),
    mMixer2(*mBlip.get()),
    mMixer3(*mBlip.get()),
    mMixer4(*mBlip.get()),
    mCf(),
    mSequencer(mCf),
    mRegs{ 0 },
    mCycletime(0),
    mLeftVolume(1),
    mRightVolume(1),
    mEnabled(false),
    mIsHighQuality(true),
    mSamplerate(samplerate),
    mBuffersize(buffersizeInSamples),
    mResizeRequired(true)
{
    resizeBuffer();
    setVolume(1.0f);

    updateQuality();
}

Apu::~Apu() = default;

Registers const& Apu::registers() const {
    return mRegs;
}

void Apu::reset() noexcept {
    endFrame();

    mMixer1.reset();
    mMixer2.reset();
    mMixer3.reset();
    mMixer4.reset();

    mSequencer.reset();
    mCf.ch1.reset();
    mCf.ch2.reset();
    mCf.ch3.reset();
    mCf.ch4.reset();
    mLeftVolume = 1;
    mRightVolume = 1;
    mEnabled = false;

    updateVolume();
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
            return mRegs.byName.nr51;
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
            updateVolume();
            mRegs.byName.nr50 = value & 0x77;
            break;
        case REG_NR51:
            mMixer1.setPanning(value & 0x11, mCycletime);
            mMixer2.setPanning((value >> 1) & 0x11, mCycletime);
            mMixer3.setPanning((value >> 2) & 0x11, mCycletime);
            mMixer4.setPanning((value >> 3) & 0x11, mCycletime);

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

        // step hardware components to the beat of the sequencer's period
        uint32_t cyclesToStep = std::min(cycles, mSequencer.timer());
        mSequencer.step(cyclesToStep);
        mCf.ch1.step(mMixer1, mCycletime, cyclesToStep);
        mCf.ch2.step(mMixer2, mCycletime, cyclesToStep);
        mCf.ch3.step(mMixer3, mCycletime, cyclesToStep);
        mCf.ch4.step(mMixer4, mCycletime, cyclesToStep);

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
    blip_end_frame(mBlip->bbuf[0], mCycletime);
    blip_end_frame(mBlip->bbuf[1], mCycletime);
    mCycletime = 0;
}

void Apu::updateVolume() {
    // apply global volume settings
    auto leftVol = mLeftVolume * mVolumeStep;
    auto rightVol = mRightVolume * mVolumeStep;

    // flat EQ, each channel has the same volume step
    // individual volume steps could be used for channel equalization (ie make CH4 quieter)
    mMixer1.setVolume(leftVol, rightVol, mCycletime);
    mMixer2.setVolume(leftVol, rightVol, mCycletime);
    mMixer3.setVolume(leftVol, rightVol, mCycletime);
    mMixer4.setVolume(leftVol, rightVol, mCycletime);
}

void Apu::updateQuality() {
    bool high12 = mQuality != Quality::low;
    bool high34 = mQuality == Quality::high;
    mMixer1.setQuality(high12);
    mMixer2.setQuality(high12);
    mMixer3.setQuality(high34);
    mMixer4.setQuality(high34);
}

// Buffer stuff

size_t Apu::availableSamples() {
    return blip_samples_avail(mBlip->bbuf[0]);
}

size_t Apu::readSamples(int16_t *dest, size_t samples) {
    auto toRead = static_cast<int>(std::min(samples, availableSamples()));
    blip_read_samples(mBlip->bbuf[0], dest, toRead, 1);
    blip_read_samples(mBlip->bbuf[1], dest + 1, toRead, 1);
    return toRead;
}

void Apu::clearSamples() {
    blip_clear(mBlip->bbuf[0]);
    blip_clear(mBlip->bbuf[1]);
}

void Apu::setQuality(Quality quality) {
    if (mQuality != quality) {
        mQuality = quality;
        updateQuality();
    }
}

void Apu::setVolume(float gain) {

    auto limitQ = (int32_t)(gain * (INT16_MAX << 16));

    // max amp on each channel is 15 so max amp is 60
    // 8 master volume levels so 60 * 8 = 480
    mVolumeStep = limitQ / 480;
    updateVolume();

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
            auto &bbuf = mBlip->bbuf[i];
            blip_delete(bbuf);
            bbuf = blip_new(static_cast<int>(mBuffersize));
            blip_set_rates(bbuf, constants::CLOCK_SPEED<double>, mSamplerate);
        }

        mResizeRequired = false;
    }
}


}
