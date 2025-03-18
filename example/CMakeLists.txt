cmake_minimum_required(VERSION 2.6)

project(example)

add_definitions(-std=c++11)

option(CUDA_USE_STATIC_CUDA_RUNTIME OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

find_package(CUDA REQUIRED)

message(STATUS "Project Source Directory: ${PROJECT_SOURCE_DIR}")
# include and link dirs of cuda and tensorrt, you need adapt them if yours are different
# cuda
include_directories(/usr/local/cuda/include)
link_directories(/usr/local/cuda/lib64)
# tensorrt
include_directories(/usr/include/aarch64-linux-gnu/)
link_directories(/usr/lib/aarch64-linux-gnu/)


set(ERROR_BITMAP_LIB_DIR "../libREMU/build")
find_library(ERROR_BITMAP_LIB REMU_mem PATHS ${ERROR_BITMAP_LIB_DIR} REQUIRED)
message(STATUS "${ERROR_BITMAP_LIB_DIR}")
message(STATUS "${ERROR_BITMAP_LIB}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Ofast -Wfatal-errors -D_MWAITXINTRIN_H_INCLUDED")

find_package(OpenCV)
include_directories(${OpenCV_INCLUDE_DIRS})
message(STATUS "OpenCV found. OpenCV: ${OpenCV_INCLUDE_DIRS}")

add_executable(resnet50 ${PROJECT_SOURCE_DIR}/resnet50.cpp)
target_link_libraries(resnet50 nvinfer)
target_link_libraries(resnet50 cudart)
target_link_libraries(resnet50 ${OpenCV_LIBS})

add_executable(resnet50_inference_error ${PROJECT_SOURCE_DIR}/resnet50_inference_error.cpp)
target_link_libraries(resnet50_inference_error nvinfer)
target_link_libraries(resnet50_inference_error cudart)
target_link_libraries(resnet50_inference_error ${ERROR_BITMAP_LIB})
target_link_libraries(resnet50_inference_error ${OpenCV_LIBS})

add_executable(resnet50_inference_error_paraonly ${PROJECT_SOURCE_DIR}/resnet50_inference_error_paraonly.cpp)
target_link_libraries(resnet50_inference_error_paraonly nvinfer)
target_link_libraries(resnet50_inference_error_paraonly cudart)
target_link_libraries(resnet50_inference_error_paraonly ${ERROR_BITMAP_LIB})
target_link_libraries(resnet50_inference_error_paraonly ${OpenCV_LIBS})

add_executable(resnet50_inference_error_dq ${PROJECT_SOURCE_DIR}/resnet50_inference_error_dq.cpp)
target_link_libraries(resnet50_inference_error_dq nvinfer)
target_link_libraries(resnet50_inference_error_dq cudart)
target_link_libraries(resnet50_inference_error_dq ${ERROR_BITMAP_LIB})
target_link_libraries(resnet50_inference_error_dq ${OpenCV_LIBS})

add_executable(resnet50_inference_error_paraonly_dq ${PROJECT_SOURCE_DIR}/resnet50_inference_error_paraonly_dq.cpp)
target_link_libraries(resnet50_inference_error_paraonly_dq nvinfer)
target_link_libraries(resnet50_inference_error_paraonly_dq cudart)
target_link_libraries(resnet50_inference_error_paraonly_dq ${ERROR_BITMAP_LIB})
target_link_libraries(resnet50_inference_error_paraonly_dq ${OpenCV_LIBS})


add_definitions(-O2 -pthread)

