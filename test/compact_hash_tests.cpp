#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_hash.hpp>

template<typename val_t>
using COMPACT_TABLE = tdc::compact_sparse_hashmap::compact_hashmap_t<val_t>;

#include "compact_hash_tests.template.hpp"
