#pragma once

#include <iostream>
#include <typeinfo>

namespace tdc {
    template<typename T>
    struct serialize {
        static void write(std::ostream& out, T const& val) {
            DCHECK(false) << "Need to implement the trait for type " << typeid(T).name();
        }
        static T read(std::istream& in) {
            DCHECK(false) << "Need to implement the trait for type " << typeid(T).name();
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
    };


}
