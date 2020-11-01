
#include "gbapu/PulseGen.hpp"

namespace {

// multiplier for frequency calculation
// 64 Hz - 131.072 KHz
static constexpr unsigned PULSE_MULTIPLIER = 4;

//                    STEP: 76543210
// Bits 24-31 - 75%   Duty: 01111110 (0x7E) _------_
// Bits 16-23 - 50%   Duty: 11100001 (0xE1) -____---
// Bits  8-15 - 25%   Duty: 10000001 (0x81) -______-
// Bits  0-7  - 12.5% Duty: 10000000 (0x80) _______-

static constexpr uint32_t DUTY_MASK = 0x7EE18180;


static constexpr uint32_t DEFAULT_PERIOD = (2048 - trackerboy::Gbs::DEFAULT_FREQUENCY) * PULSE_MULTIPLIER;
static constexpr uint8_t DEFAULT_OUTPUT = (DUTY_MASK >> (trackerboy::Gbs::DEFAULT_DUTY << 3)) & 1;

#define setOutput() mOutput = ((DUTY_MASK >> ((mDuty << 3) + mDutyCounter)) & 1)

}


namespace gbapu {


PulseGen::PulseGen() noexcept :
    Generator(DEFAULT_PERIOD, DEFAULT_OUTPUT),
    mFrequency(Gbs::DEFAULT_FREQUENCY),
    mDuty(Gbs::DEFAULT_DUTY),
    mDutyCounter(0)
{
}

Gbs::Duty PulseGen::duty() const noexcept {
    return mDuty;
}

uint16_t PulseGen::frequency() const noexcept {
    return mFrequency;
}

void PulseGen::reset() noexcept {
    mFrequency = Gbs::DEFAULT_FREQUENCY;
    mDuty = Gbs::DEFAULT_DUTY;
    restart();
}

void PulseGen::restart() noexcept {
    Generator::restart();
    mDutyCounter = 0;
    setOutput();
}

void PulseGen::setDuty(Gbs::Duty duty) noexcept {
    mDuty = duty;
}

void PulseGen::setFrequency(uint16_t frequency) noexcept {
    frequency &= Gbs::MAX_FREQUENCY;
    mFrequency = frequency;
    mPeriod = (2048 - mFrequency) * PULSE_MULTIPLIER;
}

void PulseGen::step(uint32_t cycles) noexcept {
    // this implementation uses bit shifting instead of a lookup table

    // advance the counter
    mFreqCounter += cycles;
    // number of duty steps to cycle through
    uint32_t dutysteps = mFreqCounter / mPeriod;
    mFreqCounter %= mPeriod;
    mDutyCounter = (mDutyCounter + dutysteps) & 0x7; // & 7 == % 8

    // DUTY_MASK contains all duty waveforms
    // first byte is 12.5% second is 25% and so on
    setOutput();
}



}
