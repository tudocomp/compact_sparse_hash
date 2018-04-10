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

    // TODO: make private or protected
    using TablePos = typename val_quot_storage_t<val_t>::TablePos;

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

    inline base_table_t(base_table_t&& other):
        m_cv(std::move(other.m_cv)),
        m_storage(std::move(other.m_storage)),
        m_sizing(std::move(other.m_sizing)),
        m_key_width(std::move(other.m_key_width)),
        m_val_width(std::move(other.m_val_width)),
        m_hash(std::move(other.m_hash))
    {}
    inline base_table_t& operator=(base_table_t&& other) {
        // NB: overwriting the storage does not automatically destroy the values in them.
        run_destructors_of_elements();

        m_cv = std::move(other.m_cv);
        m_storage = std::move(other.m_storage);
        m_sizing = std::move(other.m_sizing);
        m_key_width = std::move(other.m_key_width);
        m_val_width = std::move(other.m_val_width);
        m_hash = std::move(other.m_hash);

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

    inline typename val_quot_storage_t<val_t>::statistics_t stat_gather() {
        return m_storage.stat_gather(quotient_width(),
                                     value_width(),
                                     size(),
                                     m_cv.stat_allocation_size_in_bytes());
    }

    /// Returns a human-readable string representation
    /// of the entire state of the hashtable
    inline std::string debug_state() {
        std::stringstream ss;

        /*

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

        */

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

    /// Access the element represented by `handler` under
    /// the key `key` with the, possibly new, width of `key_width` bits.
    ///
    /// `handler` is a type that allows reacting correctly to different ways
    /// to access or create a new or existing value in the hashtable.
    /// See `InsertHandler` and `AddressDefaultHandler` below.
    template<typename handler_t>
    inline void access_with_handler(uint64_t key, size_t key_width, size_t value_width, handler_t&& handler) {
        grow_if_needed(this->size() + 1, key_width, value_width);
        auto const dkey = this->decompose_key(key);

        DCHECK_EQ(key, this->compose_key(dkey.initial_address, dkey.stored_quotient));

        // cases:
        // - initial address empty.
        // - initial address occupied, there is an element for this key
        //   (v[initial address] = 1).
        // - initial address occupied, there is no element for this key
        //   (v[initial address] = 0).


        // TODO
        // DCHECK_LT(bucket_layout_t::table_pos_to_idx_of_bucket(dkey.initial_address), m_buckets.size());

        if (m_storage.table_pos_is_empty(dkey.initial_address)) {
            // check if we can insert directly

            m_storage.table_set_at_empty(dkey.initial_address,
                                         dkey.stored_quotient,
                                         std::move(handler),
                                         this->quotient_width(),
                                         this->value_width());

            // we created a new group, so update the bitflags
            set_v(dkey.initial_address, true);
            set_c(dkey.initial_address, true);
            m_sizing.set_size(m_sizing.size() + 1);
        } else {
            // check if there already is a group for this key
            bool const group_exists = get_v(dkey.initial_address);

            if (group_exists) {
                auto const group = search_existing_group(dkey);

                // check if element already exists
                auto p = search_in_group(group, dkey.stored_quotient);
                if (p != pointer_type()) {
                    // There is a value for this key already.
                    handler.on_existing(p);
                } else {
                    // Insert a new value
                    table_insert_value_after_group(group, dkey, std::move(handler));
                    m_sizing.set_size(m_sizing.size() + 1);
                }
            } else {
                // insert a new group

                // pretend we already inserted the new group
                // this makes table_insert_value_after_group() find the group
                // at the location _before_ the new group
                set_v(dkey.initial_address, true);
                auto const group = search_existing_group(dkey);

                // insert the element after the found group
                table_insert_value_after_group(group, dkey, std::move(handler));

                // mark the inserted element as the start of a new group,
                // thus fixing-up the v <-> c mapping
                set_c(group.group_end, true);

                m_sizing.set_size(m_sizing.size() + 1);
            }
        }
    }

    /// Shifts all values and `c` bits of the half-open range [from, to)
    /// inside the table one to the right, and inserts the new value
    /// at the now-empty location `from`.
    ///
    /// The position `to` needs to be empty.
    template<typename handler_t>
    inline void table_shift_groups_and_insert(size_t from,
                                              size_t to,
                                              uint64_t quot,
                                              handler_t&& handler) {
        DCHECK_NE(from, to);

        for(size_t i = to; i != from;) {
            size_t next_i = m_sizing.mod_sub(i, size_t(1));

            set_c(i, get_c(next_i));

            i = next_i;
        }
        set_c(from, false);

        table_shift_elements_and_insert(from, to, quot, std::move(handler));
    }

    /// Shifts all values of the half-open range [from, to)
    /// inside the table one to the right, and inserts the new value
    /// at the now-empty location `from`.
    ///
    /// The position `to` needs to be empty.
    template<typename handler_t>
    inline void table_shift_elements_and_insert(size_t from,
                                                size_t to,
                                                key_t quot,
                                                handler_t&& handler) {
        // move from...to one to the right, then insert at from

        DCHECK(from != to);

        auto value_handler = handler.on_new();
        auto&& val = value_handler.get();

        if (to < from) {
            // if the range wraps around, we decompose into two ranges:
            // [   |      |      ]
            // | to^      ^from  |
            // ^start         end^
            // [ 2 ]      [  1   ]
            //
            // NB: because we require from != to, and insert 1 additional element,
            // we are always dealing with a minimum 2 element range,
            // and thus can not end up with a split range with length == 0.

            // inserts the new element at the start of the range,
            // and temporarily stores the element at the end of the range
            // in `val` and `quot`.
            sparse_shift(from,  table_size(), val, quot);
            sparse_shift(0, to, val, quot);
        } else {
            // inserts the new element at the start of the range,
            // and temporarily stores the element at the end of the range
            // in `val` and `quot`.
            sparse_shift(from, to, val, quot);
        }

        // insert the element from the end of the range at the free
        // position to the right of it.
        auto insert = InsertHandler(std::move(val));
        table_set_at_empty(to, quot, std::move(insert));

        // after the previous insert and a potential reallocation,
        // notify the handler about the address of the new value.
        auto new_loc = m_storage.table_pos(from);
        value_handler.new_location(get_val_quot_at(new_loc).val_ptr());
    }

    /// A Group is a half-open range [group_start, group_end)
    /// that corresponds to a group of elements in the hashtable that
    /// belong to the same initial_address.
    ///
    /// This means that `c[group_start] == 1`, and
    /// `c[group_start < x < group_end] == 0`.
    ///
    /// `groups_terminator` points to the next free location
    /// inside the hashtable.
    struct Group {
        size_t group_start;       // Group that belongs to the key.
        size_t group_end;         // It's a half-open range: [start .. end).
        size_t groups_terminator; // Next free location.
    };

    // Assumption: There exists a group at the initial address of `key`.
    // This group is either the group belonging to key,
    // or the one after it in the case that no group for `key` exists yet.
    inline Group search_existing_group(decomposed_key_t const& key) {
        auto ret = Group();
        size_t cursor = key.initial_address;

        // Walk forward from the initial address until we find a empty location.
        // TODO: This search could maybe be accelerated by:
        // - checking whole blocks in the bucket bitvector for == or != 0
        size_t v_counter = 0;
        DCHECK_EQ(get_v(cursor), true);
        for(; m_storage.table_pos_contains_value(cursor); cursor = m_sizing.mod_add(cursor)) {
            v_counter += get_v(cursor);
        }
        DCHECK_GE(v_counter, 1);
        ret.groups_terminator = cursor;

        // Walk back again to find the end of the group
        // belonging to the initial address.
        size_t c_counter = v_counter;
        for(; c_counter != 1; cursor = m_sizing.mod_sub(cursor)) {
            c_counter -= get_c(m_sizing.mod_sub(cursor));
        }
        ret.group_end = cursor;

        // Walk further back to find the start of the group
        // belonging to the initial address
        for(; c_counter != 0; cursor = m_sizing.mod_sub(cursor)) {
            c_counter -= get_c(m_sizing.mod_sub(cursor));
        }
        ret.group_start = cursor;

        return ret;
    }

    /// Search a quotient inside an existing Group.
    ///
    /// This returns a pointer to the value if its found, or null
    /// otherwise.
    inline pointer_type search_in_group(Group const& group, uint64_t stored_quotient) {
        for(size_t i = group.group_start; i != group.group_end; i = m_sizing.mod_add(i)) {
            auto sparse_entry = get_val_quot_at(i);

            if (sparse_entry.get_quotient() == stored_quotient) {
                return sparse_entry.val_ptr();
            }
        }
        return pointer_type();
    }

    inline val_quot_ptrs_t<val_t> get_val_quot_at(TablePos pos) {
        m_storage.get_val_quot_at(pos, quotient_width(), value_width());
    }

    inline val_quot_ptrs_t<val_t> get_val_quot_at(size_t pos) {
        return get_val_quot_at(m_storage.table_pos(pos));
    }

    /// Inserts a new key-value pair after an existing
    /// group, shifting all following entries one to the right as needed.
    template<typename handler_t>
    inline void table_insert_value_after_group(Group const& group,
                                               decomposed_key_t const& dkey,
                                               handler_t&& handler)
    {
        if (m_storage.table_pos_is_empty(group.group_end)) {
            // if there is no following group, just append the new entry
            m_storage.table_set_at_empty(group.group_end,
                                         dkey.stored_quotient,
                                         std::move(handler),
                                         quotient_width(),
                                         value_width());
        } else {
            // else, shift all following elements one to the right
            table_shift_groups_and_insert(group.group_end,
                                          group.groups_terminator,
                                          dkey.stored_quotient,
                                          std::move(handler));
        }
    }

    /// A non-STL conformer iterator for iterating over all elements
    /// of the hashtable exactly once,
    /// wrapping around at the end as needed.
    struct iter_all_t {
        base_table_t<val_quot_storage_t, val_t, hash_t>& m_self;
        size_t i = 0;
        size_t original_start = 0;
        uint64_t initial_address = 0;
        enum {
            EMPTY_LOCATIONS,
            FULL_LOCATIONS
        } state;

        inline iter_all_t(base_table_t<val_quot_storage_t, val_t, hash_t>& self): m_self(self) {
            // first, skip forward to the first empty location
            // so that iteration can start at the beginning of the first complete group

            i = 0;

            for(;;i++) {
                if (m_self.m_storage.table_pos_is_empty(i)) {
                    break;
                }
            }

            // Remember our startpoint so that we can recognize it when
            // we wrapped around back to it
            original_start = i;

            // We proceed to the next position so that we can iterate until
            // we reach `original_start` again.
            i = m_self.m_sizing.mod_add(i);

            // We start iterating from an empty location
            state = EMPTY_LOCATIONS;
        }

        inline bool next(uint64_t* out_initial_address, size_t* out_i) {
            // TODO: Simplify the logic here.
            // In principle, it is just a loop that
            // skips empty locations, and yields on each occupied one.

            while(true) {
                if (state == EMPTY_LOCATIONS) {
                    // skip empty locations
                    for(;;i = m_self.m_sizing.mod_add(i)) {
                        if (m_self.table_pos_contains_value(i)) {
                            // we initialize init-addr at 1 pos before the start of
                            // a group of blocks, so that the blocks iteration logic works
                            initial_address = m_self.m_sizing.mod_sub(i);
                            state = FULL_LOCATIONS;
                            break;
                        }
                        if (i == original_start) {
                            return false;
                        }
                    }
                } else {
                    // process full locations
                    if (m_self.table_pos_is_empty(i))  {
                        state = EMPTY_LOCATIONS;
                        continue;
                    }
                    if (m_self.get_c(i)) {
                        // skip forward m_v cursor
                        // to find initial address for this block
                        //
                        // this works for the first block because
                        // initial_address starts at 1 before the group
                        initial_address = m_self.m_sizing.mod_add(initial_address);

                        while(!m_self.get_v(initial_address)) {
                            initial_address = m_self.m_sizing.mod_add(initial_address);
                        }
                    }

                    *out_initial_address = initial_address;
                    *out_i = i;

                    i = m_self.m_sizing.mod_add(i);
                    return true;
                }
            }
        }
    };

    /// Check the current key width and table site against the arguments,
    /// and grows the table or quotient bitvectors as needed.
    inline void grow_if_needed(size_t new_size, size_t new_key_width, size_t new_value_width) {
        auto needs_to_grow_capacity = [&]() {
            return m_sizing.needs_to_grow_capacity(m_sizing.capacity(), new_size);
        };

        auto needs_realloc = [&]() {
            return needs_to_grow_capacity()
                || (new_key_width != key_width())
                || (new_value_width != value_width());
        };

        /*
        std::cout
                << "buckets size/cap: " << m_buckets.size()
                << ", size: " << m_sizing.size()
                << "\n";
        */

        // TODO: Could reuse the existing table if only m_key_width changes
        // TODO: The iterators is inefficient since it does redundant
        // memory lookups and address calculations.

        if (needs_realloc()) {
            size_t new_capacity = m_sizing.capacity();
            while (m_sizing.needs_to_grow_capacity(new_capacity, new_size)) {
                new_capacity = m_sizing.grown_capacity(new_capacity);
            }
            auto new_table = base_table_t<val_quot_storage_t, val_t, hash_t>(new_capacity, new_key_width, new_value_width);
            new_table.max_load_factor(this->max_load_factor());

            /*
            std::cout
                << "grow to cap " << new_table.table_size()
                << ", m_key_width: " << int(new_table.m_key_width)
                << ", real_width: " << new_table.real_width()
                << ", quot width: " << new_table.quotient_width()
                << "\n";
            */

            m_storage.drain_all(iter_all_t(*this), [&](auto&& key, auto&& val) {
                new_table.insert(std::move(key), std::move(val));
            }, quotient_width(), value_width());

            *this = std::move(new_table);
        }

        DCHECK(!needs_realloc());
    }

    /// Shifts all elements one to the right,
    /// inserts val and quot at the from position,
    /// and stores the old from element in val and quot.
    inline void sparse_shift(size_t from, size_t to, value_type& val, key_t& quot) {
        DCHECK_LT(from, to);

        // initialize iterators like this:
        // [         ]
        // ^from   to^
        //          ||
        //    <- src^|
        //    <- dest^

        auto from_loc = m_storage.table_pos(from);
        auto from_iter = InsertIter(*this, from_loc);

        auto last = m_storage.table_pos(to - 1);
        auto src = InsertIter(*this, last);
        auto dst = InsertIter(*this, m_storage.table_pos(to));

        // move the element at the last position to a temporary position
        auto  tmp_p    = get_val_quot_at(last);
        value_type tmp_val  = std::move(*tmp_p.val_ptr());
        key_t tmp_quot = tmp_p.get_quotient();

        // move all elements one to the right
        while(src != from_iter) {
            // Decrement first for backward iteration
            src.decrement();
            dst.decrement();

            // Get access to the value/quotient at src and dst
            auto src_be = src.get();
            auto dst_be = dst.get();

            // Copy value/quotient over
            *dst_be.val_ptr() = std::move(*src_be.val_ptr());
            dst_be.set_quotient(src_be.get_quotient());
        }

        // move new element into empty from position
        auto from_p = get_val_quot_at(from_loc);
        *from_p.val_ptr() = std::move(val);
        from_p.set_quotient(quot);

        // move temporary element into the parameters
        val = std::move(tmp_val);
        quot = tmp_quot;
    }
};

}}
