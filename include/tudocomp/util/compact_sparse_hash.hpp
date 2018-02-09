#pragma once

#include <memory>
#include <cstdint>
#include <utility>
#include <algorithm>

#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/ds/IntPtr.hpp>

#include "compact_sparse_hashtable/util.hpp"
#include "compact_sparse_hashtable/bucket_t.hpp"
#include "compact_sparse_hashtable/bucket_element_t.hpp"
#include "compact_sparse_hashtable/hash_functions.hpp"
#include "compact_sparse_hashtable/size_manager_t.hpp"
#include "compact_sparse_hashtable/sparse_pos_t.hpp"
#include "compact_sparse_hashtable/decomposed_key_t.hpp"

namespace tdc {namespace compact_sparse_hashtable {

template<typename val_t, typename hash_t = xorshift_t>
class compact_sparse_hashtable_t {
    using key_t = uint64_t;
    using buckets_t = std::vector<Bucket<val_t>>;

    // TODO: Change this, and fix tests
    static constexpr size_t DEFAULT_KEY_WIDTH = 16;

    struct bucket_layout_t {
        static constexpr size_t BVS_WIDTH_SHIFT = 6;
        static constexpr size_t BVS_WIDTH_MASK = 0b111111;

        static inline size_t table_pos_to_idx_of_bucket(size_t pos) {
            return pos >> BVS_WIDTH_SHIFT;
        }

        static inline size_t table_pos_to_idx_inside_bucket(size_t pos) {
            return pos & BVS_WIDTH_MASK;
        }

        static inline size_t table_size_to_bucket_size(size_t size) {
            return (size + BVS_WIDTH_MASK) >> BVS_WIDTH_SHIFT;
        }
    };

    size_manager_t m_sizing;
    uint8_t m_width;

    // Compact table data
    IntVector<uint_t<2>> m_cv;

    // Sparse table data
    buckets_t m_buckets;

    inline static constexpr size_t min_size(size_t size) {
        return (size < 2) ? 2 : size;
    }

public:

    inline compact_sparse_hashtable_t(size_t size, size_t key_width = DEFAULT_KEY_WIDTH):
        m_sizing(min_size(size)),
        m_width(key_width)
    {
        size_t cv_size = table_size();
        size_t buckets_size = bucket_layout_t::table_size_to_bucket_size(table_size());

        // std::cout << "cv_size: " << cv_size << ", buckets_size: " << buckets_size << "\n";

        m_cv.reserve(cv_size);
        m_cv.resize(cv_size);
        m_buckets.reserve(buckets_size);
        m_buckets.resize(buckets_size);
    }

    inline ~compact_sparse_hashtable_t() {
        destroy_buckets();
    }

    inline compact_sparse_hashtable_t(compact_sparse_hashtable_t&& other):
        m_sizing(std::move(other.m_sizing)),
        m_width(std::move(other.m_width)),
        m_cv(std::move(other.m_cv)),
        m_buckets(std::move(other.m_buckets))
    {
    }

    inline compact_sparse_hashtable_t& operator=(compact_sparse_hashtable_t&& other) {
        destroy_buckets();

        m_sizing = std::move(other.m_sizing);
        m_width = std::move(other.m_width);
        m_cv = std::move(other.m_cv);
        m_buckets = std::move(other.m_buckets);

        return *this;
    }

private:
    inline size_t table_size() {
        return m_sizing.capacity();
    }

    inline bool get_v(size_t pos) {
        return (m_cv.at(pos) & 0b01) != 0;
    }

    inline bool get_c(size_t pos) {
        return (m_cv.at(pos) & 0b10) != 0;
    }

    inline void set_v(size_t pos, bool v) {
        auto x = m_cv.at(pos) & 0b10;
        m_cv.at(pos) = x | (0b01 * v);
    }

    inline void set_c(size_t pos, bool c) {
        auto x = m_cv.at(pos) & 0b01;
        m_cv.at(pos) = x | (0b10 * c);
    }

    inline void sparse_drop_bucket(size_t i) {
        size_t qw = quotient_width();
        m_buckets.at(i).destroy_vals(qw);
        m_buckets.at(i) = Bucket<val_t>();
    }

    inline void destroy_buckets() {
        size_t qw = quotient_width();
        for(size_t i = 0; i < m_buckets.size(); i++) {
            if (!m_buckets[i].is_empty()) {
                m_buckets[i].destroy_vals(qw);
            }
        }
    }

