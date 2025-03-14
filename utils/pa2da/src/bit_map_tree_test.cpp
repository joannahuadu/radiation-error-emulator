#include <iostream>
#include "bit_map_tree.h"
#include "addr_mapper.h"
#include "types.h" // Adjust to include DRAMConfig definition

namespace dram_mapping {

// Provide a simple recursive helper to print the tree
// Note: We make it a friend function or place it inside BitMapTree if needed
static void printNode(const BitMapNode& node, size_t level = 0) {
    // Using the public BitsetWrapper interface:
    for (size_t i = 0; i < node.m_childBitmap.size(); i++) {
        if (node.m_childBitmap.test(i)) {
            std::cout << std::string(level, '-') << "ChildIndex=" << i << "\n";
            auto it = node.m_children.find(static_cast<int>(i));
            if (it != node.m_children.end()) {
                printNode(*(it->second), level + 1);
            }
        }
    }
}

} // namespace dram_mapping

int main() {
    // 1) Create a dummy DRAMConfig
    DRAMConfig config;
    // Populate config with any needed fields if applicable
    
    // 2) Create an AddrMapper object
    dram_mapping::AddrMapper mapper(config);

    // 3) Define some physical addresses to map
    std::vector<Addr_t> physAddrs = {0x1234, 0x4567, 0xABC0, 0x1234ABC, 0x5678FED};

    // 4) Convert these physical addresses to DRAM address vectors
    std::vector<AddrVec_t> dramAddresses;
    for (auto& addr : physAddrs) {
        dramAddresses.push_back(mapper.map_address(addr));
    }

    // 5) Build the BitMapTree from the DRAM address vectors
    dram_mapping::BitMapTree tree;
    tree.buildTree(dramAddresses);

    // 6) Print the tree structure
    std::cout << "Printing BitMapTree...\n";
    // Access root node through a new method or directly if you make it public
    // For demonstration, we'll assume you add a helper method in BitMapTree:
    //     const BitMapNode& getRoot() const;
    // Make sure to add 'friend' for printNode in BitMapNode if necessary
    dram_mapping::printNode(tree.getRoot());

    return 0;
}