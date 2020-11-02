
project(demo CXX)

add_executable(demo "demo.cpp")
target_link_libraries(demo PRIVATE gbapu)
