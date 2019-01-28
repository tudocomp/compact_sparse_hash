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

/// Stores displacement entries as `size_t` integers.
struct naive_displacement_table_t {
    template<typename T>
    friend struct ::tdc::serialize;

    /// runtime initilization arguments, if any
    struct config_args {
        config_args() = default;
    };

    /// this is called during a resize to copy over internal config values
    inline void reconstruct_overwrite_config_from(naive_displacement_table_t const& other) {
    }

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

}

}
