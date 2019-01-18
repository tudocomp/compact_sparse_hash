#pragma once

#include <limits>
#include <unordered_map>
#include <type_traits>

#include <tudocomp/util/bit_packed_layout_t.hpp>
#include <tudocomp/util/int_coder.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include <tudocomp/util/compact_hash/index_structure/elias_gamma_displacement_table_t.hpp>
#include <tudocomp/util/compact_hash/index_structure/compact_displacement_table_t.hpp>
#include <tudocomp/util/compact_hash/index_structure/naive_displacement_table_t.hpp>

#include "../storage/entry_ptr_t.hpp"

#include <tudocomp/util/serialization.hpp>

namespace tdc {namespace compact_sparse_hashmap {

template<typename displacement_table_t>
class displacement_t {
    template<typename T>
    friend struct ::tdc::serialize;

    displacement_table_t m_displace;

    displacement_t(displacement_table_t&& table):
        m_displace(std::move(table)) {}

public:
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

        template<typename F>
        inline void for_all_allocated(F f) {
            auto sctx = storage.context(table_size, widths);

            // first, skip forward to the first empty location
            // so that iteration can start at the beginning of the first complete group

            size_t i = 0;
            for(;;i++) {
                if (sctx.pos_is_empty(sctx.table_pos(i))) {
                    break;
                }
            }

            // Remember our startpoint so that we can recognize it when
            // we wrapped around back to it
            size_t const original_start = i;

            // We proceed to the next position so that we can iterate until
            // we reach `original_start` again.
            i = size_mgr.mod_add(i);

            while(true) {
                auto sctx = storage.context(table_size, widths);
                while (sctx.pos_is_empty(sctx.table_pos(i))) {
                    if (i == original_start) {
                        return;
                    }

                    i = size_mgr.mod_add(i);
                }

                auto disp = m_displace.get(i);
                uint64_t initial_address = size_mgr.mod_sub(i, disp);

                f(initial_address, i);

                i = size_mgr.mod_add(i);
            }
        }

        template<typename F>
        inline void drain_all(F f) {
            table_pos_t drain_start;
            bool first = true;

            for_all_allocated([&](auto initial_address, auto i) {
                auto sctx = storage.context(table_size, widths);
                auto p = sctx.table_pos(i);

                if (first) {
                    first = false;
                    drain_start = p;
                }

                sctx.trim_storage(&drain_start, p);
                f(initial_address, sctx.at(p));
            });
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

}

template<typename displacement_table_t>
struct serialize<compact_sparse_hashmap::displacement_t<displacement_table_t>> {
    using T = compact_sparse_hashmap::displacement_t<displacement_table_t>;

    static void write(std::ostream& out, T const& val, size_t table_size) {
        serialize<displacement_table_t>::write(out, val.m_displace, table_size);
    }

    static T read(std::istream& in, size_t table_size) {
        auto displace =
            serialize<displacement_table_t>::read(in, table_size);

        return T {
            std::move(displace)
        };
    }
    static bool equal_check(T const& lhs, T const& rhs, size_t table_size) {
        return gen_equal_check(m_displace, table_size);
    }
};

}
