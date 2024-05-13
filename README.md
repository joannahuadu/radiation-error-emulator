# radiation-error-emulator
Artifacts of USENIX NSDI 2025 Submission #519: A Case for Application-Aware Space Radiation Tolerance in Orbital Computing

## How to build the emulator?
Generate the dynamic link library *libREMU_mem.so*.
The details of the emulator is in [./libREMU/README.md](./libREMU/README.md).
```
cd libREMU
mkdir build
cd build
cmake ..
make
```

## Plug-and-play with APIs
The use of our emulator is plug-and-play through compiler-based instrumentation linking with the dynamic-link library *libREMU_mem.so*.
In [CMakeLists.txt] (./CMakeLists.txt):
'''
set(ERROR_BITMAP_LIB_DIR "./libREMU/build")
find_library(ERROR_BITMAP_LIB REMU_mem PATHS ${ERROR_BITMAP_LIB_DIR} REQUIRED)
message(STATUS "${ERROR_BITMAP_LIB_DIR}")
message(STATUS "${ERROR_BITMAP_LIB}")
...
target_link_libraries(xxx ${ERROR_BITMAP_LIB})
'''

We expose a rich API for ease of use, and just two lines of code can achieve error injection into the ROI of process space. 

We take the DNN inference program as an example.
