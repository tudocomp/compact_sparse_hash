#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "util.hpp"
#include "bucket_t.hpp"
#include "bucket_element_t.hpp"
#include "hash_functions.hpp"
#include "size_manager_t.hpp"
#include "sparse_pos_t.hpp"
#include "decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

// TODO: Remove unconditional bound checking
// - c, v bvs
// - buckets
// - elements in buckets

template<typename self_t>
class base_table_t {

};

}}
