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

template<typename val_t, typename hash_t>
class base_table_t {
public:
    /// By-value representation of a value
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
    /// Reference to a value
    using reference_type = ValRef<val_t>;
    /// Pointer to a value
    using pointer_type = ValPtr<val_t>;

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
protected:
    using key_t = uint64_t;
    using key_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

    /// Compact table data (c and v bitvectors)
    IntVector<uint_t<2>> m_cv;

    /// Size of table, and width of the stored keys and values
    size_manager_t m_sizing;
    key_width_t m_key_width;
    val_width_t m_val_width;

    /// Hash function
    hash_t m_hash {1};

    inline base_table_t(size_t size,
                        size_t key_width,
                        size_t value_width):
        m_sizing(size),
        m_key_width(key_width),
        m_val_width(value_width)
    {
        size_t cv_size = table_size();

        m_cv.reserve(cv_size);
        m_cv.resize(cv_size);

        m_hash = hash_t(real_width());
    }

    inline base_table_t(base_table_t&& other) = default;
    inline base_table_t& operator=(base_table_t&& other) = default;

    // NB: These just exist to catch bugs, and can be removed
    inline base_table_t(base_table_t const& other) = delete;
    inline base_table_t& operator=(base_table_t  const& other) = delete;

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
};

}}
