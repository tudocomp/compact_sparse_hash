#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cstdint>

#include <tudocomp/util/compact_hash/map/typedefs.hpp>
#include <tudocomp/util/serialization.hpp>
#include <tudocomp/util/heap_size.hpp>

template<typename val_t>
using map_type = tdc::compact_hash::map::sparse_elias_hashmap_t<val_t>;

int main() {
    // creates a hash table with default capacity and initial bit widths
    auto map = map_type<int>();
    for(int i = 0; i < 1000; ++i) {
        map.insert(i, i*i  + 42);
    }

    // this could just be an `ofstream` for outputting to a file.
    std::stringstream output_stream;

    // compute size of the datastructure
    auto heap_object_size = heap_size<map_type>::compute(map);

    // serialize the datastructure
    auto written_object_size = serialize<map_type>::write(output_stream, map);

    std::cout << "size in memory: " << heap_object_size.size_in_bytes() << std::endl;
    std::cout << "written bytes: " << written_object_size.size_in_bytes() << std::endl;
}
