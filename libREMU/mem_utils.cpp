#include "mem_utils.h"
#include "bitmap_tree.h"
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <fcntl.h>   
#include <cstring>
#include <limits>
#include <bitset>
#include <random>
#include <typeinfo>
#include <cxxabi.h>
#include <cstdint>
#include <memory>
#include <regex>

bool Pmem::hasP(uintptr_t Paddr) const {return Paddr >= s_Paddr && Paddr <= t_Paddr;}

bool Pmem::hasV(uintptr_t Vaddr) const {return Vaddr >= s_Vaddr && Vaddr <= t_Vaddr;}

Vmem Pmem::PtoV(uintptr_t Paddr) const {
    if (!hasP(Paddr)) {
        std::cerr << "Physical address not in the range of this Pmem." << std::endl;
        return {0, 0, 0};
    }
    uintptr_t vaddr = s_Vaddr + (Paddr - s_Paddr);
    return {vaddr, Paddr, 0};
}

Vmem Pmem::VtoP(uintptr_t Vaddr) const {
    if (!hasV(Vaddr)) {
        std::cerr << "Virtual address not in the range of this Pmem." << std::endl;
        return {0, 0};
    }
    uintptr_t paddr = s_Paddr + (Vaddr - s_Vaddr);
    return {Vaddr, paddr};
}

MemUtils::MemUtils(size_t dram_capacity_gb) : DRAM_CAPACITY_GB(dram_capacity_gb) {
    if(!parse_iomem()){
        std::cerr << "Failed to parse iomem" << std::endl;
        throw std::runtime_error("Failed to parse iomem");
    }
}

std::vector<uintptr_t> randomError(int bitnum, int seed, uintptr_t start, uintptr_t end){
    std::vector<uintptr_t> errors;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uintptr_t> dist(start, end);
    while(bitnum--){
        errors.push_back(dist(rng));
    }
    return errors;
}
uintptr_t random_uintptr(int seed, uintptr_t start, uintptr_t end) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uintptr_t> dist(start, end);
    return dist(rng);
}
std::vector<Vmem> MemUtils::get_error_Va_tree(MemUtils* self, uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, const std::string& mapping, const std::map<int,int>& errorMap, double z) {
    uintptr_t page_size = sysconf(_SC_PAGE_SIZE);
    std::vector<Pmem> pmems = getPmems(self, Vaddr, size, page_size);
    
    logfile << "Pmems details:" << std::endl;
    for (const auto& pmem : pmems) {
        logfile << "Start PA: " << std::hex << pmem.s_Paddr
                  << ", End PA: " << std::hex << pmem.t_Paddr
                  << ", Size: " << std::dec << pmem.size
                  << ", Start VA: " << std::hex << pmem.s_Vaddr
                  << ", End VA: " << std::hex << pmem.t_Vaddr
                  << ", Bias: " << std::dec << pmem.bias 
                  << ", Start DA: " << std::hex << pmem.s_Daddr
                  << ", End DA: " << std::hex << pmem.t_Daddr
                  << ", Base: " << std::dec << pmem.base << std::endl;
        break;
    }
    //读配置文件，创建DRAM层级、翻译规则和树
    BitmapTree bt_tree(mapping);
    for(const auto& pmem :pmems){
        bt_tree.addRange(pmem.s_Daddr, pmem.t_Daddr);
        // std::cout << pmem.s_Daddr << "--" << pmem.t_Daddr << "size: " << pmem.size << std::endl;
        // bt_tree.printLeafCounts();
    }
    // bt_tree.printLeafCounts();
    int totalcnt=0;
    for(const auto& pair : errorMap) totalcnt+=pair.second*pair.first;
    std::vector<Vmem> total_Verr;
    total_Verr.reserve(totalcnt);
    for (const auto& pair : errorMap) {
        int num=pair.first;
        int cnt=pair.second;
        int cntz = 0;
        std::vector<uintptr_t> errors;
        // errors.reserve(cnt*num);
        if(z>0 & num>1) {
            cntz = static_cast<int>(std::round(cnt * z));
            std::cout << "cntz: " << cntz << std::endl;
            std::vector<uintptr_t> errors1, errors2;
            if(cntz>0) errors1 = bt_tree.getError(1, cntz, 0.8);
            if((cnt-cntz)>0) errors2 = bt_tree.getError(num, cnt-cntz, 0.8);
            errors.insert(errors.end(), errors1.begin(), errors1.end());
            errors.insert(errors.end(), errors2.begin(), errors2.end());
        }else errors = bt_tree.getError(num, cnt, 0.8);
        // std::cout<<"error daddr: ";
        // for(auto err:errors)std::cout<<std::hex<<err<<" ";
        // std::cout<<std::endl;
        std::vector<Vmem> Verr;
        // Verr.reserve(cnt*num);
        Verr=getValidVA_in_pa(self, errors, pmems, num, cntz);  
        // std::cout<<"error vaddr: ";
        // for(auto err:Verr)std::cout<<std::hex<<err.vaddr<<" "<<std::hex<<err.paddr<<std::endl;
        // std::cout<<std::endl;
        total_Verr.insert(total_Verr.end(), Verr.begin(), Verr.end());
    }
    
    for(const auto& vmem: total_Verr){
        logfile << "Error PA: " << std::hex << vmem.paddr << ", mapVA: " <<std::hex << vmem.vaddr << ", dq: " << std::dec << vmem.z << std::endl;
        break;
    } 

    for(auto vmem : total_Verr){
        unsigned char* byteAddress = reinterpret_cast<unsigned char*>(vmem.vaddr);
        std::bitset<8> bitsBefore(*byteAddress);
        if (vmem.z > 0) {
            unsigned char mask = ((1 << vmem.z) - 1) << (flip_bit - vmem.z + 1);
            *byteAddress ^= mask;
        }else{
            *byteAddress ^= 1 << flip_bit;
        }
        std::bitset<8> bitsAfter(*byteAddress); 
        // logfile << bitsBefore << " -> " << bitsAfter << std::endl; // Print the binary representation
    }
    return total_Verr;
}
//random error
std::vector<uintptr_t> MemUtils::get_random_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit) {
    std::vector<uintptr_t> total_Verr;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uintptr_t> dis(Vaddr, Vaddr + size-1);
    while (total_Verr.size() < static_cast<size_t>(error_bit_num)) {
        uintptr_t vmem = dis(gen);
        if (std::find(total_Verr.begin(), total_Verr.end(), vmem) == total_Verr.end()) {
            total_Verr.push_back(vmem);
        }
    }
    std::cout << std::dec << total_Verr.size() << " valid\n";
    for(const auto& vmem: total_Verr){
        logfile << "Error VA: " <<std::hex << vmem << std::endl;
        break;
        // std::cout << "Error VA: " <<std::hex << vmem << std::endl;
    } 
    for(auto vmem : total_Verr){
        unsigned char* byteAddress = reinterpret_cast<unsigned char*>(vmem);
        // std::bitset<8> bitsBefore(*byteAddress);
        *byteAddress ^= 1 << flip_bit;
        // std::bitset<8> bitsAfter(*byteAddress); 
        // logfile << bitsBefore << " -> " << bitsAfter << std::endl; // Print the binary representation
    }
    // std::cout << std::endl;
    return total_Verr;
}

