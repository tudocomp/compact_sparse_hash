#pragma once

#include <limits>
#include <unordered_map>
#include <type_traits>
#include <cmath>

#include <tudocomp/util/bit_packed_layout_t.hpp>
#include <tudocomp/util/int_coder.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include <tudocomp/util/serialization.hpp>

namespace tdc {namespace compact_hash {

/// Stores displacement entries as integers with a width of
/// `displace_size` Bits. Displacement value larger than that
/// will be spilled into a `std::unordered_map<size_t, size_t>`.
template<size_t displace_size>
class layered_displacement_table_t {
    template<typename T>
    friend struct ::tdc::serialize;

    template<typename T>
    friend struct ::tdc::heap_size;

    using elem_t = uint_t<displace_size>;

    IntVector<elem_t> m_displace;
    std::unordered_map<size_t, size_t> m_spill;

    layered_displacement_table_t(IntVector<elem_t>&& displace,
                                 std::unordered_map<size_t, size_t>&& spill):
        m_displace(std::move(displace)), m_spill(std::move(spill)) {}
public:
    /// runtime initilization arguments, if any
    struct config_args {
        config_args() = default;
    };

    /// this is called during a resize to copy over internal config values
    inline void reconstruct_overwrite_config_from(layered_displacement_table_t const& other) {
    }

    inline layered_displacement_table_t(size_t table_size) {
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

}

template<size_t N>
struct heap_size<compact_hash::layered_displacement_table_t<N>> {
    using T = compact_hash::layered_displacement_table_t<N>;

    static object_size_t compute(T const& val, size_t table_size) {
        auto bytes = object_size_t::empty();

        DCHECK_EQ(val.m_displace.size(), table_size);
        auto size = val.m_displace.stat_allocation_size_in_bytes();
        bytes += object_size_t::exact(size);

        size_t unordered_map_size_guess
            = sizeof(decltype(val.m_spill))
            + val.m_spill.size() * sizeof(size_t) * 2;

        bytes += object_size_t::unknown_extra_data(unordered_map_size_guess);

        return bytes;
    }
};

template<size_t N>
struct serialize<compact_hash::layered_displacement_table_t<N>> {
    using T = compact_hash::layered_displacement_table_t<N>;

    static object_size_t write(std::ostream& out, T const& val, size_t table_size) {
        auto bytes = object_size_t::empty();

        DCHECK_EQ(val.m_displace.size(), table_size);
        auto data = (char const*) val.m_displace.data();
        auto size = val.m_displace.stat_allocation_size_in_bytes();
        out.write(data, size);
        bytes += object_size_t::exact(size);

        size_t spill_size = val.m_spill.size();
        out.write((char*) &spill_size, sizeof(size_t));
        bytes += object_size_t::exact(sizeof(size_t));

        for (auto pair : val.m_spill) {
            size_t k = pair.first;
            size_t v = pair.second;
            out.write((char*) &k, sizeof(size_t));
            out.write((char*) &v, sizeof(size_t));
            bytes += object_size_t::exact(sizeof(size_t) * 2);
            spill_size--;
        }

        DCHECK_EQ(spill_size, 0);

        return bytes;
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

}
