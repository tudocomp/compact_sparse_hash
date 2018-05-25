#pragma once

namespace tdc {namespace compact_sparse_hashset {

class entry_t {
    uint64_t m_id;
    bool m_key_already_exist;
    bool m_not_found;

    inline entry_t(uint64_t id, bool key_already_exist, bool not_found):
        m_id(id),
        m_key_already_exist(key_already_exist),
        m_not_found(not_found) {}
public:
    /// Creates a `entry_t` for a key that already exists in the table.
    ///
    /// The _id_ is an integer that uniquely describes the key,
    /// while only taking up approximately log2(table_size) bits.
    /// It gets invalidated if the underlying table needs to be resized.
    inline static entry_t found_exist(uint64_t id) {
        return entry_t {
            id,
            true,
            false,
        };
    }

    /// Creates a `entry_t` for a new key in the table.
    ///
    /// The _id_ is an integer that uniquely describes the key,
    /// while only taking up approximately log2(table_size) bits.
    /// It gets invalidated if the underlying table needs to be resized.
    inline static entry_t found_new(uint64_t id) {
        return entry_t {
            id,
            false,
            false,
        };
    }

    /// Creates a `entry_t` for a key that could not be found in the table.
    inline static entry_t not_found() {
        return entry_t {
            0,
            false,
            true,
        };
    }

    /// Returns true if the key exists in the table.
    inline bool found() {
        return !m_not_found;
    }

    /// Returns the _id_ of the key.
    ///
    /// The _id_ is an integer that uniquely describes the key,
    /// while only taking up approximately log2(table_size) bits.
    /// It gets invalidated if the underlying table needs to be resized.
    inline uint64_t id() {
        DCHECK(found());
        return m_id;
    }

    /// Returns true if the key already exists in the table.
    inline bool key_already_exist() {
        DCHECK(found());
        return m_key_already_exist;
    }
};

}}