    inline bool dcheck_key_width(uint64_t key) {
        uint64_t key_mask = (1ull << (m_width - 1ull) << 1ull) - 1ull;
        bool key_is_too_large = key & ~key_mask;
        return !key_is_too_large;
    }

    /// The actual amount of bits currently usable for
    /// storing a key in the hashtable.
    ///
    /// Due to implementation details, this can be
    /// larger than `key_width()`.
    inline size_t real_width() {
        /// NB: There are two cases:
        /// - If all bits of the the key fit into the initial_address space,
        ///   then the stored_quot bitvector inside the buckets would
        ///   have to store integers of width 0. This is undefined behavior
        ///   with the current code, so we add a padding bit.
        /// - Otherwise the current maximum key width `m_width`
        ///   determines the real width.
        return std::max<size_t>(m_sizing.capacity_log2() + 1, m_width);
    }

    inline size_t key_width() {
        return m_width;
    }

    inline size_t initial_address_width() {
        return m_sizing.capacity_log2();
    }

    inline size_t quotient_width() {
        return real_width() - m_sizing.capacity_log2();
    }

    inline decomposed_key_t decompose_key(uint64_t key) {
        DCHECK(dcheck_key_width(key)) << "Attempt to decompose key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not";

        uint64_t hres = hash_t::hashfn(key, real_width());

        DCHECK_EQ(hash_t::reverse_hashfn(hres, real_width()), key);

        return m_sizing.decompose_hashed_value(hres);
    }

    inline uint64_t compose_key(uint64_t initial_address, uint64_t quotient) {
        uint64_t harg = m_sizing.compose_hashed_value(initial_address, quotient);
        uint64_t key = hash_t::reverse_hashfn(harg, real_width());

        DCHECK(dcheck_key_width(key)) << "Composed key " << key << ", which requires more than the current set maximum of " << key_width() << " bits, but should not";
        return key;
    }

    template<typename handler_t>
    inline void shift_insert_handler(size_t from,
                                     size_t to,
                                     uint64_t quot,
                                     handler_t&& handler) {
        DCHECK_NE(from, to);

        for(size_t i = to; i != from;) {
            size_t next_i = mod_sub(i, size_t(1));

            set_c(i, get_c(next_i));

            i = next_i;
        }
        shift_insert_sparse_handler(from, to, quot, std::move(handler));

        set_c(from, false);
    }

    struct SearchedGroup {
        size_t group_start;       // group that belongs to the key
        size_t group_end;         // it's a half-open range: [start .. end)
        size_t groups_terminator; // next free location
    };

    // assumption: there exists a group at the location indicated by key
    // this group is either the group belonging to key,
    // or the one after it in the case that no group for key exists yet
    inline SearchedGroup search_existing_group(decomposed_key_t const& key) {
        auto ret = SearchedGroup();

        // walk forward from the initial address until we find a empty location
        // TODO: This search could maybe be accelerated by:
        // - checking whole blocks in the bucket bitvector for == or != 0
        size_t cursor = key.initial_address;
        size_t v_counter = 0;
        DCHECK_EQ(get_v(cursor), true);

        for(; sparse_exists(cursor); cursor = mod_add(cursor)) {
            v_counter += get_v(cursor);
        }
        DCHECK_GE(v_counter, 1);
        ret.groups_terminator = cursor;

        // walk back again to find start of group belong to the initial address
        size_t c_counter = v_counter;
        for(; c_counter != 1; cursor = mod_sub(cursor)) {
            c_counter -= get_c(mod_sub(cursor));
        }

        ret.group_end = cursor;

        for(; c_counter != 0; cursor = mod_sub(cursor)) {
            c_counter -= get_c(mod_sub(cursor));
        }

        ret.group_start = cursor;

        return ret;
    }

    inline val_t* search(SearchedGroup const& res, uint64_t stored_quotient) {
        for(size_t i = res.group_start; i != res.group_end; i = mod_add(i)) {
            auto sparse_entry = sparse_get_at(i);

            if (sparse_entry.get_quotient() == stored_quotient) {
                return &sparse_entry.val();
            }
        }
        return nullptr;
    }

