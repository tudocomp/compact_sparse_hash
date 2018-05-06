#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/v2/table.hpp>
#include <tudocomp/util/v2/placement.hpp>
#include <tudocomp/util/v2/adapter.hpp>
#include <tudocomp/util/v2/hash_functions.hpp>
#include <tudocomp/util/bits.hpp>

using namespace tdc;
using namespace tdc::compact_sparse_hashtable;

using uint_t40 = uint_t<40>;
using naive_displacement_t = displacement_t<naive_displacement_table_t>;
using compact_displacement_t = displacement_t<compact_displacement_table_t>;
template<typename val_t>
using csh_test_t = generic_hashtable_t<poplar_xorshift_t, buckets_bv_t<val_t>, cv_bvs_t>;
template<typename val_t>
using ch_test_t = generic_hashtable_t<poplar_xorshift_t, plain_sentinel_t<val_t>, cv_bvs_t>;

template<typename val_t>
using csh_disp_test_t = generic_hashtable_t<poplar_xorshift_t, buckets_bv_t<val_t>, naive_displacement_t>;
template<typename val_t>
using ch_disp_test_t = generic_hashtable_t<poplar_xorshift_t, plain_sentinel_t<val_t>, naive_displacement_t>;

#define COMPACT_TABLE csh_test_t

struct Init {
    uint32_t value = 0;
    bool m_copyable;

    Init() {
        value = 1;
        m_copyable = true;
    }

    Init(size_t v, bool copyable = false) {
        value = v + 2;
        m_copyable = copyable;
    }

    ~Init() {
        EXPECT_NE(value, 0) << "destroying a already destroyed value";
        value = 0;
    }

    Init(Init&& other) {
        EXPECT_NE(other.value, 0) << "moving from a already destroyed value";

        m_copyable = other.m_copyable;
        this->value = other.value;
    }

    Init& operator=(Init&& other) {
        EXPECT_NE(other.value, 0) << "moving from a already destroyed value";
        EXPECT_NE(value, 0) << "moving into a already destroyed value";

        m_copyable = other.m_copyable;
        this->value = other.value;

        return *this;
    }

    Init(Init const& other) {
        EXPECT_NE(other.value, 0) << "moving from a already destroyed value";
        EXPECT_TRUE(other.m_copyable) << "copying a non-copyable value";

        if (!other.m_copyable) {
            throw std::runtime_error("asd");
        }

        m_copyable = other.m_copyable;
        this->value = other.value;
    }

    Init& operator=(Init const& other) {
        EXPECT_NE(value, 0) << "moving into a already destroyed value";
        EXPECT_NE(other.value, 0) << "moving from a already destroyed value";
        EXPECT_TRUE(other.m_copyable) << "copying a non-copyable value";

        m_copyable = other.m_copyable;
        this->value = other.value;

        return *this;
    }

    static Init copyable(size_t v) {
        return Init(v, true);
    }
};

std::ostream& operator<<(std::ostream& os, Init const& v) {
    if (v.value == 0) {
        return os << "Init(<destroyed>)";
    } else if (v.value == 1) {
        return os << "Init()";
    } else {
        return os << "Init(" << (v.value - 2) << ")";
    }
}

bool operator==(Init const& lhs, Init const& rhs) {
    EXPECT_NE(lhs.value, 0);
    EXPECT_NE(rhs.value, 0);
    return lhs.value == rhs.value;
}

#include "compact_hash_tests.template.hpp"