// transfer byte count to human-readable string
std::string MemUtils::human_readable(size_t bytes) {
    char buffer[64];
    if (bytes >= (1ULL << 30)) {
        double gb = bytes / double(1ULL << 30);
        std::snprintf(buffer, sizeof(buffer), "%.2fG", gb);
    } else if (bytes >= (1ULL << 20)) {
        double mb = bytes / double(1ULL << 20);
        std::snprintf(buffer, sizeof(buffer), "%.2fM", mb);
    } else if (bytes >= (1ULL << 10)) {
        double kb = bytes / double(1ULL << 10);
        std::snprintf(buffer, sizeof(buffer), "%.2fK", kb);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%luB", bytes);
    }
    return std::string(buffer);
}
// write the lookup table to a file
bool MemUtils::write_pd_lut(const std::string &filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open())
        return false;
    ofs << "# pa_start pa_end da_base size" << std::endl;
    for (const auto &seg : MemUtils::pdmapper) {
        uintptr_t seg_size = seg.pa_end - seg.pa_start + 1;
        ofs << "0x" << std::hex << seg.pa_start << " 0x" << seg.pa_end
            << " 0x" << seg.da_base << " " << MemUtils::human_readable(seg_size) << std::dec << std::endl;
    }
    ofs.close();
    return true;
}
bool MemUtils::parse_iomem() {
    std::ifstream iomem("/proc/iomem");
    if (!iomem.is_open()) {
        std::cerr << "[Error] Cannot open /proc/iomem, please run as root." << std::endl;
        return false;
    }
    std::string line;
    std::regex range_regex(R"(^\s*([0-9a-fA-F]+)-([0-9a-fA-F]+)\s*:\s*(.*)$)");
    std::regex keyword_regex(R"(reserved|system ram)", std::regex_constants::icase);
    struct Range {
        uintptr_t start;
        uintptr_t end;
    };
    std::vector<Range> merged_ranges;
    while (std::getline(iomem, line)) {
        std::smatch match;
        if (std::regex_match(line, match, range_regex)) {
            std::string desc = match[3].str();
            if (std::regex_search(desc, keyword_regex)) {
                uintptr_t start = std::stoull(match[1].str(), nullptr, 16);
                uintptr_t end = std::stoull(match[2].str(), nullptr, 16);
                if (!merged_ranges.empty() && start <= merged_ranges.back().end + 1) {
                    merged_ranges.back().end = std::max(merged_ranges.back().end, end);
                } else {
                    merged_ranges.push_back({start, end});
                }
            }
        }
    }
    iomem.close();
    if (merged_ranges.empty() || merged_ranges[0].end <= merged_ranges[0].start) {
        std::cerr << "[Error] Not found any DRAM regions in /proc/iomem, please run as root." << std::endl;
        return false;
    }

    // every segment records the physical address range and the starting address after mapping
    uintptr_t current_da_base = 0;
    uintptr_t total_size = 0;
    uintptr_t seg_size = 0;
    for (const auto &r : merged_ranges) {
        seg_size = r.end - r.start + 1;
        total_size += seg_size;
        Pseg seg;
        seg.pa_start = r.start;
        seg.pa_end = r.end;
        current_da_base += r.start;
        seg.da_base = current_da_base;
        current_da_base -= (r.end+1);
        pdmapper.push_back(seg);
    }

    uintptr_t dram_capacity_bytes = DRAM_CAPACITY_GB;
    dram_capacity_bytes <<= 30; // GB -> bytes

    if (total_size < dram_capacity_bytes) {
        std::cerr << "[Warn] Total size of merged DRAM regions (" << MemUtils::human_readable(total_size)
                  << ") is less than the input DRAM_CAPACITY (" << MemUtils::human_readable(dram_capacity_bytes) << ")." << std::endl;
    }

    if (!write_pd_lut("pd_lut")) {
        std::cerr << "[Error] Failed to write the lookup table to file." << std::endl;
    }
    return true;
}
    
    // P2D: physical address to device address, and pass the base address
