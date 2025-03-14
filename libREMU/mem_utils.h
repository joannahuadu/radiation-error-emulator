#ifndef MEM_UTILS_H
#define MEM_UTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <cassert>


struct Vmem {
    uintptr_t vaddr; /**< The virtual address. */  
    uintptr_t paddr; /**< The physical address. */  
};

struct Pmem {
    /**
     * The pair of a virtual block and a physical block.
    */
    // Paddr进PV翻译
    uintptr_t s_Paddr; 
    uintptr_t t_Paddr;   
    size_t size; 
    uintptr_t s_Vaddr; 
    uintptr_t t_Vaddr; 
    size_t bias; 

    // Daddr进DRAM翻译
    uintptr_t s_Daddr; 
    uintptr_t t_Daddr;  
    size_t base; 

    //If Paddr is in range (block), return true.
    bool hasP(uintptr_t Paddr) const;

    //If Vaddr is in range (block), return true.
    bool hasV(uintptr_t Vaddr) const;

    //If hasP, translate the coresponding virtual address in the block.
    Vmem PtoV(uintptr_t Paddr) const;

    //If hasV, translate the coresponding physical address in the block.
    Vmem VtoP(uintptr_t Paddr) const;

};

struct Pseg {
    uintptr_t pa_start; 
    uintptr_t pa_end;  
    size_t da_base;     
};

class MemUtils {
    /**
     * The utils of injecting radiation-induced bit-flip errors.
    */
public:
    /**
     * Constructs a MemUtils instance and initializes the DRAM simulator.
    */

    MemUtils(size_t dram_capacity_gb);

    std::vector<Pseg> pdmapper; // mapping segments (physical address range and mapped base address)
    size_t DRAM_CAPACITY_GB;             // DRAM capacity in GB
    
    bool parse_iomem();
    static std::string human_readable(size_t bytes);
    bool write_pd_lut(const std::string &filename);
    uintptr_t P2D(uintptr_t pa, size_t &base);
    uintptr_t D2P(uintptr_t da);

    // static std::vector<Vmem> get_error_Va(MemUtils* self, uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, const std::string& cfg, const std::string& mapping, const std::map<int,int>& errorMap);
    static std::vector<Vmem> get_error_Va_tree(MemUtils* self, uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, const std::string& mapping, const std::map<int,int>& errorMap);

    static std::vector<uintptr_t> get_random_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit);
    /**
     * Obtain the block at the bias of the ROI (prepare for the sensitive memory area).
    */
    static Pmem get_block_in_pmems(MemUtils* self, uintptr_t Vaddr, size_t size, size_t bias);

private:
    
    /**
     * Get a list of pairs of physical blocks and virual blocks.
    */
    static std::vector<Pmem> getPmems(MemUtils* self, uintptr_t Vaddr, size_t size, uintptr_t page_size);

    /**
     * Verify whether the physical address is legal in ROI.
    */
    static std::vector<Vmem> getValidVA_in_pa(MemUtils* self, const std::vector<uintptr_t>& daddrs, const std::vector<Pmem>& pmems);
    /**
     * Obtain the minimum address in the list of physical blocks.
    */
    static uintptr_t getMinDA_in_pmems(const std::vector<Pmem>& pmems);
    /**
     * Obtain the maximum address in the list of physical blocks.
    */
    static uintptr_t getMaxDA_in_pmems(const std::vector<Pmem>& pmems);
};


#endif // MEM_UTILS_H
 
