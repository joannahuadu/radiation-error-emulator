# An emulator of radiation-induced bit-flip errors
We mainly focus on bit-flip errors in space caused by single-event upsets (SEUs) and multiple-cell upsets (MCUs).
- (*../example/get_error.py*) We provide our statistic error model based on the ground radiation tests.
- (*./configs/\**) We provide the DRAM configurations for users to customize according to their hardware.
- (*./mappings/\**) We provide the mapping configurations for users to customize according to their DRAM memory.
- (*mem_utils.h*) We leverage *[/proc/pid/pagemap](https://www.kernel.org/doc/Documentation/vm/pagemap.txt)* interface in the kernel to let a userspace process find out which physical frame each virtual page is mapped to.
- (*error_bitmap.h*) We improve the *[DRAM simulator](https://github.com/CMU-SAFARI/ramulator)* to make bit-flip errors mapping from physical address to DRAM hierarchy.

Note that the hardware platform is supposed to be matched with the memory configurations.
Currently *mem_utils.h* is specified to [LPDDR4-config.cfg](./configs/LPDDR4-config.cfg), and the interface for other configurations is to be released.
