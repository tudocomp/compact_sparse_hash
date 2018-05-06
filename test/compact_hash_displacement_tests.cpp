#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/v2/table.hpp>
#include <tudocomp/util/v2/placement.hpp>
#include <tudocomp/util/v2/adapter.hpp>
#include <tudocomp/util/v2/hash_functions.hpp>
#include <tudocomp/util/bits.hpp>

using namespace tdc::compact_sparse_hashtable;

template<typename val_t>
using COMPACT_TABLE = generic_hashtable_t<poplar_xorshift_t, plain_sentinel_t<val_t>, displacement_t<naive_displacement_table_t>>;

#include "compact_hash_tests.template.hpp"
