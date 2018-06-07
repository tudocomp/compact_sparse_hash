#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>

#include <tudocomp/util/compact_sparse_displacement_hash.hpp>

template<typename val_t>
using COMPACT_TABLE = tdc::compact_sparse_hashmap::compact_sparse_displacement_hashmap_t<val_t>;

#include "compact_hash_tests.template.hpp"
