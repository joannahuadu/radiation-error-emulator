#ifndef MEM_UTILS_H
#define MEM_UTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include "error_bitmap.h"

struct Vmem {
    uintptr_t vaddr; 
    uintptr_t paddr; 
    uintptr_t paddr_33; 
};

struct Pmem {
    //第34位算标志位，Paddr只保留33位
    uintptr_t s_Paddr;
    size_t s_Paddr_flag_34;
    uintptr_t s_Paddr_33;
    uintptr_t t_Paddr;
    size_t t_Paddr_flag_34;
    uintptr_t t_Paddr_33;
    size_t size;
    uintptr_t s_Vaddr;
    uintptr_t t_Vaddr;
    size_t bias;

    bool hasP(uintptr_t Paddr) const;
    bool hasV(uintptr_t Vaddr) const;
    bool hasP_33(uintptr_t Paddr) const;

    Vmem PtoV(uintptr_t Paddr) const;
    Vmem VtoP(uintptr_t Paddr) const;
    Vmem P_33toV(uintptr_t Paddr) const;
    Vmem VtoP_33(uintptr_t Paddr) const;

    bool checkVP(uintptr_t Vaddr, uintptr_t Paddr) const;
    //读pagemap判断，判断前需先对齐（&低12位0）
};

class MemUtils {
public:
    static std::vector<Vmem> get_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, const std::string& cfg, const std::string& mapping, const std::map<int,int>& errorMap);
    static std::vector<uintptr_t> get_random_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit);
    static Pmem get_block_in_pmems(uintptr_t Vaddr, size_t size, size_t bias);
    static uintptr_t get_33_bit_pa(uintptr_t Paddr);
    static uintptr_t get_34_bit_pa(uintptr_t Paddr, int flag);

private:
    static uintptr_t getPhysicalAddress(uintptr_t vaddr);
    static std::vector<Pmem> getPmems(uintptr_t Vaddr, size_t size, uintptr_t page_size);
    static bool check_pa_in_pmems(uintptr_t Paddr, const std::vector<Pmem>& pmems);
    static std::vector<Vmem> getValidVA_in_pa33s(const std::vector<uintptr_t>& paddrs, const std::vector<Pmem>& pmems);
    static std::vector<Vmem> getValidVA_in_pa34s(const std::vector<uintptr_t>& paddrs, const std::vector<Pmem>& pmems);    
    static uintptr_t getMinPA_33_in_pmems(const std::vector<Pmem>& pmems);
    static uintptr_t getMaxPA_33_in_pmems(const std::vector<Pmem>& pmems);
    static uintptr_t getMinPA_in_pmems(const std::vector<Pmem>& pmems, int flag_34);
    static uintptr_t getMaxPA_in_pmems(const std::vector<Pmem>& pmems, int flag_34);
    static int get_34_bit_flag(uintptr_t Paddr);
    static void dumpUnknownVA(const uintptr_t Vaddr,size_t size);  
    // static uintptr_t page_size;
};

#endif // MEM_UTILS_H
 