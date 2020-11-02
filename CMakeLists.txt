cmake_minimum_required(VERSION 3.8)

project (
    gbapu 
    LANGUAGES C CXX
)

option(GBAPU_DEMOS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BLIP_BUF_DIR "blip_buf-1.1.0")

include_directories(
    "include"
    ${BLIP_BUF_DIR}
)

add_library(blip_buf STATIC "${BLIP_BUF_DIR}/blip_buf.c")

set(GBAPU_SRC
    "src/Apu.cpp"
    "src/Buffer.cpp"
    "src/Envelope.cpp"
    "src/Generator.cpp"
    "src/LengthCounter.cpp"
    "src/NoiseGen.cpp"
    "src/PulseGen.cpp"
    "src/Sequencer.cpp"
    "src/Sweep.cpp"
    "src/Timer.cpp"
    "src/WaveGen.cpp"
 )

add_library(gbapu STATIC ${GBAPU_SRC})
target_link_libraries(gbapu PRIVATE blip_buf)

if (MSVC)
    target_compile_options(gbapu PRIVATE /wd26812)
endif ()

if (GBAPU_DEMOS)
    add_subdirectory(demo)
endif ()
