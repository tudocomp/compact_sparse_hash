#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hash/set/hashset_t.hpp>
#include <tudocomp/util/compact_hash/hash_functions.hpp>
#include <tudocomp/util/compact_hash/index_structure/displacement_t.hpp>

using namespace tdc::compact_hash;
using namespace tdc::compact_hash::set;

template<typename hash_t = poplar_xorshift_t>
using compact_sparse_elias_displacement_hashset_t = hashset_t<
    hash_t,
    displacement_t<
        elias_gamma_displacement_table_t<
            fixed_elias_gamma_bucket_size_t<1024>
        >
    >
>;

using COMPACT_TABLE = compact_sparse_elias_displacement_hashset_t<>;

#include "compact_hashset_tests.template.hpp"
