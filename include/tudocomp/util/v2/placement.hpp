#pragma once

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t>
struct lookup_result_t {
    val_quot_ptrs_t<val_t> entry;
    bool is_empty;
};

struct cv_bvs_t {
    IntVector<uint_t<2>> m_cv;

    inline cv_bvs_t(size_t table_size) {
        m_cv.reserve(table_size);
        m_cv.resize(table_size);
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

    template<typename storage_t, typename size_mgr_t>
    struct context_t {
        using widths_t = typename storage_t::widths_t;
        using val_t = typename storage_t::val_t_export;
        using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
        using table_pos_t = typename storage_t::table_pos_t;

        IntVector<uint_t<2>>& m_cv;
        size_t const table_size;
        widths_t const& widths;
        size_mgr_t const& size_mgr;
        storage_t& storage;

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

        /// Setter for the c and v bit at table position `pos`.
        inline void set_cv(size_t pos, uint8_t v) {
            m_cv[pos] = v;
        }

        // Assumption: There exists a group at the initial address of `key`.
        // This group is either the group belonging to key,
        // or the one after it in the case that no group for `key` exists yet.
        inline Group search_existing_group(uint64_t initial_address) {
            auto sctx = storage.context(table_size, widths);
            auto ret = Group();
            size_t cursor = initial_address;

            // Walk forward from the initial address until we find a empty location.
            // TODO: This search could maybe be accelerated by:
            // - checking whole blocks in the bucket bitvector for == or != 0
            size_t v_counter = 0;
            DCHECK_EQ(get_v(cursor), true);
            for(;
                !sctx.pos_is_empty(sctx.table_pos(cursor));
                cursor = size_mgr.mod_add(cursor))
            {
                v_counter += get_v(cursor);
            }
            DCHECK_GE(v_counter, 1);
            ret.groups_terminator = cursor;

            // Walk back again to find the end of the group
            // belonging to the initial address.
            size_t c_counter = v_counter;
            for(; c_counter != 1; cursor = size_mgr.mod_sub(cursor)) {
                c_counter -= get_c(size_mgr.mod_sub(cursor));
            }
            ret.group_end = cursor;

            // Walk further back to find the start of the group
            // belonging to the initial address
            for(; c_counter != 0; cursor = size_mgr.mod_sub(cursor)) {
                c_counter -= get_c(size_mgr.mod_sub(cursor));
            }
            ret.group_start = cursor;

            return ret;
        }

        /// Search a quotient inside an existing Group.
        ///
        /// This returns a pointer to the value if its found, or null
        /// otherwise.
        inline val_quot_ptrs_t<val_t> search_in_group(Group const& group,
                                                      uint64_t stored_quotient) {
            auto sctx = storage.context(table_size, widths);
            for(size_t i = group.group_start; i != group.group_end; i = size_mgr.mod_add(i)) {
                auto sparse_entry = sctx.at(sctx.table_pos(i));

                if (sparse_entry.get_quotient() == stored_quotient) {
                    return sparse_entry;
                }
            }
            return val_quot_ptrs_t<val_t>();
        }

        /// Inserts a new key-value pair after an existing
        /// group, shifting all following entries one to the right as needed.
        inline val_quot_ptrs_t<val_t> insert_value_after_group(
            Group const& group, uint64_t stored_quotient)
        {
            auto sctx = storage.context(table_size, widths);
            auto end_pos = sctx.table_pos(group.group_end);
            if (sctx.pos_is_empty(end_pos)) {
                // if there is no following group, just append the new entry
                return sctx.allocate_pos(end_pos);
            } else {
                // else, shift all following elements one to the right
                return shift_groups_and_insert(group.group_end,
                                               group.groups_terminator,
                                               stored_quotient);
            }
        }

        /// Shifts all values and `c` bits of the half-open range [from, to)
        /// inside the table one to the right, and inserts the new value
        /// at the now-empty location `from`.
        ///
        /// The position `to` needs to be empty.
        inline val_quot_ptrs_t<val_t> shift_groups_and_insert(
            size_t from, size_t to, uint64_t stored_quotient)
        {
            DCHECK_NE(from, to);

            for(size_t i = to; i != from;) {
                size_t next_i = size_mgr.mod_sub(i, size_t(1));

                set_c(i, get_c(next_i));

                i = next_i;
            }
            set_c(from, false);

            return shift_elements_and_insert(from, to);
        }

        /// Shifts all values of the half-open range [from, to)
        /// inside the table one to the right, and inserts the new value
        /// at the now-empty location `from`.
        ///
        /// The position `to` needs to be empty.
        inline val_quot_ptrs_t<val_t> shift_elements_and_insert(
            size_t from, size_t to)
        {
            auto sctx = storage.context(table_size, widths);
            // move from...to one to the right, then insert at from

            DCHECK(from != to);

            table_pos_t from_pos;

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

                from_pos = sparse_shift(from,  table_size);
                auto start_pos = sparse_shift(0, to);

                sctx.at(from_pos).swap_with(sctx.at(start_pos));
            } else {
                // [     |      |      ]
                //   from^      ^to

                from_pos = sparse_shift(from, to);
            }

            // insert the element from the end of the range at the free
            // position to the right of it.
            auto new_loc = sctx.allocate_pos(sctx.table_pos(to));

            auto from_ptrs = sctx.at(from_pos);
            new_loc.move_from(from_ptrs);
            return from_ptrs;
        }

        /// Shifts all elements one to the right,
        /// moving the last element to the front position,
        /// and returns a ptr pair to it.
        inline table_pos_t sparse_shift(size_t from, size_t to) {
            DCHECK_LT(from, to);
            auto sctx = storage.context(table_size, widths);

            // initialize iterators like this:
            // [         ]
            // ^from   to^
            //          ||
            //    <- src^|
            //    <- dest^

            auto from_loc = sctx.table_pos(from);
            auto from_iter = sctx.make_iter(from_loc);

            auto last = sctx.table_pos(to - 1);
            auto src = sctx.make_iter(last);
            auto dst = sctx.make_iter(sctx.table_pos(to));

            // move the element at the last position to a temporary position
            auto tmp_p = sctx.at(last);
            value_type tmp_val = std::move(*tmp_p.val_ptr());
            uint64_t tmp_quot = tmp_p.get_quotient();

            // move all elements one to the right
            // TODO: Could be optimized
            // to memcpies for different underlying layouts
            while(src != from_iter) {
                // Decrement first for backward iteration
                src.decrement();
                dst.decrement();

                // Get access to the value/quotient at src and dst
                auto src_be = src.get();
                auto dst_be = dst.get();

                // Copy value/quotient over
                dst_be.move_from(src_be);
            }

            // move last element to the front
            auto from_p = sctx.at(from_loc);
            from_p.set(std::move(tmp_val), tmp_quot);
            return from_loc;
        }

        lookup_result_t<val_t> lookup_insert(uint64_t initial_address,
                                             uint64_t stored_quotient)
        {
            auto sctx = storage.context(table_size, widths);
            auto ia_pos = sctx.table_pos(initial_address);

            if (sctx.pos_is_empty(ia_pos)) {
                // check if we can insert directly

                auto location = sctx.allocate_pos(ia_pos);
                location.set_quotient(stored_quotient);

                // we created a new group, so update the bitflags
                set_cv(initial_address, 0b11);

                return { location, true };
            } else {
                // check if there already is a group for this key
                bool const group_exists = get_v(initial_address);

                if (group_exists) {
                    auto const group = search_existing_group(initial_address);

                    // check if element already exists
                    auto p = search_in_group(group, stored_quotient);
                    if (p != val_quot_ptrs_t<val_t>()) {
                        // There is a value for this key already.
                        DCHECK_EQ(p.get_quotient(), stored_quotient);
                        return { p, false };
                    } else {
                        // Insert a new value
                        p = insert_value_after_group(group, stored_quotient);
                        p.set_quotient(stored_quotient);
                        return { p, true };
                    }
                } else {
                    // insert a new group

                    // pretend we already inserted the new group
                    // this makes table_insert_value_after_group() find the group
                    // at the location _before_ the new group
                    set_v(initial_address, true);
                    auto const group = search_existing_group(initial_address);

                    // insert the element after the found group
                    auto p = insert_value_after_group(group, stored_quotient);
                    p.set_quotient(stored_quotient);

                    // mark the inserted element as the start of a new group,
                    // thus fixing-up the v <-> c mapping
                    set_c(group.group_end, true);

                    return { p, true };
                }
            }
        }
    };
    template<typename storage_t, typename size_mgr_t>
    inline auto context(storage_t& storage,
                        size_t table_size,
                        typename storage_t::widths_t const& widths,
                        size_mgr_t const& size_mgr) {
        return context_t<storage_t, size_mgr_t> {
            m_cv, table_size, widths, size_mgr, storage
        };
    }
};


