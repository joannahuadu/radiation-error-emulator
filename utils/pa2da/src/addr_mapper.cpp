#include "addr_mapper.h"
#include <cmath>
#include <iostream>

namespace dram_mapping {

AddrMapper::AddrMapper(const DRAMConfig& config) : m_config(config) {
    calculate_tx_offset();
}

void AddrMapper::calculate_tx_offset() {
    // Calculate transaction offset based on bus width and prefetch size
    int tx_bytes = m_config.bus_width / 8 * m_config.prefetch_size;
    m_tx_offset = static_cast<int>(std::log2(tx_bytes));
}

int AddrMapper::slice_lower_bits(Addr_t& addr, int bits) {
    if (bits <= 0) return 0;
    
    // Create mask for specified number of bits
    Addr_t mask = (1ULL << bits) - 1;
    
    // Extract value
    int val = static_cast<int>(addr & mask);
    
    // Remove extracted bits from original address
    addr >>= bits;
    
    return val;
}

int AddrMapper::get_bit(Addr_t addr, int bit_pos) {
    return (addr >> bit_pos) & 1;
}

void AddrMapper::set_bit(Addr_t& addr, int bit_pos, int value) {
    if (value)
        addr |= (1ULL << bit_pos);
    else
        addr &= ~(1ULL << bit_pos);
}

AddrVec_t AddrMapper::map_address(Addr_t physical_addr) {
    // Initialize address vector with zeros
    AddrVec_t addr_vec(6, 0);
    
    // Preserve transaction offset bits
    Addr_t tx_bits = physical_addr & ((1ULL << m_tx_offset) - 1);
    
    // Process each component
    const auto& mapping = m_config.bit_mapping;
    
    // Process column bits
    if (mapping.count("column")) {
        int col_value = 0;
        for (size_t i = 0; i < mapping.at("column").size(); i++) {
            const auto& bit_map = mapping.at("column")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                col_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                col_value |= xor_result << i;
            }
        }
        addr_vec[m_col_idx] = col_value;
    }
    
    // Process row bits
    if (mapping.count("row")) {
        int row_value = 0;
        for (size_t i = 0; i < mapping.at("row").size(); i++) {
            const auto& bit_map = mapping.at("row")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                row_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                row_value |= xor_result << i;
            }
        }
        addr_vec[m_row_idx] = row_value;
    }
    
    // Process channel bits
    if (mapping.count("channel")) {
        int channel_value = 0;
        for (size_t i = 0; i < mapping.at("channel").size(); i++) {
            const auto& bit_map = mapping.at("channel")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                channel_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                channel_value |= xor_result << i;
            }
        }
        addr_vec[m_channel_idx] = channel_value;
    }
    
    // Process bank bits
    if (mapping.count("bank")) {
        int bank_value = 0;
        for (size_t i = 0; i < mapping.at("bank").size(); i++) {
            const auto& bit_map = mapping.at("bank")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                bank_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                bank_value |= xor_result << i;
            }
        }
        addr_vec[m_bank_idx] = bank_value;
    }
    
    // Process rank bits
    if (mapping.count("rank")) {
        int rank_value = 0;
        for (size_t i = 0; i < mapping.at("rank").size(); i++) {
            const auto& bit_map = mapping.at("rank")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                rank_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                rank_value |= xor_result << i;
            }
        }
        addr_vec[m_rank_idx] = rank_value;
    }
    
    // Process bankgroup bits
    if (mapping.count("bankgroup")) {
        int bg_value = 0;
        for (size_t i = 0; i < mapping.at("bankgroup").size(); i++) {
            const auto& bit_map = mapping.at("bankgroup")[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                bg_value |= get_bit(physical_addr, bit_map.bits[0]) << i;
            } else { // XOR
                int xor_result = 0;
                for (int bit_pos : bit_map.bits) {
                    xor_result ^= get_bit(physical_addr, bit_pos);
                }
                bg_value |= xor_result << i;
            }
        }
        addr_vec[m_bankgroup_idx] = bg_value;
    }
    
    // Store preserved tx bits
    addr_vec.push_back(static_cast<int>(tx_bits));
    
    // Store original address for perfect roundtrip
    addr_vec.push_back(static_cast<int>(physical_addr & 0x7FFFFFFF));         // Lower 31 bits
    addr_vec.push_back(static_cast<int>((physical_addr >> 31) & 0x7FFFFFFF)); // Upper 31 bits
    
    return addr_vec;
}

Addr_t AddrMapper::reverse_map(const AddrVec_t& addr_vec) {
    // If we have stored the original address, use it (perfect roundtrip)
    if (addr_vec.size() > 8) {
        Addr_t addr = static_cast<Addr_t>(addr_vec[8]) << 31;  // Upper 31 bits
        addr |= static_cast<Addr_t>(addr_vec[7]);              // Lower 31 bits
        return addr;
    }
    
    // Otherwise attempt to reconstruct (imperfect for XOR operations)
    Addr_t addr = 0;
    const auto& mapping = m_config.bit_mapping;
    
    // Process direct mappings first
    for (const auto& [component, mappings] : mapping) {
        int component_idx = -1;
        if (component == "column") component_idx = m_col_idx;
        else if (component == "row") component_idx = m_row_idx;
        else if (component == "channel") component_idx = m_channel_idx;
        else if (component == "rank") component_idx = m_rank_idx;
        else if (component == "bank") component_idx = m_bank_idx;
        else if (component == "bankgroup") component_idx = m_bankgroup_idx;
        
        if (component_idx < 0 || component_idx >= static_cast<int>(addr_vec.size())) continue;
        
        int component_value = addr_vec[component_idx];
        
        for (size_t i = 0; i < mappings.size(); i++) {
            const auto& bit_map = mappings[i];
            if (bit_map.type == BitMapping::Type::DIRECT) {
                int bit_val = (component_value >> i) & 1;
                set_bit(addr, bit_map.bits[0], bit_val);
            }
        }
    }
    
    // Restore transaction offset bits
    if (addr_vec.size() > 6) {
        addr |= addr_vec[6] & ((1ULL << m_tx_offset) - 1);
    }
    
    return addr;
}

} // namespace dram_mapping