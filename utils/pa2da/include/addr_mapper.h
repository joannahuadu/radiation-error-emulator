#pragma once
#include "types.h"

namespace dram_mapping {

class AddrMapper {
private:
    DRAMConfig m_config;
    int m_tx_offset;
    
    // Indices for components in the address vector
    const int m_channel_idx = 0;
    const int m_rank_idx = 1;
    const int m_bankgroup_idx = 2;
    const int m_bank_idx = 3;
    const int m_row_idx = 4;
    const int m_col_idx = 5;
    
    // Helper methods
    void calculate_tx_offset();
    int slice_lower_bits(Addr_t& addr, int bits);
    int get_bit(Addr_t addr, int bit_pos);
    void set_bit(Addr_t& addr, int bit_pos, int value);
    
public:
    AddrMapper(const DRAMConfig& config);
    
    // Core mapping methods
    AddrVec_t map_address(Addr_t physical_addr);
    Addr_t reverse_map(const AddrVec_t& addr_vec);
    
    // Get config for testing
    const DRAMConfig& get_config() const { return m_config; }
};

} // namespace dram_mapping