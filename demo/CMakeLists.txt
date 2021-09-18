
project(demo CXX)

add_library(wav STATIC "Wav.hpp" "Wav.cpp")

add_executable(demo "demo.cpp")
target_link_libraries(demo PRIVATE gbapu wav)

add_executable(random "random.cpp")
target_link_libraries(random PRIVATE gbapu wav)

add_executable(benchmark "benchmark.cpp")
target_link_libraries(benchmark PRIVATE gbapu)
