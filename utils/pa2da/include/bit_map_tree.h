#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include "addr_mapper.h"
#include "bit_tools.h"

namespace dram_mapping {

/// A simple node in the bitmap tree
class BitMapNode {
    // Make printNode a friend if you want to access private members directly
    friend void printNode(const BitMapNode&, size_t);

private:
    // Use BitsetWrapper<10> for 1024 bits
    BitsetWrapper<10> m_childBitmap;
    std::unordered_map<int, std::unique_ptr<BitMapNode>> m_children;

public:
    // Insert a single address vector into the tree, starting from this node
    void insertAddress(const AddrVec_t& addrVec, size_t level = 0) {
        if (level >= addrVec.size()) {
            return;
        }
        int key = addrVec[level];
        m_childBitmap.set(key); // Use BitsetWrapper
        if (m_children.find(key) == m_children.end()) {
            m_children[key] = std::make_unique<BitMapNode>();
        }
        m_children[key]->insertAddress(addrVec, level + 1);
    }

    // Check if a given path exists in this tree
    bool exists(const AddrVec_t& addrVec, size_t level = 0) const {
        if (level >= addrVec.size()) {
            return true;
        }
        int key = addrVec[level];
        if (!m_childBitmap.test(key)) {
            return false;
        }
        auto it = m_children.find(key);
        if (it == m_children.end()) {
            return false;
        }
        return it->second->exists(addrVec, level + 1);
    }
};

/// A helper function to print the tree
static void printNode(const BitMapNode& node, size_t level = 0) {
    for (size_t i = 0; i < node.m_childBitmap.size(); i++) {
        if (node.m_childBitmap.test(i)) {
            // Indent with '-' characters
            for (size_t j = 0; j < level; j++) {
                std::cout << "-";
            }
            std::cout << " ChildIndex=" << i << std::endl;
            auto it = node.m_children.find(static_cast<int>(i));
            if (it != node.m_children.end()) {
                printNode(*(it->second), level + 1);
            }
        }
    }
}

/// A helper class to wrap the root node and provide a build/setup function
class BitMapTree {
private:
    BitMapNode m_root;

public:
    // Build the tree from a list of address vectors
    void buildTree(const std::vector<AddrVec_t>& addressList) {
        for (auto& addr : addressList) {
            m_root.insertAddress(addr);
        }
    }

    // Check if an address vector exists in this tree
    bool addressExists(const AddrVec_t& addrVec) const {
        return m_root.exists(addrVec);
    }

    // Provide a way to print the root
    void printTree() const {
        std::cout << "Bitmap Tree Structure:\n";
        printNode(m_root, 0);
    }
};

} // namespace dram_mapping