#include "bitmap_tree.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

// BankNode 构造函数：预分配 column 节点
BitmapTree::BankNode::BankNode(int idx, int num_columns)
    : index(idx), leaf_count(0)
{
    columns.reserve(num_columns);
    for (int i = 0; i < num_columns; i++) {
        columns.emplace_back(i);
    }
}

// BankGroupNode 构造函数：预分配 bank 节点
BitmapTree::BankGroupNode::BankGroupNode(int idx, int num_banks, int num_columns)
    : index(idx), leaf_count(0)
{
    banks.reserve(num_banks);
    for (int i = 0; i < num_banks; i++) {
        banks.emplace_back(i, num_columns);
    }
}

// rtNode 构造函数
BitmapTree::rtNode::rtNode(int num_banks, int num_bankgroups, int num_columns)
    : leaf_count(0)
{
    bankgroups.reserve(num_bankgroups);
    for (int i = 0; i < num_bankgroups; i++) {
        bankgroups.emplace_back(i, num_banks, num_columns);
    }
}


// 初始化树结构
void BitmapTree::initializeTree() {
    num_bankgroups = 1 << bankgroup_bits; // 2^(bankgroup_bits)
    num_banks = 1 << bank_bits;             // 2^(bank_bits)
    num_columns = 1 << column_bits;         // 2^(column_bits)
    num_rows = 1 << row_bits;
    rt = rtNode(num_banks, num_bankgroups, num_columns);
}

// 从物理地址（已右移 dq 后）中提取某个字段的值，依据 mapping 中给定的 bit 位列表。
// 第 0 个元素为最低位。
int BitmapTree::extractField(uintptr_t addr, const std::vector<int>& bits) {
    int value = 0;
    for (size_t i = 0; i < bits.size(); i++) {
        value |= (((addr >> bits[i]) & 1UL) << i);
    }
    return value;
}

// 构造函数：解析 YAML 文件，读取 dram 层次和映射规则，并初始化树
BitmapTree::BitmapTree(const std::string& mappingFile) {
    try {
        YAML::Node root = YAML::LoadFile(mappingFile);

        // 解析 dram 层次参数，取 bankgroup、bank、column、row 四层
        bankgroup_bits = root["dram"]["hierarchy"]["bankgroup_bits"].as<int>();
        bank_bits = root["dram"]["hierarchy"]["bank_bits"].as<int>();
        column_bits = root["dram"]["hierarchy"]["column_bits"].as<int>();
        row_bits = root["dram"]["hierarchy"]["row_bits"].as<int>();

        // 解析 interface 部分：DQ 值（物理地址先右移 dq 位再映射）
        dq = root["dram"]["interface"]["DQ"].as<int>();

        // 解析映射规则（映射规则中各 key 对应的 vector 应与各层位宽匹配）
        YAML::Node mappingNode = root["mapping"]["bit_mapping"];
        map_column = mappingNode["column"].as<std::vector<int>>();
        map_bankgroup = mappingNode["bankgroup"].as<std::vector<int>>();
        map_bank = mappingNode["bank"].as<std::vector<int>>();
        map_row = mappingNode["row"].as<std::vector<int>>();

        // 初始化树
        initializeTree();
    }
    catch (const std::exception &e) {
        std::cerr << "Error reading YAML file: " << e.what() << std::endl;
        throw;
    }
}

BitmapTree::~BitmapTree() {}

// addRange：对 [s_Daddr, t_Daddr] 范围内的每个物理地址，
// 先右移 dq 位，再依照映射规则提取各层索引，并更新树中对应节点的 bitset 与叶子计数。
void BitmapTree::addRange(uintptr_t s_Daddr, uintptr_t t_Daddr) {
    // 依次处理范围内的每个物理地址
    s_Daddr>>=dq; t_Daddr>>=dq;
    for (uintptr_t shifted = s_Daddr; shifted <= t_Daddr; shifted++) {
        // 提取各层字段的值
        int col_val = extractField(shifted, map_column);
        int bankgroup_val = extractField(shifted, map_bankgroup);
        int bank_val = extractField(shifted, map_bank);
        int row_val = extractField(shifted, map_row);

        // 检查索引是否超出范围
        if (bankgroup_val < 0 || bankgroup_val >= num_bankgroups ||
            bank_val < 0 || bank_val >= num_banks ||
            col_val < 0 || col_val >= num_columns || 
            row_val < 0 || row_val >= num_rows ) {
            std::cerr << "Extracted indices out of range for address: " <<shifted<< std::endl;
            continue;
        }

        // 定位到对应的节点
        BankGroupNode &bgNode = rt.bankgroups[bankgroup_val];
        BankNode &bNode = bgNode.banks[bank_val];
        ColumnNode &colNode = bNode.columns[col_val];

        // 对 bank 节点：如果对应的 column 尚未置 1，则置 1 并更新叶子计数
        if (!bNode.column_bitmap.test(col_val)) {
            bNode.column_bitmap.set(col_val);
        }

        // 对 column 节点：如果对应的 row 尚未置 1，则置 1 并更新叶子计数
        if (!colNode.row_bitmap.test(row_val)) {
            colNode.row_bitmap.set(row_val);
            colNode.leaf_count++;  // 新的 row 更新
            rt.leaf_count++;  
            bNode.leaf_count++;    
            bgNode.leaf_count++; 
        }

    }
}


