#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hashset/generic_compact_hashset.hpp>
#include <tudocomp/util/compact_hash/common/hash_functions.hpp>
#include <tudocomp/util/compact_hashset/index_structure/displacement_t.hpp>

template<typename hash_t = tdc::compact_hash::poplar_xorshift_t>
using compact_sparse_elias_displacement_hashset_t = tdc::compact_sparse_hashset::generic_hashset_t<
    hash_t,
    tdc::compact_sparse_hashset::displacement_t<
        tdc::compact_sparse_hashset::elias_gamma_displacement_table_t<
            tdc::compact_sparse_hashset::fixed_elias_gamma_bucket_size_t<1024>
        >
    >
>;

using COMPACT_TABLE = compact_sparse_elias_displacement_hashset_t<>;

#include "compact_hashset_tests.template.hpp"