struct naive_displacement_table_t {
    std::vector<size_t> m_displace;
    inline naive_displacement_table_t(size_t table_size) {
        m_displace.reserve(table_size);
        m_displace.resize(table_size);
    }
    inline size_t get(size_t pos) {
        return m_displace[pos];
    }
    inline void set(size_t pos, size_t val) {
        m_displace[pos] = val;
    }
};

template<typename displacement_table_t>
struct displacement_t {
    displacement_table_t m_displace;

    inline displacement_t(size_t table_size):
        m_displace(table_size) {}

    template<typename storage_t, typename size_mgr_t>
    struct context_t {
        using widths_t = typename storage_t::widths_t;
        using val_t = typename storage_t::val_t_export;
        using value_type = typename cbp::cbp_repr_t<val_t>::value_type;
        using table_pos_t = typename storage_t::table_pos_t;

        displacement_table_t& m_displace;
        size_t const table_size;
        widths_t const& widths;
        size_mgr_t const& size_mgr;
        storage_t& storage;

        lookup_result_t<val_t> lookup_insert(uint64_t initial_address,
                                             uint64_t stored_quotient)
        {
            auto sctx = storage.context(table_size, widths);

            size_t cursor = initial_address;
            while(true) {
                auto pos = sctx.table_pos(cursor);

                if (sctx.pos_is_empty(pos)) {
                    auto ptrs = sctx.allocate_pos(pos);
                    m_displace.set(cursor, cursor - initial_address);
                    ptrs.set_quotient(stored_quotient);
                    return { ptrs, true };
                }

                if(m_displace.get(cursor) == cursor - initial_address) {
                    auto ptrs = sctx.at(pos);
                    if (ptrs.get_quotient() == stored_quotient) {
                        return { ptrs, false };
                    }
                }

                cursor = size_mgr.mod_add(cursor);
                DCHECK_NE(cursor, initial_address);
            }

            DCHECK(false) << "unreachable";
            return {};
        }
    };
    template<typename storage_t, typename size_mgr_t>
    inline auto context(storage_t& storage,
                        size_t table_size,
                        typename storage_t::widths_t const& widths,
                        size_mgr_t const& size_mgr) {
        return context_t<storage_t, size_mgr_t> {
            m_displace, table_size, widths, size_mgr, storage
        };
    }
};


}}
