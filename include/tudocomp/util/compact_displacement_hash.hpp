#pragma once

#include <tudocomp/util/compact_hashmap/storage/plain_sentinel_t.hpp>
#include <tudocomp/util/compact_hashmap/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hashmap/generic_compact_hashmap.hpp>
#include <tudocomp/util/compact_hash/common/hash_functions.hpp>

namespace tdc {namespace compact_sparse_hashmap {

template<typename val_t, typename hash_t = compact_hash::poplar_xorshift_t>
using compact_displacement_hashmap_t = generic_hashmap_t<hash_t,
    plain_sentinel_t<val_t>,
    displacement_t<compact_displacement_table_t>
>;

template<typename val_t, typename hash_t = compact_hash::poplar_xorshift_t>
using compact_elias_displacement_hashmap_t = generic_hashmap_t<hash_t,
    plain_sentinel_t<val_t>,
    displacement_t<elias_gamma_displacement_table_t<fixed_elias_gamma_bucket_size_t<1024>>>
>;

}}
