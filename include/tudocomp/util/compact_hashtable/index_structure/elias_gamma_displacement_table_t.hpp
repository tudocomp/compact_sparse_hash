#pragma once

#include <limits>
#include <unordered_map>
#include <type_traits>

#include <tudocomp/util/bit_packed_layout_t.hpp>
#include <tudocomp/util/int_coder.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>
#include "../storage/val_quot_ptrs_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename base_type>
struct alloc_callback_ret_t {
    base_type* data;
    uint64_t bit_offset;
};

template<typename alloc_callback_t>
class sink_t {
    alloc_callback_t m_alloc_callback;

    using base_type = std::remove_pointer_t<decltype(m_alloc_callback(0).data)>;

    static constexpr size_t base_bitsize() {
        return sizeof(base_type) * CHAR_BIT;
    }

    static constexpr base_type base_bit_mask(size_t offset) {
        return (1ull << offset);
    }
public:
    inline sink_t(alloc_callback_t alloc_callback):
        m_alloc_callback(alloc_callback) {}

    inline void write_bit(bool set) {
        auto res = m_alloc_callback(1);

        auto word_offset = res.bit_offset >> 6;
        auto bit_offset = res.bit_offset & 0b111111ull;

        res.data[word_offset] &= ~base_bit_mask(bit_offset);
        res.data[word_offset] |= base_bit_mask(bit_offset) * base_type(set);

        // std::cout << "wrote bit " << set << "\n";

        if (set) {
            DCHECK_NE(res.data[word_offset], 0);
        }
    }
    inline uint8_t read_bit() {
        auto res = m_alloc_callback(1);

        auto word_offset = res.bit_offset >> 6;
        auto bit_offset = res.bit_offset & 0b111111ull;

        bool r = (res.data[word_offset] & base_bit_mask(bit_offset)) != 0;
        // std::cout << "read bit " << r << "\n";
        return r;
    }

    template<typename T>
    inline void write_int(T value, size_t bits = sizeof(T) * CHAR_BIT) {
        // TODO: better impl, that writes more than 1 bit at a time
        tdc::write_int<T>(std::move(*this), value, bits);
    }
    template<class T>
    inline T read_int(size_t bits = sizeof(T) * CHAR_BIT) {
        // TODO: better impl, that writes more than 1 bit at a time
        return tdc::read_int<T>(std::move(*this), bits);
    }
};

struct elias_gamma_bucket_t {
    struct context_t {
    std::unique_ptr<uint64_t[]>& m_data;
    uint64_t& m_bits;
    uint64_t& m_elem_cursor;
    uint64_t& m_bit_cursor;


    inline void dump_all() {
        auto ptr = cbp::cbp_repr_t<uint_t<1>>::construct_relative_to(m_data.get(), 0, 1);
        auto s = m_bits;
        // std::cout << "[";
        while(s--) {
            // std::cout << *ptr;
        }
        // std::cout << "]";
    }

    template<typename sink_t>
    inline size_t read(sink_t&& sink) {
        // std::cout << "read()\n";
        auto r = read_elias_gamma<size_t>(sink) - 1;
        // std::cout << "increase m_elem_cursor=" << m_elem_cursor << " by 1\n";
        m_elem_cursor++;
        return r;
    }

    template<typename sink_t>
    inline void write(sink_t&& sink, size_t v) {
        // std::cout << "write(" << v << ")\n";
        write_elias_gamma<size_t>(sink, v + 1);
        // std::cout << "increase m_elem_cursor=" << m_elem_cursor << " by 1\n";
        // std::cout << "write change: "; dump_all(); // std::cout << "\n";
        m_elem_cursor++;
    }

    inline size_t encoded_bit_size(size_t v) {
        uint64_t bit_size = 0;
        uint64_t buffer = 0;

        auto f = [&](size_t bits) {
            uint64_t bit_cursor = bit_size % 64;
            DCHECK_LE(bit_cursor + bits, 64);
            auto r = alloc_callback_ret_t<uint64_t> { &buffer, bit_cursor };
            bit_size += bits;
            return r;
        };
        auto sink = sink_t<decltype(f)> { f };
        write_elias_gamma<size_t>(sink, v + 1);

        // std::cout << "encoding a " << v << " takes " << bit_size << " bits\n";
        return bit_size;
    }

