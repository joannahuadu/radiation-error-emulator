#ifndef MEM_UTILS_H
#define MEM_UTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include "error_bitmap.h"
#define SHIFTX 33

struct Vmem {
    /**
     * The pair of virtual address and physical address.
    */
    uintptr_t vaddr; /**< The virtual address. */  
    uintptr_t paddr; /**< The physical address. */  
    uintptr_t paddr_X; 
};

struct Pmem {
    /**
     * The pair of a virtual block and a physical block.
    */
    uintptr_t s_Paddr;  /**< The start address of the physical block. */  
    size_t s_Paddr_flag; /**< The flag (SHIFTX+1) of the start address. */  
    uintptr_t s_Paddr_X; /**< The start address of the physical block (SHIFTX+1 is not considered). */  
    uintptr_t t_Paddr; /**< The terminate address of the physical block. */  
    size_t t_Paddr_flag; /**< The flag (SHIFTX+1) of the terminate address. */  
    uintptr_t t_Paddr_X; /**< The terminate address of the physical block (SHIFTX+1 is not considered). */  
    size_t size; /**< The size of the physical/virtual block. */  
    uintptr_t s_Vaddr; /**< The start address of the virtual block. */  
    uintptr_t t_Vaddr; /**< The terminate address of the virtual block. */  
    size_t bias; /**< The bias of the current virtual address in this block. */  
    /**
     * If Paddr is in range (block), return true.
    */
    bool hasP(uintptr_t Paddr) const;
    /**
     * If Vaddr is in range (block), return true.
    */
    bool hasV(uintptr_t Vaddr) const;
    /**
     * If Paddr_X is in range (block), return true.
    */
    bool hasP_X(uintptr_t Paddr) const;
    /**
     * If hasP, translate the coresponding virtual address in the block.
    */
    Vmem PtoV(uintptr_t Paddr) const;
    /**
     * If hasV, translate the coresponding physical address in the block.
    */
    Vmem VtoP(uintptr_t Paddr) const;
    /**
     * If hasP_X, translate the coresponding virtual address in the block.
    */
    Vmem P_XtoV(uintptr_t Paddr) const;
};

class MemUtils {
    /**
     * The utils of injecting radiation-induced bit-flip errors.
    */
public:
    /**
     * Access the DRAM simulator to get the error virtual address and flip (in the global memory area).
    */
    static std::vector<Vmem> get_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, const std::string& cfg, const std::string& mapping, const std::map<int,int>& errorMap);

    static std::vector<uintptr_t> get_random_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit);
    /**
     * Obtain the block at the bias of the ROI (prepare for the sensitive memory area).
    */
    static Pmem get_block_in_pmems(uintptr_t Vaddr, size_t size, size_t bias);
    /**
     * Consider the low 33-bit (SHIFTX=33) of the physical address(8GB) according to the memory configuration.
     * The size of memory (e.g., 8GB, 16GB) should be user-defined (modify SHIFTX as 34 for 16GB).
    */
    static uintptr_t get_addr_bit(uintptr_t Paddr);
    static uintptr_t get_flag_bit(uintptr_t Paddr, int flag);

private:
    /**
     * Get a list of pairs of physical blocks and virual blocks.
    */
    static std::vector<Pmem> getPmems(uintptr_t Vaddr, size_t size, uintptr_t page_size);
    /**
     * Check if Paddr is in the list of physical blocks.
    */
    static bool check_pa_in_pmems(uintptr_t Paddr, const std::vector<Pmem>& pmems);
    /**
     * Verify whether the physical address is legal in ROI.
    */
    static std::vector<Vmem> getValidVA_in_pa(const std::vector<uintptr_t>& paddrs, const std::vector<Pmem>& pmems);
    /**
     * Obtain the minimum address in the list of physical blocks.
    */
    static uintptr_t getMinPA_in_pmems(const std::vector<Pmem>& pmems, int flag);
    /**
     * Obtain the maximum address in the list of physical blocks.
    */
    static uintptr_t getMaxPA_in_pmems(const std::vector<Pmem>& pmems, int flag);
    /**
     * Obtain the 34bit (flag).
    */
    static int get_flag(uintptr_t Paddr);
    // static void dumpUnknownVA(const uintptr_t Vaddr,size_t size);  
};

#endif // MEM_UTILS_H
 
