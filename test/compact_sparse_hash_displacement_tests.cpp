#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/compact_hashtable/table.hpp>
#include <tudocomp/util/compact_hashtable/placement.hpp>
#include <tudocomp/util/compact_hashtable/adapter.hpp>
#include <tudocomp/util/compact_hashtable/hash_functions.hpp>
#include <tudocomp/util/bits.hpp>

using namespace tdc::compact_sparse_hashtable;

template<typename val_t>
using COMPACT_TABLE = generic_hashtable_t<poplar_xorshift_t, buckets_bv_t<val_t>, displacement_t<naive_displacement_table_t>>;

#include "compact_hash_tests.template.hpp"
