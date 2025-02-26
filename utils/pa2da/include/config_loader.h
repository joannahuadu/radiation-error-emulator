#pragma once
#include "types.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <memory>

namespace dram_mapping {

class ConfigLoader {
public:
    static DRAMConfig load_config(const std::string& yaml_file);
    
private:
    static void parse_bit_mapping(const YAML::Node& mapping_node, DRAMConfig& config);
};

} // namespace dram_mapping