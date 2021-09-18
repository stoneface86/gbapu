cmake_minimum_required(VERSION 3.8)

project (
    gbapu 
    LANGUAGES C CXX
)

option(GBAPU_DEMOS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBAPU_SRC
    "src/_internal/Channel.cpp"
    "src/_internal/Mixer.cpp"
    "src/_internal/NoiseChannel.cpp"
    "src/_internal/PulseChannel.cpp"
    "src/_internal/Sequencer.cpp"
    "src/_internal/SweepPulseChannel.cpp"
    "src/_internal/Timer.cpp"
    "src/_internal/WaveChannel.cpp"
    "src/Apu.cpp"
 )

add_library(gbapu STATIC ${GBAPU_SRC})
target_include_directories(gbapu
    PUBLIC "include"
)

if (MSVC)
    target_compile_options(gbapu PRIVATE /wd26812)
endif ()

if (GBAPU_DEMOS)
    add_subdirectory(demo)
endif ()
