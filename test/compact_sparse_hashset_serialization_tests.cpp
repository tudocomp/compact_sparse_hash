#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hashset/generic_compact_hashset.hpp>
#include <tudocomp/util/compact_hashset/hash_functions.hpp>
#include <tudocomp/util/compact_hashset/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hashset/index_structure/cv_bvs_t.hpp>

#include <tudocomp/util/serialization.hpp>

template<typename hash_t = tdc::compact_sparse_hashset::poplar_xorshift_t>
using compact_sparse_displacement_hashset_t = tdc::compact_sparse_hashset::generic_hashset_t<
    hash_t,
    tdc::compact_sparse_hashset::displacement_t<
        tdc::compact_sparse_hashset::compact_displacement_table_t<4>
    >
>;


template<typename hash_t = tdc::compact_sparse_hashset::poplar_xorshift_t>
using compact_sparse_hashset_t = tdc::compact_sparse_hashset::generic_hashset_t<
    hash_t,
    tdc::compact_sparse_hashset::cv_bvs_t
>;


template<typename hash_t = tdc::compact_sparse_hashset::poplar_xorshift_t>
using compact_sparse_elias_displacement_hashset_t = tdc::compact_sparse_hashset::generic_hashset_t<
    hash_t,
    tdc::compact_sparse_hashset::displacement_t<
        tdc::compact_sparse_hashset::elias_gamma_displacement_table_t<
            tdc::compact_sparse_hashset::fixed_elias_gamma_bucket_size_t<1024>
        >
    >
>;


template<typename table_t>
void serialize_test() {
    using tdc::serialize;
    auto a = table_t(8, 16);
    {
        auto& ch = a;
        ch.max_load_factor(1.0);
        ch.lookup_insert(3);
        ch.lookup_insert(3 + 8);
        ch.lookup_insert(5);
        ch.lookup_insert(5 + 8);
        ch.lookup_insert(5 + 16);
        ch.lookup_insert(5 + 24);
        ch.lookup_insert(4);
    }

    std::stringstream ss;
    serialize<table_t>::write(ss, a);
    auto b = serialize<table_t>::read(ss);
}

TEST(serialize, compact_sparse_displacement_hashset_t) {
    serialize_test<compact_sparse_displacement_hashset_t<>>();
}

TEST(serialize, compact_sparse_hashset_t) {
    serialize_test<compact_sparse_hashset_t<>>();
}

TEST(serialize, compact_sparse_elias_displacement_hashset_t) {
    serialize_test<compact_sparse_elias_displacement_hashset_t<>>();
}
