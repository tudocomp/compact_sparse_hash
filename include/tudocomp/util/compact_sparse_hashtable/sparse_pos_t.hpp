#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename buckets_t, typename bucket_layout_t>
class sparse_pos_t {
private:
    buckets_t* m_buckets;
public:
    size_t const bucket_pos;
    size_t const bit_pos;
    uint64_t const b_mask;

    inline sparse_pos_t() {}

    inline sparse_pos_t(size_t pos, buckets_t& buckets):
        m_buckets(&buckets),

        // bucket index based on position (division by 64 bits)
        bucket_pos(bucket_layout_t::table_pos_to_idx_of_bucket(pos)),

        // remainder position of pos inside the bucket (modulo by 64 bits)
        bit_pos(bucket_layout_t::table_pos_to_idx_inside_bucket(pos)),

        // mask for the single bit we deal with
        b_mask(1ull << bit_pos)
    {}

    // check if the right bit is set in the bucket's bv
    inline bool exists_in_bucket() const {
        // bitvector of the bucket
        uint64_t bv = m_buckets->at(bucket_pos).bv();

        return (bv & b_mask) != 0;
    }

    // calculate offset of element in bucket for current pos
    // based on number of set bits in bv
    inline size_t offset_in_bucket() const {
        // bitvector of the bucket
        uint64_t bv = m_buckets->at(bucket_pos).bv();

        return __builtin_popcountll(bv & (b_mask - 1));
    }
};

}}
