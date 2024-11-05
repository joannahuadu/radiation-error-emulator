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

template <typename T>
class ErrorBitmap {
private:
    uintptr_t s_paddr;
    uintptr_t e_paddr;
    uintptr_t page_size;
    // int error_bit_num;
    // int seed;
    Memory<T>* memory;

public:
    ErrorBitmap(uintptr_t start, uintptr_t end, uintptr_t page_size);
    void REMU(const std::string& cfg, const std::string& mapping = "");
    std::vector<uintptr_t> calculateError(int error_bit_num, int seed);
};

template <typename T>
ErrorBitmap<T>::ErrorBitmap(uintptr_t start, uintptr_t end, uintptr_t page_size) : 
    s_paddr(start), e_paddr(end), page_size(page_size), memory(nullptr) {}

template<typename T>
std::vector<uintptr_t> ErrorBitmap<T>::calculateError(int error_bit_num, int seed) {
    vector<uintptr_t> errors; 
    int ro_ref;
    int co_ref;
    mt19937 gen;
    gen.seed(seed);
    uniform_int_distribution<int> dist_ro(memory->s_lvl[int(T::Level::Row)], memory->e_lvl[int(T::Level::Row)]);
    //  Channel, Rank, Bank, Row
    int co_min;
    int co_max;
    if(memory->e_vec[int(T::Level::Channel)] == 0 && memory->e_vec[int(T::Level::Rank)] == 0 && memory->e_vec[int(T::Level::Bank)] == 0 && memory->e_vec[int(T::Level::Row)] == 0){
        assert((memory->s_lvl[int(T::Level::Column)] != memory->e_lvl[int(T::Level::Column)]) && (memory->e_vec[int(T::Level::Column)] != 0) && "Invalid column");
        co_min = memory->s_lvl[int(T::Level::Column)];
        co_max = memory->e_lvl[int(T::Level::Column)];
    }
    else{
        co_min = 0;
        co_max = (1 << memory->e_vec[int(T::Level::Column)])-1;
        memory->s_lvl[int(T::Level::Column)] = co_min;
        memory->e_lvl[int(T::Level::Column)] = co_max;
    }
    // cout << "co_min-co_max: " << co_min << " " <<co_max << endl;
    uniform_int_distribution<int> dist_co(co_min, co_max);
    ro_ref = dist_ro(gen);
    co_ref = dist_co(gen);
    if(memory->s_lvl[int(T::Level::Row)] == memory->e_lvl[int(T::Level::Row)]){
        ro_ref = -1;
    }
    // cout << "ref: " << dec << ro_ref << " " << co_ref << endl;

    try {
        memory->offset(errors, ro_ref, co_ref, error_bit_num, seed);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return errors;
} 

template <typename T>
void ErrorBitmap<T>::REMU(const std::string& cfg, const std::string& mapping) {
    // configuration settings
    Config configs(cfg);
    const std::string& standard = configs["standard"];
    if (standard == "") {
        throw std::invalid_argument("DRAM standard should be specified.");
    }

    // mapping settings
    if (!mapping.empty()) {
        configs.add("mapping", mapping);
    } else {
        throw std::invalid_argument("Mapping is empty!");
    }

    std::ifstream file(configs["mapping"]);
    if (!file.good()) {
        throw std::invalid_argument("Invalid mapping or bad mapping file!");
    }

    std::vector<uintptr_t> errors;
    if (standard == "LPDDR4") {
        LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
        int C = configs.get_channels(), R = configs.get_ranks();
        // Check and Set channel, rank number
        lpddr4->set_channel_number(C);
        lpddr4->set_rank_number(R);
        memory = new Memory<T>(lpddr4, configs);
        memory->getAddressInfo(s_paddr, e_paddr); 
        // for(int i=0; i<static_cast<int>(memory->s_vec.size()); i++){
        //     cout<<"s_vec \t"<<lpddr4->level_str[i]<<": "<<memory->s_vec[i]<<endl;
        // }
        // for(int i=0; i<static_cast<int>(memory->e_vec.size()); i++){
        //     cout<<"e_vec \t"<<lpddr4->level_str[i]<<": "<<memory->e_vec[i]<<endl;
        // }
    }
}
#endif // ERROR_BITMAP_H
