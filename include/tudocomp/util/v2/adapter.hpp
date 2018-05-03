#pragma once

#include "util.hpp"
#include "size_manager_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename hash_t, typename storage_t, typename placement_t>
class generic_hashtable_t {
    using val_t = typename storage_t::val_t_export;
public:
    /// By-value representation of a value
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
    /// Reference to a value
    using reference_type = ValRef<val_t>;
    /// Pointer to a value
    using pointer_type = ValPtr<val_t>;

    /// Default value of the `key_width` parameter of the constructor.
    static constexpr size_t DEFAULT_KEY_WIDTH = 1;
    static constexpr size_t DEFAULT_VALUE_WIDTH = 1;
    static constexpr size_t DEFAULT_TABLE_SIZE = 0;

    inline generic_hashtable_t(generic_hashtable_t&& other) = default;
    inline generic_hashtable_t& operator=(generic_hashtable_t&& other) {
        // NB: overwriting the storage does not automatically destroy the values in them.
        run_destructors_of_elements();

        m_sizing = std::move(other.m_sizing);
        m_key_width = std::move(other.m_key_width);
        m_val_width = std::move(other.m_val_width);
        m_storage = std::move(other.m_storage);
        m_placement = std::move(other.m_placement);
        m_hash = std::move(other.m_hash);

        return *this;
    }
    // NB: These just exist to catch bugs, and could be removed
    inline generic_hashtable_t(generic_hashtable_t const& other) = delete;
    inline generic_hashtable_t& operator=(generic_hashtable_t  const& other) = delete;

    /// Constructs a hashtable with a initial table size `size`,
    /// and a initial key bit-width `key_width`.
    inline generic_hashtable_t(size_t size = DEFAULT_TABLE_SIZE,
                                      size_t key_width = DEFAULT_KEY_WIDTH,
                                      size_t value_width = DEFAULT_VALUE_WIDTH):
        m_sizing(size),
        m_key_width(key_width),
        m_val_width(value_width),
        m_storage(table_size(), storage_widths()),
        m_placement(table_size()),
        m_hash(real_width())
    {
    }

    inline ~generic_hashtable_t() {
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

        auto result = grow_and_insert(key, raw_key_width, raw_val_width);

        if (result.is_empty) {
            result.entry.set_val_no_drop(std::move(value));
        } else {
            result.entry.set_val(std::move(value));
        }
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
        auto raw_key_width = std::max<size_t>(key_width, this->key_width());
        auto raw_val_width = std::max<size_t>(value_width, this->value_width());

        auto result = grow_and_insert(key, raw_key_width, raw_val_width);

        if (result.is_empty) {
            result.entry.set_val_no_drop(value_type());
        }

        pointer_type addr = result.entry.val_ptr();
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

private:
    using key_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;
    using widths_t = typename storage_t::widths_t;

    /// Size of table, and width of the stored keys and values
    size_manager_t m_sizing;
    key_width_t m_key_width;
    val_width_t m_val_width;

    /// Storage of the table elements
    storage_t m_storage;

    /// Placement management structures
    placement_t m_placement;

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

    inline widths_t storage_widths() {
        return { quotient_width(), value_width() };
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
        DCHECK(false) << "figure out later";
        //storage().run_destructors_of_elements();
    }

    /// Access the element represented by `handler` under
    /// the key `key` with the, possibly new, width of `key_width` bits.
    ///
    /// `handler` is a type that allows reacting correctly to different ways
    /// to access or create a new or existing value in the hashtable.
    /// See `InsertHandler` and `AddressDefaultHandler` below.
    inline auto grow_and_insert(uint64_t key, size_t key_width, size_t value_width) {
        grow_if_needed(this->size() + 1, key_width, value_width);
        auto const dkey = this->decompose_key(key);

        DCHECK_EQ(key, this->compose_key(dkey.initial_address, dkey.stored_quotient));

        auto pctx = m_placement.context(m_storage, table_size(), storage_widths(), m_sizing);

        auto result = pctx.lookup_insert(dkey.initial_address, dkey.stored_quotient);

        if (result.is_empty) {
            m_sizing.set_size(m_sizing.size() + 1);
        }

        return result;
    }

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
            auto new_table = generic_hashtable_t<hash_t, storage_t, placement_t>(
                new_capacity, new_key_width, new_value_width);
            new_table.max_load_factor(this->max_load_factor());

            /*
            std::cout
                << "grow to cap " << new_table.table_size()
                << ", m_key_width: " << int(new_table.m_key_width)
                << ", real_width: " << new_table.real_width()
                << ", quot width: " << new_table.quotient_width()
                << "\n";
            */

            auto pctx = m_placement.context(m_storage, table_size(), storage_widths(), m_sizing);
            pctx.drain_all([&](auto initial_address, auto kv) {
                auto stored_quotient = kv.get_quotient();
                auto key = compose_key(initial_address, stored_quotient);
                new_table.insert(key, std::move(*kv.val_ptr()));
            });

            *this = std::move(new_table);
        }

        DCHECK(!needs_realloc());
    }
};

}}
