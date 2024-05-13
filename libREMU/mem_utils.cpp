#include "mem_utils.h"
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


bool Pmem::hasP(uintptr_t Paddr) const {
    return Paddr >= s_Paddr && Paddr <= t_Paddr;
}
bool Pmem::hasP_33(uintptr_t Paddr) const {
    return Paddr >= s_Paddr_33 && Paddr <= t_Paddr_33;
}

bool Pmem::hasV(uintptr_t Vaddr) const {
    return Vaddr >= s_Vaddr && Vaddr <= t_Vaddr;
}

Vmem Pmem::PtoV(uintptr_t Paddr) const {
    if (!hasP(Paddr)) {
        std::cerr << "Physical address not in the range of this Pmem." << std::endl;
        return {0, 0, 0};
    }
    uintptr_t vaddr = s_Vaddr + (Paddr - s_Paddr);
    return {vaddr, Paddr, MemUtils::get_33_bit_pa(Paddr)};
}
Vmem Pmem::P_33toV(uintptr_t Paddr) const {
    if (!hasP_33(Paddr)) {
        std::cerr << "Physical address not in the range of this Pmem." << std::endl;
        return {0, 0, 0};
    }
    uintptr_t vaddr = s_Vaddr + (Paddr - s_Paddr_33);
    return {vaddr, MemUtils::get_34_bit_pa(Paddr, s_Paddr_flag_34), Paddr};
}
Vmem Pmem::VtoP(uintptr_t Vaddr) const {
    if (!hasV(Vaddr)) {
        std::cerr << "Virtual address not in the range of this Pmem." << std::endl;
        return {0, 0, 0};
    }
    uintptr_t paddr = s_Paddr + (Vaddr - s_Vaddr);
    return {Vaddr, paddr, MemUtils::get_33_bit_pa(paddr) };
}

