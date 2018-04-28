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
        using val_t_export = val_t;

        buckets_t m_buckets;

        inline buckets_bv_t() {}
        inline buckets_bv_t(size_t table_size, widths_t widths) {
            size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);

            m_buckets = std::make_unique<my_bucket_t[]>(buckets_size);
        }
        using table_pos_t = sparse_pos_t<my_bucket_t, bucket_layout_t, val_t>;

        // pseudo-iterator for iterating over bucket elements
        // NB: does not wrap around!
        struct iter_t {
            my_bucket_t const*        m_bucket;
            val_quot_ptrs_t<val_t>    m_b_start;
            val_quot_ptrs_t<val_t>    m_b_end;
            widths_t                  m_widths;

            inline void set_bucket_elem_range(size_t end_offset) {
                size_t start_offset = 0;
                DCHECK_LE(start_offset, end_offset);

                m_b_start = m_bucket->at(start_offset, m_widths);
                m_b_end   = m_bucket->at(end_offset, m_widths);
            }

            inline iter_t(my_bucket_t const* buckets,
                          size_t buckets_size,
                          table_pos_t const& pos,
                          widths_t const& widths):
                m_widths(widths)
            {
                // NB: Using pointer arithmetic here, because
                // we can (intentionally) end up with the address 1-past
                // the end of the vector, which represents an end-iterator.
                m_bucket = buckets + pos.idx_of_bucket;

                if(pos.idx_of_bucket < buckets_size) {
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

            inline bool operator!=(iter_t& other) {
                return !(m_b_end.ptr_eq(other.m_b_end));
            }
        };

        struct context_t {
            buckets_t& m_buckets;
            size_t const table_size;
            widths_t const& widths;

            inline void destroy_vals() {
                size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);

                for(size_t i = 0; i < buckets_size; i++) {
                    m_buckets[i].destroy_vals(widths);
                }
            }
            inline table_pos_t table_pos(size_t pos) {
                return table_pos_t { pos, m_buckets.get() };
            }
            inline val_quot_ptrs_t<val_t> allocate_pos(table_pos_t pos) {
                DCHECK(!pos.exists_in_bucket());

                auto& bucket = pos.bucket();
                auto offset_in_bucket = pos.offset_in_bucket();
                uint64_t new_bucket_bv = bucket.bv() | pos.bit_mask_in_bucket;

                return bucket.insert_at(offset_in_bucket, new_bucket_bv, widths);
            }
            inline val_quot_ptrs_t<val_t> at(table_pos_t pos) {
                DCHECK(pos.exists_in_bucket());

                auto& bucket = pos.bucket();
                auto offset_in_bucket = pos.offset_in_bucket();

                return bucket.at(offset_in_bucket, widths);
            }
            inline bool pos_is_empty(table_pos_t pos) {
                return !pos.exists_in_bucket();
            }
            inline iter_t make_iter(table_pos_t const& pos) {
                size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size);
                return iter_t(m_buckets.get(), buckets_size, pos, widths);
            }
        };
        inline auto context(size_t table_size, widths_t const& widths) {
            return context_t {
                m_buckets, table_size, widths
            };
        }
    };

    template<typename val_t>
    struct plain_sentinel_t {
        using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
        using qvd_t = quot_val_data_seq_t<val_t>;
        using widths_t = typename qvd_t::QVWidths;
        using val_t_export = val_t;

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

            auto ctx = context(table_size, widths);

            for(size_t i = 0; i < table_size; i++) {
                auto elem = ctx.allocate_pos(ctx.table_pos(i));
                elem.set_no_drop(value_type(m_empty_value), 0);
            }
        }
        struct table_pos_t {
            uint64_t* m_alloc;
            size_t const offset;
        };
        // pseudo-iterator for iterating over bucket elements
        // NB: does not wrap around!
        struct iter_t {
            val_quot_ptrs_t<val_t>    m_end;
            value_type const&         m_empty_value;

            inline iter_t(val_quot_ptrs_t<val_t> endpos,
                          value_type const& empty_value):
                m_end(endpos),
                m_empty_value(empty_value)
            {
            }

            inline val_quot_ptrs_t<val_t> get() {
                return m_end;
            }

            inline void decrement() {
                do {
                    m_end.decrement_ptr();
                } while(*m_end.val_ptr() == m_empty_value);
            }

            inline bool operator!=(iter_t& other) {
                return !(m_end.ptr_eq(other.m_end));
            }
        };

        struct context_t {
            std::unique_ptr<uint64_t[]>& m_alloc;
            value_type const& m_empty_value;
            size_t const table_size;
            widths_t const& widths;

            inline void destroy_vals() {
                qvd_t::destroy_vals(m_alloc.get());
            }

            inline table_pos_t table_pos(size_t pos) {
                return table_pos_t { m_alloc.get(), pos };
            }
            inline val_quot_ptrs_t<val_t> allocate_pos(table_pos_t pos) {
                return qvd_t::at(m_alloc.get(), table_size, pos.offset, widths);
            }
            inline val_quot_ptrs_t<val_t> at(table_pos_t pos) {
                return qvd_t::at(m_alloc.get(), table_size, pos.offset, widths);
            }
            inline bool pos_is_empty(table_pos_t pos) {
                return *at(pos).val_ptr() == m_empty_value;
            }
            inline iter_t make_iter(table_pos_t const& pos) {
                return iter_t {
                    qvd_t::at(m_alloc.get(), table_size, pos.offset, widths),
                    m_empty_value,
                };
            }
        };
        inline auto context(size_t table_size, widths_t const& widths) {
            return context_t {
                m_alloc, m_empty_value, table_size, widths
            };
        }
    };
}}
