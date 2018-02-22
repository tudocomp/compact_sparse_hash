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

namespace tdc {namespace compact_sparse_hashtable {

// TODO: Remove unconditional bound checking
// - c, v bvs
// - buckets
// - elements in buckets

template<typename val_t, typename hash_t = xorshift_t>
class compact_sparse_hashtable_t {
    using key_t = uint64_t;
    using buckets_t = std::vector<bucket_t<val_t>>;

    // TODO: Change this, and fix tests
    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    /// Compact table data (c and v bitvectors)
    IntVector<uint_t<2>> m_cv;

    /// Sparse table data (buckets)
    buckets_t m_buckets;

    /// Size of table, and width of the stored keys
    size_manager_t m_sizing;
    uint8_t m_width;

public:
    // TODO: Fix to use real value type
    using value_type = val_t;

    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline compact_sparse_hashtable_t(size_t size = DEFAULT_TABLE_SIZE, size_t key_width = DEFAULT_KEY_WIDTH):
        m_sizing(size),
        m_width(key_width)
    {
        size_t cv_size = table_size();
        size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size());

        m_cv.reserve(cv_size);
        m_cv.resize(cv_size);
        m_buckets.reserve(buckets_size);
        m_buckets.resize(buckets_size);
    }

    inline ~compact_sparse_hashtable_t() {
        // NB: destroying the buckets vector will just destroy the bucket in it,
        /// which will not destroy their elements.
        run_destructors_of_bucket_elements();
    }

    inline compact_sparse_hashtable_t(compact_sparse_hashtable_t&& other):
        m_cv(std::move(other.m_cv)),
        m_buckets(std::move(other.m_buckets)),
        m_sizing(std::move(other.m_sizing)),
        m_width(std::move(other.m_width))
    {
    }

    inline compact_sparse_hashtable_t& operator=(compact_sparse_hashtable_t&& other) {
        // NB: overwriting the buckets vector will just destroy the bucket in it,
        /// which will not destroy their elements.
        run_destructors_of_bucket_elements();

        m_cv = std::move(other.m_cv);
        m_buckets = std::move(other.m_buckets);
        m_sizing = std::move(other.m_sizing);
        m_width = std::move(other.m_width);

        return *this;
    }

    // TODO: Change to STL-conform interface?

    /// Inserts a key-value pair into the hashtable,
    /// where `key` has a width of `key_width` bits.
    ///
    /// The hashtable will grow and increases its `key_width()`
    /// as needed to fit the new element.
    ///
    /// Note: Calling this for incrementally inserting
    /// new elements with increasing key widths is more efficient
    /// than calling `grow_key_width()` separately,
    /// since it fuses the reallocation needed for both a key-width change
    /// and a table size increase.
    inline void insert(Key key, val_t&& value) {
        access_with_handler(key.value(), std::max(key.width(), m_width), InsertHandler {
            std::move(value)
        });
    }

    /// Returns a reference to the element with key `key`,
    /// where `key` has a width of `key_width` bits.
    ///
    /// The hashtable will grow and increases its `key_width()`
    /// as needed to fit the new element.
    ///
    /// If the value does not already exist in the table, it will be
    /// default-constructed.
    ///
    /// Note: Calling this for incrementally inserting
    /// new elements with increasing key widths is more efficient
    /// than calling `grow_key_width()` separately,
    /// since it fuses the reallocation needed for both a key-width change
    /// and a table size increase.
    inline ValRef<val_t> operator[](Key key) {
        ValPtr<val_t> addr = ValPtr<val_t>();

        access_with_handler(key.value(), std::max(key.width(), m_width), AddressDefaultHandler {
            &addr
        });

        DCHECK(addr != ValPtr<val_t>());

        return *addr;
    }

