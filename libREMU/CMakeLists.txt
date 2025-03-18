cmake_minimum_required(VERSION 1.0)
project(error_bitmap_lib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

set(SOURCES
    bitmap_tree.h 
    bitmap_tree.cpp
    mem_utils.h
    mem_utils.cpp
)

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIR})

add_library(REMU_mem SHARED ${SOURCES})

target_link_libraries(REMU_mem yaml-cpp)