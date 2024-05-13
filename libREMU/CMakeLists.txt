cmake_minimum_required(VERSION 1.0)
project(error_bitmap_lib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

set(SOURCES
    ./src/Config.h
    ./src/Config.cpp
    ./src/LPDDR4.h
    ./src/LPDDR4.cpp
    ./src/Memory.h
    error_bitmap.h
    error_bitmap.cpp
    mem_utils.h
    mem_utils.cpp
)

add_library(REMU_mem SHARED ${SOURCES})