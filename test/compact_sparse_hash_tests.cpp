#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/compact_sparse_hash.hpp>
#include <tudocomp/util/bits.hpp>

#define COMPACT_TABLE compact_sparse_hashtable_t

static bool print_init = false;
static uint64_t create_counter = 0;
static uint64_t move_counter = 0;
static uint64_t copy_counter = 0;
static uint64_t destroy_counter = 0;

struct Init {
    uint32_t value = 0;
    bool allow_move = false;
    bool allow_copy = false;
    bool destroyed = false;

    Init() {
        create_counter++;
    }

    Init(size_t v, bool allow_copy, bool allow_move) {
        value = v;
        this->allow_copy = allow_copy;
        this->allow_move = allow_move;
        create_counter++;
    }

    ~Init() {
        EXPECT_FALSE(destroyed);
        destroyed = true;
        destroy_counter++;
    }

    Init(Init&& other) {
        this->allow_copy = other.allow_copy;
        this->allow_move = other.allow_move;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
        EXPECT_TRUE(allow_move);
        move_counter++;
    }

    Init& operator=(Init&& other) {
        this->allow_copy = other.allow_copy;
        this->allow_move = other.allow_move;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
        EXPECT_TRUE(allow_move);
        move_counter++;

        return *this;
    }

    Init(Init const& other) {
        this->allow_copy = other.allow_copy;
        this->allow_move = other.allow_move;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
        EXPECT_TRUE(allow_copy);
        copy_counter++;
    }

    Init& operator=(Init const& other) {
        this->allow_copy = other.allow_copy;
        this->allow_move = other.allow_move;
        this->destroyed = other.destroyed;
        this->value = other.value;

        EXPECT_FALSE(destroyed);
        EXPECT_TRUE(allow_copy);
        copy_counter++;

        return *this;
    }

    static Init c(size_t v) {
        return Init(v, true, false);
    }
    static Init m(size_t v) {
        return Init(v, false, true);
    }
    static Init cm(size_t v) {
        return Init(v, true, true);
    }

    static void reset() {
        create_counter = 0;
        move_counter = 0;
        copy_counter = 0;
        destroy_counter = 0;
    }

    template<typename F>
    static void check(F f) {
        f(create_counter, move_counter, copy_counter, destroy_counter);
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


