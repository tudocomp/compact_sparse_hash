#pragma once

#include <iostream>
#include <typeinfo>
#include <tuple>
#include <memory>

namespace tdc {
    class object_size_t {
        size_t m_bytes = 0;
        bool m_has_unknown_parts = false;

        object_size_t() = default;
        object_size_t(size_t bytes, bool has_unknown_parts):
            m_bytes(bytes), m_has_unknown_parts(has_unknown_parts) {}
    public:
        inline static object_size_t empty() {
            return object_size_t(0, false);
        }
        inline static object_size_t exact(size_t size) {
            return object_size_t(size, false);
        }
        inline static object_size_t unknown_extra_data(size_t size) {
            return object_size_t(size, true);
        }

        inline object_size_t operator+(object_size_t const& other) const {
            return object_size_t(
                m_bytes + other.m_bytes,
                m_has_unknown_parts || other.m_has_unknown_parts);
        }
        inline object_size_t& operator+=(object_size_t const& other) {
            m_bytes += other.m_bytes;
            m_has_unknown_parts |= other.m_has_unknown_parts;
            return *this;
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

        inline friend std::ostream& operator<<(std::ostream& out, object_size_t const& v) {
            if (!v.is_exact()) {
                out << ">=";
            }
            out << v.size_in_kibibytes();
            out << " KiB";
            return out;
        }
    };

    template<typename T>
    struct heap_size {
        static object_size_t compute(T const& val) {
            return object_size_t::unknown_extra_data(sizeof(T));
        }
    };

#define gen_heap_size_without_indirection(...) \
    template<>\
    struct heap_size<__VA_ARGS__> {\
        static object_size_t compute(__VA_ARGS__ const& val) {\
            return object_size_t::exact(sizeof(__VA_ARGS__));\
        }\
    };

    gen_heap_size_without_indirection(bool)
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

    template<typename T>
    struct heap_size<std::unique_ptr<T[]>> {
        static object_size_t compute(std::unique_ptr<T[]> const& val, size_t size) {
            auto bytes = object_size_t::exact(sizeof(val));

            for (size_t i = 0; i < size; i++) {
                bytes += heap_size<T>::compute(val[i]);
            }

            return bytes;
        }
    };
}
