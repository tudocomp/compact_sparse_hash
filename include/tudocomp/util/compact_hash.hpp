#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "compact_sparse_hashtable/util.hpp"
#include "compact_sparse_hashtable/bucket_t.hpp"
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

template<typename val_t>
class compact_torage_t {

public:
    inline compact_sparse_storage_t() {}
    inline compact_sparse_storage_t(size_t table_size) {
        // [...] TODO
    }

    inline void run_destructors_of_elements(size_t qw, size_t vw) {
        // [...] TODO
    }

    /// Run the destructors of the elements of the `i`-th bucket,
    /// and drop it from the hashtable, replacing it with an empty one.
    inline void drop_bucket(size_t i, size_t qw, size_t vw) {
        // [...] TODO
    }

    template<typename iter_all_t, typename F>
    inline void drain_all(F f, iter_all_t&& iter, size_t qw, size_t vw) {
        bool start_of_bucket = false;
        size_t bucket = 0;

        uint64_t initial_address;
        size_t i;
        while(iter.next(&initial_address, &i)) {
            auto p = table_pos(i);

            // drop buckets of old table as they get emptied out
            if (p.offset_in_bucket() == 0) {
                if (start_of_bucket) {
                    DCHECK_NE(bucket, p.idx_of_bucket);
                    drop_bucket(bucket, qw, vw);
                }

                start_of_bucket = true;
                bucket = p.idx_of_bucket;
            }

            f(initial_address, p);
        }
    }

    using TablePos = sparse_pos_t<buckets_t, bucket_layout_t>;
    inline TablePos table_pos(size_t pos) {
        return TablePos { pos, m_buckets };
    }

    /// Returns true if there is no element at the index `i`
    /// in the hashtable.
    inline bool table_pos_is_empty(size_t i) {
        return !table_pos_contains_value(i);
    }

    /// Returns true if there is an element at the index `i`
    /// in the hashtable.
    inline bool table_pos_contains_value(size_t i) {
        return table_pos(i).exists_in_bucket();
    }

    inline val_quot_ptrs_t<val_t> get_val_quot_at(TablePos pos, size_t qw, size_t vw) {
        DCHECK(pos.exists_in_bucket());
        return pos.bucket().at(pos.offset_in_bucket(), qw, vw);
    }

    /// Insert a key-value pair into a empty location in the table.
    template<typename handler_t>
    inline void table_set_at_empty(size_t pos, key_t quot, handler_t&& handler,
                                   size_t qw, size_t vw) {
        auto data = table_pos(pos);
        DCHECK(!data.exists_in_bucket());

        // we will insert a new element
        auto value_handler = handler.on_new();
        auto&& val = value_handler.get();

        // figure out which bucket to access
        auto& bucket = data.bucket();

        // insert element & grow bucket as appropriate
        insert_in_bucket(bucket,
                         data.offset_in_bucket(),
                         data.bit_mask_in_bucket,
                         qw,
                         vw,
                         std::move(val),
                         quot);

        // notify handler with location of new element
        auto new_loc = bucket.at(data.offset_in_bucket(), qw, vw);
        value_handler.new_location(new_loc.val_ptr());
    }

    // pseudo-iterator for iterating over bucket elements
    // NB: does not wrap around!
    struct buckets_iter_t {
        bucket_t<val_t> const* m_bucket;
        val_quot_ptrs_t<val_t>    m_b_start;
        val_quot_ptrs_t<val_t>    m_b_end;
        size_t               m_quotient_width;
        size_t               m_value_width;

        inline void set_bucket_elem_range(size_t end_offset) {
            size_t start_offset = 0;
            DCHECK_LE(start_offset, end_offset);

            m_b_start = m_bucket->at(start_offset, m_quotient_width, m_value_width);
            m_b_end   = m_bucket->at(end_offset, m_quotient_width, m_value_width);
        }

        inline buckets_iter_t(compact_sparse_storage_t& table,
                              TablePos const& pos, size_t qw, size_t vw) {
            m_quotient_width = qw;
            m_value_width = vw;

            // NB: Using pointer arithmetic here, because
            // we can (intentionally) end up with the address 1-past
            // the end of the vector, which represents an end-iterator.
            m_bucket = table.m_buckets.data() + pos.idx_of_bucket;

            if(pos.idx_of_bucket < table.m_buckets.size()) {
                set_bucket_elem_range(pos.offset_in_bucket());
            } else {
                // use default constructed nullptr val_quot_ptrs_ts
            }
        }

        inline val_quot_ptrs_t<val_t> get() {
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

        inline bool operator!=(buckets_iter_t& other) {
            return !(m_b_end.ptr_eq(other.m_b_end));
        }
    };

    using InsertIter = buckets_iter_t;

    inline InsertIter make_insert_iter(TablePos const& pos,
                                       size_t qw,
                                       size_t vw) {
        return InsertIter(*this, pos, qw, vw);
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
    inline statistics_t stat_gather(size_t qw, size_t vw, size_t size, size_t cv_allocation) {
        statistics_t r;

        r.buckets = m_buckets.size();

        for (auto const& b : m_buckets) {
            r.allocated_buckets += (!b.is_empty());
            r.buckets_real_allocated_capacity_in_bytes += b.stat_allocation_size_in_bytes(qw, vw);
        }

        // Calculate real allocated bytes
        // NB: Sizes of members with constant size are not included (eg, sizeof(m_buckets))
        // Size of buckets vector
        r.real_allocated_capacity_in_bytes += m_buckets.capacity() * sizeof(bucket_t<val_t>);
        r.real_allocated_capacity_in_bytes += r.buckets_real_allocated_capacity_in_bytes;
        // Size of cv bitvectors
        r.real_allocated_capacity_in_bytes += cv_allocation;

        // Calculate minimum bits needed
        // Occupation bitvector inside allocated buckets
        r.theoretical_minimum_size_in_bits += r.allocated_buckets * 64;
        // Quotient bitvectors across all buckets
        r.theoretical_minimum_size_in_bits += size * qw;
        // Values across all buckets
        r.theoretical_minimum_size_in_bits += size * vw;
        // Size of cv bitvectors
        r.theoretical_minimum_size_in_bits += size * 2;

        return r;
    }
};

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
