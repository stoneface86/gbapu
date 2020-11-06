#pragma once

#include <cstddef>
#include <cstdint>

namespace gbapu {

namespace constants {

constexpr uint16_t MAX_FREQUENCY = 2047;

constexpr size_t WAVE_RAMSIZE = 16;

constexpr uint8_t MAX_TERM_VOLUME = 7;

template <typename T>
constexpr T CLOCK_SPEED = T(4194304);

}


}
