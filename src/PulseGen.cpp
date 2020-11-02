
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


static constexpr uint32_t DEFAULT_PERIOD = (2048 - gbapu::Gbs::DEFAULT_FREQUENCY) * PULSE_MULTIPLIER;
static constexpr uint8_t DEFAULT_OUTPUT = (DUTY_MASK >> (gbapu::Gbs::DEFAULT_DUTY << 3)) & 1;

//#define setOutput() mOutput = ((DUTY_MASK >> ((mDuty << 3) + mDutyCounter)) & 1)
#define setOutput() mOutput = (mDutyWaveform >> mDutyCounter) & 1
#define dutyWaveform(duty) ((DUTY_MASK >> (duty << 3)) & 0xFF)

}


namespace gbapu {


PulseGen::PulseGen() noexcept :
    Generator(DEFAULT_PERIOD, DEFAULT_OUTPUT),
    mFrequency(Gbs::DEFAULT_FREQUENCY),
    mDuty(Gbs::DEFAULT_DUTY),
    mDutyWaveform(dutyWaveform(Gbs::DEFAULT_DUTY)),
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
    setFrequency(Gbs::DEFAULT_FREQUENCY);
    setDuty(Gbs::DEFAULT_DUTY);
    restart();
}

// restarting the pulse channel resets the duty counter
// this may result in a click sound
void PulseGen::restart() noexcept {
    Generator::restart();
    mDutyCounter = 0;
    setOutput();
}

void PulseGen::setDuty(Gbs::Duty duty) noexcept {
    mDuty = duty;
    mDutyWaveform = dutyWaveform(duty);
}

void PulseGen::setFrequency(uint16_t frequency) noexcept {
    frequency &= Gbs::MAX_FREQUENCY;
    mFrequency = frequency;
    mPeriod = (2048 - mFrequency) * PULSE_MULTIPLIER;
}

void PulseGen::step(uint32_t cycles) noexcept {
    // this implementation uses bit shifting instead of a lookup table

    if (stepTimer(cycles)) {
        // increment duty counter
        mDutyCounter = (mDutyCounter + 1) & 0x7;
        setOutput();
    }

}



}
