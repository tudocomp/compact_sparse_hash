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
class compact_hashtable_t: base_table_t<compact_hashtable_t<val_t, hash_t>> {
    template<typename T>
    friend class base_table_t;
public:
    /// By-value representation of a value
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
    /// Reference to a value
    using reference_type = ValRef<val_t>;
    /// Pointer to a value
    using pointer_type = ValPtr<val_t>;

private:
    using key_t = uint64_t;

    using key_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    // TODO: Change this, and fix tests
    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_VALUE_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    /// Compact table data (c and v bitvectors)
    IntVector<uint_t<2>> m_cv;

    /// Compact stored data (values and quots)
    IntVector<val_t> m_values;
    IntVector<dynamic_t> m_quots;
    value_type m_empty_value;

    /// Size of table, and width of the stored keys and values
    size_manager_t m_sizing;
    key_width_t m_key_width;
    val_width_t m_val_width;

    /// Hash function
    hash_t m_hash {1};

public:
    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline compact_hashtable_t(size_t size = DEFAULT_TABLE_SIZE,
                               size_t key_width = DEFAULT_KEY_WIDTH,
                               size_t value_width = DEFAULT_VALUE_WIDTH,
                               value_type empty_value = value_type()):
        m_sizing(size),
        m_key_width(key_width),
        m_val_width(value_width),
        m_empty_value(empty_value)
    {
        size_t tsize = table_size();

        m_cv.reserve(tsize);
        m_cv.resize(tsize);

        m_quots.reserve(tsize);
        m_quots.resize(tsize);

        m_values.reserve(tsize);
        m_values.resize(tsize, m_empty_value);

        m_hash = hash_t(real_width());
    }

    inline ~compact_hashtable_t() {
        // NB: destroying the buckets vector will just destroy the bucket in it,
        // which will not destroy their elements.
        run_destructors_of_bucket_elements();
    }

    inline compact_hashtable_t(compact_hashtable_t&& other):
        m_cv(std::move(other.m_cv)),
        m_values(std::move(other.m_values)),
        m_quots(std::move(other.m_quots)),
        m_empty_value(std::move(other.m_empty_value)),
        m_sizing(std::move(other.m_sizing)),
        m_key_width(std::move(other.m_key_width)),
        m_val_width(std::move(other.m_val_width)),
        m_hash(std::move(other.m_hash))
    {
    }

    inline compact_hashtable_t& operator=(compact_hashtable_t&& other) {
        // NB: overwriting the buckets vector will just destroy the bucket in it,
        /// which will not destroy their elements.
        run_destructors_of_bucket_elements();

        m_cv = std::move(other.m_cv);
        m_values = std::move(other.m_values);
        m_quots = std::move(other.m_quots);
        m_empty_value = std::move(other.m_empty_value);
        m_sizing = std::move(other.m_sizing);
        m_key_width = std::move(other.m_key_width);
        m_val_width = std::move(other.m_val_width);
        m_hash = std::move(other.m_hash);

        return *this;
    }

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
        auto raw_key_width = std::max(key_width, m_key_width.get_width());
        auto raw_val_width = std::max(value_width, m_val_width.get_width());

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

        auto raw_key_width = std::max(key_width, m_key_width.get_width());
        auto raw_val_width = std::max(value_width, m_val_width.get_width());

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
        auto raw_key_width = std::max<size_t>(key_width, m_key_width.get_width());
        grow_if_needed(size(), raw_key_width, value_width());
    }

    /// Grow the key and value width as needed.
    ///
    /// Note that it is more efficient to change the width directly during
    /// insertion of a new value.
    inline void grow_kv_width(size_t key_width, size_t value_width) {
        auto raw_key_width = std::max<size_t>(key_width, m_key_width.get_width());
        auto raw_val_width = std::max<size_t>(value_width, m_val_width.get_width());
        grow_if_needed(size(), raw_key_width, raw_val_width);
    }

    /// Returns the amount of elements inside the datastructure.
    inline size_t size() const {
        return m_sizing.size();
    }

    /// Returns the current size of the hashtable.
    /// This value is greater-or-equal the amount of the elements
    /// currently contained in it, which is represented by `size()`.
    inline size_t table_size() {
        return m_sizing.capacity();
    }

    /// Current width of the keys stored in this datastructure.
    inline size_t key_width() {
        return m_key_width.get_width();
    }

    /// Current width of the values stored in this datastructure.
    inline size_t value_width() {
        return m_val_width.get_width();
    }

    /// Amount of bits of the key, that are stored implicitly
    /// by its position in the table.
    inline size_t initial_address_width() {
        return m_sizing.capacity_log2();
    }

    /// Amount of bits of the key, that are stored explicitly
    /// in the buckets.
    inline size_t quotient_width() {
        return real_width() - m_sizing.capacity_log2();
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

    /// Sets the maximum load factor
    /// (how full the table can get before re-allocating).
    ///
    /// Expects a value `0.0 < z < 1.0`.
    inline void max_load_factor(float z) {
        m_sizing.max_load_factor(z);
    }

    /// Returns the maximum load factor.
    inline float max_load_factor() const noexcept {
        return m_sizing.max_load_factor();
    }

    // -----------------------
    // Evaluation and Debugging
    // -----------------------

    struct statistics_t {
        size_t real_allocated_capacity_in_bytes = 0;
        uint64_t theoretical_minimum_size_in_bits = 0;
    };
    inline statistics_t stat_gather() {
        statistics_t r;

        // Calculate real allocated bytes
        // NB: Sizes of members with constant size are not included (eg, sizeof(m_buckets))
        r.real_allocated_capacity_in_bytes += m_values.stat_allocation_size_in_bytes();
        r.real_allocated_capacity_in_bytes += m_quots.stat_allocation_size_in_bytes();
        r.real_allocated_capacity_in_bytes += m_cv.stat_allocation_size_in_bytes();

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

        bool gap_active = false;
        size_t gap_start;
        size_t gap_end;

        auto print_gap = [&](){
            if (gap_active) {
                gap_active = false;
                ss << "    [" << gap_start << " to " << gap_end << " empty]\n";
            }
        };

        auto add_gap = [&](size_t i){
            if (!gap_active) {
                gap_active = true;
                gap_start = i;
            }
            gap_end = i;
        };

        std::vector<std::string> lines(table_size());

        uint64_t initial_address;
        size_t j;
        auto iter = iter_all_t(*this);
        while(iter.next(&initial_address, &j)) {
            std::stringstream ss2;

            auto kv = get_bucket_elem_at(j);
            auto stored_quotient = kv.get_quotient();
            auto val_ptr = kv.val_ptr();
            key_t key = compose_key(initial_address, stored_quotient);

            ss2 << j
                << "\t: v = " << get_v(j)
                << ", c = " << get_c(j)
                << ", quot = " << stored_quotient
                << ", iadr = " << initial_address
                << "\t, key = " << key
                << "\t, value = " << *val_ptr
                << "\t (" << val_ptr << ")";

            lines.at(j) = ss2.str();
        }

        ss << "key_width(): " << key_width() << "\n";
        ss << "size: " << size() << "\n";
        ss << "[\n";
        for (size_t i = 0; i < table_size(); i++) {
            bool cv_exist = lines.at(i) != "";

            DCHECK_EQ(cv_exist, table_pos_contains_value(i));

            if (cv_exist) {
                print_gap();
                ss << "    "
                    << lines.at(i)
                    << "\n";
            } else {
                add_gap(i);
            }
        }
        print_gap();
        ss << "]";

        return ss.str();
    }

private:

};

}}
