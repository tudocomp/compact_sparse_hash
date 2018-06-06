
#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/util/bit_packed_layout_t.hpp>
#include "../util.hpp"
#include "quot_ptrs_t.hpp"

namespace tdc {namespace compact_sparse_hashset {

struct quot_data_seq_t {
    /// Calculates the offsets of the two different arrays inside the allocation.
    struct Layout {
        cbp::cbp_layout_element_t<dynamic_t> quots_layout;
        size_t overall_qword_size;

        inline Layout(): quots_layout(), overall_qword_size(0) {
        }
    };
    inline static Layout calc_sizes(size_t size, uint8_t quot_width) {
        DCHECK_NE(size, 0);

        auto layout = cbp::bit_layout_t();

        // The quotients
        auto quots = layout.cbp_elements<dynamic_t>(size, quot_width);

        Layout r;
        r.quots_layout = quots;
        r.overall_qword_size = layout.get_size_in_uint64_t_units();
        return r;
    }

    /// Creates the pointers to the beginnings of the two arrays inside
    /// the allocation.
    inline static QuotPtr ptr(uint64_t* alloc, size_t size, uint8_t quot_width) {
        DCHECK(size != 0);
        auto layout = calc_sizes(size, quot_width);

        return layout.quots_layout.ptr_relative_to(alloc);
    }

    /// Returns a `val_quot_ptrs_t` to position `pos`,
    /// or a sentinel value that acts as a one-pass-the-end pointer for the empty case.
    inline static QuotPtr at(uint64_t* alloc, size_t size, size_t pos, uint8_t quot_width) {
        if(size != 0) {
            return ptr(alloc, size, quot_width) + pos;
        } else {
            DCHECK_EQ(pos, 0);
            return QuotPtr();
        }
    }
};

}}