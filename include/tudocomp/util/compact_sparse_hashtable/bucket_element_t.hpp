#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
class bucket_t;

/// Represents a pair of pointers to value and quotient inside a bucket.
template<typename val_t>
class bucket_element_t {
    val_t* m_val_ptr;
    QuotPtr m_quot_ptr;

    friend class bucket_t<val_t>;

    inline bucket_element_t(val_t* val_ptr,
                      QuotPtr quot_ptr):
        m_val_ptr(val_ptr),
        m_quot_ptr(quot_ptr)
    {
    }

public:
    inline bucket_element_t():
        m_val_ptr(nullptr), m_quot_ptr() {}

    inline uint64_t get_quotient() {
        return uint64_t(*m_quot_ptr);
    }

    inline void set_quotient(uint64_t v) {
        *m_quot_ptr = v;
    }

    inline void swap_quotient(uint64_t& other) {
        uint64_t tmp = uint64_t(*m_quot_ptr);
        std::swap(other, tmp);
        *m_quot_ptr = tmp;
    }

    inline val_t& val() {
        return *m_val_ptr;
    }

    inline val_t const& val() const {
        return *m_val_ptr;
    }

    inline void increment_ptr() {
        m_quot_ptr++;
        m_val_ptr++;
    }
    inline void decrement_ptr() {
        m_quot_ptr--;
        m_val_ptr--;
    }

    inline bool ptr_eq(bucket_element_t const& other) const {
        return m_val_ptr == other.m_val_ptr;
    }
};

}}
