#pragma once

#include <limits>
#include <unordered_map>

#include <tudocomp/util/int_coder.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>
#include "../storage/val_quot_ptrs_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename alloc_callback_t>
struct sink_t {
    uint64_t* m_bit_cursor;
    alloc_callback_t m_alloc_callback;

    inline void write_bit(bool set) {
        uint64_t* data = m_alloc_callback(1);

        auto word_offset = (*m_bit_cursor) >> 6;
        auto bit_offset = (*m_bit_cursor) & 0b111111ull;

        data[word_offset] |= (1ull << (63 - bit_offset));
        (*m_bit_cursor)++;
    }
    inline uint8_t read_bit() {
        uint64_t* data = m_alloc_callback(1);

        auto word_offset = (*m_bit_cursor) >> 6;
        auto bit_offset = (*m_bit_cursor) & 0b111111ull;

        auto r = (data[word_offset] & (1ull << (63 - bit_offset))) != 0;
        (*m_bit_cursor)++;
        return r;
    }

    template<typename T>
    inline void write_int(T value, size_t bits = sizeof(T) * CHAR_BIT) {
        tdc::write_int<T>(std::move(*this), value, bits);
    }
    template<class T>
    inline T read_int(size_t bits = sizeof(T) * CHAR_BIT) {
        return tdc::read_int<T>(std::move(*this), bits);
    }
};

template<typename alloc_callback_t>
struct elias_gamma_t {
    uint64_t* m_elem_cursor;
    uint64_t* m_bit_cursor;
    alloc_callback_t m_alloc_callback;

    inline void encode(size_t v) {
        auto sink = sink_t<alloc_callback_t> { m_bit_cursor, m_alloc_callback };
        write_elias_gamma<size_t>(std::move(sink), v);
        (*m_elem_cursor)++;
    }
    inline size_t decode() {
        auto sink = sink_t<alloc_callback_t> { m_bit_cursor, m_alloc_callback };
        auto r = read_elias_gamma<size_t>(std::move(sink));
        (*m_elem_cursor)++;
        return r;
    }
};

template<template<typename> typename variable_coder_t>
struct variable_coder_bucket_t {
    std::unique_ptr<uint64_t[]> m_data;
    uint64_t m_bits;
    uint64_t m_elem_cursor = 0;
    uint64_t m_bit_cursor = 0;

    template<typename alloc_callback_t>
    inline variable_coder_t<alloc_callback_t> make_varcoder(
        uint64_t* elem_cursor,
        uint64_t* bit_cursor,
        alloc_callback_t alloc_callback) {
        return variable_coder_t<alloc_callback_t> {
            elem_cursor,
            bit_cursor,
            alloc_callback
        };
    }

    inline size_t bits2alloc(uint64_t bits) {
        return (bits + 63ull) >> 6ull;
    }

    inline void realloc(size_t old_size, size_t new_size) {
        auto n = std::make_unique<uint64_t[]>(new_size);
        for (size_t i = 0; i < old_size; i++) {
            n[i] = m_data[i];
        }
        m_data = std::move(n);
    }

    inline auto read_coder() {
        return make_varcoder(
            &m_elem_cursor,
            &m_bit_cursor,
            [&](size_t bits) {
                return m_data.get();
            }
        );
    }

    inline void seek(size_t pos) {
        if (pos < m_elem_cursor) {
            m_elem_cursor = 0;
            m_bit_cursor = 0;
        }
        while(m_elem_cursor < pos) {
            read_coder().decode();
        }
    }

    inline variable_coder_bucket_t(size_t size) {
        size_t single_size = 0;
        auto coder = make_varcoder(
            &m_elem_cursor,
            &m_bit_cursor,
            [&](size_t bits) {
                single_size += bits;
                auto old_size = bits2alloc(m_bits);
                auto new_size = bits2alloc(m_bits + bits);
                if (new_size > old_size) {
                    realloc(old_size, new_size);
                    m_bits += bits;
                }
                return m_data.get();
            }
        );

        if (0 < size) {
            coder.encode(0);
        }

        auto all_bits = single_size * size;
        realloc(bits2alloc(m_bits), bits2alloc(all_bits));

        for(size_t i = 1; i < size; i++) {
            coder.encode(0);
        }
    }

    inline size_t get(size_t pos) {
        seek(pos);
        return read_coder().decode();
    }

    inline void set(size_t pos, size_t val) {
        seek(pos);
    }
};

struct elias_gamma_displacement_table_t {
    variable_coder_bucket_t<elias_gamma_t> m_bucket;

    inline elias_gamma_displacement_table_t(size_t table_size):
        m_bucket(table_size) {}

    inline size_t get(size_t pos) {
        return m_bucket.get(pos);
    }
    inline void set(size_t pos, size_t val) {
        m_bucket.set(pos, val);
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

}}
