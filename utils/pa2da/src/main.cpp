#include "addr_mapper.h"
#include "config_loader.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <bitset>

void print_config_details(const dram_mapping::DRAMConfig& config) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "DRAM CONFIGURATION DETAILS:" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    // Print hierarchy details
    std::cout << "Hierarchy:" << std::endl;
    std::cout << "  Channels: " << (1 << config.channel_bits) << " (bits: " << config.channel_bits << ")" << std::endl;
    std::cout << "  Ranks: " << (1 << config.rank_bits) << " (bits: " << config.rank_bits << ")" << std::endl;
    std::cout << "  Bankgroups: " << (1 << config.bankgroup_bits) << " (bits: " << config.bankgroup_bits << ")" << std::endl;
    std::cout << "  Banks per group: " << (1 << config.bank_bits) << " (bits: " << config.bank_bits << ")" << std::endl;
    std::cout << "  Rows: " << (1 << config.row_bits) << " (bits: " << config.row_bits << ")" << std::endl;
    std::cout << "  Columns: " << (1 << config.column_bits) << " (bits: " << config.column_bits << ")" << std::endl;
    
    // Print interface details
    std::cout << "Interface:" << std::endl;
    std::cout << "  Bus width: " << config.bus_width << " bits" << std::endl;
    std::cout << "  Prefetch size: " << config.prefetch_size << "n" << std::endl;
    std::cout << "  Transaction size: " << (config.bus_width / 8 * config.prefetch_size) << " bytes" << std::endl;
    
    // Print address bit analysis
    int total_address_bits = config.channel_bits + config.rank_bits + 
                            config.bankgroup_bits + config.bank_bits +
                            config.row_bits + config.column_bits +
                            (int)log2(config.bus_width / 8 * config.prefetch_size);
    
    // Calculate theoretical capacity
    uint64_t theoretical_capacity = (1ULL << (total_address_bits - (int)log2(config.bus_width / 8 * config.prefetch_size)))
                                   * (config.bus_width / 8 * config.prefetch_size);
    
    // Use hardware capacity if specified
    uint64_t actual_capacity = (config.hardware_capacity > 0) ? 
                               config.hardware_capacity : theoretical_capacity;
    
    std::cout << "Address Bit Analysis:" << std::endl;
    std::cout << "  Total address bits used: " << total_address_bits << std::endl;
    
    // Show both theoretical and actual capacity if they differ
    std::cout << "  Total theoretical capacity: " << theoretical_capacity << " bytes ";
    if (total_address_bits >= 30) {
        std::cout << "(" << (theoretical_capacity >> 30) << " GB)";
    } else if (total_address_bits >= 20) {
        std::cout << "(" << (theoretical_capacity >> 20) << " MB)";
    } else if (total_address_bits >= 10) {
        std::cout << "(" << (theoretical_capacity >> 10) << " KB)";
    }
    std::cout << std::endl;
    
    // Show actual hardware capacity if it differs
    if (config.hardware_capacity > 0 && config.hardware_capacity != theoretical_capacity) {
        std::cout << "  Actual hardware capacity: " << actual_capacity << " bytes ";
        if (actual_capacity >= (1ULL << 30)) {
            std::cout << "(" << (actual_capacity >> 30) << " GB)";
        } else if (actual_capacity >= (1ULL << 20)) {
            std::cout << "(" << (actual_capacity >> 20) << " MB)";
        } else if (actual_capacity >= (1ULL << 10)) {
            std::cout << "(" << (actual_capacity >> 10) << " KB)";
        }
        std::cout << std::endl;
    }
    
    // Print mapping details
    std::cout << "Bit Mapping:" << std::endl;
    for (const auto& [component, mappings] : config.bit_mapping) {
        std::cout << "  " << component << ": ";
        for (size_t i = 0; i < mappings.size(); i++) {
            const auto& bit_map = mappings[i];
            if (bit_map.type == dram_mapping::BitMapping::Type::DIRECT) {
                std::cout << bit_map.bits[0];
            } else {
                std::cout << "XOR(";
                for (size_t j = 0; j < bit_map.bits.size(); j++) {
                    std::cout << bit_map.bits[j];
                    if (j < bit_map.bits.size() - 1) std::cout << ",";
                }
                std::cout << ")";
            }
            if (i < mappings.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "============================================================" << std::endl;
}

void print_address_mapping(const dram_mapping::AddrVec_t& addr_vec) {
    std::cout << "DRAM Address Components:" << std::endl;
    std::cout << "Channel:    " << addr_vec[0] << std::endl;
    std::cout << "Rank:       " << addr_vec[1] << std::endl;
    std::cout << "Bankgroup:  " << addr_vec[2] << std::endl;
    std::cout << "Bank:       " << addr_vec[3] << std::endl;
    std::cout << "Row:        " << addr_vec[4] << std::endl;
    std::cout << "Column:     " << addr_vec[5] << std::endl;
    
    if (addr_vec.size() > 6) {
        std::cout << "TX Bits:    0x" << std::hex << addr_vec[6] << std::dec << std::endl;
    }
}

void run_test_with_config(const std::string& config_file) {
    using namespace dram_mapping;
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "Testing with config: " << config_file << std::endl;
    std::cout << "============================================================" << std::endl;
    
    // Load configuration
    DRAMConfig config = ConfigLoader::load_config(config_file);
    print_config_details(config);
    
    // Create mapper (changed from CustomMapper to AddrMapper)
    AddrMapper mapper(config);
    
    // Test addresses
    std::vector<std::pair<Addr_t, std::string>> test_addresses = {
        {0x0000001033ffffff, "Test 64GB"},
        {0x1C826FFFF, "Test address 1"},
        {0x1885FFB62, "Test address 2"},
        {0x1234567890, "Standard test address"},
        {0xABCDEF0123, "Address with LSB pattern"},
        {0x0000000400, "Small address"},
        {0xFFFFFFFF00, "Large address near max"},
        {0x8000000000, "Address with high bit set"},
        {0x1111111111, "Repeating pattern address"},
        {0x0000000001, "Smallest non-zero address"},
        {0xABCDEF0120, "Address aligned to transaction size"},
        {0xABCDEF0121, "Address with LSB = 1"},
        {0xABCDEF0123, "Address with LSB = 3"},
        {0xFEDCBA9876, "Descending pattern address"},
        // Additional test cases for edge cases
        {0x00000000FF, "Small address with all bits in offset range set"},
        {0xFFFFFFFFFF, "Maximum 40-bit address"},
        {0x12345678FF, "Address with all offset bits set"}
 
    };
    
    int match_count = 0, total_count = test_addresses.size();
    
    // Test each address
    for (const auto& [addr, desc] : test_addresses) {
        std::cout << "\nTesting: " << desc << std::endl;
        std::cout << "Physical Address: 0x" << std::hex << std::setw(10) 
                  << std::setfill('0') << addr << std::dec << std::endl;
        
        AddrVec_t result = mapper.map_address(addr);
        print_address_mapping(result);
        
        Addr_t reversed = mapper.reverse_map(result);
        std::cout << "Reversed Address: 0x" << std::hex << std::setw(10)
                  << std::setfill('0') << reversed << std::dec << std::endl;
        
        bool matches = (addr == reversed);
        if (!matches) {
            std::cout << "WARNING: Address roundtrip mismatch!" << std::endl;
            std::cout << "Difference: 0x" << std::hex 
                      << (addr ^ reversed) << std::dec << std::endl;
        } else {
            match_count++;
        }
        
        std::cout << "------------------" << std::endl;
    }
    
    // Print summary
    std::cout << "\nSummary: " << match_count << "/" << total_count 
              << " addresses matched (" 
              << (match_count * 100 / total_count) << "%)" << std::endl;
}

int main() {
    run_test_with_config("configs/lpddr4_jetson_nx.yaml");
    run_test_with_config("configs/lpddr5_jetson_agx_orin.yaml"); // Added line
    return 0;
}