// 打印仅叶子计数信息的树结构
void BitmapTree::printLeafCounts() const {
    std::cout << "BitmapTree Leaf Counts:" << std::endl;
    std::cout << "Virtual Root [leaf_count: " << rt.leaf_count << "]" << std::endl;
    // 遍历所有 BankGroup
    for (size_t i = 0; i < rt.bankgroups.size(); ++i) {
        const BankGroupNode &bg = rt.bankgroups[i];
        std::cout << "BankGroup " << bg.index << " [leaf_count: " << bg.leaf_count << "]" << std::endl;
        // 遍历 BankGroup 下的所有 Bank
        for (size_t j = 0; j < bg.banks.size(); ++j) {
            const BankNode &b = bg.banks[j];
            std::cout << "  Bank " << b.index << " [leaf_count: " << b.leaf_count << "]" << std::endl;
            // 遍历 Bank 下的所有 Column
            for (size_t k = 0; k < b.columns.size(); ++k) {
                const ColumnNode &col = b.columns[k];
                std::cout << "    Column " << col.index << " [leaf_count: " << col.leaf_count << "]" << std::endl;
            }
        }
    }
}

std::string BitmapTree::bitsetToHex(const std::string &bitstr) {
    int len = bitstr.size();
    int pad = (4 - (len % 4)) % 4;  // 计算需要补齐的 0 个数
    std::string padded(pad, '0');
    padded += bitstr;  // 补齐后，字符串长度必然是4的倍数
    std::string hexStr;
    for (size_t i = 0; i < padded.size(); i += 4) {
        std::string nibble = padded.substr(i, 4);
        int value = 0;
        for (char c : nibble) {
            value = (value << 1) + (c - '0');
        }
        char hexDigit = (value < 10) ? ('0' + value) : ('A' + (value - 10));
        hexStr.push_back(hexDigit);
    }
    // 移除多余的前导0（保留至少一个字符）
    size_t pos = hexStr.find_first_not_of('0');
    if (pos != std::string::npos) {
        hexStr = hexStr.substr(pos);
    } else {
        hexStr = "0";
    }
    return hexStr;
}
std::string BitmapTree::compressZeros(const std::string& str, int threshold = 3) {
    std::ostringstream oss;
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '0') {
            size_t j = i;
            while (j < str.size() && str[j] == '0') {
                ++j;
            }
            size_t count = j - i;
            if (count >= static_cast<size_t>(threshold)) {
                oss << "...";
            } else {
                oss << std::string(count, '0');
            }
            i = j;
        } else {
            oss << str[i];
            ++i;
        }
    }
    return oss.str();
}
void BitmapTree::printDetailed() const {
    std::cout << "BitmapTree Detailed Structure:" << std::endl;
    std::cout << "Virtual Root [leaf_count: " << rt.leaf_count << "]" << std::endl;

    // 遍历所有 BankGroup
    for (size_t i = 0; i < rt.bankgroups.size(); ++i) {
        const BankGroupNode &bg = rt.bankgroups[i];
        std::cout << "BankGroup " << bg.index << " [leaf_count: " << bg.leaf_count << "]" << std::endl;
        // 遍历 BankGroup 下的所有 Bank
        for (size_t j = 0; j < bg.banks.size(); ++j) {
            const BankNode &b = bg.banks[j];
            std::cout << "  Bank " << b.index << " [leaf_count: " << b.leaf_count << "]" << std::endl;
            // 将 Bank 的 column_bitmap 转为 hex 格式并压缩连续的 '0'
            std::string colBitmapStr = bitsetToHex(b.column_bitmap.to_string());
            colBitmapStr = compressZeros(colBitmapStr, 3);
            std::cout << "    Column Bitmap: 0x" << colBitmapStr << std::endl;
            // 遍历 Bank 下的所有 Column
            for (size_t k = 0; k < b.columns.size(); ++k) {
                const ColumnNode &col = b.columns[k];
                std::cout << "    Column " << col.index << " [leaf_count: " << col.leaf_count << "]" << std::endl;
                // 将 Column 的 row_bitmap 转为 hex 格式并压缩连续的 '0'
                std::string rowBitmapStr = bitsetToHex(col.row_bitmap.to_string());
                rowBitmapStr = compressZeros(rowBitmapStr, 3);
                std::cout << "      Row Bitmap: 0x" << rowBitmapStr << std::endl;
            }
        }
    }
}

std::vector<uintptr_t> BitmapTree::getError(const std::map<int,int>& errorMap){
    std::vector<uintptr_t> errors;
    return errors;
}