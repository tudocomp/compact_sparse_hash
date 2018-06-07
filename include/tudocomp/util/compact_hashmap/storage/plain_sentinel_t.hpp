#pragma once

#include <memory>

#include "../util.hpp"
#include "quot_val_data.hpp"

// Table for uninitalized elements

namespace tdc {namespace compact_sparse_hashmap {
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
                // NB: Using at because allocate_pos()
                // destroys the location first.
                auto elem = ctx.at(ctx.table_pos(i));
                elem.set_no_drop(value_type(m_empty_value), 0);
            }
        }
        struct table_pos_t {
            size_t offset;
            inline table_pos_t(): offset(-1) {}
            inline table_pos_t(size_t o): offset(o) {}
            inline table_pos_t& operator=(table_pos_t const& other) = default;
            inline table_pos_t(table_pos_t const& other) = default;
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
                return m_end != other.m_end;
            }
        };

        struct context_t {
            std::unique_ptr<uint64_t[]>& m_alloc;
            value_type const& m_empty_value;
            size_t const table_size;
            widths_t widths;

            inline void destroy_vals() {
                qvd_t::destroy_vals(m_alloc.get(), table_size, widths);
            }

            inline table_pos_t table_pos(size_t pos) {
                return table_pos_t { pos };
            }
            inline val_quot_ptrs_t<val_t> allocate_pos(table_pos_t pos) {
                DCHECK_LT(pos.offset, table_size);
                auto tmp = at(pos);

                // NB: allocate_pos returns a unitialized location,
                // but all locations are per default initialized with a empty_value.
                // Therefore we destroy the existing value first.
                tmp.uninitialize();

                return tmp;
            }
            inline val_quot_ptrs_t<val_t> at(table_pos_t pos) {
                DCHECK_LT(pos.offset, table_size);
                return qvd_t::at(m_alloc.get(), table_size, pos.offset, widths);
            }
            inline bool pos_is_empty(table_pos_t pos) {
                DCHECK_LT(pos.offset, table_size);
                return *at(pos).val_ptr() == m_empty_value;
            }
            inline iter_t make_iter(table_pos_t const& pos) {
                // NB: One-pass-the-end is acceptable for a end iterator
                DCHECK_LE(pos.offset, table_size);
                return iter_t {
                    qvd_t::at(m_alloc.get(), table_size, pos.offset, widths),
                    m_empty_value,
                };
            }
            inline void trim_storage(table_pos_t* last_start, table_pos_t const& end) {
                // Nothing to be done
            }
        };
        inline auto context(size_t table_size, widths_t const& widths) {
            return context_t {
                m_alloc, m_empty_value, table_size, widths,
            };
        }
    };
}}
