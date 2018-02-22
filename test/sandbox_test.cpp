#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <tudocomp/util/compact_sparse_hash.hpp>
#include <tudocomp/util/bits.hpp>


using namespace tdc;
using namespace tdc::compact_sparse_hashtable;
using namespace std;

TEST(Sandbox, example) {
   auto map = tdc::compact_sparse_hashtable::compact_sparse_hashtable_t<int>(0, 4); // creates a hash table with zero entries, set the bit-width of the keys to three
   std::cout << "Key Width: " << map.key_width() << std::endl;
std::cout << "Add i -> i*i from i = 0 up to 15" << std::endl;
   for(int i = 0; i <= 15; ++i) { // interval [0..15] can be represented by 4-bits
           map.insert(i, std::move(i*i)); // insert key i, value i*i
           std::cout << i << " -> " << map[i] << std::endl; // map[i] returns value with key i
   }
   std::cout << "Size: " << map.size() << std::endl;
std::cout << "Update all values, set to i -> i" << std::endl;
   for(int i = 0; i <= 15; ++i) {
	 std::cout << "Previously: " << i << " -> " << map[i] << std::endl; // map[i] returns value with key i
       map[i] = i;
       std::cout << "Now: " << i << " -> " << map[i] << std::endl;
   }
   std::cout << "Size: " << map.size() << std::endl;
   std::cout << "Add 10 additional elements with key-width 9" << std::endl;
   for(int i = 1; i < 11; ++i) { // interval [0..15]<<5 can be represented by 9-bits
	   map.insert(i<<5, std::move(i+1), 9); // insert key i<<5, value i, key have a width of 9
	   std::cout << (i<<5) << " -> " << map[i<<5] << std::endl; // map[i] returns value with key i
   }
    std::cout << "Key Width: " << map.key_width() << std::endl;
    std::cout << "Size: " << map.size() << std::endl;
    std::cout << "Old values are still stored: " << std::endl;
   for(int i = 0; i <= 15; ++i) {
       std::cout << i << " -> " << map[i] << std::endl;
   }

 }