uintptr_t MemUtils::P2D(uintptr_t pa, size_t &base) {
    if (pdmapper.empty()) {
        std::cerr << "[Error] No mapping segments found." << std::endl;
        return 0;
    }
    uintptr_t da = 0;
    for (const auto &seg : pdmapper) {
        if (pa >= seg.pa_start && pa <= seg.pa_end) {
            da = pa - seg.da_base;
            base = seg.da_base;
            return da;
        }
    }
    std::cerr << "[Error] Physical address not found in mapping segments." << std::endl;
    return 0;
}

    // D2P: device address to physical address
uintptr_t MemUtils::D2P(uintptr_t da) {
    if (pdmapper.empty()) {
        std::cerr << "[Error] No mapping segments found." << std::endl;
        return 0;
    }
    uintptr_t pa = 0;
    for (const auto &seg : pdmapper) {
        if (da>= seg.pa_start-seg.da_base && da <= seg.pa_end-seg.da_base) {
            pa = da + seg.da_base;
            return pa;
        }
    }
    std::cerr << "[Error] Device address not found in mapping segments." << std::endl;
    return 0;
}

std::vector<Pmem> MemUtils::getPmems(MemUtils* self, uintptr_t Vaddr, size_t size, uintptr_t page_size) {
    // std::cout << "page_size: " << page_size <<std::endl;
    uintptr_t currentVaddr = Vaddr, currentPaddr;
    uintptr_t endVaddr = Vaddr + size;
    std::vector<Pmem> pmems;
    uintptr_t firstPageOffset = currentVaddr % page_size;
    uintptr_t lastPageOffset = (endVaddr % page_size) == 0 ? page_size : endVaddr % page_size;
    Pmem currentPmem;
    bool firstPmem = true;

    pid_t pid = getpid();
    std::string pagemap_path = "/proc/" + std::to_string(pid) + "/pagemap";
    int pagemap_fd = open(pagemap_path.c_str(), O_RDONLY);
    if (pagemap_fd == -1) {
        std::cerr << "Error opening " << pagemap_path << ": " << strerror(errno) << std::endl;
    }
    unsigned long long entry=0, pfn=0;
    size_t offset=0;
    ssize_t read_bytes;

    while (currentVaddr < endVaddr) {
        //  getPhysicalAddress  currentVaddr --> currentPaddr
        offset = (currentVaddr / page_size) * sizeof(unsigned long long);
        read_bytes = pread(pagemap_fd, &entry, sizeof(entry), offset);
        if (read_bytes < 0) {
            std::cerr << "Failed to read pagemap entry: " << strerror(errno) << std::endl; close(pagemap_fd);
        } else if (read_bytes != sizeof(entry)) {
            std::cerr << "Incomplete read from pagemap: expected " << sizeof(entry) << ", got " << read_bytes << std::endl; close(pagemap_fd); 
        }
        if ((entry & (1ULL << 63)) == 0) { std::cerr<< "Page not present in memory." << std::endl;}
        pfn = entry & ((1ULL << 55) - 1);
        assert((1<<12) == page_size);
        currentPaddr = (pfn << 12) | (currentVaddr & (page_size - 1));

        if (firstPmem || currentPmem.t_Paddr + 1 != currentPaddr) {
            if (!firstPmem) {
                pmems.push_back(currentPmem);
            }
            currentPmem.s_Paddr = currentPaddr;
            currentPmem.s_Vaddr = currentVaddr;
            currentPmem.bias = currentVaddr - Vaddr;
            firstPmem = false;
        }

        currentPmem.t_Paddr = currentPaddr + page_size - 1;
        currentPmem.t_Vaddr = currentVaddr + page_size - 1;
        currentPmem.size = currentPmem.t_Vaddr - currentPmem.s_Vaddr + 1;
        if (currentVaddr == Vaddr) { // fisrt page
            currentPmem.size -= firstPageOffset;
        }
        if (currentVaddr + page_size > endVaddr) { // last page
            currentPmem.size -= (page_size - lastPageOffset);
            currentPmem.t_Vaddr = endVaddr - 1;
            currentPmem.t_Paddr = currentPaddr + lastPageOffset - 1;
        }

        if (currentVaddr == Vaddr && firstPageOffset != 0) {
            currentVaddr += (page_size - firstPageOffset);
        } else {
            currentVaddr += page_size;
        }
    }

    if (!firstPmem) {
        pmems.push_back(currentPmem);
    }

    close(pagemap_fd);
    for(auto &pmem: pmems){
        pmem.s_Daddr = self->P2D(pmem.s_Paddr, pmem.base);
        pmem.t_Daddr = pmem.s_Daddr + pmem.size-1;
    }

    return pmems;
}

