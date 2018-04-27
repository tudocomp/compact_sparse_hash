
#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"
#include "val_quot_ptrs_t.hpp"
#include <tudocomp/util/bit_packed_layout_t.hpp>

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
struct quot_val_data_seq_t {
    using quot_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    struct QVWidths {
        size_t quot_width;
        val_width_t const& val_width;
    };

    /// Calculates the offsets of the two different arrays inside the allocation.
    struct Layout {
        cbp::cbp_layout_element_t<val_t> vals_layout;
        cbp::cbp_layout_element_t<dynamic_t> quots_layout;
        size_t overall_qword_size;

        inline Layout(): vals_layout(), quots_layout(), overall_qword_size(0) {
        }
    };
    inline static Layout calc_sizes(size_t size, QVWidths widths) {
        DCHECK_NE(size, 0);
        DCHECK_LE(alignof(val_t), alignof(uint64_t));

        auto layout = cbp::bit_layout_t();
        auto quots_width = quot_width_t(widths.quot_width);

        // The values
        auto values = layout.cbp_elements<val_t>(size, widths.val_width);

        // The quotients
        auto quots = layout.cbp_elements<dynamic_t>(size, quots_width);

        Layout r;
        r.vals_layout = values;
        r.quots_layout = quots;
        r.overall_qword_size = layout.get_size_in_uint64_t_units();
        return r;
    }

    /// Creates the pointers to the beginnings of the two arrays inside
    /// the allocation.
    struct Ptrs {
        ValPtr<val_t> vals_ptr;
        QuotPtr quots_ptr;
    };
    inline static Ptrs ptrs(uint64_t* alloc, size_t size, QVWidths widths) {
        DCHECK(size != 0);
        auto layout = calc_sizes(size, widths);

        return Ptrs {
            layout.vals_layout.ptr_relative_to(alloc),
            layout.quots_layout.ptr_relative_to(alloc),
        };
    }

    // Run destructors of each element in the bucket.
    inline static void destroy_vals(uint64_t* alloc, size_t size, QVWidths widths) {
        if (size != 0) {
            auto start = ptrs(alloc, size, widths).vals_ptr;
            auto end = start + size;

            for(; start != end; start++) {
                cbp::cbp_repr_t<val_t>::call_destructor(start);
            }
        }
    }

    /// Returns a `val_quot_ptrs_t` to position `pos`,
    /// or a sentinel value that acts as a one-pass-the-end pointer for the empty case.
    inline static val_quot_ptrs_t<val_t> at(uint64_t* alloc, size_t size, size_t pos, QVWidths widths) {
        if(size != 0) {
            auto ps = ptrs(alloc, size, widths);
            return val_quot_ptrs_t<val_t>(ps.vals_ptr + pos, ps.quots_ptr + pos);
        } else {
            DCHECK_EQ(pos, 0);
            return val_quot_ptrs_t<val_t>();
        }
    }
};

}}
