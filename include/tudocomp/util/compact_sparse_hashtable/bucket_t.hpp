#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "util.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
class bucket_element_t;

template<typename val_t>
class Bucket {
    std::unique_ptr<uint64_t[]> m_data;

public:
    inline bool is_empty() const {
        return m_data.get() == nullptr;
    }

private:

    static inline bool is_aligned(void const* const pointer, size_t byte_count) {
        return (uintptr_t)pointer % byte_count == 0;
    }

    static inline size_t size(uint64_t bv) {
        return __builtin_popcountll(bv);
    }

    struct Layout {
        size_t vals_qword_offset;
        size_t quots_qword_offset;
        size_t overall_qword_size;
    };
    inline static Layout calc_sizes(size_t size, size_t quot_width) {
        DCHECK_NE(size, 0);
        DCHECK_LE(alignof(val_t), alignof(uint64_t));

        size_t vals_size_in_bytes = sizeof(val_t) * size;
        size_t vals_size_in_qwords = (vals_size_in_bytes + 7) / 8;

        uint64_t quots_size_in_bits = quot_width * size;
        size_t quots_size_in_qwords = (quots_size_in_bits + 63ull) / 64ull;

        Layout r;

        r.vals_qword_offset = 1;
        r.quots_qword_offset = r.vals_qword_offset + vals_size_in_qwords;

        r.overall_qword_size = 1 + vals_size_in_qwords + quots_size_in_qwords;

        /*
        std::cout
            << "size=" << size
            << ", quot_width=" << quot_width
            << ", sizeof(val_t)=" << sizeof(val_t)
            << ", alignof(val_t)=" << alignof(val_t)
            << ", sizeof(uint64_t)=" << sizeof(uint64_t)
            << ", alignof(uint64_t)=" << alignof(uint64_t)
            << ", vals_qword_offset=" << r.vals_qword_offset
            << ", quots_qword_offset=" << r.quots_qword_offset
            << ", overall_qword_size=" << r.overall_qword_size
            << "\n";
        */

        return r;
    }

    struct Ptrs {
        val_t* vals_ptr;
        QuotPtr quots_ptr;
    };
    inline Ptrs ptrs(size_t quot_width) const {
        DCHECK(!is_empty());
        size_t size = this->size();

        auto layout = calc_sizes(size, quot_width);

        uint64_t* const vals_start = &m_data[layout.vals_qword_offset];
        uint64_t* const quots_ptr = &m_data[layout.quots_qword_offset];

        val_t* const vals_ptr = reinterpret_cast<val_t*>(vals_start);

        DCHECK(is_aligned(vals_ptr, alignof(val_t))) << (void*)vals_ptr << ", " << alignof(val_t);
        DCHECK(is_aligned(quots_ptr, alignof(uint64_t)));

        return Ptrs {
            vals_ptr,
            make_quot_ptr(quots_ptr, quot_width),
        };
    }

public:
    inline Bucket(): m_data() {}
    explicit inline Bucket(uint64_t bv, size_t quot_width) {
        if (bv != 0) {
            auto layout = calc_sizes(size(bv), quot_width);

            m_data = std::make_unique<uint64_t[]>(layout.overall_qword_size);
            m_data[0] = bv;

            // NB: We call this for its alignment asserts
            ptrs(quot_width);
        } else {
            m_data.reset();
        }
    }

    inline uint64_t bv() const {
        if (!is_empty()) {
            return m_data[0];
        } else {
            return 0;
        }
    }

    inline size_t size() const {
        return size(bv());
    }

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

    inline Bucket(Bucket&& other) = default;
    inline Bucket& operator=(Bucket&& other) = default;

    inline bucket_element_t<val_t> at(size_t pos, size_t quot_width) const {
        if(!is_empty()) {
            auto ps = ptrs(quot_width);
            return bucket_element_t<val_t>(ps.vals_ptr + pos, ps.quots_ptr + pos);
        } else {
            DCHECK_EQ(pos, 0);
            return bucket_element_t<val_t>(nullptr, make_quot_ptr(nullptr, quot_width));
        }
    }
};


template<typename val_t>
inline void insert_in_bucket(Bucket<val_t>& bucket,
                                      size_t new_elem_bucket_pos,
                                      uint64_t new_elem_bv_bit,
                                      size_t qw,
                                      val_t&& val,
                                      uint64_t quot)
{
    static_assert(sizeof(Bucket<val_t>) == sizeof(void*), "unique_ptr is more than 1 ptr large!");

    size_t const size = bucket.size();

    // TODO: check out different sizing strategies
    // eg, the known sparse_hash repo uses overallocation for small buckets

    // create a new bucket with enough size for the new element
    auto new_bucket = Bucket<val_t>(bucket.bv() | new_elem_bv_bit, qw);

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
    for(size_t i = new_elem_bucket_pos; i < size; i++) {
        // TODO: Iterate pointers instead?
        auto new_elem = new_bucket.at(i + 1, qw);
        auto old_elem = bucket.at(i, qw);

        new_elem.set_quotient(old_elem.get_quotient());
        new(&new_elem.val()) val_t(std::move(old_elem.val()));
    }

    // destroy old empty elements
    bucket.destroy_vals(qw);

    bucket = std::move(new_bucket);
}

}}
