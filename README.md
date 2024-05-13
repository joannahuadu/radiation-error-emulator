# radiation-error-emulator
Artifacts of USENIX NSDI 2025 Submission #519: A Case for Application-Aware Space Radiation Tolerance in Orbital Computing

## How to build the emulator?
Generate the dynamic link library *libREMU_mem.so*.
The details of the emulator are in [./libREMU/README.md](./libREMU/README.md).
```
cd libREMU
mkdir build
cd build
cmake ..
make
```

## Plug-and-play with APIs
The use of our emulator is plug-and-play through compiler-based instrumentation linking with the dynamic-link library *libREMU_mem.so*.
Linking in [CMakeLists.txt] (./CMakeLists.txt):
```
set(ERROR_BITMAP_LIB_DIR "./libREMU/build")
find_library(ERROR_BITMAP_LIB REMU_mem PATHS ${ERROR_BITMAP_LIB_DIR} REQUIRED)
message(STATUS "${ERROR_BITMAP_LIB_DIR}")
message(STATUS "${ERROR_BITMAP_LIB}")
...
target_link_libraries(xxx ${ERROR_BITMAP_LIB})
```

We expose a rich API for ease of use, and just two lines of code can achieve error injection into the ROI of process space. 
```

```
We take the TensorRT DNN inference program as an [example](./example).
1. Build the program linking with *libREMU_mem.so*.
```
cd example
mkdir build
cd build
cmake ..
make
```
3. Create the TensorRT inference engine to match your hardware computing capacity. See details at https://github.com/wang-xinyu/tensorrtx.
```
./resnet50 -s [.wts] [.engine] 
```
4. Execute the program under the bit-flip errors.
```
python ../run.py
```