    inline val_t* search(uint64_t key) {
        auto dkey = decompose_key(key);
        if (get_v(dkey.initial_address)) {
            return search(search_existing_group(dkey), dkey.stored_quotient);
        }
        return nullptr;
    }

public:

    // TODO: This is not a std interface
    inline void insert(uint64_t key, val_t&& value) {
        insert(key, std::move(value), m_width);
    }
    inline void insert(uint64_t key, val_t&& value, size_t key_width) {
        insert_handler(key, key_width, InsertHandler {
            std::move(value)
        });
    }

    inline val_t& operator[](uint64_t key) {
        return index(key, m_width);
    }

    inline val_t& index(uint64_t key, size_t key_width) {
        val_t* addr = nullptr;

        insert_handler(key, key_width, AddressDefaultHandler {
            &addr
        });

        DCHECK(addr != nullptr);

        return *addr;
    }

    inline size_t size() const {
        return m_sizing.size();
    }

private:
    // Handler for inserting an element that exists as a rvalue reference.
    // This will overwrite an existing element.
    class InsertHandler {
        val_t&& m_value;
    public:
        InsertHandler(val_t&& value): m_value(std::move(value)) {}

        inline auto on_new() {
            struct InsertHandlerOnNew {
                val_t&& m_value;
                inline val_t& get() {
                    return m_value;
                }
                inline void new_location(val_t& value) {
                }
            };

            return InsertHandlerOnNew {
                std::move(m_value),
            };
        }

        inline void on_existing(val_t& value) {
            m_value = std::move(value);
        }
    };

    // Handler for getting the address of an element in the map.
    // If none exists yet, it will be default constructed.
    class AddressDefaultHandler {
        val_t** m_address = nullptr;
    public:
        AddressDefaultHandler(val_t** address): m_address(address) {}

        inline auto on_new() {
            struct AddressDefaultHandlerOnNew {
                val_t m_value;
                val_t** m_address;
                inline val_t& get() {
                    return m_value;
                }
                inline void new_location(val_t& value) {
                    *m_address = &value;
                }
            };

            return AddressDefaultHandlerOnNew {
                val_t(),
                m_address,
            };
        }

        inline void on_existing(val_t& value) {
            *m_address = &value;
        }
    };

    template<typename handler_t>
    inline void insert_after(SearchedGroup const& res,
                             decomposed_key_t const& dkey,
                             handler_t&& handler)
    {
        // this will insert the value at the end of the range defined by res

        if (sparse_is_empty(res.group_end)) {
            // if there is no following group, just append the new entry

            sparse_set_at_empty_handler(res.group_end,
                                        dkey.stored_quotient,
                                        std::move(handler));
        } else {
            // else, shift all following elements

            shift_insert_handler(res.group_end,
                                 res.groups_terminator,
                                 dkey.stored_quotient,
                                 std::move(handler));
        }
    }

    template<typename handler_t>
    inline void insert_handler(uint64_t key, size_t key_width, handler_t&& handler) {
        grow_if_needed(key_width);
        auto const dkey = decompose_key(key);

        DCHECK_EQ(key, compose_key(dkey.initial_address, dkey.stored_quotient));

        // cases:
        // - initial address empty
        // - initial address full, v[initial address] = 1
        // - initial address full, v[initial address] = 0

        DCHECK_LT(bucket_layout_t::table_pos_to_idx_of_bucket(dkey.initial_address),
                  m_buckets.size());

        if (sparse_is_empty(dkey.initial_address)) {
            // check if we can insert directly

            sparse_set_at_empty_handler(dkey.initial_address,
                                        dkey.stored_quotient,
                                        std::move(handler));

            // we created a new group, so update the bitflags
            set_v(dkey.initial_address, true);
            set_c(dkey.initial_address, true);
            m_sizing.size()++;
        } else {
            // check if there already is a group for this key
            bool const group_exists = get_v(dkey.initial_address);

            if (group_exists) {
                auto const res = search_existing_group(dkey);

                // check if element already exists
                auto p = search(res, dkey.stored_quotient);
                if (p != nullptr) {
                    handler.on_existing(*p);
                } else {
                    insert_after(res, dkey, std::move(handler));
                    m_sizing.size()++;
                }
            } else {
                // insert a new group

                // pretend we already inserted the new group
                // this makes insert_after() find the group
                // at the location _before_ the new group
                set_v(dkey.initial_address, true);
                auto const res = search_existing_group(dkey);

                // insert the element after the found group
                insert_after(res, dkey, std::move(handler));

                // mark the inserted element as the start of a new group,
                // thus fixing-up the v <-> c mapping
                set_c(res.group_end, true);

                m_sizing.size()++;
            }
        }
    }

