#include "bitmap_tree.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <random>
#include <unistd.h>
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
        value |= (((addr >> bits[i]) & 1UL) << (bits[i]-bits[bits.size()-1]));
        // std::cout << value << " " << addr << " " << bits[i] << ((addr >> bits[i]) & 1UL) << (bits[i]-bits[bits.size()-1]) << std::endl;
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

/**
float x: the probability of occurring in the same word-line, which means the same row and adjacent column.
float y: the probability of occurring in the same bit-line, which means the same column and adjacent row.
float z: the probability of occurring in the stacking direction, which means the same column, same row and adjacent DQ.
*/
std::vector<uintptr_t> BitmapTree::getError(int num, int cnt, float x, float y, float z){
    std::vector<uintptr_t> errors;
    errors.clear();
    errors.reserve(num*cnt);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uintptr_t> dqDist(0, (1UL << dq) - 1);        

    // 定义逆映射函数：将各层索引转换回物理地址（注意：物理地址在映射前右移过 dq 位）
    auto reverseMapping = [this](int bg, int b, int c, int r, uintptr_t dq_rand) -> uintptr_t {
        uintptr_t addr = 0;
        int field = bg;
        // bankgroup
        for (size_t i = 0; i < map_bankgroup.size(); i++) {
            if (field & (1 << i))
                addr |= (1UL << map_bankgroup[map_bankgroup.size()-1-i]); //fix
        }
        // bank
        field = b;
        for (size_t i = 0; i < map_bank.size(); i++) {
            if (field & (1 << i))
                addr |= (1UL << map_bank[map_bank.size()-1-i]); //fix
        }
        // column
        field = c;
        for (size_t i = 0; i < map_column.size(); i++) {
            if (field & (1 << i))
                addr |= (1UL << map_column[map_column.size()-1-i]); //fix
        }
        // row
        field = r;
        for (size_t i = 0; i < map_row.size(); i++) {
            if (field & (1 << i))
                addr |= (1UL << map_row[map_row.size()-1-i]); //fix
        }
        // 恢复 dq 位
        addr = (addr << dq) | dq_rand;
        return addr;
    };      
    /**
    num==1的情况
    利用各层（rt、bankgroup、bank、column）的 leaf_count 信息，直接做分层随机采样，而不必遍历整个树。
    基本思想是：
    从全局叶子总数 rt.leaf_count 中随机选出一个目标下标。
    依次在 bankgroup、bank、column 这几层中，用当前子树的 leaf_count 来确定下标所在的子节点，然后在最底层（ColumnNode 的 row_bitmap 中）定位到具体的叶子。
    重复采样，直到获得所需的 cnt 个唯一叶子（物理地址）。
    */
    if(num==1){
        int totalLeaves = rt.leaf_count;
        int seuFound = 0;
        while(seuFound < cnt) {
            // 在全局范围内随机选一个目标下标 [0, totalLeaves-1]
            int target = std::uniform_int_distribution<int>(0, totalLeaves - 1)(gen);
            int remaining = target;
            int selected_bg = -1;
            // 根据 bankgroup 的 leaf_count 定位目标 bankgroup
            for (int bg = 0; bg < num_bankgroups; bg++) {
                if (remaining < rt.bankgroups[bg].leaf_count) {
                    selected_bg = bg;
                    break;
                } else {
                    remaining -= rt.bankgroups[bg].leaf_count;
                }
            }
            if(selected_bg < 0) continue;
            
            BankGroupNode &bgNode = rt.bankgroups[selected_bg];
            int selected_bank = -1;
            // 在 bankgroup 内，根据每个 bank 的 leaf_count 选择 bank
            for (int b = 0; b < num_banks; b++) {
                if (remaining < bgNode.banks[b].leaf_count) {
                    selected_bank = b;
                    break;
                } else {
                    remaining -= bgNode.banks[b].leaf_count;
                }
            }
            if(selected_bank < 0) continue;
            
            BankNode &bankNode = bgNode.banks[selected_bank];
            int selected_col = -1;
            // 在 bank 内，根据每个 ColumnNode 的 leaf_count 选择 column
            for (int c = 0; c < num_columns; c++) {
                ColumnNode &colNode = bankNode.columns[c];
                if (colNode.leaf_count > 0) {
                    if (remaining < colNode.leaf_count) {
                        selected_col = c;
                        break;
                    } else {
                        remaining -= colNode.leaf_count;
                    }
                }
            }
            if(selected_col < 0) continue;
            
            ColumnNode &colNode = bankNode.columns[selected_col];
            int selected_row = -1;
            // fix: 让row的选择更随机
            int randStart = std::uniform_int_distribution<int>(0, 131071)(gen);
            if (randStart > 0) selected_row = colNode.row_bitmap._Find_next(randStart);
            if(selected_row==colNode.row_bitmap.size()) selected_row=colNode.row_bitmap._Find_first();
            // std::cout << "["  << seuFound << "]: " << selected_bg << " "<< selected_bank << " " << selected_col << " " << selected_row  << std::endl;
            // 根据选中的 bankgroup、bank、column、row 得到物理地址
            uintptr_t addr = reverseMapping(selected_bg, selected_bank, selected_col, selected_row, dqDist(gen));
            errors.push_back(addr);
            seuFound++;
        }
        return errors;
    }else{
        int totalBankLeaves = 0;
        for (int bg = 0; bg < num_bankgroups; bg++) {
            for (int b = 0; b < num_banks; b++) {
                totalBankLeaves += rt.bankgroups[bg].banks[b].leaf_count;
            }
        }
        int mcuFound = 0;
        int MAX_ATTEMPTS = 128;
        int Attempts = 0;
        while(mcuFound < cnt && Attempts < MAX_ATTEMPTS*cnt){
            Attempts++;
            int target = std::uniform_int_distribution<int>(0, totalBankLeaves - 1)(gen);
            int remaining = target;
            int selected_bg = -1;
            int selected_bank = -1;
            for (int bg = 0; bg < num_bankgroups; bg++) {
                for (int b = 0; b < num_banks; b++) {
                    if (remaining < rt.bankgroups[bg].banks[b].leaf_count) {
                        selected_bg = bg;
                        selected_bank = b;
                        break;
                    } else {
                        remaining -= rt.bankgroups[bg].banks[b].leaf_count;
                    }
                }
                if(selected_bank >= 0) break;
            }
            BankGroupNode &bgNode = rt.bankgroups[selected_bg];
            BankNode &bankNode = bgNode.banks[selected_bank];
            //选定bank

            //ADD: shape of MCU
            std::uniform_real_distribution<double> ran(0, 1);
            int x_num = 1;
            int y_num = 0;
            while(x_num + y_num < num){
                double rand_num = ran(gen); 
                if (rand_num >= 0.2) x_num+=1;
                else y_num+=1;
            }
            // fix: segmetation fault: x_num>0
            std::vector<int> foundColPos(x_num, 0);
            for(int j=0;j<y_num;++j)foundColPos[target % x_num]+=1;
            
            //Start select.
            std::bitset<1024> foundCols = bankNode.column_bitmap;
            std::bitset<131072> foundRows;
            for (int i = 1; i < x_num; i++) {
                foundCols &= (bankNode.column_bitmap << i); //fix: should move left
            }
            if(foundCols.none())continue; //如果没有相邻列，则直接下一个target 
            int randStart = std::uniform_int_distribution<int>(0, 1023)(gen);
            for(int col=foundCols._Find_next(randStart);col!=foundCols.size();col=foundCols._Find_next(col)){
                foundRows.set();
                for(int i=0;i<x_num;i++){
                    foundRows &= (bankNode.columns[i+col].row_bitmap);
                    for(int j=0;j<foundColPos[i];j++){
                        // TODO: some shapes are not included, might move right >>j and selected_row-j
                        foundRows&=(bankNode.columns[i+col].row_bitmap<<j); // fix: should move left
                    }
                }
                // std::cout << foundRows << " " << foundRows.any() << std::endl;
                if(foundRows.any()){ //如果col的相邻列没有相同行，则下一个col
                    int selected_row = -1;
                    randStart = std::uniform_int_distribution<int>(0, 131071)(gen);
                    if (randStart > 0) selected_row = foundRows._Find_next(randStart);
                    if(selected_row==foundRows.size()) selected_row=foundRows._Find_first();
                    int selected_dq=dqDist(gen);
                    for(int i=0;i<x_num;i++){
                        // std::cout << "["  << i << "]: " << selected_bg << " "<< selected_bank << " " << col+i << " " << selected_row << " " << selected_dq << std::endl;
                        uintptr_t addr=reverseMapping(selected_bg, selected_bank, col+i, selected_row, selected_dq);
                        errors.push_back(addr);
                        for(int j=1;j<=foundColPos[i];j++){      
                            // std::cout << "[" << i <<", " << j << "]: " << selected_bg << " "<< selected_bank << " " << col+i << " " << selected_row+j << " " << selected_dq << std::endl;                 
                            uintptr_t addr=reverseMapping(selected_bg, selected_bank, col+i, selected_row+j , selected_dq);
                            errors.push_back(addr);
                        }
                    }
                    mcuFound+=1;
                    break;
                }
            }
        }
    }
    return errors;
}