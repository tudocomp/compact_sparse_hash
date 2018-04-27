#pragma once

#include <memory>
#include "util.hpp"
#include "bucket_t.hpp"
#include "sparse_pos_t.hpp"

// Table for uninitalized elements

namespace tdc {namespace compact_sparse_hashtable {
    template<typename val_t>
    struct buckets_bv_t {
        using my_bucket_t = bucket_t<val_t, 8>;
        using bucket_layout_t = typename my_bucket_t::bucket_layout_t;
        using buckets_t = std::unique_ptr<my_bucket_t[]>;
        using qvd_t = quot_val_data_seq_t<val_t>;
        using widths_t = typename qvd_t::QVWidths;

        buckets_t m_buckets;

        inline buckets_bv_t() {}
        inline buckets_bv_t(size_t table_size, widths_t widths) {
            size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);

            m_buckets = std::make_unique<my_bucket_t[]>(buckets_size);
        }
        inline void destroy_vals(size_t table_size, widths_t widths) {
            size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);

            for(size_t i = 0; i < buckets_size; i++) {
                m_buckets[i].destroy_vals(widths);
            }
        }
        using table_pos_t = sparse_pos_t<my_bucket_t, bucket_layout_t, val_t>;
        inline table_pos_t table_pos(size_t pos, size_t table_size, widths_t const& widths) {
            return table_pos_t { pos, m_buckets.get(), table_size, widths };
        }
        inline val_quot_ptrs_t<val_t> allocate_pos(table_pos_t pos) {
            DCHECK(!pos.exists_in_bucket());

            auto& bucket = pos.bucket();
            auto offset_in_bucket = pos.offset_in_bucket();
            uint64_t new_bucket_bv = bucket.bv() | pos.bit_mask_in_bucket;

            return bucket.insert_at(offset_in_bucket, new_bucket_bv, pos.widths);
        }
        inline val_quot_ptrs_t<val_t> at(table_pos_t pos) {
            DCHECK(pos.exists_in_bucket());

            auto& bucket = pos.bucket();
            auto offset_in_bucket = pos.offset_in_bucket();

            return bucket.at(offset_in_bucket, pos.widths);
        }
        inline bool pos_is_empty(table_pos_t pos) {
            return !pos.exists_in_bucket();
        }
    };

    // IMPORTANT IDEA!
    // CAN USE quot STORAGE FOR SENTINEL VALUE

    template<typename val_t>
    struct plain_sentinel_t {
        using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
        using qvd_t = quot_val_data_seq_t<val_t>;
        using widths_t = typename qvd_t::QVWidths;

        std::unique_ptr<uint64_t[]> m_alloc;
        value_type m_empty_value;

        inline plain_sentinel_t() {}
        inline plain_sentinel_t(size_t table_size,
                                widths_t widths,
                                value_type const& empty_value = value_type()):
            m_empty_value(empty_value)
        {
            size_t alloc_size = qvd_t::calc_sizes(table_size, widths).overall_qword_size;
            m_alloc = std::make_unique<uint64_t[]>(alloc_size);

            for(size_t i = 0; i < table_size; i++) {
                auto elem = allocate_pos(table_pos(i, table_size, widths));
                elem.set_no_drop(value_type(m_empty_value), 0);
            }
        }
        inline void destroy_vals(size_t table_size, widths_t widths) {
            qvd_t::destroy_vals(m_alloc.get(), table_size, widths);
        }
        struct table_pos_t {
            uint64_t* m_alloc;
            size_t const offset;
            size_t const table_size;
            widths_t const widths;
        };
        inline table_pos_t table_pos(size_t pos, size_t table_size, widths_t const& widths) {
            return table_pos_t { m_alloc.get(), pos, table_size, widths };
        }
        inline val_quot_ptrs_t<val_t> allocate_pos(table_pos_t pos) {
            return qvd_t::at(m_alloc.get(), pos.table_size, pos.offset, pos.widths);
        }
        inline val_quot_ptrs_t<val_t> at(table_pos_t pos) {
            return qvd_t::at(m_alloc.get(), pos.table_size, pos.offset, pos.widths);
        }
        inline bool pos_is_empty(table_pos_t pos) {
            return *at(pos).val_ptr() == m_empty_value;
        }
    };
}}