    template<typename int_t>
    inline int_t mod_add(int_t v, int_t add = 1) {
        size_t mask = table_size() - 1;
        return (v + add) & mask;
    }
    template<typename int_t>
    inline int_t mod_sub(int_t v, int_t sub = 1) {
        size_t mask = table_size() - 1;
        return (v - sub) & mask;
    }

    struct iter_all_t {
        compact_sparse_hashtable_t<val_t>& m_self;
        size_t i = 0;
        size_t original_start = 0;
        uint64_t initial_address = 0;
        enum {
            EMPTY_LOCATIONS,
            FULL_LOCATIONS
        } state = EMPTY_LOCATIONS;

        inline iter_all_t(compact_sparse_hashtable_t<val_t>& self): m_self(self) {
            // first, skip forward to the first empty location
            // so that iteration can start at the beginning of the first complete group

            i = 0;

            for(;;i++) {
                if (m_self.sparse_is_empty(i)) {
                    break;
                }
            }

            // Remember our startpoint so that we can recognize it when
            // we wrapped around back to it
            original_start = i;

            // We proceed to the next position so that we can iterate until
            // we reach `original_start` again.
            i = m_self.mod_add(i);
        }

        inline bool next(uint64_t* out_initial_address, size_t* out_i) {
            while(true) {
                if (state == EMPTY_LOCATIONS) {
                    // skip empty locations
                    for(;;i = m_self.mod_add(i)) {
                        if (m_self.sparse_exists(i)) {
                            // we initialize init-addr at 1 pos before the start of
                            // a group of blocks, so that the blocks iteration logic works
                            initial_address = m_self.mod_sub(i);
                            state = FULL_LOCATIONS;
                            break;
                        }
                        if (i == original_start) {
                            return false;
                        }
                    }
                } else {
                    // process full locations
                    if (m_self.sparse_is_empty(i))  {
                        state = EMPTY_LOCATIONS;
                        continue;
                    }
                    if (m_self.get_c(i)) {
                        // skip forward m_v cursor
                        // to find initial address for this block
                        //
                        // this works for the first block because
                        // initial_address starts at 1 before the group
                        initial_address = m_self.mod_add(initial_address);

                        while(!m_self.get_v(initial_address)) {
                            initial_address = m_self.mod_add(initial_address);
                        }
                    }

                    *out_initial_address = initial_address;
                    *out_i = i;

                    i = m_self.mod_add(i);
                    return true;
                }
            }
        }
    };


    inline void grow_if_needed(size_t new_width) {
        auto needs_capacity_change = [&]() {
            return(m_sizing.capacity() / 2) <= (m_sizing.size() + 1);
        };

        auto needs_realloc = [&]() {
            return needs_capacity_change() || (new_width != m_width);
        };

        /*
        std::cout
                << "buckets size/cap: " << m_buckets.size()
                << ", size: " << m_sizing.size()
                << "\n";
        */

        if (needs_realloc()) {
            size_t new_capacity;
            if (needs_capacity_change()) {
                new_capacity = m_sizing.capacity() * 2;
            } else {
                new_capacity = m_sizing.capacity();
            }
            auto new_table = compact_sparse_hashtable_t(new_capacity, new_width);

            /*
            std::cout
                << "grow to cap " << new_table.table_size()
                << ", m_width: " << int(new_table.m_width)
                << ", real_width: " << new_table.real_width()
                << ", quot width: " << new_table.quotient_width()
                << "\n";
            */

            bool start_of_bucket = false;
            size_t bucket = 0;

            uint64_t initial_address;
            size_t i;
            auto iter = iter_all_t(*this);
            while(iter.next(&initial_address, &i)) {
                auto p = sparse_pos(i);

                // drop buckets of old table as they get emptied out
                if (p.offset_in_bucket() == 0) {
                    if (start_of_bucket) {
                        DCHECK_NE(bucket, p.bucket_pos);
                        sparse_drop_bucket(bucket);
                    }

                    start_of_bucket = true;
                    bucket = p.bucket_pos;
                }

                auto kv = sparse_get_at(p);
                auto stored_quotient = kv.get_quotient();
                auto& val = kv.val();
                key_t key = compose_key(initial_address, stored_quotient);

                new_table.insert(key, std::move(val));
            }

            *this = std::move(new_table);
        }

        DCHECK(!needs_realloc());
    }

public:
    // for tests:

