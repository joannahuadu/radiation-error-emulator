#ifndef BITMAP_TREE_H
#define BITMAP_TREE_H

#include <string>
#include <vector>
#include <bitset>
#include <cstdint>
class BitmapTree {
    
public:
    // 构造函数：mapping 为 YAML 文件的路径
    BitmapTree(const std::string& mapping);
    ~BitmapTree();

    // 添加物理地址范围 [s_Daddr, t_Daddr]（包含两端）的更新
    void addRange(uintptr_t s_Daddr, uintptr_t t_Daddr);
    void printDetailed() const;
    void printLeafCounts() const;
    static std::string bitsetToHex(const std::string &bitstr);
    static std::string compressZeros(const std::string& str, int threshold);

private:
    // column 节点：叶节点，不再有子节点，
    // 同时维护一个 bitset，用于标记 row 的状态（长度为 2^(row_bits)，例如 2^17=131072）
    struct ColumnNode {
        int index;  // 列号
        std::bitset<131072> row_bitmap; 
        int leaf_count; // 此 column 下已置 1 的 row 数

        ColumnNode(int idx) : index(idx), leaf_count(0) {}
    };

    // bank 节点：下有最多 2^(column_bits) 个 column 节点，同时维护一个 bitset<1024>
    // 用于记录哪些 column 已被更新；还维护一个叶子计数，表示本 bank 下（column层）的更新数
    struct BankNode {
        int index;  
        std::vector<ColumnNode> columns; // 大小为 2^(column_bits)
        std::bitset<1024> column_bitmap;  // 每个位对应一个 column 是否被更新
        int leaf_count; // 本 bank 下已更新的 column 数（第一次更新时置1）

        BankNode(int idx, int num_columns);
    };

    // bankgroup 节点：下有最多 2^(bank_bits) 个 bank 节点，同时维护一个叶子计数，
    // 表示该 bankgroup 下（bank 层）的更新数
    struct BankGroupNode {
        int index;
        std::vector<BankNode> banks; 
        int leaf_count;

        BankGroupNode(int idx, int num_banks, int num_columns);
    };

    // 虚根节点，其下存放所有 bankgroup 节点
    //std::vector<BankGroupNode> bankgroups;
    struct rtNode {
        std::vector<BankGroupNode> bankgroups; 
        int leaf_count; 
        rtNode() : leaf_count(0) {}
        rtNode(int num_banks, int num_bankgroups, int num_columns);
    }rt;

    // 从 YAML 文件中解析的映射规则
    int dq; // DQ 值：物理地址在映射前先右移 dq 位
    std::vector<int> map_column;   
    std::vector<int> map_bankgroup; 
    std::vector<int> map_bank;    
    std::vector<int> map_row;     

    // 从 dram->hierarchy 中解析得到的各层位宽（case只用 bankgroup、bank、column、row 四层）
    int bankgroup_bits;
    int bank_bits;
    int column_bits;
    int row_bits;

    int num_bankgroups;
    int num_banks;
    int num_columns;
    int num_rows;

    // 根据映射规则从物理地址（处理过 dq 后）中提取对应字段的值
    int extractField(uintptr_t addr, const std::vector<int>& bits);

    // 根据 dram 层次信息初始化整棵树
    void initializeTree();
};


#endif 