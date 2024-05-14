# radiation-error-emulator
Artifacts of USENIX NSDI 2025 Submission #519: A Case for Application-Aware Space Radiation Tolerance in Orbital Computing

## How to build the emulator?

1. Prerequisites

   To build our emulator, the following software versions are recommended.
    - GNU Make 4.1
    - CMake 3.10.2
    - C++11
    - g++7.5.0
    
    We support the embedded hardware NVIDIA Jetson Xavier NX with 8GB 128-bit LPDDR4x. Supports for other hardware will be released soon.
    - Machine: aarch64
    - CUDA Arch BIN: 7.2
    - L4T: 32.7.1
    - Jetpack: 4.6.1
    - System: Linux
    - Distribution: Ubuntu 18.04
      
3. We build our emulator as a dynamic link library *libREMU_mem.so*.
The details of our emulator are in [./libREMU/README.md](./libREMU/README.md).
    ```sh
    cd libREMU
    mkdir build
    cd build
    cmake ..
    make
    ```

## Plug-and-play with APIs
The use of our emulator is plug-and-play through compiler-based instrumentation linking with the dynamic-link library *libREMU_mem.so*.
The linking example is in [CMakeLists.txt](./example/CMakeLists.txt):
```cmake
set(ERROR_BITMAP_LIB_DIR "/path/libREMU/build")
find_library(ERROR_BITMAP_LIB REMU_mem PATHS ${ERROR_BITMAP_LIB_DIR} REQUIRED)
message(STATUS "${ERROR_BITMAP_LIB_DIR}")
message(STATUS "${ERROR_BITMAP_LIB}")
...
target_link_libraries(xxx ${ERROR_BITMAP_LIB})
```

We expose a rich API for ease of use, and just two lines of code can achieve error injection into the ROI of process space. 
```cpp
...
#include "mem_utils.h"
MemUtils memUtils;
// Specify the sensitive areas, Vaddr is the virtual address in the process space (i.e., start address of ROI), size is the ROI's size, and bias is the location of the specified area.
Pmem block =  memUtils.get_block_in_pmems(Vaddr, size, bias);
// Get the error virtual addresses and flip the most significant bit in each byte.
memUtils.get_error_Va(block.s_Vaddr, block.size, logfile, bitflip, bitidx, cfg, mapping, errorMap);
...
```
We take the TensorRT DNN inference program as an [example](./example).
- Dataset: Download from [sample.zip](https://drive.google.com/file/d/1QHEVYMOCAgnGyAfnzhESHlEvuuMktfB_/view?usp=drive_link)
- Trained model: Download from [resnet50_resisc45.wts](https://drive.google.com/file/d/1aeASCls2B8Zk1925b7T10Ay89_3of0ku/view?usp=drive_link)
  
1. Prerequisites
    
    To build the example, the following software versions are recommended.
    - GNU Make 4.1
    - CMake 3.10.2
    - C++11
    - g++7.5.0
    - CUDA-10.2.300 + cuDNN-8.2.1
    - OpenCV 4.1.1
    - TensorRT 8.2.1.9
    - Python 3.6.9


2. Build the program linking with *libREMU_mem.so*.
```sh
cd example
mkdir build
cd build
cmake ..
make
```
3. Configure user-defined inputs of our emulator. Change your path.
```cpp
    // e.g., resnet50_inference_error.cpp:199-201
    std::string cfg = "/path/libREMU/configs/LPDDR4-config.cfg";
    std::string mapping = "/path/libREMU/mappings/LPDDR4_row_interleaving_16.map";
    std::string error_file = "/path/example/error_counts_"+std::to_string(bitflip) + ".txt";
```
4. Create the TensorRT inference engine to match your hardware computing capacity.
```sh
./resnet50 -s [.wts] [.engine] 
```
5. Execute the program under the bit-flip errors, we envelope the command in Python scripts.
```sh
python ../run.py
```
 (Require *sudo* permissions, since Linux 4.0 only users with the CAP_SYS_ADMIN capability can get physical frame numbers (PFNs))
