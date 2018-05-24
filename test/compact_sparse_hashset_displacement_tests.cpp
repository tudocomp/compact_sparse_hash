#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hashset/generic_compact_hashset.hpp>
#include <tudocomp/util/compact_hashset/hash_functions.hpp>
#include <tudocomp/util/compact_hashset/index_structure/displacement_t.hpp>

template<typename hash_t = tdc::compact_sparse_hashset::poplar_xorshift_t>
using compact_sparse_displacement_hashset_t = tdc::compact_sparse_hashset::generic_hashset_t<hash_t,
    tdc::compact_sparse_hashset::displacement_t<tdc::compact_sparse_hashset::compact_displacement_table_t>
>;

using COMPACT_TABLE = compact_sparse_displacement_hashset_t<>;

#include "compact_hashset_tests.template.hpp"
