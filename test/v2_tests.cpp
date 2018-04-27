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

TEST(Foo, bar) {

}

template<typename val_t>
void BucketTest() {
    using widths_t = typename bucket_t<val_t, 8>::widths_t;

    auto b = bucket_t<val_t, 8>();

    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;
    val_width_t vw { 7 };
    widths_t ws { 5, vw };
    b = bucket_t<val_t, 8>(2, ws);

    ASSERT_EQ(b.bv(), 2);
    ASSERT_EQ(b.size(), 1);
    ASSERT_EQ(b.is_empty(), false);

    b.at(0, ws);

    b.stat_allocation_size_in_bytes(ws);

    b.destroy_vals(ws);
}

TEST(Bucket, uint8_t_test) {
    BucketTest<uint8_t>();
}

TEST(Bucket, uint64_t_test) {
    BucketTest<uint64_t>();
}

TEST(Bucket, dynamic_t_test) {
    BucketTest<dynamic_t>();
}

