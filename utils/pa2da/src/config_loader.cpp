#include "config_loader.h"
#include <iostream>
#include <fstream>
#include <cmath>

namespace dram_mapping {

DRAMConfig ConfigLoader::load_config(const std::string& yaml_file) {
    DRAMConfig config;
    
    try {
        // Check if the file exists
        std::ifstream file(yaml_file);
        if (!file.good()) {
            std::cerr << "Config file not found: " << yaml_file << std::endl;
            throw std::runtime_error("Config file not found");
        }
        
        YAML::Node yaml = YAML::LoadFile(yaml_file);
        
        // Load basic configuration
        config.channel_bits = yaml["dram"]["hierarchy"]["channel_bits"].as<int>();
        config.rank_bits = yaml["dram"]["hierarchy"]["rank_bits"].as<int>();
        config.bankgroup_bits = yaml["dram"]["hierarchy"]["bankgroup_bits"].as<int>();
        config.bank_bits = yaml["dram"]["hierarchy"]["bank_bits"].as<int>();
        config.row_bits = yaml["dram"]["hierarchy"]["row_bits"].as<int>();
        config.column_bits = yaml["dram"]["hierarchy"]["column_bits"].as<int>();
        
        config.bus_width = yaml["dram"]["interface"]["bus_width"].as<int>();
        config.prefetch_size = yaml["dram"]["interface"]["prefetch_size"].as<int>();
        
        // NEW: Check for hardware capacity override
        if (yaml["dram"]["hardware_capacity"]) {
            config.hardware_capacity = yaml["dram"]["hardware_capacity"].as<uint64_t>();
        } else {
            // Calculate based on bit fields as default
            int tx_size = config.bus_width / 8 * config.prefetch_size;
            config.hardware_capacity = (1ULL << (config.channel_bits + config.rank_bits + 
                                           config.bankgroup_bits + config.bank_bits +
                                           config.row_bits + config.column_bits)) * tx_size;
        }
        
        // Parse bit mappings
        if (yaml["mapping"]["bit_mapping"]) {
            parse_bit_mapping(yaml["mapping"]["bit_mapping"], config);
        }
        
        return config;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        throw;
    }
}

void ConfigLoader::parse_bit_mapping(const YAML::Node& mapping_node, DRAMConfig& config) {
    // Process each component
    for (const auto& component : {"column", "row", "channel", "rank", "bank", "bankgroup"}) {
        if (mapping_node[component]) {
            const auto& mapping = mapping_node[component];
            
            // Handle direct bit mapping (array of integers)
            if (mapping.IsSequence() && mapping.size() > 0 && mapping[0].IsScalar()) {
                std::vector<BitMapping> bits;
                for (size_t i = 0; i < mapping.size(); i++) {
                    bits.push_back(BitMapping(mapping[i].as<int>()));
                }
                config.bit_mapping[component] = bits;
            } 
            // Handle XOR mappings
            else if (mapping.IsSequence()) {
                std::vector<BitMapping> bits;
                for (size_t i = 0; i < mapping.size(); i++) {
                    if (mapping[i]["xor"]) {
                        std::vector<int> xor_bits;
                        const auto& xor_node = mapping[i]["xor"];
                        for (size_t j = 0; j < xor_node.size(); j++) {
                            xor_bits.push_back(xor_node[j].as<int>());
                        }
                        bits.push_back(BitMapping::makeXOR(xor_bits));
                    }
                }
                config.bit_mapping[component] = bits;
            }
        }
    }
}

} // namespace dram_mapping