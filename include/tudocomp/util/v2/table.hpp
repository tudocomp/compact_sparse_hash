#pragma once

#include <memory>
#include "util.hpp"
#include "bucket_t.hpp"

// Table for uninitalized elements

namespace tdc {namespace compact_sparse_hashtable {
    template<typename val_t>
    struct plain_sentinel_t {
        using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
        val_t m_empty_value;

        using quot_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
        using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

        std::unique_ptr<uint64_t[]> m_data;

        inline plain_sentinel_t(): m_data() {}

        /// Construct a bucket, reserving space according to the bitvector
        /// `bv` and `quot_width`.
        inline plain_sentinel_t(size_t size, size_t quot_width, val_width_t const& val_width) {
            auto layout = calc_sizes(size, quot_width, val_width);

            m_data = std::make_unique<uint64_t[]>(layout.overall_qword_size);

            // NB: We call this for its alignment asserts
            ptrs(quot_width, val_width);
        }

        inline plain_sentinel_t(plain_sentinel_t&& other) = default;
        inline plain_sentinel_t& operator=(plain_sentinel_t&& other) = default;

        // Run destructors of each element in the bucket.
        inline void destroy_vals(size_t size, size_t quot_width, val_width_t const& val_width) {
            auto start = ptrs(quot_width, val_width).vals_ptr;
            auto end = start + size;

            for(; start != end; start++) {
                cbp::cbp_repr_t<val_t>::call_destructor(start);
            }
        }
    };

    template<typename val_t>
    struct buckets_bv {
        using buckets_t = std::unique_ptr<bucket_t<val_t, 8>[]>;

        buckets_t m_buckets;
    };
}}
