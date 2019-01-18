#pragma once

#include "storage/entry_ptr_t.hpp"
#include "storage/bucket_data_layout_t.hpp"

namespace tdc {namespace compact_sparse_hashset {

struct no_satellite_data_t {
private:
    using qvd_t = quot_data_seq_t;
    using quot_width_t = uint8_t;
public:
    static constexpr bool has_sentinel = false;
    using entry_ptr_t = quot_ptrs_t;
    using entry_bit_width_t = quot_width_t;

    using bucket_data_layout_t = qvd_t;

};

}}
