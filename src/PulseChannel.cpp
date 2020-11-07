
#include "gbapu.hpp"

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


static constexpr uint32_t DEFAULT_PERIOD = (2048 - 0) * PULSE_MULTIPLIER;

//#define setOutput() mOutput = ((DUTY_MASK >> ((mDuty << 3) + mDutyCounter)) & 1)
#define setOutput() mOutput = (mDutyWaveform >> mDutyCounter) & 1
#define dutyWaveform(duty) ((DUTY_MASK >> (duty << 3)) & 0xFF)

}


namespace gbapu::_internal {


PulseChannel::PulseChannel() noexcept :
    EnvChannelBase(DEFAULT_PERIOD, 64),
    mDuty(3),
    mDutyWaveform(dutyWaveform(3)),
    mDutyCounter(0)
{
}

uint8_t PulseChannel::readDuty() const noexcept {
    return mDuty << 6;
}

void PulseChannel::reset() noexcept {
    EnvChannelBase::reset();
    mFrequency = 0;
    writeDuty(3);
    restart();
}

// restarting the pulse channel resets the duty counter
// this may result in a click sound
void PulseChannel::restart() noexcept {
    EnvChannelBase::restart();
    mDutyCounter = 0;
    setOutput();
}

void PulseChannel::writeDuty(uint8_t duty) noexcept {
    mDuty = duty;
    mDutyWaveform = dutyWaveform(duty);
}

void PulseChannel::stepOscillator(uint32_t timestamp) noexcept {
    (void)timestamp;
    // this implementation uses bit shifting instead of a lookup table
    
    // increment duty counter
    mDutyCounter = (mDutyCounter + 1) & 0x7;
    setOutput();
    

}

void PulseChannel::setPeriod() noexcept {
    mPeriod = (2048 - mFrequency) * PULSE_MULTIPLIER;
}



}
