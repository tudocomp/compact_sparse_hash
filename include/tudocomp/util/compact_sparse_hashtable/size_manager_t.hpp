#pragma once

#include <cstdint>

#include "decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

/// This manages the size of the hashtable, and related calculations.
class size_manager_t {
    /*
     * TODO: This is currently hardcoded to work with power-of-two table sizes.
     * Generalize it to allows arbitrary growth functions.
     */

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
    /// Create the size manager with an initial table size `capacity`
    inline size_manager_t(size_t capacity) {
        capacity = adjust_size(capacity);

        m_size = 0;
        CHECK(is_pot(capacity));
        m_capacity_log2 = log2_upper(capacity);
    }

    /// Returns the amount of elements currently stored in the hashtable.
    inline size_t size() const {
        return m_size;
    }

    /// Update the amount of elements currently stored in the hashtable
    inline void set_size(size_t new_size) {
        DCHECK_LT(new_size, capacity());
        m_size = new_size;
    }

    /// The amount of bits used by the current table size.
    // TODO: Remove/make private
    inline uint8_t capacity_log2() const {
        return m_capacity_log2;
    }

    /// The current table size.
    inline size_t capacity() const {
        return 1ull << m_capacity_log2;
    }

    /// Check if the capacity needs to grow for the size given as the
    /// argument.
    inline bool needs_to_grow_capacity(size_t new_size) const {
        return(capacity() / 2) <= new_size;
    }

    /// Returns the new capacity after growth.
    inline size_t grown_capacity() const {
        return capacity() * 2;
    }

    /// Decompose the hash value such that `initial_address`
    /// covers the entire table, and `quotient` contains
    /// the remaining bits.
    inline decomposed_key_t decompose_hashed_value(uint64_t hres) {
        uint64_t shift = capacity_log2();

        return decomposed_key_t {
            hres & ((1ull << shift) - 1ull),
            hres >> shift,
        };
    }

    /// Composes a hash value from an `initial_address` and `quotient`.
    inline uint64_t compose_hashed_value(uint64_t initial_address, uint64_t quotient) {
        uint64_t shift = capacity_log2();
        uint64_t harg = (quotient << shift) | initial_address;
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
