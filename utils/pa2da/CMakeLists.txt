cmake_minimum_required(VERSION 3.10)
project(dram_mapping_v3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find yaml-cpp package
find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add source files (update the filename here)
set(SOURCES
    src/addr_mapper.cpp
    src/config_loader.cpp
    src/main.cpp
)

# Create executable
add_executable(mapper_test ${SOURCES})
target_link_libraries(mapper_test ${YAML_CPP_LIBRARIES})

# Copy config files to build directory
add_custom_command(TARGET mapper_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/configs
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/configs ${CMAKE_BINARY_DIR}/configs
)