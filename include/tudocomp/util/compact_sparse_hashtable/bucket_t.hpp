#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"
#include <tudocomp/util/bit_packed_layout_t.hpp>

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
class bucket_element_t;

/// A bucket of quotient-value pairs in a sparse compact hashtable.
///
/// It consists of a pointer to a single heap allocation, that contains:
/// - A 64-bit bitvector of currently stored elements.
/// - An array of `val_t` values.
/// - A dynamic-width bitvector of quotients.
///
/// An empty bucket does not allocate any memory.
///
/// WARNING:
/// To prevent the overhead of unnecessary default-constructions,
/// the bucket does not initialize or destroy the value and quotient parts
/// of the allocation in its constructor/destructor.
/// Instead, it relies on the surrounding container to initialize and destroy
/// the values correctly.
// TODO: Investigate changing this semantic to automatic initialization
// and destruction.
template<typename val_t>
class bucket_t {
    std::unique_ptr<uint64_t[]> m_data;

public:
    inline bucket_t(): m_data() {}

    /// Construct a bucket, reserving space according to the bitvector
    /// `bv` and `quot_width`.
    inline bucket_t(uint64_t bv, size_t quot_width) {
        if (bv != 0) {
            auto layout = calc_sizes(popcount(bv), quot_width);

            m_data = std::make_unique<uint64_t[]>(layout.overall_qword_size);
            m_data[0] = bv;

            // NB: We call this for its alignment asserts
            ptrs(quot_width);
        } else {
            m_data.reset();
        }
    }

    inline bucket_t(bucket_t&& other) = default;
    inline bucket_t& operator=(bucket_t&& other) = default;

    /// Returns the bitvector of contained elements.
    inline uint64_t bv() const {
        if (!is_empty()) {
            return m_data[0];
        } else {
            return 0;
        }
    }

    /// Returns the amount of elements in the bucket.
    inline size_t size() const {
        return popcount(bv());
    }

    // Run destructors of each element in the bucket.
    inline void destroy_vals(size_t quot_width) {
        if (!is_empty()) {
            size_t size = this->size();

            val_t* start = ptrs(quot_width).vals_ptr;
            val_t* end = start + size;

            for(; start != end; start++) {
                start->~val_t();
            }
        }
    }

    /// Returns a `bucket_element_t` to position `pos`,
    /// or a sentinel value that acts as a one-pass-the-end pointer.
    inline bucket_element_t<val_t> at(size_t pos, size_t quot_width) const {
        if(!is_empty()) {
            auto ps = ptrs(quot_width);
            return bucket_element_t<val_t>(ps.vals_ptr + pos, ps.quots_ptr + pos);
        } else {
            DCHECK_EQ(pos, 0);
            return bucket_element_t<val_t>();
        }
    }

    inline bool is_empty() const {
        return m_data.get() == nullptr;
    }

    inline size_t stat_allocation_size_in_bytes(size_t quot_width) const {
        if (!is_empty()) {
            return calc_sizes(size(), quot_width).overall_qword_size * sizeof(uint64_t);
        } else {
            return 0;
        }
    }

private:
    static inline bool is_aligned(void const* const pointer, size_t byte_count) {
        return (uintptr_t)pointer % byte_count == 0;
    }

    /// Calculates the offsets of the two different arrays inside the allocation.
    struct Layout {
        int_vector::maybe_bit_packed_layout_element_t<val_t> vals_layout;
        int_vector::maybe_bit_packed_layout_element_t<dynamic_t> quots_layout;
        size_t overall_qword_size;
    };
    inline static Layout calc_sizes(size_t size, size_t quot_width) {
        using namespace int_vector;

        DCHECK_NE(size, 0);
        DCHECK_LE(alignof(val_t), alignof(uint64_t));

        auto layout = bit_layout_t();
        auto quots_width = maybe_bit_packed_width_t<dynamic_t>(quot_width);

        // The initial occupation bv
        layout.aligned_elements<uint64_t>(1);

        // The values
        auto values = layout.aligned_elements<val_t>(size);

        // The quotients
        auto quots = layout.maybe_bit_packed_elements<dynamic_t>(size, quots_width);

        Layout r;
        r.vals_layout = values;
        r.quots_layout = quots;
        r.overall_qword_size = layout.get_size_in_uint64_t_units();
        return r;
    }

    /// Creates the pointers to the beginnings of the two arrays inside
    /// the allocation.
    struct Ptrs {
        ValPtr<val_t> vals_ptr;
        QuotPtr quots_ptr;
    };
    inline Ptrs ptrs(size_t quot_width) const {
        DCHECK(!is_empty());
        auto layout = calc_sizes(size(), quot_width);

        return Ptrs {
            layout.vals_layout.ptr_relative_to(m_data.get()),
            layout.quots_layout.ptr_relative_to(m_data.get()),
        };
    }
};

/// Insert a new element into the bucket, growing it as needed
template<typename val_t>
inline void insert_in_bucket(bucket_t<val_t>& bucket,
                                      size_t new_elem_bucket_pos,
                                      uint64_t new_elem_bv_bit,
                                      size_t qw,
                                      val_t&& val,
                                      uint64_t quot)
{
    // Just a sanity check that can not live inside or outside `bucket_t` itself.
    static_assert(sizeof(bucket_t<val_t>) == sizeof(void*), "unique_ptr is more than 1 ptr large!");

    // TODO: check out different sizing strategies
    // eg, the known sparse_hash repo uses overallocation for small buckets

    // create a new bucket with enough size for the new element
    auto new_bucket = bucket_t<val_t>(bucket.bv() | new_elem_bv_bit, qw);

    // move all elements before the new element's location from old bucket into new bucket
    for(size_t i = 0; i < new_elem_bucket_pos; i++) {
        // TODO: Iterate pointers instead?
        auto new_elem = new_bucket.at(i, qw);
        auto old_elem = bucket.at(i, qw);

        new_elem.set_quotient(old_elem.get_quotient());
        new(&new_elem.val()) val_t(std::move(old_elem.val()));
    }

    // move new element into its location in the new bucket
    {
        auto new_elem = new_bucket.at(new_elem_bucket_pos, qw);
        new_elem.set_quotient(quot);
        new(&new_elem.val()) val_t(std::move(val));
    }

    // move all elements after the new element's location from old bucket into new bucket
    for(size_t i = new_elem_bucket_pos; i < bucket.size(); i++) {
        // TODO: Iterate pointers instead?
        auto new_elem = new_bucket.at(i + 1, qw);
        auto old_elem = bucket.at(i, qw);

        new_elem.set_quotient(old_elem.get_quotient());
        new(&new_elem.val()) val_t(std::move(old_elem.val()));
    }

    // destroy old empty elements, and overwrite with new bucket
    bucket.destroy_vals(qw);
    bucket = std::move(new_bucket);
}

}}
