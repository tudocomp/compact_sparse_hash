#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hashset/generic_compact_hashset.hpp>
#include <tudocomp/util/compact_hashset/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hashset/index_structure/cv_bvs_t.hpp>

#include <tudocomp/util/compact_hashmap/generic_compact_hashmap.hpp>
#include <tudocomp/util/compact_hashmap/index_structure/displacement_t.hpp>
#include <tudocomp/util/compact_hashmap/index_structure/cv_bvs_t.hpp>

#include <tudocomp/util/compact_hash/common/hash_functions.hpp>
#include <tudocomp/util/serialization.hpp>

template<typename table_t, typename build_func>
void serialize_test_builder(build_func f) {
    using tdc::serialize;
    auto a = f();

    std::stringstream ss;
    serialize<table_t>::write(ss, a);
    auto b = serialize<table_t>::read(ss);

    ASSERT_TRUE(serialize<table_t>::equal_check(a, b));

    auto c = f();

    ASSERT_TRUE(serialize<table_t>::equal_check(a, c));
    ASSERT_TRUE(serialize<table_t>::equal_check(b, c));
}


template<typename table_t>
void serialize_test() {
    serialize_test_builder<table_t>([] {
        auto ch = table_t(8, 16);
        ch.max_load_factor(1.0);
        ch.lookup_insert(3);
        ch.lookup_insert(3 + 8);
        ch.lookup_insert(5);
        ch.lookup_insert(5 + 8);
        ch.lookup_insert(5 + 16);
        ch.lookup_insert(5 + 24);
        ch.lookup_insert(4);
        return ch;
    });
    serialize_test_builder<table_t>([] {
        auto ch = table_t(8, 16);
        ch.max_load_factor(1.0);
        ch.lookup_insert(3);
        ch.lookup_insert(3 + 8);
        ch.lookup_insert(5);
        ch.lookup_insert(5 + 8);
        ch.lookup_insert(5 + 16);
        ch.lookup_insert(5 + 24);
        ch.lookup_insert(4);
        return ch;
    });

    serialize_test_builder<table_t>([] {
        auto ch = table_t(0, 10);

        auto add = [&](auto key) {
            ch.lookup_insert(key);
        };

        for(size_t i = 0; i < 1000; i++) {
            add(i);
        }

        return ch;
    });

    serialize_test_builder<table_t>([] {
        auto ch = table_t(0, 10);

        uint8_t bits = 1;

        auto add = [&](auto key) {
            bits = std::max(bits, tdc::bits_for(key));

            ch.lookup_insert_key_width(key, bits);

        };

        for(size_t i = 0; i < 1000; i++) {
            add(i);
        }

        return ch;
    });

    serialize_test_builder<table_t>([] {
        auto ch = table_t(0, 0);

        uint8_t bits = 1;

        auto add = [&](auto key) {
            bits = std::max(bits, tdc::bits_for(key));
            ch.lookup_insert_key_width(key, bits);
        };


        for(size_t i = 0; i < 10000; i++) {
            add(i*13ull);
        }

        return ch;
    });
}

#define gen_test(name, ...)        \
TEST(serialize, name) {            \
    serialize_test<__VA_ARGS__>(); \
}

namespace chc = tdc::compact_hash;

namespace chs = tdc::compact_sparse_hashset;

gen_test(set_poplar_displacement_compact_4,
    chs::generic_hashset_t<
        chc::poplar_xorshift_t,
        chs::displacement_t<
            chs::compact_displacement_table_t<4>
        >
    >
)

gen_test(set_poplar_cv,
    chs::generic_hashset_t<
        chc::poplar_xorshift_t,
        chs::cv_bvs_t
    >
)

gen_test(set_poplar_displacement_elias_fixed_1024,
    chs::generic_hashset_t<
        chc::poplar_xorshift_t,
        chs::displacement_t<
            chs::elias_gamma_displacement_table_t<
                chs::fixed_elias_gamma_bucket_size_t<1024>
            >
        >
    >
)

gen_test(set_poplar_displacement_elias_growing,
    chs::generic_hashset_t<
        chc::poplar_xorshift_t,
        chs::displacement_t<
            chs::elias_gamma_displacement_table_t<
                chs::growing_elias_gamma_bucket_size_t
            >
        >
    >
)

// tdc::compact_sparse_hashmap::generic_hashmap_t<poplar::bijective_hash::Xorshift, tdc::compact_sparse_hashmap::buckets_bv_t<unsigned long>, tdc::compact_sparse_hashmap::cv_bvs_t>

/*
namespace chm = tdc::compact_sparse_hashmap;

gen_test(map_poplar_displacement_compact_4,
    chm::generic_hashmap_t<
        chc::poplar_xorshift_t,
        chm::displacement_t<
            chm::compact_displacement_table_t<4>
        >
    >
)

gen_test(map_poplar_cv,
    chm::generic_hashmap_t<
        chc::poplar_xorshift_t,
        chm::cv_bvs_t
    >
)

gen_test(map_poplar_displacement_elias_fixed_1024,
    chm::generic_hashmap_t<
        chc::poplar_xorshift_t,
        chm::displacement_t<
            chm::elias_gamma_displacement_table_t<
                chm::fixed_elias_gamma_bucket_size_t<1024>
            >
        >
    >
)

// tdc::compact_sparse_hashmap::generic_hashmap_t<poplar::bijective_hash::Xorshift, tdc::compact_sparse_hashmap::buckets_bv_t<unsigned long>, tdc::compact_sparse_hashmap::cv_bvs_t>
gen_test(map_poplar_displacement_elias_growing,
    chm::generic_hashmap_t<
        chc::poplar_xorshift_t,
        chm::displacement_t<
            chm::elias_gamma_displacement_table_t<
                chm::growing_elias_gamma_bucket_size_t
            >
        >
    >
)
*/
