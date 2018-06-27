#pragma once

#include <iostream>

namespace tdc {
    template<typename T>
    struct serialize {
        static void write(std::ostream& out, T const& val) {
            DCHECK(false) << "Need to implement the trait for a custom type";
        }
        static T read(std::istream& in) {
            DCHECK(false) << "Need to implement the trait for a custom type";
        }
    };
}
