#pragma once

#include <tudocomp/util/compact_hashmap/storage/buckets_bv_t.hpp>
#include <tudocomp/util/compact_hashmap/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hashmap/generic_compact_hashmap.hpp>
#include <tudocomp/util/compact_hashmap/hash_functions.hpp>

namespace tdc {namespace compact_sparse_hashmap {

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_sparse_displacement_hashmap_t = generic_hashmap_t<hash_t,
    buckets_bv_t<val_t>,
    displacement_t<compact_displacement_table_t>
>;

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_sparse_elias_displacement_hashmap_t = generic_hashmap_t<hash_t,
    buckets_bv_t<val_t>,
    displacement_t<elias_gamma_displacement_table_t>
>;

}}
