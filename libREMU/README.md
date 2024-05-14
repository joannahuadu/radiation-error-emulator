# An emulator of radiation-induced bit-flip errors
We mainly focus on bit-flip errors in space caused by single-event upsets (SEUs) and multiple-cell upsets (MCUs).

Configure user-defined inputs of our emulator
- (*../example/get_error.py*) We generate our statistic error model (e.g., [error_counts_100.txt](../example/error_counts_100.txt)) based on the ground radiation tests.
- (*./configs/\**) We provide the DRAM configurations for users to customize according to their hardware.
- (*./mappings/\**) We provide the mapping configurations for users to customize according to their DRAM memory.

End-to-end error Mapping
- (*mem_utils.h:getPmems*) We leverage *[/proc/pid/pagemap](https://www.kernel.org/doc/Documentation/vm/pagemap.txt)* interface in the kernel to let a userspace process find out which physical frame each virtual page is mapped to.
- (*error_bitmap.h:REMU*) We improve the *[DRAM simulator](https://github.com/CMU-SAFARI/ramulator)* (*[./src/Memory.h](./src/Memory.h)*) to make bit-flip errors mapping from physical address to DRAM hierarchy.

Note that the hardware platform is supposed to be matched with your DRAM configurations.
Currently *mem_utils.h* is specified to [LPDDR4-config.cfg](./configs/LPDDR4-config.cfg), and the interface for other configurations is to be released.
