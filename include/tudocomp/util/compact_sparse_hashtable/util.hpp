#pragma once

#include <cstdint>

#include <tudocomp/util/bit_packed_layout_t.hpp>

namespace tdc {namespace compact_sparse_hashtable {

inline uint8_t log2_upper(uint64_t v) { // TODO: this is slow. Use the highest set bit
    uint8_t m = 0;
    uint64_t n = v;
    while(n) {
        n >>= 1;
        m++;
    }
    m--;
    return m;
}

inline bool is_pot(size_t n) {
    return (n > 0ull && ((n & (n - 1ull)) == 0ull));
}

using QuotPtr = typename int_vector::maybe_bit_packed_pointer_t<dynamic_t>;
template<typename val_t>
using ValPtr = typename int_vector::maybe_bit_packed_pointer_t<val_t>;
template<typename val_t>
using ValRef = typename int_vector::maybe_bit_packed_reference_t<val_t>;

inline size_t popcount(uint64_t value) {
    return __builtin_popcountll(value);
}

}}
