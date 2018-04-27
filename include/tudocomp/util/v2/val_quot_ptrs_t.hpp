#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"

namespace tdc {namespace compact_sparse_hashtable {

/// Represents a pair of pointers to value and quotient inside a bucket.
template<typename val_t>
class val_quot_ptrs_t {
    ValPtr<val_t> m_val_ptr;
    mutable QuotPtr m_quot_ptr;

public:
    inline val_quot_ptrs_t(ValPtr<val_t> val_ptr,
                      QuotPtr quot_ptr):
        m_val_ptr(val_ptr),
        m_quot_ptr(quot_ptr)
    {
    }

    inline val_quot_ptrs_t():
        m_val_ptr(), m_quot_ptr() {}

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

    inline ValPtr<val_t> val_ptr() const {
        return m_val_ptr;
    }

    inline QuotPtr quot_ptr() const {
        return m_quot_ptr;
    }

    inline void increment_ptr() {
        m_quot_ptr++;
        m_val_ptr++;
    }
    inline void decrement_ptr() {
        m_quot_ptr--;
        m_val_ptr--;
    }

    inline bool ptr_eq(val_quot_ptrs_t const& other) const {
        return m_val_ptr == other.m_val_ptr;
    }
};

}}
