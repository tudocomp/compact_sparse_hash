#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "compact_sparse_hashtable/util.hpp"
#include "compact_sparse_hashtable/bucket_t.hpp"
#include "compact_sparse_hashtable/bucket_element_t.hpp"
#include "compact_sparse_hashtable/hash_functions.hpp"
#include "compact_sparse_hashtable/size_manager_t.hpp"
#include "compact_sparse_hashtable/sparse_pos_t.hpp"
#include "compact_sparse_hashtable/decomposed_key_t.hpp"
#include "compact_sparse_hashtable/base_table_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

// TODO: Remove unconditional bound checking
// - c, v bvs
// - buckets
// - elements in buckets

template<typename val_t, typename hash_t = poplar_xorshift_t>
class compact_hashtable_t: public base_table_t<val_t, hash_t> {
    using base_t = base_table_t<val_t, hash_t>;
    friend base_t;
    using base_t::real_width;
    using typename base_t::InsertHandler;
    using typename base_t::AddressDefaultHandler;
    using base_t::get_v;
    using base_t::get_c;
    using base_t::set_v;
    using base_t::set_c;
    using base_t::m_hash;
    using base_t::m_sizing;
    using base_t::decompose_key;
    using base_t::compose_key;

    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_VALUE_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    /// Compact stored data (values and quots)
    IntVector<val_t> m_values;
    IntVector<dynamic_t> m_quots;
    typename base_t::value_type m_empty_value;

public:
    using typename base_t::value_type;
    using typename base_t::reference_type;
    using typename base_t::pointer_type;
    using base_t::size;
    using base_t::table_size;
    using base_t::key_width;
    using base_t::value_width;
    using base_t::quotient_width;

    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline compact_hashtable_t(size_t size = DEFAULT_TABLE_SIZE,
                               size_t key_width = DEFAULT_KEY_WIDTH,
                               size_t value_width = DEFAULT_VALUE_WIDTH,
                               value_type const& empty_value = value_type()):
        base_t(size, key_width, value_width),
        m_empty_value(empty_value)
    {
        size_t tsize = table_size();

        m_quots.reserve(tsize);
        m_quots.resize(tsize);

        m_values.reserve(tsize);
        m_values.resize(tsize, m_empty_value);
    }

    inline compact_hashtable_t(compact_hashtable_t&& other) = default;
    inline compact_hashtable_t& operator=(compact_hashtable_t&& other) = default;

    // TODO: Change to STL-conform interface?

    /// Inserts a key-value pair into the hashtable.
    inline void insert(uint64_t key, value_type&& value) {
        insert_kv_width(key, std::move(value), key_width(), value_width());
    }

    /// Inserts a key-value pair into the hashtable,
    /// and grow the key width as needed.
    inline void insert_key_width(uint64_t key, value_type&& value, uint8_t key_width) {
        insert_kv_width(key, std::move(value), key_width, value_width());
    }

    /// Inserts a key-value pair into the hashtable,
    /// and grow the key and value width as needed.
    inline void insert_kv_width(uint64_t key, value_type&& value, uint8_t key_width, uint8_t value_width) {
        auto raw_key_width = std::max<size_t>(key_width, this->key_width());
        auto raw_val_width = std::max<size_t>(value_width, this->value_width());

        access_with_handler(key, raw_key_width, raw_val_width, InsertHandler {
            std::move(value)
        });
    }

    /// Returns a reference to the element with key `key`.
    ///
    /// If the value does not already exist in the table, it will be
    /// default-constructed.
    inline reference_type access(uint64_t key) {
        return access_kv_width(key, key_width(), value_width());
    }

    /// Returns a reference to the element with key `key`,
    /// and grow the key width as needed.
    ///
    /// If the value does not already exist in the table, it will be
    /// default-constructed.
    inline reference_type access_key_width(uint64_t key, uint8_t key_width) {
        return access_kv_width(key, key_width, value_width());
    }

    /// Returns a reference to the element with key `key`,
    /// and grow the key and value width as needed.
    ///
    /// If the value does not already exist in the table, it will be
    /// default-constructed.
    inline reference_type access_kv_width(uint64_t key, uint8_t key_width, uint8_t value_width) {
        pointer_type addr = pointer_type();

        auto raw_key_width = std::max<size_t>(key_width, this->key_width());
        auto raw_val_width = std::max<size_t>(value_width, this->value_width());

        access_with_handler(key, raw_key_width, raw_val_width, AddressDefaultHandler {
            &addr
        });

        DCHECK(addr != pointer_type());

        return *addr;
    }

    /// Returns a reference to the element with key `key`.
    ///
    /// This has the same semantic is `access(key)`.
    inline reference_type operator[](uint64_t key) {
        return access(key);
    }

    /// Grow the key width as needed.
    ///
    /// Note that it is more efficient to change the width directly during
    /// insertion of a new value.
    inline void grow_key_width(size_t key_width) {
        auto raw_key_width = std::max<size_t>(key_width, this->key_width());
        grow_if_needed(size(), raw_key_width, value_width());
    }

    /// Grow the key and value width as needed.
    ///
    /// Note that it is more efficient to change the width directly during
    /// insertion of a new value.
    inline void grow_kv_width(size_t key_width, size_t value_width) {
        auto raw_key_width = std::max<size_t>(key_width, this->key_width());
        auto raw_val_width = std::max<size_t>(value_width, this->value_width());
        grow_if_needed(size(), raw_key_width, raw_val_width);
    }

    // TODO: STL-conform API?
    /// Search for a key inside the hashtable.
    ///
    /// This returns a pointer to the value if its found, or null
    /// otherwise.
    inline pointer_type search(uint64_t key) {
        auto dkey = decompose_key(key);
        if (get_v(dkey.initial_address)) {
            return search_in_group(search_existing_group(dkey), dkey.stored_quotient);
        }
        return pointer_type();
    }

    // -----------------------
    // Evaluation and Debugging
    // -----------------------

    struct statistics_t {
        size_t buckets = 1;
        size_t allocated_buckets = 1;
        size_t buckets_real_allocated_capacity_in_bytes = 0;

        size_t real_allocated_capacity_in_bytes = 0;
        uint64_t theoretical_minimum_size_in_bits = 0;
    };
    inline statistics_t stat_gather() {
        statistics_t r;

        // Calculate real allocated bytes
        // NB: Sizes of members with constant size are not included (eg, sizeof(m_buckets))
        r.real_allocated_capacity_in_bytes += m_values.stat_allocation_size_in_bytes();
        r.real_allocated_capacity_in_bytes += m_quots.stat_allocation_size_in_bytes();
        r.real_allocated_capacity_in_bytes += this->m_cv.stat_allocation_size_in_bytes();

        // Calculate minimum bits needed
        // Quotient bitvectors across all buckets
        r.theoretical_minimum_size_in_bits += size() * quotient_width();
        // Values across all buckets
        r.theoretical_minimum_size_in_bits += size() * value_width();
        // Size of cv bitvectors
        r.theoretical_minimum_size_in_bits += size() * 2;

        return r;
    }

    /// Returns a human-readable string representation
    /// of the entire state of the hashtable
    inline std::string debug_state() {
        std::stringstream ss;
        return ss.str();
    }

private:

};

}}