    inline std::string debug_state() {
        std::stringstream ss;

        bool gap_active = false;
        size_t gap_start;
        size_t gap_end;

        auto print_gap = [&](){
            if (gap_active) {
                gap_active = false;
                ss << "    [" << gap_start << " to " << gap_end << " empty]\n";
            }
        };

        auto add_gap = [&](size_t i){
            if (!gap_active) {
                gap_active = true;
                gap_start = i;
            }
            gap_end = i;
        };

        std::vector<std::string> lines(table_size());

        uint64_t initial_address;
        size_t j;
        auto iter = iter_all_t(*this);
        while(iter.next(&initial_address, &j)) {
            std::stringstream ss2;

            auto kv = sparse_get_at(j);
            auto stored_quotient = kv.get_quotient();
            auto& val = kv.val();
            key_t key = compose_key(initial_address, stored_quotient);

            ss2 << j
                << "\t: v = " << get_v(j)
                << ", c = " << get_c(j)
                << ", quot = " << stored_quotient
                << ", iadr = " << initial_address
                << "\t, key = " << key
                << "\t, value = " << val
                << "\t (" << &val << ")";

            lines.at(j) = ss2.str();
        }

        ss << "[\n";
        for (size_t i = 0; i < table_size(); i++) {
            bool cv_exist = lines.at(i) != "";

            DCHECK_EQ(cv_exist, sparse_exists(i));

            if (cv_exist) {
                print_gap();
                ss << "    "
                    << lines.at(i)
                    << "\n";
            } else {
                add_gap(i);
            }
        }
        print_gap();
        ss << "]";

        return ss.str();
    }

    inline void debug_check_single(uint64_t key, val_t const& val) {
        auto ptr = search(key);
        ASSERT_NE(ptr, nullptr) << "key " << key << " not found!";
        if (ptr != nullptr) {
            ASSERT_EQ(*ptr, val) << "value is " << *ptr << " instead of " << val;
        }
    }

    inline void debug_print() {
        std::cout << "m_width: " << uint32_t(m_width) << "\n",
        std::cout << "size: " << size() << "\n",
        std::cout << debug_state() << "\n";
    }

private:
    template<typename handler_t>
    inline void sparse_set_at_empty_handler(size_t pos, key_t quot, handler_t&& handler) {
        auto data = sparse_pos(pos);
        DCHECK(!data.exists_in_bucket());

        // TODO: Check that the sparse_pos to a invalid position works correctly

        // figure out which bucket to access
        auto& bucket = m_buckets[data.bucket_pos];
        size_t const qw = quotient_width();

        // we will insert a new element
        auto value_handler = handler.on_new();
        auto& val = value_handler.get();

        // insert element & grow bucket as appropriate
        insert_in_bucket(bucket, data.offset_in_bucket(), data.b_mask, qw, std::move(val), quot);

        // notify handler with location of new element
        auto new_loc = bucket.at(data.offset_in_bucket(), qw);
        value_handler.new_location(new_loc.val());
    }

    inline bool sparse_is_empty(size_t i) {
        return !sparse_exists(i);
    }

    inline bool sparse_exists(size_t pos) {
        return sparse_pos(pos).exists_in_bucket();
    }

