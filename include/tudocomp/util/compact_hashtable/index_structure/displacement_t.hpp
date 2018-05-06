#pragma once

#include <limits>
#include <unordered_map>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>
#include "../val_quot_ptrs_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

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


struct compact_displacement_table_t {
    using elem_t = uint_t<4>;

    IntVector<elem_t> m_displace;
    std::unordered_map<size_t, size_t> m_spill;
    inline compact_displacement_table_t(size_t table_size) {
        m_displace.reserve(table_size);
        m_displace.resize(table_size);
    }
    inline size_t get(size_t pos) {
        size_t max = elem_t(std::numeric_limits<elem_t>::max());
        size_t tmp = elem_t(m_displace[pos]);
        if (tmp == max) {
            return m_spill[pos];
        } else {
            return tmp;
        }
    }
    inline void set(size_t pos, size_t val) {
        size_t max = elem_t(std::numeric_limits<elem_t>::max());
        if (val >= max) {
            m_displace[pos] = max;
            m_spill[pos] = val;
        } else {
            m_displace[pos] = val;
        }
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
        using pointer_type = ValPtr<val_t>;

        displacement_table_t& m_displace;
        size_t const table_size;
        widths_t widths;
        size_mgr_t const& size_mgr;
        storage_t& storage;

        lookup_result_t<val_t> lookup_insert(uint64_t initial_address,
                                             uint64_t stored_quotient)
        {
            auto sctx = storage.context(table_size, widths);

            auto cursor = initial_address;
            while(true) {
                auto pos = sctx.table_pos(cursor);

                if (sctx.pos_is_empty(pos)) {
                    auto ptrs = sctx.allocate_pos(pos);
                    m_displace.set(cursor, size_mgr.mod_sub(cursor, initial_address));
                    ptrs.set_quotient(stored_quotient);
                    return { ptrs, true };
                }

                if(m_displace.get(cursor) == size_mgr.mod_sub(cursor, initial_address)) {
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

        /// A non-STL conformer iterator for iterating over all elements
        /// of the hashtable exactly once,
        /// wrapping around at the end as needed.
        struct iter_all_t {
            context_t<storage_t, size_mgr_t>& m_self;
            size_t i = 0;
            size_t original_start = 0;
            uint64_t initial_address = 0;

            inline iter_all_t(context_t<storage_t, size_mgr_t>& self): m_self(self) {
                auto sctx = m_self.storage.context(m_self.table_size, m_self.widths);

                // first, skip forward to the first empty location
                // so that iteration can start at the beginning of the first complete group

                i = 0;

                for(;;i++) {
                    if (sctx.pos_is_empty(sctx.table_pos(i))) {
                        break;
                    }
                }

                // Remember our startpoint so that we can recognize it when
                // we wrapped around back to it
                original_start = i;

                // We proceed to the next position so that we can iterate until
                // we reach `original_start` again.
                initial_address = i;
                i = m_self.size_mgr.mod_add(i);
            }

            inline bool next(uint64_t* out_initial_address, size_t* out_i) {
                auto sctx = m_self.storage.context(m_self.table_size, m_self.widths);
                while (sctx.pos_is_empty(sctx.table_pos(i))) {
                    if (i == original_start) {
                        return false;
                    }

                    initial_address = i;
                    i = m_self.size_mgr.mod_add(i);
                }

                auto disp = m_self.m_displace.get(i);
                initial_address = m_self.size_mgr.mod_sub(i, disp);

                *out_initial_address = initial_address;
                *out_i = i;

                i = m_self.size_mgr.mod_add(i);
                return true;
            }
        };

        template<typename F>
        inline void drain_all(F f) {
            iter_all_t iter { *this };

            // bool start_of_bucket = false;
            // size_t bucket = 0;

            uint64_t initial_address;
            size_t i;

            table_pos_t drain_start;
            bool first = true;

            while(iter.next(&initial_address, &i)) {
                auto sctx = storage.context(table_size, widths);
                auto p = sctx.table_pos(i);

                if (first) {
                    first = false;
                    drain_start = p;
                }

                sctx.trim_storage(&drain_start, p);
                f(initial_address, sctx.at(p));
            }
        }

        inline pointer_type search(uint64_t const initial_address,
                                   uint64_t stored_quotient) {
            auto sctx = storage.context(table_size, widths);
            auto cursor = initial_address;
            while(true) {
                auto pos = sctx.table_pos(cursor);

                if (sctx.pos_is_empty(pos)) {
                    return pointer_type();
                }

                if(m_displace.get(cursor) == size_mgr.mod_sub(cursor, initial_address)) {
                    auto ptrs = sctx.at(pos);
                    if (ptrs.get_quotient() == stored_quotient) {
                        return ptrs.val_ptr();
                    }
                }

                cursor = size_mgr.mod_add(cursor);
                DCHECK_NE(cursor, initial_address);
            }

            return pointer_type();
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
