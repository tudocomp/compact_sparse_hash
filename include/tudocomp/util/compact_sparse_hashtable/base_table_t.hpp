#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "util.hpp"
#include "bucket_t.hpp"
#include "val_quot_ptrs_t.hpp"
#include "hash_functions.hpp"
#include "size_manager_t.hpp"
#include "sparse_pos_t.hpp"
#include "decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

// TODO: Remove unconditional bound checking
// - c, v bvs
// - buckets
// - elements in buckets

template<template<typename> typename val_quot_storage_t, typename val_t, typename hash_t>
class base_table_t {
public:
    /// By-value representation of a value
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
    /// Reference to a value
    using reference_type = ValRef<val_t>;
    /// Pointer to a value
    using pointer_type = ValPtr<val_t>;

    inline base_table_t(size_t size,
                        size_t key_width,
                        size_t value_width
                       ):
        m_sizing(size),
        m_key_width(key_width),
        m_val_width(value_width)
    {
        size_t cv_size = table_size();

        m_cv.reserve(cv_size);
        m_cv.resize(cv_size);

        m_hash = hash_t(real_width());

        m_storage = val_quot_storage_t<val_t>(cv_size);
    }

    inline base_table_t(base_table_t&& other) {
        do_move(std::move(other));
    }
    inline base_table_t& operator=(base_table_t&& other) {
        // NB: overwriting the storage does not automatically destroy the values in them.
        run_destructors_of_elements();
        do_move(std::move(other));
        return *this;
    }

    // NB: These just exist to catch bugs, and could be removed
    inline base_table_t(base_table_t const& other) = delete;
    inline base_table_t& operator=(base_table_t  const& other) = delete;

    inline ~base_table_t() {
        // NB: overwriting the storage does not automatically destroy the values in them.
        run_destructors_of_elements();
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

    inline typename val_quot_storage_t::statistics_t stat_gather() {
        return m_storage.stat_gather();
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

            auto kv = get_val_quot_at(j);
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
    using key_t = uint64_t;
    using key_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    /// Compact table data (c and v bitvectors)
    IntVector<uint_t<2>> m_cv;

    /// Value and quotient storage
    val_quot_storage_t<val_t> m_storage;

    /// Size of table, and width of the stored keys and values
    size_manager_t m_sizing;
    key_width_t m_key_width;
    val_width_t m_val_width;

    /// Hash function
    hash_t m_hash {1};

    inline void do_move(base_table_t&& other) {
        m_cv = std::move(other.m_cv);
        m_storage = std::move(other.m_storage);
        m_sizing = std::move(other.m_sizing);
        m_key_width = std::move(other.m_key_width);
        m_val_width = std::move(other.m_val_width);
        m_hash = std::move(other.m_hash);
    }

    /// The actual amount of bits currently usable for
    /// storing a key in the hashtable.
    ///
    /// Due to implementation details, this can be
    /// larger than `key_width()`.
    ///
    /// Specifically, there are currently two cases:
    /// - If all bits of the the key fit into the initial-address space,
    ///   then the quotient bitvector inside the buckets would
    ///   have to store integers of width 0. This is undefined behavior
    ///   with the current code, so we add a padding bit.
    /// - Otherwise the current maximum key width `m_key_width`
    ///   determines the real width.
    inline size_t real_width() {
        return std::max<size_t>(m_sizing.capacity_log2() + 1, m_key_width.get_width());
    }

    /// Handler for inserting an element that exists as a rvalue reference.
    /// This will overwrite an existing element.
    class InsertHandler {
        value_type&& m_value;
    public:
        InsertHandler(value_type&& value): m_value(std::move(value)) {}

        inline auto on_new() {
            struct InsertHandlerOnNew {
                value_type&& m_value;
                inline value_type&& get() {
                    return std::move(m_value);
                }
                inline void new_location(pointer_type value) {
                    // don't care
                }
            };

            return InsertHandlerOnNew {
                std::move(m_value),
            };
        }

        inline void on_existing(pointer_type value) {
            *value = std::move(m_value);
        }
    };

    /// Handler for getting the address of an element in the map.
    /// If none exists yet, it will be default constructed.
    class AddressDefaultHandler {
        pointer_type* m_address = nullptr;
    public:
        AddressDefaultHandler(pointer_type* address): m_address(address) {}

        inline auto on_new() {
            struct AddressDefaultHandlerOnNew {
                value_type m_value;
                pointer_type* m_address;
                inline value_type&& get() {
                    return std::move(m_value);
                }
                inline void new_location(pointer_type value) {
                    *m_address = value;
                }
            };

            return AddressDefaultHandlerOnNew {
                value_type(),
                m_address,
            };
        }

        inline void on_existing(pointer_type value) {
            *m_address = value;
        }
    };


    /// Getter for the v bit at table position `pos`.
    inline bool get_v(size_t pos) {
        return (m_cv[pos] & 0b01) != 0;
    }

    /// Getter for the c bit at table position `pos`.
    inline bool get_c(size_t pos) {
        return (m_cv[pos] & 0b10) != 0;
    }

    /// Setter for the v bit at table position `pos`.
    inline void set_v(size_t pos, bool v) {
        auto x = m_cv[pos] & 0b10;
        m_cv[pos] = x | (0b01 * v);
    }

    /// Setter for the c bit at table position `pos`.
    inline void set_c(size_t pos, bool c) {
        auto x = m_cv[pos] & 0b01;
        m_cv[pos] = x | (0b10 * c);
    }

    /// Debug check that a key does not occupy more bits than the
    /// hashtable currently allows.
    inline bool dcheck_key_width(uint64_t key) {
        uint64_t key_mask = (1ull << (key_width() - 1ull) << 1ull) - 1ull;
        bool key_is_too_large = key & ~key_mask;
        return !key_is_too_large;
    }

    /// Decompose a key into its initial address and quotient.
    inline decomposed_key_t decompose_key(uint64_t key) {
        DCHECK(dcheck_key_width(key)) << "Attempt to decompose key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not.";

        uint64_t hres = m_hash.hash(key);

        DCHECK_EQ(m_hash.hash_inv(hres), key);

        return m_sizing.decompose_hashed_value(hres);
    }

    /// Compose a key from its initial address and quotient.
    inline uint64_t compose_key(uint64_t initial_address, uint64_t quotient) {
        uint64_t harg = m_sizing.compose_hashed_value(initial_address, quotient);
        uint64_t key = m_hash.hash_inv(harg);

        DCHECK(dcheck_key_width(key)) << "Composed key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not.";
        return key;
    }

    /// Run the destructors of the bucket elements,
    /// but don't drop them from the table.
    ///
    /// WARNING: This should only be called
    /// before an operation that would call the destructors of the buckets
    /// themselves, like in the destructor of the hashtable.
    ///
    /// The reason this exists is that a bucket_t does not
    /// initialize or destroy the elements in it automatically,
    /// to prevent unneeded empty-constructions of its elements.
    /// TODO: Is this still a useful semantic? A bucket_t can manage its own data.
    inline void run_destructors_of_elements() {
        size_t qw = quotient_width();
        size_t vw = value_width();
        m_storage.run_destructors_of_elements(qw, vw);
    }
};

}}
