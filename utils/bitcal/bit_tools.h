#ifndef BIT_TOOLS_H
#define BIT_TOOLS_H

#include <bitset>
#include <variant>
#include <stdexcept>
#include <string>
#include <vector>

// PowerOfTwo的模板声明
template<size_t K>
struct PowerOfTwo {
    static constexpr size_t value = 2 * PowerOfTwo<K-1>::value;
};

template<>
struct PowerOfTwo<0> {
    static constexpr size_t value = 1;
};

// BitsetWrapper的模板声明
template<size_t K>
class BitsetWrapper {
private:
    static constexpr size_t N = PowerOfTwo<K>::value;
    std::bitset<N> bits;

public:
    BitsetWrapper();
    void set(size_t pos);
    void reset(size_t pos);
    bool test(size_t pos) const;
    size_t count() const;
    size_t size() const;
    bool hasConsecutiveOnes(size_t k) const;
    size_t hasConsecutiveCount(size_t k) const;
    BitsetWrapper hasConsecutiveOnesPos(size_t k) const;
    size_t find_first() const;
    size_t find_next(size_t pos) const;
};

// PowerBitset的声明
class PowerBitset {
private:
    template<size_t... Is>
    struct BitsetVariantHelper {
        using type = std::variant<BitsetWrapper<Is>...>;
    };

    using BitsetVariant = typename BitsetVariantHelper<
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18
    >::type;

    BitsetVariant storage;
    size_t power; // 存储k值

    // 在编译时创建所有可能的大小的variant
    template<size_t K>
    static BitsetVariant create_storage_for_power();

    template<size_t K, size_t... Rest>
    static BitsetVariant create_storage(size_t k);

public:
    explicit PowerBitset(size_t k);
    void set(size_t pos);
    void reset(size_t pos);
    bool test(size_t pos) const;
    size_t size() const;
    bool hasConsecutiveOnes(size_t k) const;
    size_t hasConsecutiveCount(size_t k) const;
    std::vector<size_t> hasConsecutiveOnesPos(size_t k) const;
};

// 模板实现必须放在头文件中
#include "bit_tools.inl"

#endif // BIT_TOOLS_H