// 这个文件包含模板的实现
template<size_t K>
BitsetWrapper<K>::BitsetWrapper() : bits() {}

template<size_t K>
void BitsetWrapper<K>::set(size_t pos) {
    bits.set(pos);
}

template<size_t K>
void BitsetWrapper<K>::reset(size_t pos) {
    bits.reset(pos);
}

template<size_t K>
bool BitsetWrapper<K>::test(size_t pos) const{
    return bits.test(pos);
}

template<size_t K>
size_t BitsetWrapper<K>::count() const{
    return bits.count();
}

template<size_t K>
size_t BitsetWrapper<K>::size() const{
    return N;
}

template<size_t K>
size_t BitsetWrapper<K>::find_first() const{
    return bits._Find_first();
}

template<size_t K>
size_t BitsetWrapper<K>::find_next(size_t pos) const{
    return bits._Find_next(pos);
}

template<size_t K>
bool BitsetWrapper<K>::hasConsecutiveOnes(size_t k) const{
    if (k == 0) return true;
    if (k > N) return false;
    std::bitset<N> temp = bits;
    for (size_t i = 1; i < k; i++) {
        temp &= (bits >> i);
    }
    return temp.any();
}

template<size_t K>
size_t BitsetWrapper<K>::hasConsecutiveCount(size_t k) const{
    if (k == 0) return 0;
    if (k > N) return 0;
    std::bitset<N> temp = bits;
    for (size_t i = 1; i < k; i++) {
        temp &= (bits >> i);
    }
    return temp.count();
}

template<size_t K>
BitsetWrapper<K> BitsetWrapper<K>::hasConsecutiveOnesPos(size_t k) const{
    if (k == 0) {
        BitsetWrapper<K> result;
        result.bits = bits;
        return result;
    }
    if (k > N) return BitsetWrapper<K>();
    std::bitset<N> temp = bits;
    for (size_t i = 1; i < k; i++) {
        temp &= (bits >> i);
    }
    BitsetWrapper<K> result;
    result.bits = temp;
    return result;
}

// PowerBitset的模板方法实现
template<size_t K>
typename PowerBitset::BitsetVariant PowerBitset::create_storage_for_power() {
    return BitsetWrapper<K>();
}

template<size_t K, size_t... Rest>
typename PowerBitset::BitsetVariant PowerBitset::create_storage(size_t k) {
    if (k == K) {
        return create_storage_for_power<K>();
    }
    if constexpr (sizeof...(Rest) > 0) {
        return create_storage<Rest...>(k);
    }
    throw std::invalid_argument("Invalid power value");
}