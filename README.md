Compact Sparse Hash Table
========

The compact sparse hash table is a blend of compact hashing [1] and 
[Google's sparse hash table](https://github.com/sparsehash/sparsehash).
Our hash table is more memory efficient than both variants when the hash table is not much filled.
The restriction is that it can only hash integer keys, but of arbitrary bit width.

# Usage

A minimal example is
``
#include <tudocomp/util/compact_sparse_hash.hpp>
// creates a hash table with zero entries, set the bit-width of the keys to four
auto map = tdc::compact_sparse_hashtable::compact_sparse_hashtable_t<int>(0, 4); 
for(int i = 0; i <= 15; ++i) { // interval [0..15] can be represented by 4-bits 
	map.insert(i, std::move(i*i)); // insert key i, value i*i 
	std::cout << i << " -> " << map[i] << std::endl; // map[i] returns value with key i 
} 
``

# How it works

It applies the approach of Cleary [1], in which a bijective hash function is used for 
determining the initial position, i.e., where to try to store a (key,value)-pair.
The bijective hash functions allows us to store only a fragment of the key in the hash table.
The complete key of a fragment can be restored by additionally knowing its initial address.
Unfortunately, due to collisions, it happens that the (fragment key,value)-pair is misplaced (i.e., it is not 
stored at its initial address). 
The initial address can be restored by additionally storing two bit vectors, and restricting the 
collision resolving to linear probing.
In summary, this technique saves space by not saving the full keys, but only a fragment of it plus two bit vectors
of the length of the hash table.

To further slim down the space footprint, we apply the trick of the sparse hash table:
Instead of allocating a large hash table, we allocate a vector of pointers to buckets.
Each bucket represents a section of length B of the hash table, such that we have n/B buckets if the hash table is of size n
(we assure that n is divible by B such that all buckets have the same length B).
Although a bucket could store B elements, it only acquires space for the actually saved (fragment key,value)-pairs in it.
For that, it stores a bit vector of length B marking with a one all positions in its section of the hash table that are actually occupied by a
(fragment key,value)-pair.
The (fragment key,value)-pair corresponding to the i-th one in the bit vector is the i-th element of the bucket.
Given that we want to access the j-th element in the section belonging to a bucket,
we need to count how many one's up to the j-th position are stored in the bit vector in the bucket.
This number is the entry number of the element in the bucket.
By keeping B small enough, we argue that the entire bucket can be stored in cache, allowing us to work with the bit vector
with modern CPU instructions like popcount. 
When inserting a new (key,value)-pair into the bucket, we update the bit vector, and move the stored elements adequately
(like in a standard std::vector). However, this is not a performance bottleneck, since again, with a sufficiently small bucket since,
this operation is computed efficiently on modern computer hardware.
Currently, we have set the bucket size B to 64.

In summary, we have the following constraints:
* keys have to be integers
* linear probing
* hash table size is always a power of two
* hash function must be bijective
* API is not STL-conform

# Features
* The bit width of the keys can be updated on-line.
  Changing the bit width causes a rehashing of the complete hash table.
* Supports multiple hash functions. Currently, a xorshift hash function is implemented.
* On resizing the hash table, each bucket of the old hash table is rehashed and subsequently freed,
  such that there is no high memory peak like in traditional hash tables that need to keep entire old and new hash table
  in RAM during a resize operation.


# Dependencies

The project is written in modern `C++14`.
It uses `cmake` to build the library.

The external dependencies are:

* [Google Logging (glog)](https://github.com/google/glog) (0.34 or later).
* [Google Test](https://github.com/google/googletest) (1.7.0 or later) __[Just for running the unit tests]__.

`cmake` searches the external dependencies first on the system, 
or automatically downloads and builds them from their official repositories.
In that regard, a proper installation of the dependencies is not required.

# License

The code in this repository is published under the
[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)

# Todo
* When additionally restricting the values to be integers, we can avoid padding: 
  We currently byte-align the values to allow the reinterpretation of its content (just by casting).
  By restricting to integer values, we can write the values bit-compact in a bit vector.
* Additionally, in the case that we work with values that are integers, 
  we want to support setting the width of the integer values online to further slim down memory consumption.

# References
[1] J. G. Cleary. Compact hash tables using bidirectional linear probing. IEEE Trans. Computers, 33(9): 828-834, 1984.
