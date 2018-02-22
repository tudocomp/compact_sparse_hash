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

    using quot_width_t = typename cbp::cbp_repr_t<dynamic_t>::width_repr_t;
    using val_width_t = typename cbp::cbp_repr_t<val_t>::width_repr_t;

public:
    inline bucket_t(): m_data() {}

    /// Construct a bucket, reserving space according to the bitvector
    /// `bv` and `quot_width`.
    inline bucket_t(uint64_t bv, size_t quot_width, val_width_t const& val_width) {
        if (bv != 0) {
            auto layout = calc_sizes(popcount(bv), quot_width, val_width);

            m_data = std::make_unique<uint64_t[]>(layout.overall_qword_size);
            m_data[0] = bv;

            // NB: We call this for its alignment asserts
            ptrs(quot_width, val_width);
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
    inline void destroy_vals(size_t quot_width, val_width_t const& val_width) {
        if (!is_empty()) {
            size_t size = this->size();

            auto start = ptrs(quot_width, val_width).vals_ptr;
            auto end = start + size;

            for(; start != end; start++) {
                cbp::cbp_repr_t<val_t>::call_destructor(start);
            }
        }
    }

    /// Returns a `bucket_element_t` to position `pos`,
    /// or a sentinel value that acts as a one-pass-the-end pointer.
    inline bucket_element_t<val_t> at(size_t pos, size_t quot_width, val_width_t const& val_width) const {
        if(!is_empty()) {
            auto ps = ptrs(quot_width, val_width);
            return bucket_element_t<val_t>(ps.vals_ptr + pos, ps.quots_ptr + pos);
        } else {
            DCHECK_EQ(pos, 0);
            return bucket_element_t<val_t>();
        }
    }

    inline bool is_empty() const {
        return m_data.get() == nullptr;
    }

    inline size_t stat_allocation_size_in_bytes(size_t quot_width, val_width_t const& val_width) const {
        if (!is_empty()) {
            return calc_sizes(size(), quot_width, val_width).overall_qword_size * sizeof(uint64_t);
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
        cbp::cbp_layout_element_t<val_t> vals_layout;
        cbp::cbp_layout_element_t<dynamic_t> quots_layout;
        size_t overall_qword_size;
    };
    inline static Layout calc_sizes(size_t size, size_t quot_width, val_width_t const& val_width) {
        DCHECK_NE(size, 0);
        DCHECK_LE(alignof(val_t), alignof(uint64_t));

        auto layout = cbp::bit_layout_t();
        auto quots_width = quot_width_t(quot_width);

        // The initial occupation bv
        layout.aligned_elements<uint64_t>(1);

        // The values
        auto values = layout.cbp_elements<val_t>(size, val_width);

        // The quotients
        auto quots = layout.cbp_elements<dynamic_t>(size, quots_width);

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
    inline Ptrs ptrs(size_t quot_width, val_width_t const& val_width) const {
        DCHECK(!is_empty());
        auto layout = calc_sizes(size(), quot_width, val_width);

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
                                      size_t vw,
                                      typename cbp::cbp_repr_t<val_t>::value_type&& val,
                                      uint64_t quot)
{
    // Just a sanity check that can not live inside or outside `bucket_t` itself.
    static_assert(sizeof(bucket_t<val_t>) == sizeof(void*), "unique_ptr is more than 1 ptr large!");

    // TODO: check out different sizing strategies
    // eg, the known sparse_hash repo uses overallocation for small buckets

    // create a new bucket with enough size for the new element
    auto new_bucket = bucket_t<val_t>(bucket.bv() | new_elem_bv_bit, qw, vw);

    // move all elements before the new element's location from old bucket into new bucket
    for(size_t i = 0; i < new_elem_bucket_pos; i++) {
        // TODO: Iterate pointers instead?
        auto new_elem = new_bucket.at(i, qw, vw);
        auto old_elem = bucket.at(i, qw, vw);

        new_elem.set_quotient(old_elem.get_quotient());
        cbp::cbp_repr_t<val_t>::construct_val_from_ptr(new_elem.val_ptr(), old_elem.val_ptr());
    }

    // move new element into its location in the new bucket
    {
        auto new_elem = new_bucket.at(new_elem_bucket_pos, qw, vw);
        new_elem.set_quotient(quot);
        cbp::cbp_repr_t<val_t>::construct_val_from_rval(new_elem.val_ptr(), std::move(val));
    }

    // move all elements after the new element's location from old bucket into new bucket
    for(size_t i = new_elem_bucket_pos; i < bucket.size(); i++) {
        // TODO: Iterate pointers instead?
        auto new_elem = new_bucket.at(i + 1, qw, vw);
        auto old_elem = bucket.at(i, qw, vw);

        new_elem.set_quotient(old_elem.get_quotient());
        cbp::cbp_repr_t<val_t>::construct_val_from_ptr(new_elem.val_ptr(), old_elem.val_ptr());
    }

    // destroy old empty elements, and overwrite with new bucket
    bucket.destroy_vals(qw, vw);
    bucket = std::move(new_bucket);
}

}}