    // shifts all elements one to the right,
    // inserts val and quot at the from position,
    // and stores the old from element in val and quot.
    inline void sparse_shift(size_t from, size_t to, val_t& val, key_t& quot) {
        // pseudo-iterator for iterating over bucket elements
        // NB: does not wrap around!
        struct iter {
            Bucket<val_t> const* m_bucket;
            BucketElem<val_t>    m_b_start;
            BucketElem<val_t>    m_b_end;
            size_t               m_quotient_width;

            inline void set_bucket_elem_range(size_t end_offset) {
                size_t start_offset = 0;
                DCHECK_LE(start_offset, end_offset);

                m_b_start = m_bucket->at(start_offset, m_quotient_width);
                m_b_end   = m_bucket->at(end_offset, m_quotient_width);
            }

            inline iter(compact_sparse_hashtable_t& table,
                        SparsePos const& pos) {
                m_quotient_width = table.quotient_width();

                // NB: Using pointer arithmetic here, because
                // we can (intentionally) end up with the address 1-past
                // the end of the vector, which represents a end-iterator
                m_bucket = table.m_buckets.data() + pos.bucket_pos;

                if(pos.bucket_pos < table.m_buckets.size()) {
                    set_bucket_elem_range(pos.offset_in_bucket());
                } else {
                    // use default constructed nullptr BucketElems
                }
            }

            inline BucketElem<val_t> get() {
                return m_b_end;
            }

            inline void decrement() {
                if (!m_b_start.ptr_eq(m_b_end)) {
                    m_b_end.decrement_ptr();
                } else {
                    do {
                        --m_bucket;
                    } while(m_bucket->bv() == 0);
                    set_bucket_elem_range(m_bucket->size() - 1);
                }
            }

            inline bool operator!=(iter& other) {
                return !(m_b_end.ptr_eq(other.m_b_end));
            }
        };

        DCHECK_LT(from, to);

        // initialize iterators like this:
        // [         ]
        // ^from   to^
        //          ||
        //    <- src^|
        //    <- dest^

        auto from_loc = sparse_pos(from);
        auto from_iter = iter(*this, from_loc);

        auto last = sparse_pos(to - 1);
        auto src = iter(*this, last);
        auto dst = iter(*this, sparse_pos(to));

        // move the element at the last position to a temporary position
        auto  tmp_p    = sparse_get_at(last);
        val_t tmp_val  = std::move(tmp_p.val());
        key_t tmp_quot = tmp_p.get_quotient();

        // move all elements one to the right
        while(src != from_iter) {
            // Decrement first for backward iteration
            src.decrement();
            dst.decrement();

            // Get access to the value/quotient at src and dst
            auto src_be = src.get();
            auto dst_be = dst.get();

            // Copy value/quotient over
            dst_be.val() = std::move(src_be.val());
            dst_be.set_quotient(src_be.get_quotient());
        }

        // move new element into empty from position
        auto from_p = sparse_get_at(from_loc);
        from_p.val() = std::move(val);
        from_p.set_quotient(quot);

        // move temporary element into the parameters
        val = std::move(tmp_val);
        quot = tmp_quot;
    }

    template<typename handler_t>
    inline void shift_insert_sparse_handler(size_t from,
                                            size_t to,
                                            key_t quot,
                                            handler_t&& handler) {
        // move from...to one to the right, then insert at from

        DCHECK(from != to);

        auto value_handler = handler.on_new();
        auto& val = value_handler.get();

        if (to < from) {
            // if the range wraps around, we decompose into two ranges:
            // [   |      |      ]
            // | to^      ^from  |
            // ^start         end^
            // [ 2 ]      [  1   ]
            //
            // NB: because we require from != to, and insert 1 additional element,
            // we are always dealing with a minimum 2 element range,
            // and thus can not end up with a split range with length == 0

            // inserts the new element at the start of the range,
            // and temporarily stores the element at the end of the range
            // in `val` and `quot`
            sparse_shift(from,  table_size(), val, quot);
            sparse_shift(0, to, val, quot);
        } else {
            // inserts the new element at the start of the range,
            // and temporarily stores the element at the end of the range
            // in `val` and `quot`
            sparse_shift(from, to, val, quot);
        }

        // insert the element from the end of the range at the free
        // position to the right of it
        auto insert = InsertHandler(std::move(val));
        sparse_set_at_empty_handler(to, quot, std::move(insert));

        // after the previous insert and a potential reallocation,
        // notify the handler about the address of the new value
        auto new_loc = sparse_pos(from);
        value_handler.new_location(sparse_get_at(new_loc).val());
    }


    using SparsePos = sparse_pos_t<buckets_t, bucket_layout_t>;

    SparsePos sparse_pos(size_t pos) {
        return SparsePos { pos, m_buckets };
    }

    inline BucketElem<val_t> sparse_get_at(SparsePos pos) {
        DCHECK(pos.exists_in_bucket());
        size_t qw = quotient_width();

        return m_buckets[pos.bucket_pos].at(pos.offset_in_bucket(), qw);
    }

    inline BucketElem<val_t> sparse_get_at(size_t pos) {
        return sparse_get_at(sparse_pos(pos));
    }
};

}}