std::vector<Vmem> MemUtils::get_error_Va(uintptr_t Vaddr, size_t size, std::ofstream& logfile, int error_bit_num, int flip_bit, 
const std::string& cfg, const std::string& mapping, const std::map<int,int>& errorMap) {
    uintptr_t page_size = sysconf(_SC_PAGE_SIZE);
    std::vector<Pmem> pmems = getPmems(Vaddr, size, page_size);
    // Open the log file for writing
    if (!logfile.is_open()) {
        std::cerr << "Failed to open log file" << std::endl;
        // Return an empty vector since we can't log anything
        return {};
    }
    logfile << "Pmems details:" << std::endl;
    for (const auto& pmem : pmems) {
        logfile << "Start PA: " << std::hex << pmem.s_Paddr
                  << ", End PA: " << std::hex << pmem.t_Paddr
                  << ", Start 33 PA: " << std::hex << pmem.s_Paddr_33
                  << ", End 33 PA: " << std::hex << pmem.t_Paddr_33
                  << ", flag 34: " << std::dec << pmem.s_Paddr_flag_34
                  << ", Size: " << std::dec << pmem.size
                  << ", Start VA: " << std::hex << pmem.s_Vaddr
                  << ", End VA: " << std::hex << pmem.t_Vaddr
                  << ", Bias: " << std::dec << pmem.bias << std::endl;
        break;
    }
    uintptr_t min_Paddr_0=getMinPA_in_pmems(pmems, 0);//0: 1 xxxx xxxx
    uintptr_t max_Paddr_0=getMaxPA_in_pmems(pmems, 0);
    logfile << "min-max: " << std::hex << min_Paddr_0 << " " << max_Paddr_0  <<std::endl;
    std::cout << "min-max: " << std::hex << min_Paddr_0 << " " << max_Paddr_0  <<std::endl;


    uintptr_t min_Paddr_1=get_33_bit_pa(getMinPA_in_pmems(pmems,1));//1: 2 xxxx xxxx
    uintptr_t max_Paddr_1=get_33_bit_pa(getMaxPA_in_pmems(pmems,1));
    logfile << "min-max: " << std::hex << min_Paddr_1 << " " << max_Paddr_1  <<std::endl;
    std::cout << "min-max: " << std::hex << min_Paddr_1 << " " << max_Paddr_1  <<std::endl;
    float portion=(float)(max_Paddr_0-min_Paddr_0)/(float)(max_Paddr_0-min_Paddr_0+max_Paddr_1-min_Paddr_1);
    std::cout<<" 33bit range / 33+34bit range : "<<portion<<std::endl;
    float tot_portion=(float)(size)/(float)(max_Paddr_0-min_Paddr_0+max_Paddr_1-min_Paddr_1);
    std::cout<<" size / 33+34bit range : "<<tot_portion<<std::endl;
    // access to the DRAM simulator
    std::random_device rd;
    std::vector<Vmem> total_Verr;
    int getcnt=0;
    int duplicnt=0;
    std::cout << "errors: ";
    for (const auto& pair : errorMap) { //first error, second count
        int totalcnt=pair.second;
        int bitnum=pair.first;
        std::cout << std::dec <<bitnum << "-" << totalcnt << " ";
        int cnt0=(int)(totalcnt*portion);
        int cnt1=totalcnt-cnt0;
        for(int i=0;i<cnt0;i++){
            while(true){
                int seed = rd();
                ErrorBitmap error_bitmap(min_Paddr_0, max_Paddr_0, page_size, bitnum, seed);
                std::vector<uintptr_t> errors = error_bitmap.REMU(cfg, mapping);    
                // logfile<<std::dec <<getcnt<<"th REMU : ";
                // std::cout<<std::dec <<getcnt<<"th REMU: ";
                // for(auto kk:errors){
                //     logfile<<std::hex<<kk<<" ";
                //     std::cout<<std::hex<<kk<<" ";
                // }

                std::vector<Vmem> Verr;
                Verr=getValidVA_in_pa33s(errors, pmems);  

                // logfile<<"\n "<<std::dec <<Verr.size()<<" valid in "<<std::dec <<errors.size()<<"\n";
                // std::cout<<"\n "<<std::dec <<Verr.size()<<" valid in "<<std::dec <<errors.size()<<"\n";


                if(Verr.size()==errors.size()){
                    bool isDuplicate = false;
                    for (const auto& vmem : Verr) {
                        auto it = std::find_if(total_Verr.begin(), total_Verr.end(),
                                            [&vmem](const Vmem& existing) {
                                                return vmem.vaddr == existing.vaddr;
                                            });
                        if (it != total_Verr.end()) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    if (!isDuplicate) {
                        total_Verr.insert(total_Verr.end(), Verr.begin(), Verr.end());
                        // logfile << std::dec << getcnt << "th 33bit creation succeed: "
                        //         << "get " << std::dec << Verr.size() << " targets in " << std::dec << errors.size() << " PAs" << std::endl;
                        // std::cout << std::dec << getcnt << "th 33bit creation succeed: "
                        //         << "get " << std::dec << Verr.size() << " targets in " << std::dec << errors.size() << " PAs" << std::endl;
                        break;
                    }else duplicnt++;
                    // break;
                }else{
                    getcnt++;
                }     
            }
        }
        for(int i=0;i<cnt1;i++){
            while(true){
                int seed = rd();
                ErrorBitmap error_bitmap(min_Paddr_1, max_Paddr_1, page_size, bitnum, seed);
                std::vector<uintptr_t> errors = error_bitmap.REMU(cfg, mapping);    
                // logfile<<std::dec <<getcnt<<"th REMU : ";
                // std::cout<<std::dec <<getcnt<<"th REMU: ";
                // for(auto kk:errors){
                //     logfile<<std::hex<<kk<<" ";
                //     std::cout<<std::hex<<kk<<" ";
                // }

                std::vector<Vmem> Verr;
                Verr=getValidVA_in_pa34s(errors, pmems);    

                // logfile<<"\n "<<std::dec <<Verr.size()<<" valid in "<<std::dec <<errors.size()<<"\n";
                // std::cout<<"\n "<<std::dec <<Verr.size()<<" valid in "<<std::dec <<errors.size()<<"\n";

                if(Verr.size()==errors.size()){
                    bool isDuplicate = false;
                    for (const auto& vmem : Verr) {
                        auto it = std::find_if(total_Verr.begin(), total_Verr.end(),
                                            [&vmem](const Vmem& existing) {
                                                return vmem.vaddr == existing.vaddr;
                                            });
                        if (it != total_Verr.end()) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    if (!isDuplicate) {
                        total_Verr.insert(total_Verr.end(), Verr.begin(), Verr.end());
                        // std::cout << std::dec << getcnt << "th 34bit creation succeed: "
                        //         << "get " << std::dec << Verr.size() << " targets in " << std::dec << errors.size() << " PAs" << std::endl;
                        break;
                    }else duplicnt++;
                    // break;
                }else{
                    getcnt++;
                }     
            }
        }
    }
    std::cout << std::dec << getcnt <<" tried, "<<std::dec <<duplicnt<<" duplicated, "<<std::dec << total_Verr.size() << " valid\n";

    for(const auto& vmem: total_Verr){
        logfile << "Error PA: " << std::hex << vmem.paddr_33<< ", mapVA: " <<std::hex << vmem.vaddr << std::endl;
        break;
        // std::cout << "Error PA: " << std::hex << vmem.paddr_33<< ", mapVA: " <<std::hex << vmem.vaddr << std::endl;
        // dumpUnknownVA(vmem.vaddr, 16);
    } 


    // flipping the bits
    // logfile << "\n InjectFault details: "<<std::endl;
    for(auto vmem : total_Verr){
        unsigned char* byteAddress = reinterpret_cast<unsigned char*>(vmem.vaddr);
        // std::bitset<8> bitsBefore(*byteAddress);
        *byteAddress ^= 1 << flip_bit; 
        // std::bitset<8> bitsAfter(*byteAddress); 
        // logfile << bitsBefore << " -> " << bitsAfter << std::endl; // Print the binary representation
    }
    // std::cout << std::endl;
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
    // flipping the bits
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

std::vector<Pmem> MemUtils::getPmems(uintptr_t Vaddr, size_t size, uintptr_t page_size) {
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
        currentPaddr = (pfn << 12) | (currentVaddr & (page_size - 1));

        // Check whether it is a new physical block, merge the continuous pages into a block
        if (firstPmem || currentPmem.t_Paddr + 1 != currentPaddr) {
            if (!firstPmem) {
                pmems.push_back(currentPmem);
            }
            currentPmem.s_Paddr = currentPaddr;
            currentPmem.s_Vaddr = currentVaddr;
            currentPmem.bias = currentVaddr - Vaddr;
            firstPmem = false;
        }

        // Update the physical block
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

        // Move to the next virtual page
        if (currentVaddr == Vaddr && firstPageOffset != 0) {
            currentVaddr += (page_size - firstPageOffset);
        } else {
            currentVaddr += page_size;
        }
    }

    if (!firstPmem) {
        pmems.push_back(currentPmem);
    }

    for(auto& pmem: pmems){
        pmem.s_Paddr_33=MemUtils::get_33_bit_pa(pmem.s_Paddr);
        pmem.s_Paddr_flag_34=MemUtils::get_34_bit_flag(pmem.s_Paddr);
        pmem.t_Paddr_33=MemUtils::get_33_bit_pa(pmem.t_Paddr);
        pmem.t_Paddr_flag_34=MemUtils::get_34_bit_flag(pmem.t_Paddr);
    }

    return pmems;
}

//
uintptr_t MemUtils::get_33_bit_pa(uintptr_t Paddr){
    return Paddr & ((1ULL << 33) - 1);
}
int MemUtils::get_34_bit_flag(uintptr_t Paddr){
    return (Paddr >> 33);
}
uintptr_t MemUtils::get_34_bit_pa(uintptr_t Paddr, int flag){
    // if(flag!=0 && flag!=1){
    //     std::cerr << "Error: Bit must be 0 or 1" << std::endl;
    //     return 0;
    // }
    Paddr &= ((1ULL << 33) - 1);
    return Paddr | (static_cast<uintptr_t>(flag) << 33);
}

uintptr_t MemUtils::getMinPA_33_in_pmems(const std::vector<Pmem>& pmems) {
    uintptr_t minAddr = std::numeric_limits<uintptr_t>::max();
    for (const auto& pmem : pmems) {
        if (pmem.s_Paddr_33 < minAddr) {
            minAddr = pmem.s_Paddr_33;
        }
    }
    return minAddr == std::numeric_limits<uintptr_t>::max() ? 0 : minAddr; 
}
uintptr_t MemUtils::getMinPA_in_pmems(const std::vector<Pmem>& pmems, int flag_34) {
    uintptr_t minAddr = std::numeric_limits<uintptr_t>::max();
    for (const auto& pmem : pmems)if(pmem.s_Paddr_flag_34==flag_34) {
        if (pmem.s_Paddr < minAddr) {
            minAddr = pmem.s_Paddr;
        }
    }
    return minAddr == std::numeric_limits<uintptr_t>::max() ? 0 : minAddr; 
}

uintptr_t MemUtils::getMaxPA_33_in_pmems(const std::vector<Pmem>& pmems) {
    uintptr_t maxAddr = 0; 
    for (const auto& pmem : pmems) {
        if (pmem.t_Paddr_33 > maxAddr) {
            maxAddr = pmem.t_Paddr_33;
        }
    }
    return maxAddr;
}
uintptr_t MemUtils::getMaxPA_in_pmems(const std::vector<Pmem>& pmems, int flag_34) {
    uintptr_t maxAddr = 0; 
    for (const auto& pmem : pmems) if(pmem.t_Paddr_flag_34==flag_34){
        if (pmem.t_Paddr > maxAddr) {
            maxAddr = pmem.t_Paddr;
        }
    }
    return maxAddr;
}

Pmem MemUtils::get_block_in_pmems(uintptr_t Vaddr, size_t size, size_t bias) {
    uintptr_t page_size = sysconf(_SC_PAGE_SIZE);
    std::vector<Pmem> pmems = getPmems(Vaddr, size, page_size);
    for (const auto& pmem : pmems)if(pmem.hasV(Vaddr+bias))return pmem;
}


bool MemUtils::check_pa_in_pmems(uintptr_t Paddr, const std::vector<Pmem>& pmems) {
    for (const auto& pmem : pmems)if(pmem.hasP(Paddr))return true;
    return false; 
}


std::vector<Vmem> MemUtils::getValidVA_in_pa33s(const std::vector<uintptr_t>& paddrs, const std::vector<Pmem>& pmems){
    std::vector<Vmem> vmems;
    for (uintptr_t paddr : paddrs) { 
        // std::cout << "error: " <<std::hex<< paddr << " ";
        for (const auto& pmem : pmems) {
            if (pmem.hasP_33(paddr)) { 
                // std::cout << "here!!" << std::endl;
                Vmem vmem = pmem.P_33toV(paddr); 
                if (vmem.vaddr != 0) { 
                    vmems.push_back(vmem); 
                }
                break; 
            }
        }
    }
    return vmems; 
}

std::vector<Vmem> MemUtils::getValidVA_in_pa34s(const std::vector<uintptr_t>& paddrs, const std::vector<Pmem>& pmems){
    std::vector<Vmem> vmems;
    for (uintptr_t paddr : paddrs) { 
        // std::cout << "error: " <<std::hex<< paddr << " ";
        for (const auto& pmem : pmems) {
            if (pmem.hasP_33(paddr)&&pmem.s_Paddr_flag_34) { 
                // std::cout << "here!!" << std::endl;
                Vmem vmem = pmem.P_33toV(paddr); 
                if (vmem.vaddr != 0) { 
                    vmems.push_back(vmem); 
                }
                break; 
            }
        }
    }
    return vmems; 
}

void MemUtils::dumpUnknownVA(const uintptr_t Vaddr,size_t size){
    std::cout << "Interpreting address " << std::hex << Vaddr << " as different types:" << std::endl;
    
    size_t aligns[] = {1, 2, 4, 8};

    for (size_t align : aligns) {
        std::cout << "Alignment: " << align << std::endl;
        for (size_t offset = 0; offset < size; offset += align) {
            uintptr_t addr = Vaddr + offset;
            std::cout << "Offset: " << offset << std::endl;

            if (align >= alignof(int8_t)) {
                const int8_t* srcPtr = reinterpret_cast<const int8_t*>(addr);
                int convertedValue = static_cast<int>(*srcPtr);
                std::cout << "As int8(static_cast(int)): " << convertedValue << std::endl;
                std::vector<int> dstVec(1);
                std::transform(srcPtr, srcPtr + 1, dstVec.begin(), [](int8_t in) { return static_cast<int>(in); });

                std::cout << "As int8(transform(1)): " << dstVec[0] << std::endl;
            }
            if (align >= alignof(int16_t)) {
                std::cout << "As int16: " << *(reinterpret_cast<const int16_t*>(addr)) << std::endl;
            }
            if (align >= alignof(int32_t)) {
                std::cout << "As int32: " << *(reinterpret_cast<const int32_t*>(addr)) << std::endl;
            }
            if (align >= alignof(int64_t)) {
                std::cout << "As int64: " << *(reinterpret_cast<const int64_t*>(addr)) << std::endl;
            }

            if (align >= alignof(float)) {
                std::cout << "As float32 (fp32): " << *(reinterpret_cast<const float*>(addr)) << std::endl;
            }
            if (align >= alignof(double)) {
                std::cout << "As float64 (fp64): " << *(reinterpret_cast<const double*>(addr)) << std::endl;
            }

            if (align >= alignof(char)) {
                std::cout << "As char: " << *(reinterpret_cast<const char*>(addr)) << std::endl;
            }

        }
    }
}
