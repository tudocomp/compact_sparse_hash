#pragma once

#include <iostream>
#include <typeinfo>
#include <tuple>

namespace tdc {
    inline bool equal_diagnostic(bool v, char const* msg) {
        if (!v) {
            std::cerr << "not equal: " << msg << "\n";
        }
        return v;
    }
#define gen_equal_diagnostic(e) \
    equal_diagnostic(e, #e)

#define gen_equal_check(field, ...)                                          \
    gen_equal_diagnostic(                                                         \
        serialize<decltype(lhs.field)>::equal_check(lhs.field, rhs.field, ##__VA_ARGS__))

    template<typename T>
    struct serialize {
        static void write(std::ostream& out, T const& val) {
            CHECK(false) << "Need to implement the trait for type " << typeid(T).name();
        }
        static T read(std::istream& in) {
            CHECK(false) << "Need to implement the trait for type " << typeid(T).name();
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            CHECK(false) << "Need to implement the trait for type " << typeid(T).name();
            return false;
        }
    };

    template<>
    struct serialize<uint8_t> {
        using T = uint8_t;

        static void write(std::ostream& out, T const& val) {
            out.put(val);
        }
        static T read(std::istream& in) {
            char v;
            in.get(v);
            return v;
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            return gen_equal_diagnostic(lhs == rhs);
        }
    };
    template<>
    struct serialize<float> {
        using T = float;

        static void write(std::ostream& out, T const& val) {
            out.write((char const*) &val, sizeof(T));
        }
        static T read(std::istream& in) {
            T val;
            in.read((char*) &val, sizeof(T));
            return val;
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            return gen_equal_diagnostic(lhs == rhs);
        }
    };
    template<>
    struct serialize<unsigned int> {
        using T = unsigned int;

        static void write(std::ostream& out, T const& val) {
            out.write((char const*) &val, sizeof(T));
        }
        static T read(std::istream& in) {
            T val;
            in.read((char*) &val, sizeof(T));
            return val;
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            return gen_equal_diagnostic(lhs == rhs);
        }
    };
    template<>
    struct serialize<unsigned long int> {
        using T = unsigned long int;

        static void write(std::ostream& out, T const& val) {
            out.write((char const*) &val, sizeof(T));
        }
        static T read(std::istream& in) {
            T val;
            in.read((char*) &val, sizeof(T));
            return val;
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            return gen_equal_diagnostic(lhs == rhs);
        }
    };
    template<>
    struct serialize<unsigned long long int> {
        using T = unsigned long long int;

        static void write(std::ostream& out, T const& val) {
            out.write((char const*) &val, sizeof(T));
        }
        static T read(std::istream& in) {
            T val;
            in.read((char*) &val, sizeof(T));
            return val;
        }
        static bool equal_check(T const& lhs, T const& rhs) {
            return gen_equal_diagnostic(lhs == rhs);
        }
    };


}
