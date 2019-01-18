#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/util/compact_hash/util.hpp>
#include "../entry_t.hpp"

namespace tdc {namespace compact_sparse_hashset {
using namespace compact_hash;

/// Represents a pair of pointers to value and quotient inside a bucket.
class quot_ptrs_t {
    mutable QuotPtr m_quot_ptr;

public:
    inline quot_ptrs_t(QuotPtr quot_ptr):
        m_quot_ptr(quot_ptr)
    {
    }

    inline quot_ptrs_t():
        m_quot_ptr() {}

    inline uint64_t get_quotient() const {
        return uint64_t(*m_quot_ptr);
    }

    inline void set_quotient(uint64_t v) const {
        *m_quot_ptr = v;
    }

    inline void swap_quotient(uint64_t& other) const {
        uint64_t tmp = uint64_t(*m_quot_ptr);
        std::swap(other, tmp);
        *m_quot_ptr = tmp;
    }

    inline QuotPtr quot_ptr() const {
        return m_quot_ptr;
    }

    inline void increment_ptr() {
        m_quot_ptr++;
    }
    inline void decrement_ptr() {
        m_quot_ptr--;
    }

    inline friend bool operator==(quot_ptrs_t const& lhs,
                                  quot_ptrs_t const& rhs)
    {
        return lhs.m_quot_ptr == rhs.m_quot_ptr;
    }

    inline friend bool operator!=(quot_ptrs_t const& lhs,
                                  quot_ptrs_t const& rhs)
    {
        return lhs.m_quot_ptr != rhs.m_quot_ptr;
    }

    inline void set(uint64_t quot) {
        set_quotient(quot);
    }

    inline void set_no_drop(uint64_t quot) {
        set_quotient(quot);
    }

    inline void move_from(quot_ptrs_t other) {
        set_quotient(other.get_quotient());
    }

    inline void init_from(quot_ptrs_t other) {
        set_quotient(other.get_quotient());
    }

    inline void swap_with(quot_ptrs_t other) {
        uint64_t tmp_quot = get_quotient();
        move_from(other);
        other.set(tmp_quot);
    }

    inline void uninitialize() {
    }
};

struct lookup_result_t {
    quot_ptrs_t entry;
    bool is_empty;
};

}}
