
project(demo CXX)

add_library(wave_writer STATIC "${CMAKE_SOURCE_DIR}/${BLIP_BUF_DIR}/wave_writer.c")
target_include_directories(wave_writer PUBLIC "${CMAKE_SOURCE_DIR}/${BLIP_BUF_DIR}")

add_executable(demo "demo.cpp")
target_link_libraries(demo PRIVATE gbapu wave_writer)

add_executable(random "random.cpp")
target_link_libraries(random PRIVATE gbapu wave_writer)

add_executable(benchmark "benchmark.cpp")
target_link_libraries(benchmark PRIVATE gbapu)
