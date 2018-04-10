#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "compact_sparse_hashtable/util.hpp"
#include "compact_sparse_hashtable/bucket_t.hpp"
#include "compact_sparse_hashtable/hash_functions.hpp"
#include "compact_sparse_hashtable/size_manager_t.hpp"
#include "compact_sparse_hashtable/sparse_pos_t.hpp"
#include "compact_sparse_hashtable/decomposed_key_t.hpp"
#include "compact_sparse_hashtable/base_table_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

// TODO: Remove unconditional bound checking
// - c, v bvs
// - buckets
// - elements in buckets

template<typename val_t>
class compact_sparse_storage_t {
    using buckets_t = std::vector<bucket_t<val_t>>;

    /// Sparse table data (buckets)
    buckets_t m_buckets;

public:
    inline compact_sparse_storage_t() {}
    inline compact_sparse_storage_t(size_t table_size) {
        size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);

        m_buckets.reserve(buckets_size);
        m_buckets.resize(buckets_size);
    }

    inline void run_destructors_of_elements(size_t qw, size_t vw) {
        for(size_t i = 0; i < m_buckets.size(); i++) {
            m_buckets[i].destroy_vals(qw, vw);
        }
    }

    /// Maps hashtable position to position of the corresponding bucket,
    /// and the position inside of it.
    struct bucket_layout_t {
        static constexpr size_t BVS_WIDTH_SHIFT = 6;
        static constexpr size_t BVS_WIDTH_MASK = 0b111111;

        static inline size_t table_pos_to_idx_of_bucket(size_t pos) {
            return pos >> BVS_WIDTH_SHIFT;
        }

        static inline size_t table_pos_to_idx_inside_bucket(size_t pos) {
            return pos & BVS_WIDTH_MASK;
        }

        static inline size_t table_size_to_bucket_size(size_t size) {
            return (size + BVS_WIDTH_MASK) >> BVS_WIDTH_SHIFT;
        }
    };

    /// Run the destructors of the elements of the `i`-th bucket,
    /// and drop it from the hashtable, replacing it with an empty one.
    inline void drop_bucket(size_t i, size_t qw, size_t vw) {
        DCHECK_LT(i, m_buckets.size());
        m_buckets[i].destroy_vals(qw, vw);
        m_buckets[i] = bucket_t<val_t>();
    }

    template<typename iter_all_t, typename F>
    inline void drain_all(F f, iter_all_t&& iter, size_t qw, size_t vw) {
        bool start_of_bucket = false;
        size_t bucket = 0;

        uint64_t initial_address;
        size_t i;
        while(iter.next(&initial_address, &i)) {
            auto p = table_pos(i);

            // drop buckets of old table as they get emptied out
            if (p.offset_in_bucket() == 0) {
                if (start_of_bucket) {
                    DCHECK_NE(bucket, p.idx_of_bucket);
                    drop_bucket(bucket, qw, vw);
                }

                start_of_bucket = true;
                bucket = p.idx_of_bucket;
            }

            auto kv = get_val_quot_at(p);
            auto stored_quotient = kv.get_quotient();

            key_t key = compose_key(initial_address, stored_quotient);

            f(std::move(key), std::move(*kv.val_ptr()));
        }
    }

    using TablePos = sparse_pos_t<buckets_t, bucket_layout_t>;
    inline TablePos table_pos(size_t pos) {
        return TablePos { pos, m_buckets };
    }

    /// Returns true if there is no element at the index `i`
    /// in the hashtable.
    inline bool table_pos_is_empty(size_t i) {
        return !table_pos_contains_value(i);
    }

    /// Returns true if there is an element at the index `i`
    /// in the hashtable.
    inline bool table_pos_contains_value(size_t i) {
        return table_pos(i).exists_in_bucket();
    }

    inline val_quot_ptrs_t<val_t> get_val_quot_at(TablePos pos, size_t qw, size_t vw) {
        DCHECK(pos.exists_in_bucket());
        return pos.bucket().at(pos.offset_in_bucket(), qw, vw);
    }

    /// Insert a key-value pair into a empty location in the table.
    template<typename handler_t>
    inline void table_set_at_empty(size_t pos, key_t quot, handler_t&& handler,
                                   size_t qw, size_t vw) {
        auto data = table_pos(pos);
        DCHECK(!data.exists_in_bucket());

        // we will insert a new element
        auto value_handler = handler.on_new();
        auto&& val = value_handler.get();

        // figure out which bucket to access
        auto& bucket = data.bucket();

        // insert element & grow bucket as appropriate
        insert_in_bucket(bucket,
                         data.offset_in_bucket(),
                         data.bit_mask_in_bucket,
                         qw,
                         vw,
                         std::move(val),
                         quot);

        // notify handler with location of new element
        auto new_loc = bucket.at(data.offset_in_bucket(), qw, vw);
        value_handler.new_location(new_loc.val_ptr());
    }

    // pseudo-iterator for iterating over bucket elements
    // NB: does not wrap around!
    struct buckets_iter_t {
        bucket_t<val_t> const* m_bucket;
        val_quot_ptrs_t<val_t>    m_b_start;
        val_quot_ptrs_t<val_t>    m_b_end;
        size_t               m_quotient_width;
        size_t               m_value_width;

        inline void set_bucket_elem_range(size_t end_offset) {
            size_t start_offset = 0;
            DCHECK_LE(start_offset, end_offset);

            m_b_start = m_bucket->at(start_offset, m_quotient_width, m_value_width);
            m_b_end   = m_bucket->at(end_offset, m_quotient_width, m_value_width);
        }

        inline buckets_iter_t(compact_sparse_storage_t& table,
                              TablePos const& pos, size_t qw, size_t vw) {
            m_quotient_width = qw;
            m_value_width = vw;

            // NB: Using pointer arithmetic here, because
            // we can (intentionally) end up with the address 1-past
            // the end of the vector, which represents an end-iterator.
            m_bucket = table.m_buckets.data() + pos.idx_of_bucket;

            if(pos.idx_of_bucket < table.m_buckets.size()) {
                set_bucket_elem_range(pos.offset_in_bucket());
            } else {
                // use default constructed nullptr val_quot_ptrs_ts
            }
        }

        inline val_quot_ptrs_t<val_t> get() {
            return m_b_end;
        }

        inline void decrement() {
            if (!m_b_start.ptr_eq(m_b_end)) {
                m_b_end.decrement_ptr();
            } else {
                do {
                    --m_bucket;
                } while(m_bucket->bv() == 0);
                set_bucket_elem_range(m_bucket->size() - 1);
            }
        }

        inline bool operator!=(buckets_iter_t& other) {
            return !(m_b_end.ptr_eq(other.m_b_end));
        }
    };

    using InsertIter = buckets_iter_t;

    // -----------------------
    // Evaluation and Debugging
    // -----------------------

    struct statistics_t {
        size_t buckets = 0;
        size_t allocated_buckets = 0;
        size_t buckets_real_allocated_capacity_in_bytes = 0;

        size_t real_allocated_capacity_in_bytes = 0;
        uint64_t theoretical_minimum_size_in_bits = 0;
    };
    inline statistics_t stat_gather(size_t qw, size_t vw, size_t size, size_t cv_allocation) {
        statistics_t r;

        r.buckets = m_buckets.size();

        for (auto const& b : m_buckets) {
            r.allocated_buckets += (!b.is_empty());
            r.buckets_real_allocated_capacity_in_bytes += b.stat_allocation_size_in_bytes(qw, vw);
        }

        // Calculate real allocated bytes
        // NB: Sizes of members with constant size are not included (eg, sizeof(m_buckets))
        // Size of buckets vector
        r.real_allocated_capacity_in_bytes += m_buckets.capacity() * sizeof(bucket_t<val_t>);
        r.real_allocated_capacity_in_bytes += r.buckets_real_allocated_capacity_in_bytes;
        // Size of cv bitvectors
        r.real_allocated_capacity_in_bytes += cv_allocation;

        // Calculate minimum bits needed
        // Occupation bitvector inside allocated buckets
        r.theoretical_minimum_size_in_bits += r.allocated_buckets * 64;
        // Quotient bitvectors across all buckets
        r.theoretical_minimum_size_in_bits += size * qw;
        // Values across all buckets
        r.theoretical_minimum_size_in_bits += size * vw;
        // Size of cv bitvectors
        r.theoretical_minimum_size_in_bits += size * 2;

        return r;
    }
};

template<typename val_t, typename hash_t = poplar_xorshift_t>
class compact_sparse_hashtable_t: public base_table_t<compact_sparse_storage_t, val_t, hash_t> {
    using base_t = base_table_t<compact_sparse_storage_t, val_t, hash_t>;
public:

    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_VALUE_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline compact_sparse_hashtable_t(size_t size = DEFAULT_TABLE_SIZE,
                                      size_t key_width = DEFAULT_KEY_WIDTH,
                                      size_t value_width = DEFAULT_VALUE_WIDTH):
        base_t(size, key_width, value_width)
    {
    }

    inline compact_sparse_hashtable_t(compact_sparse_hashtable_t&& other) = default;
    inline compact_sparse_hashtable_t& operator=(compact_sparse_hashtable_t&& other) = default;
};

}}
