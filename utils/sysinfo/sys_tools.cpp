
#include "sys_tools.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <unistd.h>



uint64_t getMaxPhysicalAddressBits() {
    FILE* file = popen("sudo tail -n 1 /sys/kernel/debug/memblock/memory", "r");
    if (!file) {
        throw std::runtime_error("Unable to open memory info file.");
    }
    char line[256];
    if (!fgets(line, sizeof(line), file)) {
        throw std::runtime_error("Failed to read memory info.");
    }
    fclose(file);

    const char* addrStart = strstr(line, "..");
    if (!addrStart) {
        throw std::runtime_error("Invalid memory block format.");
    }

    uint64_t maxAddress;
    sscanf(addrStart + 2, "%lx", &maxAddress);

    return static_cast<uint64_t>(std::log2(maxAddress) + 1);
}

uint64_t getPageSize(){
    uint64_t page_size = sysconf(_SC_PAGE_SIZE);
    return page_size;
}