    inline auto fixed_sink() {
        auto f = [&](size_t bits) {
            DCHECK_LE(m_bit_cursor + bits, m_bits);
            auto r = alloc_callback_ret_t<uint64_t> { m_data.get(), m_bit_cursor };
            // std::cout << "increase m_bit_cursor=" << m_bit_cursor << " by " << bits << "\n";
            m_bit_cursor += bits;
            return r;
        };
        return sink_t<decltype(f)> { f };
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

    inline void seek(size_t pos) {
        if (pos < m_elem_cursor) {
            m_elem_cursor = 0;
            m_bit_cursor = 0;
        }
        while(m_elem_cursor < pos) {
            read(fixed_sink());
        }
    }

    inline void realloc_bits(uint64_t bits) {
        if (bits2alloc(bits) != bits2alloc(m_bits)) {
            realloc(bits2alloc(m_bits), bits2alloc(bits));
        }
        m_bits = bits;
        // std::cout << "realloc " << m_bits << " bits, @" << bits2alloc(m_bits) << "\n";
    }

    inline size_t get(size_t pos) {
        seek(pos);
        return read(fixed_sink());
    }

    inline void shift_bits(uint64_t from, uint64_t to, uint64_t size) {
        /* TODO: Efficient shift-based moving
        if from > to {
            uint64_t word_start = from % 64 + 1;
            uint64_t word_end = (from + size) % 64;
        }
        if from < to {

        }
        */

        // std::cout << "shift from " << from << ", to " << to << ", size " << size << "\n";
        DCHECK_LE(from, m_bits);
        DCHECK_LE(to, m_bits);

        DCHECK_LE(from + size, m_bits);
        DCHECK_LE(to + size, m_bits);

        auto from_ptr = cbp::cbp_repr_t<uint_t<1>>::construct_relative_to(m_data.get(), from, 1);
        auto to_ptr = cbp::cbp_repr_t<uint_t<1>>::construct_relative_to(m_data.get(), to, 1);
        auto from_end_ptr = from_ptr + size;

        if (to < from) {
            while(from_ptr != from_end_ptr) {
                *to_ptr = *from_ptr;
                to_ptr++;
                from_ptr++;
                size--;
            }

            DCHECK_EQ(size, 0);
        }
        if (to > from) {
            from_ptr += size;
            to_ptr += size;
            from_end_ptr -= size;

            while(from_ptr != from_end_ptr) {
                to_ptr--;
                from_ptr--;
                *to_ptr = *from_ptr;
                size--;
            }

            DCHECK_EQ(size, 0);
        }
    }

    inline void set(size_t pos, size_t val) {
        // std::cout << "set " << pos << " to " << val << "\n";
        // std::cout << "vector: ";
        dump_all();
        // std::cout << "\n";

        seek(pos);

        auto const backup_bit_cursor = m_bit_cursor;
        auto const backup_elem_cursor = m_elem_cursor;
        // std::cout << "cursor: elem=" << m_elem_cursor << ", bits=" << m_bit_cursor << "\n";
        auto const existing_val = get(pos);
        // std::cout << "cursor: elem=" << m_elem_cursor << ", bits=" << m_bit_cursor << "\n";
        // std::cout << "existing_val: " << existing_val << "\n";

        if (existing_val != val) {
            m_bit_cursor = backup_bit_cursor;
            m_elem_cursor = backup_elem_cursor;
            // std::cout << "cursor: elem=" << m_elem_cursor << ", bits=" << m_bit_cursor << "\n";

            auto existing_val_bit_size = encoded_bit_size(existing_val);
            // std::cout << "existing_val_bit_size: " << existing_val_bit_size << "\n";

            auto new_val_bit_size = encoded_bit_size(val);
            // std::cout << "new_val_bit_size: " << new_val_bit_size << "\n";

            auto new_bit_size = m_bits + new_val_bit_size - existing_val_bit_size;
            auto existing_bit_size = m_bits;

            // std::cout << "existing_bit_size: " << existing_bit_size << "\n";
            // std::cout << "new_bit_size: " << new_bit_size << "\n";

            if (new_bit_size < existing_bit_size) {
                // Shift left, then shrink

                shift_bits(m_bit_cursor + existing_val_bit_size,
                           m_bit_cursor + new_val_bit_size,
                           existing_bit_size - (m_bit_cursor + existing_val_bit_size));
                realloc_bits(new_bit_size);
            } else {
                // Grow first, the shift right

                realloc_bits(new_bit_size);
                shift_bits(m_bit_cursor + existing_val_bit_size,
                           m_bit_cursor + new_val_bit_size,
                           existing_bit_size - (m_bit_cursor + existing_val_bit_size));
            }

            write(fixed_sink(), val);
        }

        {
            m_bit_cursor = backup_bit_cursor;
            m_elem_cursor = backup_elem_cursor;
            auto const post_val_check = get(pos);
            DCHECK_EQ(val, post_val_check);
        }
        // std::cout << "\n";
    }
    };

    auto context(uint64_t& element_cursor, uint64_t& bit_cursor) {
        return context_t {
            m_data,
            m_bits,
            element_cursor,
            bit_cursor,
        };
    }

    std::unique_ptr<uint64_t[]> m_data;
    uint64_t m_bits = 0;

    inline elias_gamma_bucket_t(size_t size,
                                uint64_t& elem_cursor,
                                uint64_t& bit_cursor)
    {
        auto ctx = this->context(elem_cursor, bit_cursor);

        // Allocate memory for all encoded 0s
        auto all_bits = ctx.encoded_bit_size(0) * size;
        // std::cout << "-------\n";
        // std::cout << "size: " << size << "\n";
        ctx.realloc_bits(all_bits);

        // Encode all 0s.
        // TODO: Just copy the encoding of the first one
        for(size_t i = 0; i < size; i++) {
            ctx.write(ctx.fixed_sink(), 0);
        }
    }
};

struct elias_gamma_displacement_table_t {
    uint64_t m_elem_cursor = 0;
    uint64_t m_bit_cursor = 0;
    size_t m_bucket_cursor = 0;

    elias_gamma_bucket_t m_bucket;

    inline elias_gamma_displacement_table_t(size_t table_size):
        m_bucket(table_size, m_elem_cursor, m_bit_cursor) {}

    inline size_t get(size_t pos) {
        return m_bucket.context(m_elem_cursor, m_bit_cursor).get(pos);
    }
    inline void set(size_t pos, size_t val) {
        m_bucket.context(m_elem_cursor, m_bit_cursor).set(pos, val);
    }
};

}}
