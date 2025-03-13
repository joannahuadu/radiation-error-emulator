#pragma once
#include <vector>
#include <map>
#include <cstdint>
#include <string>

namespace dram_mapping {

using Addr_t = uint64_t;
using AddrVec_t = std::vector<int>;

// Simplified structure for bit mapping
struct BitMapping {
    enum class Type { DIRECT, XOR };
    
    Type type;
    std::vector<int> bits;  // Direct bits or bits to XOR
    
    // Constructor for direct mapping
    BitMapping(int bit) : type(Type::DIRECT), bits({bit}) {}
    
    // Constructor for a list of direct bits
    BitMapping(const std::vector<int>& direct_bits) : type(Type::DIRECT), bits(direct_bits) {}
    
    // Constructor for XOR relationships
    static BitMapping makeXOR(const std::vector<int>& xor_bits) {
        BitMapping mapping({});
        mapping.type = Type::XOR;
        mapping.bits = xor_bits;
        return mapping;
    }
};

// Config structure
struct DRAMConfig {
    int channel_bits;
    int rank_bits;
    int bankgroup_bits;
    int bank_bits;
    int row_bits;
    int column_bits;
    int bus_width;
    int prefetch_size;
    
    // NEW: Override capacity with actual hardware value
    uint64_t hardware_capacity = 0; // 0 means use calculated value
    
    // Component to bit mapping
    std::map<std::string, std::vector<BitMapping>> bit_mapping;
};

} // namespace dram_mapping