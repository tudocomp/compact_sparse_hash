#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/compact_sparse_hash.hpp>
#include <tudocomp/util/bits.hpp>

#define COMPACT_TABLE compact_sparse_hashtable_t

static bool print_init = false;
static uint64_t create_counter = 0;

struct Init {
    uint32_t value = 0;
    bool is_special = false;
    bool destroyed = false;

    Init() {
        create_counter++;
        value = create_counter;
        this->is_special = false;
    }

    Init(size_t v, bool is_special) {
        value = v;
        this->is_special = is_special;
    }

    ~Init() {
        EXPECT_FALSE(destroyed);
        destroyed = true;
    }

    Init(Init&& other) {
        this->is_special = other.is_special;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
    }

    Init& operator=(Init&& other) {
        this->is_special = other.is_special;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);

        return *this;
    }

    Init(Init const& other) {
        this->is_special = other.is_special;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
    }

    Init& operator=(Init const& other) {
        this->is_special = other.is_special;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);

        return *this;
    }

    static Init special() {
        return Init(0, true);
    }
    static Init normal(size_t v) {
        return Init(v, false);
    }

    static void reset() {
        create_counter = 0;
    }
};

std::ostream& operator<<(std::ostream& os, Init const& v) {
    return os << "Init(" << v.value << ")";
}

bool operator==(Init const& lhs, Init const& rhs) {
    EXPECT_FALSE(lhs.destroyed);
    EXPECT_FALSE(rhs.destroyed);
    return lhs.value == rhs.value;
}

#include "compact_hash_tests.template.hpp"


