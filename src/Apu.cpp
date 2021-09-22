
#include "gbapu.hpp"

#include <algorithm>

namespace gbapu {

Apu::Apu(unsigned samplerate, size_t buffersizeInSamples) :
    mMixer(),
    mHardware(),
    mCycletime(0),
    mLeftVolume(1),
    mRightVolume(1),
    mEnabled(false),
    mSamplerate(samplerate),
    mBuffersize(buffersizeInSamples)
{
    setVolume(1.0f);
    mMixer.setBuffer(mBuffersize);
    mMixer.setSamplerate(samplerate);
}

void Apu::reset() noexcept {
    mCycletime = 0;
    mMixer.clear();

    mHardware.reset();

    mLeftVolume = 1;
    mRightVolume = 1;
    mEnabled = false;

    updateVolume();
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
            return mHardware.sweep().readRegister();
        case REG_NR11:
            return 0x3F | (mHardware.channel<0>().duty() << 6);
        case REG_NR12:
            return mHardware.envelope<0>().readRegister();
        case REG_NR13:
            return 0xFF;
        case REG_NR14:
            return mHardware.lengthCounter<0>().isEnabled() ? 0xFF : 0xBF;
        
        // ===== CH2 =====

        case REG_NR21:
            return 0x3F | (mHardware.channel<1>().duty() << 6);
        case REG_NR22:
            return mHardware.envelope<1>().readRegister();
        case REG_NR23:
            return 0xFF;
        case REG_NR24:
            return mHardware.lengthCounter<1>().isEnabled() ? 0xFF : 0xBF;

        // ===== CH3 =====

        case REG_NR30:
            return mHardware.channel<2>().isDacOn() ? 0xFF : 0x7F;
        case REG_NR31:
            return 0xFF;
        case REG_NR32:
            return 0x9F | (mHardware.channel<2>().volume() << 5);
        case REG_NR33:
            return 0xFF;
        case REG_NR34:
            return mHardware.lengthCounter<2>().isEnabled() ? 0xFF : 0xBF;

        // ===== CH4 =====

        case REG_NR41:
            return 0xFF;
        case REG_NR42:
            return mHardware.envelope<3>().readRegister();
        case REG_NR43:
            return mHardware.channel<3>().frequency() & 0xFF;
        case REG_NR44:
            return mHardware.lengthCounter<3>().isEnabled() ? 0xFF : 0xBF;

       // ===== Sound control ======

        case REG_NR50:
            // Not implemented: Vin, always read back as 0
            return ((mLeftVolume - 1) << 4) | (mRightVolume - 1);
        case REG_NR51:
            return mNr51;
        case REG_NR52:
        {
            uint8_t nr52 = mEnabled ? 0xF0 : 0x70;
            if (mHardware.channel<0>().isDacOn()) {
                nr52 |= 0x1;
            }
            if (mHardware.channel<1>().isDacOn()) {
                nr52 |= 0x2;
            }
            if (mHardware.channel<2>().isDacOn()) {
                nr52 |= 0x4;
            }
            if (mHardware.channel<3>().isDacOn()) {
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
            if (auto &ch = mHardware.channel<2>(); !ch.isDacOn()) {
                return ch.waveram()[reg - REG_WAVERAM];
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
            mHardware.sweep().writeRegister(value);
            break;
        case REG_NR11:
            mHardware.channel<0>().setDuty(value >> 6);
            mHardware.lengthCounter<0>().setCounter(value & 0x3F);
            break;
        case REG_NR12:
            mHardware.writeEnvelope<0>(value);
            break;
        case REG_NR13:
            mHardware.writeFrequencyLsb<0>(value);
            break;
        case REG_NR14:
            mHardware.writeFrequencyMsb<0>(value);
            break;
        case REG_NR21:
            mHardware.channel<1>().setDuty(value >> 6);
            mHardware.lengthCounter<1>().setCounter(value & 0x3F);
            break;
        case REG_NR22:
            mHardware.writeEnvelope<1>(value);
            break;
        case REG_NR23:
            mHardware.writeFrequencyLsb<1>(value);
            break;
        case REG_NR24:
            mHardware.writeFrequencyMsb<1>(value);
            break;
        case REG_NR30:
            mHardware.channel<2>().setDacEnabled(!!(value & 0x80));
            break;
        case REG_NR31:
            mHardware.lengthCounter<2>().setCounter(value);
            break;
        case REG_NR32:
            mHardware.channel<2>().setVolume((value >> 5) & 0x3);
            break;
        case REG_NR33:
            mHardware.writeFrequencyLsb<2>(value);
            break;
        case REG_NR34:
            mHardware.writeFrequencyMsb<2>(value);
            break;
        case REG_NR41:
            mHardware.lengthCounter<3>().setCounter(value & 0x3F);
            break;
        case REG_NR42:
            mHardware.writeEnvelope<3>(value);
            break;
        case REG_NR43:
            mHardware.writeFrequencyLsb<3>(value);
            break;
        case REG_NR44:
            mHardware.writeFrequencyMsb<3>(value);
            break;
        case REG_NR50: {
            // do nothing with the Vin bits
            // Vin will not be emulated since no cartridge in history ever made use of it
            mLeftVolume = ((value >> 4) & 0x7) + 1;
            mRightVolume = (value & 0x7) + 1;

            // a change in volume will require a transition to the new volume step
            // this transition is done by modifying the DC offset

            auto oldVolumeLeft = mMixer.leftVolume();
            auto oldVolumeRight = mMixer.rightVolume();

            // calculate and set the new volume in the mixer
            updateVolume();

            // volume differentials
            auto leftVolDiff = mMixer.leftVolume() - oldVolumeLeft;
            auto rightVolDiff = mMixer.rightVolume() - oldVolumeRight;

            float dcLeft = 0.0f;
            float dcRight = 0.0f;

            auto const& mix = mHardware.mix();
            for (size_t i = 0; i != mix.size(); ++i) {
                auto mode = mix[i];
                auto output = mHardware.lastOutput(i) - 7.5f;

                if (_internal::modePansLeft(mode)) {
                    dcLeft += leftVolDiff * output;
                }
                if (_internal::modePansRight(mode)) {
                    dcRight += rightVolDiff * output;
                }

            }
            mMixer.mixDc(dcLeft, dcRight, mCycletime);
            break;
        }
        case REG_NR51: {
            mNr51 = value;
            auto panning = value;
            _internal::ChannelMix mix;
            for (size_t i = 0; i != mix.size(); ++i) {
                switch (panning & 0x11) {
                    case 0x00:
                        mix[i] = _internal::MixMode::mute;
                        break;
                    case 0x01:
                        mix[i] = _internal::MixMode::right;
                        break;
                    case 0x10:
                        mix[i] = _internal::MixMode::left;
                        break;
                    case 0x11:
                        mix[i] = _internal::MixMode::middle;
                        break;
                }

                panning >>= 1;
            }
            mHardware.setMix(mix, mMixer, mCycletime);

            break;
        }
        case REG_NR52:
            if (!!(value & 0x80) != mEnabled) {
                
                if (mEnabled) {
                    // shutdown
                    // zero out all registers
                    for (uint8_t i = REG_NR10; i != REG_NR52; ++i) {
                        writeRegister(static_cast<Reg>(i), 0, 0);
                    }
                    mEnabled = false;
                } else {
                    // startup
                    mEnabled = true;
                    //mHf.gen1.softReset();
                    //mHf.gen2.softReset();
                    //mHf.gen3.softReset();
                    //mSequencer.reset();
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
            if (auto &ch = mHardware.channel<2>(); !ch.isDacOn()) {
                ch.waveram()[reg - REG_WAVERAM] = value;
            }
            // ignore write if enabled
            break;
        default:
            return;
    }
}

void Apu::step(uint32_t cycles) {
//    while (cycles) {

//        // step hardware components to the beat of the sequencer's period
//        uint32_t cyclesToStep = std::min(cycles, mSequencer.timer());
//        mSequencer.step(cyclesToStep);
//        mCf.ch1.step(mMixer, mPannings[0], mCycletime, cyclesToStep);
//        mCf.ch2.step(mMixer, mPannings[1], mCycletime, cyclesToStep);
//        mCf.ch3.step(mMixer, mPannings[2], mCycletime, cyclesToStep);
//        mCf.ch4.step(mMixer, mPannings[3], mCycletime, cyclesToStep);

//        // update cycle counters
//        cycles -= cyclesToStep;
//        mCycletime += cyclesToStep;
//    }
    mHardware.run(mMixer, mCycletime, cycles);
    mCycletime += cycles;
}

void Apu::stepTo(uint32_t time) {
    if (time <= mCycletime) {
        return;
    }

    step(time - mCycletime);
}

void Apu::endFrame() {
    mMixer.endFrame(mCycletime);
    mCycletime = 0;
}

void Apu::updateVolume() {
    // apply global volume settings
    auto leftVol = mLeftVolume * mVolumeStep;
    auto rightVol = mRightVolume * mVolumeStep;
    mMixer.setVolume(leftVol, rightVol);

}

// Buffer stuff

size_t Apu::availableSamples() {
    return mMixer.availableSamples();
}

size_t Apu::readSamples(float *dest, size_t samples) {
    return mMixer.readSamples(dest, samples);
}

void Apu::clearSamples() {
    mMixer.clear();
}

void Apu::setVolume(float gain) {

    // max amp on each channel is 15 so max amp is 60
    // 8 master volume levels so 60 * 8 = 480
    mVolumeStep = gain / 480;
    updateVolume();

}

void Apu::setSamplerate(unsigned samplerate) {
    if (mSamplerate != samplerate) {
        mSamplerate = samplerate;
        mMixer.setSamplerate(samplerate);
    }
}

void Apu::setBuffersize(size_t samples) {
    if (mBuffersize != samples) {
        mBuffersize = samples;
        mMixer.setBuffer(samples);
    }
}


}
