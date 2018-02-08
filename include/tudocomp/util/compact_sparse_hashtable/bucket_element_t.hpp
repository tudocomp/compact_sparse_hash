#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
class Bucket;

template<typename val_t>
class BucketElem {
    val_t* m_val_ptr;
    QuotPtr m_quot_ptr;

    friend class Bucket<val_t>;

    inline BucketElem(val_t* val_ptr,
                      QuotPtr quot_ptr):
        m_val_ptr(val_ptr),
        m_quot_ptr(quot_ptr)
    {
    }

public:
    inline BucketElem():
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

    inline bool ptr_eq(BucketElem const& other) {
        return m_val_ptr == other.m_val_ptr;
    }
};

}}
