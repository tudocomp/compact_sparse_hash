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

#include "compact_hash_tests.template.hpp"


