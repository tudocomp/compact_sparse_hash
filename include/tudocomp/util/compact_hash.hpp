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
class compact_torage_t {
    // [...] TODO

    value_type m_empty_value;

public:
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;

    inline compact_sparse_storage_t() {}
    inline compact_sparse_storage_t(size_t table_size, value_type const& empty_value) {
        // [...] TODO
        m_empty_value = empty_value;
    }

    // [...] TODO
    using TablePos = ;

    // pseudo-iterator for iterating over bucket elements
    // NB: does not wrap around!
    struct buckets_iter_t {
        // [...] TODO


        inline buckets_iter_t(buckets_t const& buckets,
                            TablePos const& pos, size_t qw, size_t vw):
            m_quotient_width(qw),
            m_value_width(vw)
        {
            // [...] TODO
        }

        inline val_quot_ptrs_t<val_t> get() {
            // [...] TODO
        }

        inline void decrement() {
            // [...] TODO
        }

        inline bool operator!=(buckets_iter_t& other) {
            // [...] TODO
        }
    };

    using InsertIter = buckets_iter_t;

    // TODO:
    struct statistics_t {
        size_t buckets = 0;
        size_t allocated_buckets = 0;
        size_t buckets_real_allocated_capacity_in_bytes = 0;

        size_t real_allocated_capacity_in_bytes = 0;
        uint64_t theoretical_minimum_size_in_bits = 0;
    };

    class with_qv_width_t {
        // [...] TODO
        size_t const m_quotient_width;
        size_t const m_value_width;
    public:
        inline with_qv_width_t(buckets_t& buckets, size_t qw, size_t vw):
            // [...] TODO
            m_quotient_width(qw),
            m_value_width(vw) {}

        inline with_qv_width_t(with_qv_width_t&&) = default;
        inline with_qv_width_t& operator=(with_qv_width_t&&) = delete;

        inline void run_destructors_of_elements() {
            // [...] TODO
        }

        template<typename iter_all_t, typename F>
        inline void drain_all(F f, iter_all_t&& iter) {
            // [...] TODO

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
                        drop_bucket(bucket);
                    }

                    start_of_bucket = true;
                    bucket = p.idx_of_bucket;
                }

                f(initial_address, p);
            }
        }

        inline TablePos table_pos(size_t pos) {
            // [...] TODO
        }

        /// Returns true if there is no element at the index `i`
        /// in the hashtable.
        inline bool table_pos_is_empty(size_t i) {
            return !table_pos_contains_value(i);
        }

        /// Returns true if there is an element at the index `i`
        /// in the hashtable.
        inline bool table_pos_contains_value(size_t i) {
            // [...] TODO
        }

        inline val_quot_ptrs_t<val_t> get_val_quot_at(TablePos pos) {
            // [...] TODO
        }

        /// Insert a key-value pair into a empty location in the table.
        template<typename handler_t>
        inline void table_set_at_empty(size_t pos, key_t quot, handler_t&& handler) {
            // [...] TODO
        }

        inline InsertIter make_insert_iter(TablePos const& pos) {
            size_t qw = m_quotient_width;
            size_t vw = m_value_width;
            // [...] TODO
            return InsertIter(m_buckets, pos, qw, vw);
        }

        inline value_type empty_value() const {
            return m_empty_value;
        }

        // -----------------------
        // Evaluation and Debugging
        // -----------------------


        inline statistics_t stat_gather(size_t size, size_t cv_allocation) {
            statistics_t r;

            size_t qw = m_quotient_width;
            size_t vw = m_value_width;

            // [...] TODO

            return r;
        }
    };
    friend class with_qv_with_t;
    inline with_qv_width_t with_qv_width(size_t qw, size_t vw) const {
        // [...] TODO
        return with_qv_width_t {
            m_buckets, qw, vw
        };
    }
};

template<typename val_t, typename hash_t = poplar_xorshift_t>
class compact_hashtable_t: public base_table_t<compact_sparse_storage_t, val_t, hash_t> {
    using base_t = base_table_t<compact_sparse_storage_t, val_t, hash_t>;
public:

    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_VALUE_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline compact_hashtable_t(size_t size = DEFAULT_TABLE_SIZE,
                                      size_t key_width = DEFAULT_KEY_WIDTH,
                                      size_t value_width = DEFAULT_VALUE_WIDTH,
                                      value_type const& empty_value = value_type()):
        base_t(size, key_width, value_width, empty_value)
    {
    }

    inline compact_hashtable_t(compact_hashtable_t&& other) = default;
    inline compact_hashtable_t& operator=(compact_hashtable_t&& other) = default;
};

}}
