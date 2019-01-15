#pragma once

namespace tdc {namespace compact_sparse_hashmap {

template<typename sentinel_type>
struct sentinel_config_t {
    static constexpr bool has_sentinel = true;

};

}}
