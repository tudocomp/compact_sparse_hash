#pragma once

#include <cstdint>

#include "decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

class size_manager_t {
    /// TODO: Remove this experiment
    static constexpr bool HIGH_BITS_RANDOM = false;

    uint8_t m_capacity_log2;
    size_t m_size;

public:
    inline uint8_t capacity_log2() const {
        return m_capacity_log2;
    }

    inline size_manager_t(size_t capacity) {
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

    inline decomposed_key_t decompose_hashed_value(uint64_t hres) {
        uint64_t shift = capacity_log2();

        if (HIGH_BITS_RANDOM) {
            shift = 64 - shift;
            return decomposed_key_t {
                hres >> shift,
                hres & ((1ull << shift) - 1ull),
            };
        } else {
            return decomposed_key_t {
                hres & ((1ull << shift) - 1ull),
                hres >> shift,
            };
        }
    }

    inline uint64_t compose_hashed_value(uint64_t initial_address, uint64_t quotient) {
        uint64_t shift = capacity_log2();

        uint64_t harg;
        if (HIGH_BITS_RANDOM) {
            shift = 64 - shift;
            harg = (initial_address << shift) | quotient;
        } else {
            harg = (quotient << shift) | initial_address;
        }

        return harg;
    }
};

}}
