#pragma once

#include <limits>
#include <unordered_map>
#include <type_traits>

#include <tudocomp/util/bit_packed_layout_t.hpp>
#include <tudocomp/util/int_coder.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>
#include "../storage/quot_ptrs_t.hpp"

#include <tudocomp/util/serialization.hpp>

#include "elias_gamma_displacement_table_t.hpp"

namespace tdc {namespace compact_sparse_hashset {

/// Stores displacement entries as `size_t` integers.
struct naive_displacement_table_t {
    template<typename T>
    friend struct ::tdc::serialize;

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

/// Stores displacement entries as integers with a width of
/// `displace_size` Bits. Displacement value larger than that
/// will be spilled into a `std::unordered_map<size_t, size_t>`.
template<size_t displace_size>
class compact_displacement_table_t {
    template<typename T>
    friend struct ::tdc::serialize;

    using elem_t = uint_t<displace_size>;

    IntVector<elem_t> m_displace;
    std::unordered_map<size_t, size_t> m_spill;

    compact_displacement_table_t(IntVector<elem_t>&& displace,
                                 std::unordered_map<size_t, size_t>&& spill):
        m_displace(std::move(displace)), m_spill(std::move(spill)) {}
public:

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
        using quot_width_t = typename storage_t::quot_width_t;
        using table_pos_t = typename storage_t::table_pos_t;

        displacement_table_t& m_displace;
        size_t const table_size;
        quot_width_t widths;
        size_mgr_t const& size_mgr;
        storage_t& storage;

        entry_t lookup_insert(uint64_t initial_address,
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
                    return entry_t::found_new(cursor);
                }

                if(m_displace.get(cursor) == size_mgr.mod_sub(cursor, initial_address)) {
                    auto ptrs = sctx.at(pos);
                    if (ptrs.get_quotient() == stored_quotient) {
                        return entry_t::found_exist(cursor);
                    }
                }

                cursor = size_mgr.mod_add(cursor);
                DCHECK_NE(cursor, initial_address);
            }

            DCHECK(false) << "unreachable";
            return entry_t::not_found();
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

        inline entry_t search(uint64_t const initial_address,
                              uint64_t stored_quotient) {
            auto sctx = storage.context(table_size, widths);
            auto cursor = initial_address;
            while(true) {
                auto pos = sctx.table_pos(cursor);

                if (sctx.pos_is_empty(pos)) {
                    return entry_t::not_found();
                }

                if(m_displace.get(cursor) == size_mgr.mod_sub(cursor, initial_address)) {
                    auto ptrs = sctx.at(pos);
                    if (ptrs.get_quotient() == stored_quotient) {
                        return entry_t::found_exist(cursor);
                    }
                }

                cursor = size_mgr.mod_add(cursor);
                DCHECK_NE(cursor, initial_address);
            }

            DCHECK(false) << "unreachable";
            return entry_t::not_found();
        }
    };
    template<typename storage_t, typename size_mgr_t>
    inline auto context(storage_t& storage,
                        size_t table_size,
                        typename storage_t::quot_width_t const& widths,
                        size_mgr_t const& size_mgr) {
        return context_t<storage_t, size_mgr_t> {
            m_displace, table_size, widths, size_mgr, storage
        };
    }
};
}

template<size_t N>
struct serialize<compact_sparse_hashset::compact_displacement_table_t<N>> {
    using T = compact_sparse_hashset::compact_displacement_table_t<N>;

    static void write(std::ostream& out, T const& val, size_t table_size) {
        DCHECK_EQ(val.m_displace.size(), table_size);
        auto data = (char const*) val.m_displace.data();
        auto size = val.m_displace.stat_allocation_size_in_bytes();
        out.write(data, size);

        size_t spill_size = val.m_spill.size();
        out.write((char*) &spill_size, sizeof(size_t));

        for (auto pair : val.m_spill) {
            size_t k = pair.first;
            size_t v = pair.second;
            out.write((char*) &k, sizeof(size_t));
            out.write((char*) &v, sizeof(size_t));
            spill_size--;
        }

        DCHECK_EQ(spill_size, 0);
    }

    static T read(std::istream& in, size_t table_size) {
        auto disp = IntVector<uint_t<N>>();
        disp.reserve(table_size);
        disp.resize(table_size);
        auto data = (char*) disp.data();
        auto size = disp.stat_allocation_size_in_bytes();
        in.read(data, size);

        auto spill = std::unordered_map<size_t, size_t>();
        size_t spill_size;
        in.read((char*) &spill_size, sizeof(size_t));

        for (size_t i = 0; i < spill_size; i++) {
            size_t k;
            size_t v;
            in.read((char*) &k, sizeof(size_t));
            in.read((char*) &v, sizeof(size_t));

            spill[k] = v;
        }

        return T {
            std::move(disp),
            std::move(spill),
        };
    }

    static bool equal_check(T const& lhs, T const& rhs, size_t table_size) {
        return gen_equal_diagnostic(lhs.m_displace == rhs.m_displace)
        && gen_equal_diagnostic(lhs.m_spill == rhs.m_spill);
    }
};

template<typename displacement_table_t>
struct serialize<compact_sparse_hashset::displacement_t<displacement_table_t>> {
    using T = compact_sparse_hashset::displacement_t<displacement_table_t>;

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
