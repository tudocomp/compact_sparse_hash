#pragma once

#include <tudocomp/util/compact_hashtable/storage/plain_sentinel_t.hpp>
#include <tudocomp/util/compact_hashtable/placement.hpp>
#include <tudocomp/util/compact_hashtable/generic_compact_hashtable.hpp>
#include <tudocomp/util/compact_hashtable/hash_functions.hpp>

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t, typename hash_t = poplar_xorshift_t>
using compact_displacement_hashtable_t = generic_hashtable_t<hash_t,
    plain_sentinel_t<val_t>,
    displacement_t<compact_displacement_table_t>
>;

}}
