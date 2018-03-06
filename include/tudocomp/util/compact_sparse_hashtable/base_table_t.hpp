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

template<template<typename, typename> typename self_t, typename val_t, typename hash_t>
class base_table_t {
    friend self_t<val_t, hash_t>;

    inline self_t<val_t, hash_t>& self() {
        return static_cast<self_t<val_t, hash_t>&>(*this);
    }

    inline self_t<val_t, hash_t> const& self() const {
        return static_cast<self_t<val_t, hash_t> const&>(*this);
    }

private:
    /// By-value representation of a value
    using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
    /// Reference to a value
    using reference_type = ValRef<val_t>;
    /// Pointer to a value
    using pointer_type = ValPtr<val_t>;

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
        return std::max<size_t>(self().m_sizing.capacity_log2() + 1, self().m_key_width.get_width());
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
        return (self().m_cv[pos] & 0b01) != 0;
    }

    /// Getter for the c bit at table position `pos`.
    inline bool get_c(size_t pos) {
        return (self().m_cv[pos] & 0b10) != 0;
    }

    /// Setter for the v bit at table position `pos`.
    inline void set_v(size_t pos, bool v) {
        auto x = self().m_cv[pos] & 0b10;
        self().m_cv[pos] = x | (0b01 * v);
    }

    /// Setter for the c bit at table position `pos`.
    inline void set_c(size_t pos, bool c) {
        auto x = self().m_cv[pos] & 0b01;
        self().m_cv[pos] = x | (0b10 * c);
    }
};

}}
