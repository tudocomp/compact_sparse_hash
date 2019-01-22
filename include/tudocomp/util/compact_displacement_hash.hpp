#pragma once

#include <tudocomp/util/compact_hash/map/hashmap_t.hpp>
#include <tudocomp/util/compact_hash/hash_functions.hpp>
#include <tudocomp/util/compact_hash/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hash/storage/plain_sentinel_t.hpp>

namespace tdc {namespace compact_hash {namespace map {

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_displacement_hashmap_t = hashmap_t<
    val_t, hash_t, plain_sentinel_t,
    displacement_t<layered_displacement_table_t<4>>
>;

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_elias_displacement_hashmap_t = hashmap_t<
    val_t, hash_t, plain_sentinel_t,
    displacement_t<elias_gamma_displacement_table_t<fixed_elias_gamma_bucket_size_t<1024>>>
>;

}}}
