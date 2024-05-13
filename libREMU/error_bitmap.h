#ifndef ERROR_BITMAP_H
#define ERROR_BITMAP_H

#include "src/Config.h"
#include "src/Memory.h"
#include "src/LPDDR4.h"
#include <vector>
#include <iostream>
#include <ctime>
#include <random>
#include <unistd.h>

using namespace std;
using namespace famulator;

class ErrorBitmap {
private:
    uintptr_t s_paddr;
    uintptr_t e_paddr;
    uintptr_t page_size;
    int error_bit_num;
    int seed;

public:
    ErrorBitmap(uintptr_t start, uintptr_t end, uintptr_t page_size, int error_bit_num, int seed);

    template<typename T>
    std::vector<uintptr_t> calculateError(const Config& configs, T* spec);

    std::vector<uintptr_t> REMU(const std::string& cfg, const std::string& mapping = "");

};

#endif // ERROR_BITMAP_H
