cmake_minimum_required(VERSION 3.8)

project (
    gbapu 
    LANGUAGES C CXX
)

set(BLIP_BUF_DIR "blip_buf-1.1.0")

include_directories(
    "include"
    ${BLIP_BUF_DIR}
)

add_library(blip_buf STATIC "${BLIP_BUF_DIR}/blip_buf.c")

set(GBAPU_SRC
    "src/Envelope.cpp"
    "src/Generator.cpp"
    "src/NoiseGen.cpp"
    "src/PulseGen.cpp"
    "src/Sequencer.cpp"
    "src/Sweep.cpp"
    "src/WaveGen.cpp"
)

add_library(gbapu STATIC ${GBAPU_SRC})
target_link_libraries(gbapu PRIVATE blip_buf)
