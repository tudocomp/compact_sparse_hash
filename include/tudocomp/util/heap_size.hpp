#pragma once

#include <iostream>
#include <typeinfo>
#include <tuple>

namespace tdc {
    class size_result_t {
        size_t m_bytes = 0;
        bool m_has_unknown_parts = false;

        size_result_t() = default;
        size_result_t(size_t bytes, bool has_unknown_parts):
            m_bytes(bytes), m_has_unknown_parts(has_unknown_parts) {}
    public:
        template<typename T>
        inline static size_result_t object_without_indirection() {
            return size_result_t(sizeof(T), false);
        }
        template<typename T>
        inline static size_result_t object_with_unknown_child_data_size() {
            return size_result_t(sizeof(T), true);
        }
        template<typename T>
        inline static size_result_t object_with_known_child_data_size() {
            return size_result_t(sizeof(T), false);
        }

        inline size_result_t operator+(size_result_t const& other) const {
            return size_result_t(
                m_bytes + other.m_bytes,
                m_has_unknown_parts || other.m_has_unknown_parts);
        }

        inline size_t size_in_bytes() const {
            return m_bytes;
        }

        inline double size_in_kibibytes() const {
            return double(m_bytes) / 1024.0;
        }

        inline double size_in_mebibytes() const {
            return double(m_bytes) / 1024.0 / 1024.0;
        }

        inline bool is_exact() const {
            return !m_has_unknown_parts;
        }
    };

    template<typename T>
    struct heap_size {
        static size_result_t compute(T const& val) {
            return size_result_t::object_with_unknown_child_data_size<T>();
        }
    };

#define gen_heap_size_without_indirection(...) \
    template<>\
    struct heap_size<__VA_ARGS__> {\
        static size_result_t compute(__VA_ARGS__ const& val) {\
            return size_result_t::object_without_indirection<__VA_ARGS__>();\
        }\
    };

    gen_heap_size_without_indirection(unsigned char)
    gen_heap_size_without_indirection(signed char)
    gen_heap_size_without_indirection(char)
    gen_heap_size_without_indirection(unsigned short int)
    gen_heap_size_without_indirection(unsigned int)
    gen_heap_size_without_indirection(unsigned long int)
    gen_heap_size_without_indirection(unsigned long long int)
    gen_heap_size_without_indirection(signed short int)
    gen_heap_size_without_indirection(signed int)
    gen_heap_size_without_indirection(signed long int)
    gen_heap_size_without_indirection(signed long long int)
    gen_heap_size_without_indirection(float)
    gen_heap_size_without_indirection(double)
}
