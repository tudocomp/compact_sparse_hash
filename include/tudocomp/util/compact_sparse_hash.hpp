#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/util/compact_hashtable/table.hpp>
#include <tudocomp/util/compact_hashtable/placement.hpp>
#include <tudocomp/util/compact_hashtable/adapter.hpp>
#include <tudocomp/util/compact_hashtable/hash_functions.hpp>

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_sparse_hashtable_t = generic_hashtable_t<hash_t, buckets_bv_t<val_t>, cv_bvs_t>;

}}
