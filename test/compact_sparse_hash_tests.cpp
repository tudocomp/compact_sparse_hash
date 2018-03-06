#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/compact_sparse_hash.hpp>
#include <tudocomp/util/bits.hpp>

#define COMPACT_TABLE compact_sparse_hashtable_t
static bool print_init = false;
static uint64_t global_c = 0;
struct Init {
    uint16_t c = 0;
    bool empty = false;
    bool fake = false;

    Init() {
        c = global_c++;
        if (print_init) std::cout << "construct(" << c << ")\n";
    }
    Init(size_t v) {
        c = v;
        if (print_init) std::cout << "construct fake(" << c << ")\n";
        fake = true;
    }
    ~Init() {
        if (empty) {
            if (print_init) std::cout << "destruct(-)\n";
        } else if (fake) {
            if (print_init) std::cout << "destruct fake(" << c << ")\n";
        } else {
            if (print_init) std::cout << "destruct(" << c << ")\n";
        }
    }
    Init(Init&& other) {
        c = other.c;
        other.empty = true;
        if (print_init) std::cout << "move(" << c << ")\n";
    }
    Init& operator=(Init&& other) {
        if (!empty) {
            auto old_c = c;
            c = other.c;
            other.empty = true;
            if (print_init) std::cout << "move(" << other.c << " -> " << old_c << ")\n";
        } else {
            empty = false;
            c = other.c;
            other.empty = true;
            if (print_init) std::cout << "move(" << other.c << " -> -)\n";
        }
        return *this;
    }

    Init(const Init& other) = delete;
    Init& operator=(const Init& other) = delete;

    static void reset() {
        global_c = 0;
    }
};
#include "compact_hash_tests.template.hpp"


