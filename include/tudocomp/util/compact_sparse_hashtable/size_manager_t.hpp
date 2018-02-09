#pragma once

#include <cstdint>

#include "decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

class size_manager_t {
    /// TODO: Remove this experiment
    static constexpr bool HIGH_BITS_RANDOM = false;

    uint8_t m_capacity_log2;
    size_t m_size;

    /// Adjust the user-specified size of the table as needed
    /// by the current implementation.
    ///
    /// In this case, the grow function multiplies the capacity by two,
    /// so we need to start at a value != 0.
    inline static size_t adjust_size(size_t size) {
        return (size < 2) ? 2 : size;
    }
public:
    inline uint8_t capacity_log2() const {
        return m_capacity_log2;
    }

    inline size_manager_t(size_t capacity) {
        capacity = adjust_size(capacity);

        m_size = 0;
        CHECK(is_pot(capacity));
        m_capacity_log2 = log2_upper(capacity);
    }

    inline size_t size() const {
        return m_size;
    }

    inline void set_size(size_t new_size) {
        DCHECK_LT(new_size, capacity());
        m_size = new_size;
    }

    inline size_t capacity() const {
        return 1ull << m_capacity_log2;
    }

    inline bool needs_to_grow_capacity(size_t new_size) const {
        return(capacity() / 2) <= new_size;
    }

    inline size_t grown_capacity() const {
        return capacity() * 2;
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


    /// Adds the `add` value to `v`, and wraps it around the current capacity.
    template<typename int_t>
    inline int_t mod_add(int_t v, int_t add = 1) {
        size_t mask = capacity() - 1;
        return (v + add) & mask;
    }

    /// Subtracts the `sub` value to `v`, and wraps it around the current capacity.
    template<typename int_t>
    inline int_t mod_sub(int_t v, int_t sub = 1) {
        size_t mask = capacity() - 1;
        return (v - sub) & mask;
    }
};

}}
