#pragma once

#include "storage/val_quot_ptrs_t.hpp"
#include "storage/quot_val_data.hpp"

namespace tdc {namespace compact_sparse_hashmap {

template<typename val_t>
struct satellite_data_t {
private:
    using qvd_t = quot_val_data_seq_t<val_t>;
    using widths_t = typename qvd_t::QVWidths;
public:
    static constexpr bool has_sentinel = true;
    using entry_ptr_t = val_quot_ptrs_t<val_t>;
    using entry_bit_width_t = widths_t;


    using tmp_val_t = val_t;
};

}}
