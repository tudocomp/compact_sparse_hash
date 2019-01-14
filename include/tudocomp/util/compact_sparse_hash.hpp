#pragma once

#include <tudocomp/util/compact_hash/common/hash_functions.hpp>
#include <tudocomp/util/compact_hashmap/storage/buckets_bv_t.hpp>
#include <tudocomp/util/compact_hashmap/index_structure/cv_bvs_t.hpp>
#include <tudocomp/util/compact_hashmap/generic_compact_hashmap.hpp>

namespace tdc {namespace compact_sparse_hashmap {

template<typename val_t, typename hash_t = compact_hash::poplar_xorshift_t>
using compact_sparse_hashmap_t = generic_hashmap_t<hash_t,
    buckets_bv_t<val_t>,
    cv_bvs_t
>;

}}
