#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/v2/table.hpp>
#include <tudocomp/util/v2/placement.hpp>
#include <tudocomp/util/v2/adapter.hpp>
#include <tudocomp/util/v2/bucket_t.hpp>
#include <tudocomp/util/bit_packed_layout_t.hpp>

using namespace tdc::compact_sparse_hashtable;
using namespace tdc;

template<typename val_t>
void BucketTest() {
    using widths_t = typename bucket_t<val_t, 8>::widths_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    auto b = bucket_t<val_t, 8>();

    val_width_t vw { 7 };
    widths_t ws { 5, vw };
    b = bucket_t<val_t, 8>(0b10, ws);

    ASSERT_EQ(b.bv(), 2);
    ASSERT_EQ(b.size(), 1);
    ASSERT_EQ(b.is_empty(), false);

    auto p1 = b.at(0, ws);
    p1.set_no_drop(3, 4);

    b.stat_allocation_size_in_bytes(ws);

    auto p2 = b.insert_at(0, 0b11, ws);
    p2.set_no_drop(5, 6);

    p2.set(7, 8);

    b.destroy_vals(ws);
}

#define MakeBucketTest(tname, tty) \
TEST(Bucket, tname##_test) {  \
    BucketTest<tty>();    \
}

MakeBucketTest(uint8_t, uint8_t);
MakeBucketTest(uint64_t, uint64_t);
MakeBucketTest(dynamic_t, dynamic_t);
MakeBucketTest(uint_t40, uint_t<40>);

template<template<typename> typename table_t, typename val_t>
void TableTest() {
    using tab_t = table_t<val_t>;
    using widths_t = typename bucket_t<val_t, 8>::widths_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    auto t = tab_t();

    val_width_t vw { 7 };
    widths_t ws { 5, vw };
    size_t table_size = 16;
    t = tab_t(table_size, ws);

    for(size_t i = 0; i < table_size; i++) {
        auto pos = t.table_pos(i, table_size, ws);
        ASSERT_EQ(t.pos_is_empty(pos), true);

        auto elem = t.allocate_pos(pos);
        elem.set_no_drop(i + 1, i + 2);
    }

    for(size_t i = 0; i < table_size; i++) {
        auto pos = t.table_pos(i, table_size, ws);
        ASSERT_EQ(t.pos_is_empty(pos), false);

        auto elem = t.at(pos);
        ASSERT_EQ(*elem.val_ptr(), i + 1);
        ASSERT_EQ(elem.get_quotient(), i + 2);
    }

}

#define MakeTableTest(tab, tname, tty)     \
TEST(Table, tab##_##tname##_test) {        \
    TableTest<tab, tty>(); \
}

MakeTableTest(plain_sentinel_t, uint8_t, uint8_t);
MakeTableTest(buckets_bv_t,     uint8_t, uint8_t);
MakeTableTest(plain_sentinel_t, uint64_t, uint64_t);
MakeTableTest(buckets_bv_t,     uint64_t, uint64_t);
MakeTableTest(plain_sentinel_t, dynamic_t, dynamic_t);
MakeTableTest(buckets_bv_t,     dynamic_t, dynamic_t);
MakeTableTest(plain_sentinel_t, uint_t40, uint_t<40>);
MakeTableTest(buckets_bv_t,     uint_t40, uint_t<40>);