    /// Increase the width of the stored keys to `key_width` bits.
    ///
    /// Note: Calling this for incrementally inserting
    /// new elements with increasing key widths is _less_ efficient
    /// than calling `insert()` with the new key width.
    inline void grow_key_width(size_t key_width) {
        grow_if_needed(size(), std::max<size_t>(key_width, m_width));
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
        return m_width;
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

    /// Amount of bits of the value, that are stored explicitly
    /// in the buckets.
    inline size_t value_width() {
        return sizeof(val_t) * CHAR_BIT;
    }

    // TODO: STL-conform API?
    /// Search for a key inside the hashtable.
    ///
    /// This returns a pointer to the value if its found, or null
    /// otherwise.
    inline ValPtr<val_t> search(Key key) {
        auto dkey = decompose_key(key.value());
        if (get_v(dkey.initial_address)) {
            return search_in_group(search_existing_group(dkey), dkey.stored_quotient);
        }
        return ValPtr<val_t>();
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
        size_t buckets = 0;
        size_t allocated_buckets = 0;
        size_t buckets_real_allocated_capacity_in_bytes = 0;

        size_t real_allocated_capacity_in_bytes = 0;
        uint64_t theoretical_minimum_size_in_bits = 0;
    };
    inline statistics_t stat_gather() {
        statistics_t r;

        r.buckets = m_buckets.size();

        for (auto const& b : m_buckets) {
            r.allocated_buckets += (!b.is_empty());
            r.buckets_real_allocated_capacity_in_bytes += b.stat_allocation_size_in_bytes(quotient_width());
        }

        // Calculate real allocated bytes
        // NB: Sizes of members with constant size are not included (eg, sizeof(m_buckets))
        // Size of buckets vector
        r.real_allocated_capacity_in_bytes += m_buckets.capacity() * sizeof(bucket_t<val_t>);
        r.real_allocated_capacity_in_bytes += r.buckets_real_allocated_capacity_in_bytes;
        // Size of cv bitvectors
        r.real_allocated_capacity_in_bytes += m_cv.stat_allocation_size_in_bytes();

        // Calculate minimum bits needed
        // Occupation bitvector inside allocated buckets
        r.theoretical_minimum_size_in_bits += r.allocated_buckets * 64;
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
    /// Maps hashtable position to position of the corresponding bucket,
    /// and the position inside of it.
    struct bucket_layout_t {
        static constexpr size_t BVS_WIDTH_SHIFT = 6;
        static constexpr size_t BVS_WIDTH_MASK = 0b111111;

        static inline size_t table_pos_to_idx_of_bucket(size_t pos) {
            return pos >> BVS_WIDTH_SHIFT;
        }

        static inline size_t table_pos_to_idx_inside_bucket(size_t pos) {
            return pos & BVS_WIDTH_MASK;
        }

        static inline size_t table_size_to_bucket_size(size_t size) {
            return (size + BVS_WIDTH_MASK) >> BVS_WIDTH_SHIFT;
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

    /// Run the destructors of the elements of the `i`-th bucket,
    /// and drop it from the hashtable, replacing it with an empty one.
    inline void drop_bucket(size_t i) {
        DCHECK_LT(i, m_buckets.size());
        size_t qw = quotient_width();
        m_buckets[i].destroy_vals(qw);
        m_buckets[i] = bucket_t<val_t>();
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
    inline void run_destructors_of_bucket_elements() {
        size_t qw = quotient_width();
        for(size_t i = 0; i < m_buckets.size(); i++) {
            m_buckets[i].destroy_vals(qw);
        }
    }

    /// Debug check that a key does not occupy more bits than the
    /// hashtable currently allows.
    inline bool dcheck_key_width(uint64_t key) {
        uint64_t key_mask = (1ull << (m_width - 1ull) << 1ull) - 1ull;
        bool key_is_too_large = key & ~key_mask;
        return !key_is_too_large;
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
    /// - Otherwise the current maximum key width `m_width`
    ///   determines the real width.
    inline size_t real_width() {
        return std::max<size_t>(m_sizing.capacity_log2() + 1, m_width);
    }

    /// Decompose a key into its initial address and quotient.
    inline decomposed_key_t decompose_key(uint64_t key) {
        DCHECK(dcheck_key_width(key)) << "Attempt to decompose key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not.";

        uint64_t hres = hash_t::hashfn(key, real_width());

        DCHECK_EQ(hash_t::reverse_hashfn(hres, real_width()), key);

        return m_sizing.decompose_hashed_value(hres);
    }

    /// Compose a key from its initial address and quotient.
    inline uint64_t compose_key(uint64_t initial_address, uint64_t quotient) {
        uint64_t harg = m_sizing.compose_hashed_value(initial_address, quotient);
        uint64_t key = hash_t::reverse_hashfn(harg, real_width());

        DCHECK(dcheck_key_width(key)) << "Composed key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not.";
        return key;
    }

    /// Access the element represented by `handler` under
    /// the key `key` with the, possibly new, width of `key_width` bits.
    ///
    /// `handler` is a type that allows reacting correctly to different ways
    /// to access or create a new or existing value in the hashtable.
    /// See `InsertHandler` and `AddressDefaultHandler` below.
    template<typename handler_t>
    inline void access_with_handler(uint64_t key, size_t key_width, handler_t&& handler) {
        grow_if_needed(size() + 1, key_width);
        auto const dkey = decompose_key(key);

        DCHECK_EQ(key, compose_key(dkey.initial_address, dkey.stored_quotient));

        // cases:
        // - initial address empty.
        // - initial address occupied, there is an element for this key
        //   (v[initial address] = 1).
        // - initial address occupied, there is no element for this key
        //   (v[initial address] = 0).

        DCHECK_LT(bucket_layout_t::table_pos_to_idx_of_bucket(dkey.initial_address),
                  m_buckets.size());

        if (table_pos_is_empty(dkey.initial_address)) {
            // check if we can insert directly

            table_set_at_empty(dkey.initial_address,
                               dkey.stored_quotient,
                               std::move(handler));

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
                if (p != ValPtr<val_t>()) {
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

    /// Handler for inserting an element that exists as a rvalue reference.
    /// This will overwrite an existing element.
    class InsertHandler {
        val_t&& m_value;
    public:
        InsertHandler(val_t&& value): m_value(std::move(value)) {}

        inline auto on_new() {
            struct InsertHandlerOnNew {
                val_t&& m_value;
                inline val_t&& get() {
                    return std::move(m_value);
                }
                inline void new_location(ValPtr<val_t> value) {
                    // don't care
                }
            };

            return InsertHandlerOnNew {
                std::move(m_value),
            };
        }

        inline void on_existing(ValPtr<val_t> value) {
            *value = std::move(m_value);
        }
    };

    /// Handler for getting the address of an element in the map.
    /// If none exists yet, it will be default constructed.
    class AddressDefaultHandler {
        ValPtr<val_t>* m_address = nullptr;
    public:
        AddressDefaultHandler(ValPtr<val_t>* address): m_address(address) {}

        inline auto on_new() {
            struct AddressDefaultHandlerOnNew {
                val_t m_value;
                ValPtr<val_t>* m_address;
                inline val_t&& get() {
                    return std::move(m_value);
                }
                inline void new_location(ValPtr<val_t> value) {
                    *m_address = value;
                }
            };

            return AddressDefaultHandlerOnNew {
                val_t(),
                m_address,
            };
        }

        inline void on_existing(ValPtr<val_t> value) {
            *m_address = value;
        }
    };

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
        auto new_loc = sparse_pos(from);
        value_handler.new_location(get_bucket_elem_at(new_loc).val_ptr());
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
        for(; table_pos_contains_value(cursor); cursor = m_sizing.mod_add(cursor)) {
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
    inline ValPtr<val_t> search_in_group(Group const& group, uint64_t stored_quotient) {
        for(size_t i = group.group_start; i != group.group_end; i = m_sizing.mod_add(i)) {
            auto sparse_entry = get_bucket_elem_at(i);

            if (sparse_entry.get_quotient() == stored_quotient) {
                return sparse_entry.val_ptr();
            }
        }
        return ValPtr<val_t>();
    }

    /// Inserts a new key-value pair after an existing
    /// group, shifting all following entries one to the right as needed.
    template<typename handler_t>
    inline void table_insert_value_after_group(Group const& group,
                                               decomposed_key_t const& dkey,
                                               handler_t&& handler)
    {
        if (table_pos_is_empty(group.group_end)) {
            // if there is no following group, just append the new entry
            table_set_at_empty(group.group_end,
                               dkey.stored_quotient,
                               std::move(handler));
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
        compact_sparse_hashtable_t<val_t>& m_self;
        size_t i = 0;
        size_t original_start = 0;
        uint64_t initial_address = 0;
        enum {
            EMPTY_LOCATIONS,
            FULL_LOCATIONS
        } state;

        inline iter_all_t(compact_sparse_hashtable_t<val_t>& self): m_self(self) {
            // first, skip forward to the first empty location
            // so that iteration can start at the beginning of the first complete group

            i = 0;

            for(;;i++) {
                if (m_self.table_pos_is_empty(i)) {
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
    inline void grow_if_needed(size_t new_size, size_t new_width) {
        auto needs_to_grow_capacity = [&]() {
            return m_sizing.needs_to_grow_capacity(m_sizing.capacity(), new_size);
        };

        auto needs_realloc = [&]() {
            return needs_to_grow_capacity() || (new_width != m_width);
        };

        /*
        std::cout
                << "buckets size/cap: " << m_buckets.size()
                << ", size: " << m_sizing.size()
                << "\n";
        */

        // TODO: Could reuse the existing table if only m_width changes
        // TODO: The iterators is inefficient since it does redundant
        // memory lookups and address calculations.

        if (needs_realloc()) {
            size_t new_capacity = m_sizing.capacity();
            while (m_sizing.needs_to_grow_capacity(new_capacity, new_size)) {
                new_capacity = m_sizing.grown_capacity(new_capacity);
            }
            auto new_table = compact_sparse_hashtable_t(new_capacity, new_width);
            new_table.max_load_factor(this->max_load_factor());

            /*
            std::cout
                << "grow to cap " << new_table.table_size()
                << ", m_width: " << int(new_table.m_width)
                << ", real_width: " << new_table.real_width()
                << ", quot width: " << new_table.quotient_width()
                << "\n";
            */

            bool start_of_bucket = false;
            size_t bucket = 0;

            uint64_t initial_address;
            size_t i;
            auto iter = iter_all_t(*this);
            while(iter.next(&initial_address, &i)) {
                auto p = sparse_pos(i);

                // drop buckets of old table as they get emptied out
                if (p.offset_in_bucket() == 0) {
                    if (start_of_bucket) {
                        DCHECK_NE(bucket, p.idx_of_bucket);
                        drop_bucket(bucket);
                    }

                    start_of_bucket = true;
                    bucket = p.idx_of_bucket;
                }

                auto kv = get_bucket_elem_at(p);
                auto stored_quotient = kv.get_quotient();

                key_t key = compose_key(initial_address, stored_quotient);

                new_table.insert(key, std::move(*kv.val_ptr()));
            }

            *this = std::move(new_table);
        }

        DCHECK(!needs_realloc());
    }

    /// Insert a key-value pair into a empty location in the table.
    template<typename handler_t>
    inline void table_set_at_empty(size_t pos, key_t quot, handler_t&& handler) {
        auto data = sparse_pos(pos);
        DCHECK(!data.exists_in_bucket());

        // figure out which bucket to access
        auto& bucket = data.bucket();
        size_t const qw = quotient_width();

        // we will insert a new element
        auto value_handler = handler.on_new();
        auto&& val = value_handler.get();

        // insert element & grow bucket as appropriate
        insert_in_bucket(bucket,
                         data.offset_in_bucket(),
                         data.bit_mask_in_bucket,
                         qw,
                         std::move(val),
                         quot);

        // notify handler with location of new element
        auto new_loc = bucket.at(data.offset_in_bucket(), qw);
        value_handler.new_location(new_loc.val_ptr());
    }

    /// Returns true if there is no element at the index `i`
    /// in the hashtable.
    inline bool table_pos_is_empty(size_t i) {
        return !table_pos_contains_value(i);
    }

    /// Returns true if there is an element at the index `i`
    /// in the hashtable.
    inline bool table_pos_contains_value(size_t i) {
        return sparse_pos(i).exists_in_bucket();
    }

    /// Shifts all elements one to the right,
    /// inserts val and quot at the from position,
    /// and stores the old from element in val and quot.
    inline void sparse_shift(size_t from, size_t to, val_t& val, key_t& quot) {
        // pseudo-iterator for iterating over bucket elements
        // NB: does not wrap around!
        struct iter {
            bucket_t<val_t> const* m_bucket;
            bucket_element_t<val_t>    m_b_start;
            bucket_element_t<val_t>    m_b_end;
            size_t               m_quotient_width;

            inline void set_bucket_elem_range(size_t end_offset) {
                size_t start_offset = 0;
                DCHECK_LE(start_offset, end_offset);

                m_b_start = m_bucket->at(start_offset, m_quotient_width);
                m_b_end   = m_bucket->at(end_offset, m_quotient_width);
            }

            inline iter(compact_sparse_hashtable_t& table,
                        SparsePos const& pos) {
                m_quotient_width = table.quotient_width();

                // NB: Using pointer arithmetic here, because
                // we can (intentionally) end up with the address 1-past
                // the end of the vector, which represents an end-iterator.
                m_bucket = table.m_buckets.data() + pos.idx_of_bucket;

                if(pos.idx_of_bucket < table.m_buckets.size()) {
                    set_bucket_elem_range(pos.offset_in_bucket());
                } else {
                    // use default constructed nullptr bucket_element_ts
                }
            }

            inline bucket_element_t<val_t> get() {
                return m_b_end;
            }

            inline void decrement() {
                if (!m_b_start.ptr_eq(m_b_end)) {
                    m_b_end.decrement_ptr();
                } else {
                    do {
                        --m_bucket;
                    } while(m_bucket->bv() == 0);
                    set_bucket_elem_range(m_bucket->size() - 1);
                }
            }

            inline bool operator!=(iter& other) {
                return !(m_b_end.ptr_eq(other.m_b_end));
            }
        };

        DCHECK_LT(from, to);

        // initialize iterators like this:
        // [         ]
        // ^from   to^
        //          ||
        //    <- src^|
        //    <- dest^

        auto from_loc = sparse_pos(from);
        auto from_iter = iter(*this, from_loc);

        auto last = sparse_pos(to - 1);
        auto src = iter(*this, last);
        auto dst = iter(*this, sparse_pos(to));

        // move the element at the last position to a temporary position
        auto  tmp_p    = get_bucket_elem_at(last);
        val_t tmp_val  = std::move(*tmp_p.val_ptr());
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
        auto from_p = get_bucket_elem_at(from_loc);
        *from_p.val_ptr() = std::move(val);
        from_p.set_quotient(quot);

        // move temporary element into the parameters
        val = std::move(tmp_val);
        quot = tmp_quot;
    }

    using SparsePos = sparse_pos_t<buckets_t, bucket_layout_t>;

    SparsePos sparse_pos(size_t pos) {
        return SparsePos { pos, m_buckets };
    }

    inline bucket_element_t<val_t> get_bucket_elem_at(SparsePos pos) {
        DCHECK(pos.exists_in_bucket());
        size_t qw = quotient_width();

        return pos.bucket().at(pos.offset_in_bucket(), qw);
    }

    inline bucket_element_t<val_t> get_bucket_elem_at(size_t pos) {
        return get_bucket_elem_at(sparse_pos(pos));
    }
};

}}