uintptr_t MemUtils::getMinDA_in_pmems(const std::vector<Pmem>& pmems) {
    uintptr_t minAddr = std::numeric_limits<uintptr_t>::max();
    for (const auto& pmem : pmems)
        if (pmem.s_Paddr < minAddr){
            minAddr = pmem.s_Paddr-pmem.base;
        }
            
    return minAddr == std::numeric_limits<uintptr_t>::max() ? 0 : minAddr; 
}

uintptr_t MemUtils::getMaxDA_in_pmems(const std::vector<Pmem>& pmems) {
    uintptr_t maxAddr = 0; 
    for (const auto& pmem : pmems){
            if (pmem.t_Paddr > maxAddr) 
                maxAddr = pmem.t_Paddr-pmem.base;
    }
    return maxAddr;
}

Pmem MemUtils::get_block_in_pmems(MemUtils* self, uintptr_t Vaddr, size_t size, size_t bias) {
    uintptr_t page_size = sysconf(_SC_PAGE_SIZE);
    std::vector<Pmem> pmems = getPmems(self, Vaddr, size, page_size);
    for (const auto& pmem : pmems)if(pmem.hasV(Vaddr+bias))return pmem;
    return {};
}

std::vector<Vmem> MemUtils::getValidVA_in_pa(MemUtils* self, const std::vector<uintptr_t>& daddrs, const std::vector<Pmem>& pmems, int num, int cntz){
    std::vector<Vmem> vmems;
    // for (uintptr_t daddr : daddrs) { 
    for (size_t i=0;i<daddrs.size();++i) {
        uintptr_t paddr = self->D2P(daddrs[i]);
        for (const auto& pmem : pmems) {
            if (pmem.hasP(paddr)) { 
                Vmem vmem = pmem.PtoV(paddr); 
                vmem.z = (i < cntz) ? num : 0;
                if (vmem.vaddr != 0) { 
                    vmems.push_back(vmem); 
                }
                break; 
            }
        }
    }
    return vmems; 
}
