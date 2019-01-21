#pragma once

#include <tudocomp/util/compact_hash/map/hashmap_t.hpp>
#include <tudocomp/util/compact_hash/hash_functions.hpp>
#include <tudocomp/util/compact_hash/storage/buckets_bv_t.hpp>
#include <tudocomp/util/compact_hash/index_structure/cv_bvs_t.hpp>

namespace tdc {namespace compact_hash {namespace map {

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_sparse_hashmap_t
    = hashmap_t<val_t, hash_t, buckets_bv_t, cv_bvs_t>;

}}}
