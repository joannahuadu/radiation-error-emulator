#include "bit_tools.h"

PowerBitset::PowerBitset(size_t k) : power(k) {
    if (k > 18) {
        throw std::invalid_argument("Power cannot exceed 18");
    }
    // 使用变参模板创建合适大小的BitsetWrapper
    storage = create_storage<
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18
    >(k);
}

void PowerBitset::set(size_t pos) {
    std::visit([pos](auto& wrapper) { wrapper.set(pos); }, storage);
}


void PowerBitset::reset(size_t pos) {
    std::visit([pos](auto& wrapper) { wrapper.reset(pos); }, storage);
}

bool PowerBitset::test(size_t pos) const {
    return std::visit([pos](const auto& wrapper) { 
        return wrapper.test(pos); 
    }, storage);
}

size_t PowerBitset::size() const {
    return std::visit([](const auto& wrapper) {
        return wrapper.size();
    }, storage);
}

bool PowerBitset::hasConsecutiveOnes(size_t k) const {
    return std::visit([k](const auto& wrapper) {
        return wrapper.hasConsecutiveOnes(k);
    }, storage);
}

size_t PowerBitset::hasConsecutiveCount(size_t k) const {
    return std::visit([k](const auto& wrapper) {
        return wrapper.hasConsecutiveCount(k);
    }, storage);
}

std::vector<size_t> PowerBitset::hasConsecutiveOnesPos(size_t k) const {
    std::vector<size_t> result;
    std::visit([k, &result](const auto& wrapper) {
        auto temp=wrapper.hasConsecutiveOnesPos(k);
        for(size_t i=temp.find_first();i<temp.size();i=temp.find_next(i)){
            result.push_back(i);
        }
    }, storage);
    return result;
}