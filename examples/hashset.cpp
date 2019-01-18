#include <iostream>
#include <vector>

#include <cstdint>
#include <tudocomp/util/compact_hashset/generic_compact_hashset.hpp>
#include <tudocomp/util/compact_hash/hash_functions.hpp>
#include <tudocomp/util/compact_hashset/index_structure/cv_bvs_t.hpp>
#include <algorithm>
#include <tudocomp/util/serialization.hpp>




using set_type = tdc::compact_sparse_hashset::generic_hashset_t<
    tdc::compact_sparse_hashset::poplar_xorshift_t,
    tdc::compact_sparse_hashset::cv_bvs_t
    >;

int main() {
    // creates a set with capacity zero and bit-width five
    auto set = set_type(0, 5);
    for(int i = 0; i <= 4; ++i) { // can hash keys in the range [0..2**5-1]
         set.lookup_insert(i*i);
    }
    for(int i = 0; i <= 15; ++i) {
        auto ret = set.lookup(i);
        if(ret.found()) {
            std::cout << "Id of node : " << ret.id() << std::endl; // returns the unique ID of the entry. This ID does not change until resizing occurs.
            std::cout << i << " -> " << ret.found() << std::endl; // checks whether set[i] is set
            std::cout << std::endl;
        }
    }
    std::stringstream ss;
    tdc::serialize<set_type>::write(ss, set);
    std::cout << ss.str() << std::endl;

}

