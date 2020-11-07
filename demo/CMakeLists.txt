
project(demo CXX)

add_library(wave_writer STATIC "${CMAKE_SOURCE_DIR}/${BLIP_BUF_DIR}/wave_writer.c")

add_executable(demo "demo.cpp")
target_link_libraries(demo PRIVATE gbapu wave_writer)
