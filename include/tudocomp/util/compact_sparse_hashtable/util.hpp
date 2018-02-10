#pragma once

#include <cstdint>

#include <tudocomp/ds/IntPtr.hpp>

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

using QuotPtr = IntPtr<dynamic_t>;
inline QuotPtr make_quot_ptr(uint64_t* ptr, size_t quot_width) {
    using namespace int_vector;

    return IntPtrBase<QuotPtr>(ptr, 0, quot_width);
}

inline size_t popcount(uint64_t value) {
    return __builtin_popcountll(value);
}

}}
