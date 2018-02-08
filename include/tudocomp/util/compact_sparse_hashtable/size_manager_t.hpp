#pragma once

#include <cstdint>

namespace tdc {namespace compact_sparse_hashtable {

class SizeManager {
    uint8_t m_capacity_log2;
    size_t m_size;

public:

    inline SizeManager(size_t capacity) {
        m_size = 0;
        CHECK(is_pot(capacity));
        m_capacity_log2 = log2_upper(capacity);
    }

    inline size_t& size() {
        return m_size;
    }

    inline size_t const& size() const {
        return m_size;
    }

    inline size_t capacity() const {
        return 1ull << m_capacity_log2;
    }

    inline uint8_t capacity_log2() const {
        return m_capacity_log2;
    }
};

}}
