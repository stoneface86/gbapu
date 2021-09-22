cmake_minimum_required(VERSION 3.8)

project (
    gbapu 
    LANGUAGES C CXX
)

option(GBAPU_DEMOS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBAPU_SRC
    "src/_internal.cpp"
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
