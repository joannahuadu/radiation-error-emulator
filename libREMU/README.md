# An emulator of radiation-induced bit-flip errors
We mainly focus on bit-flip errors in space caused by single-event upsets (SEUs) and multiple-cell upsets (MCUs).
- (*mem_utils.h*) We leverage *[/proc/pid/pagemap](https://www.kernel.org/doc/Documentation/vm/pagemap.txt)* interface in the kernel to let a userspace process find out which physical frame each virtual page is mapped to.
- (*error_bitmap.h*) We improve the *[DRAM simulator](https://github.com/CMU-SAFARI/ramulator)* to make bit-flip errors mapping from physical address to DRAM hierarchy.
