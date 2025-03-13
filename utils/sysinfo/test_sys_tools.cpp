#include <iostream>
#include "sys_tools.h"

int main() {
    try {
        int maxBits = getMaxPhysicalAddressBits();
        std::cout << "MaxPhysicalAddressBits: " << maxBits << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Err: " << e.what() << std::endl;
    }

    try {
        int pagesize = getPageSize();
        std::cout << "PageSize: " << pagesize << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Err: " << e.what() << std::endl;

    }
    return 0;
}
