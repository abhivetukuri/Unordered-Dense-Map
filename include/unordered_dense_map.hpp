#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <string>
#include <limits>
#include <stdexcept>
#include <iterator>
#include <algorithm>

namespace detail
{
    class WyHash
    {
    public:
        static uint64_t hash(const void *key, size_t len, uint64_t seed = 0);

    private:
        static uint64_t wyhash64_mum(uint64_t A, uint64_t B);
        static uint64_t wyhash64_read(const uint8_t *p, size_t k);
    };

    template <typename T>
    struct hash_traits
    {
        static uint64_t hash(const T &key)
        {
            return WyHash::hash(&key, sizeof(T));
        }
        static uint8_t fingerprint(const T &key)
        {
            uint64_t h = hash(key);
            return static_cast<uint8_t>(h & 0xFF);
        }
    };

    template <>
    struct hash_traits<std::string>
    {
        static uint64_t hash(const std::string &key);
        static uint8_t fingerprint(const std::string &key);
    };

    uint64_t mix_hash(uint64_t hash);

    struct Bucket
    {
        uint64_t fingerprint : 8;  // 8-bit fingerprint for quick comparison
        uint64_t distance : 8;     // Distance from ideal position (for robin-hood)
        uint64_t occupied : 1;     // Whether bucket is occupied
        uint64_t tombstone : 1;    // Whether bucket is a tombstone
        uint64_t entry_index : 46; // Index into entries_ vector (up to 70 trillion entries)

        Bucket() : fingerprint(0), distance(0), occupied(0), tombstone(0), entry_index(0) {}

        bool is_empty() const { return !occupied && !tombstone; }
        bool is_tombstone() const { return !occupied && tombstone; }
        bool is_occupied() const { return occupied; }

        void clear()
        {
            fingerprint = 0;
            distance = 0;
            occupied = 0;
            tombstone = 0;
            entry_index = 0;
        }

        void set_occupied(uint8_t fp, uint8_t dist, size_t idx)
        {
            fingerprint = fp;
            distance = dist;
            occupied = 1;
            tombstone = 0;
            entry_index = idx;
        }

        void set_tombstone()
        {
            occupied = 0;
            tombstone = 1;
        }
    };
}

template <typename Key, typename Value, typename Hash = detail::hash_traits<Key>>
class unordered_dense_map
{
private:
    static constexpr size_t INITIAL_CAPACITY = 16;
    static constexpr float MAX_LOAD_FACTOR = 0.75f;
    static constexpr size_t MAX_DISTANCE = 255;

    struct Entry
    {
        Key key;
        Value value;

        Entry() = default;
        Entry(const Key &k, const Value &v) : key(k), value(v) {}
        Entry(Key &&k, Value &&v) : key(std::move(k)), value(std::move(v)) {}
    };

    std::vector<detail::Bucket> buckets_;
    std::vector<Entry> entries_;
    size_t size_;
    size_t capacity_;

public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = Entry;
    using size_type = size_t;

    unordered_dense_map() : size_(0), capacity_(INITIAL_CAPACITY)
    {
        buckets_.resize(capacity_);
    }
    unordered_dense_map(const unordered_dense_map &other) = default;
    unordered_dense_map(unordered_dense_map &&other) = default;
    unordered_dense_map &operator=(const unordered_dense_map &other) = default;
    unordered_dense_map &operator=(unordered_dense_map &&other) = default;

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type max_size() const { return std::numeric_limits<size_type>::max(); }

    Value &operator[](const Key &key)
    {
        auto [it, inserted] = try_emplace(key, Value{});
        return it->value;
    }

    Value &at(const Key &key)
    {
        auto it = find(key);
        if (it == end())
            throw std::out_of_range("Key not found");
        return it->value;
    }

    const Value &at(const Key &key) const
    {
        auto it = find(key);
        if (it == end())
            throw std::out_of_range("Key not found");
        return it->value;
    }

    class iterator
    {
    public:
        unordered_dense_map *map_;
        size_t index_;

        iterator(unordered_dense_map *map, size_t index) : map_(map), index_(index) {}

        Entry &operator*()
        {
            if (index_ >= map_->entries_.size())
                throw std::out_of_range("Iterator out of bounds");
            return map_->entries_[index_];
        }
        Entry *operator->()
        {
            if (index_ >= map_->entries_.size())
                throw std::out_of_range("Iterator out of bounds");
            return &map_->entries_[index_];
        }

        iterator &operator++()
        {
            ++index_;
            return *this;
        }
        iterator operator++(int)
        {
            iterator tmp = *this;
            ++index_;
            return tmp;
        }
        bool operator==(const iterator &other) const { return map_ == other.map_ && index_ == other.index_; }
        bool operator!=(const iterator &other) const { return !(*this == other); }
    };

    class const_iterator
    {
    public:
        const unordered_dense_map *map_;
        size_t index_;

        const_iterator(const unordered_dense_map *map, size_t index) : map_(map), index_(index) {}

        const Entry &operator*() const
        {
            if (index_ >= map_->entries_.size())
                throw std::out_of_range("Iterator out of bounds");
            return map_->entries_[index_];
        }
        const Entry *operator->() const
        {
            if (index_ >= map_->entries_.size())
                throw std::out_of_range("Iterator out of bounds");
            return &map_->entries_[index_];
        }

        const_iterator &operator++()
        {
            ++index_;
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator tmp = *this;
            ++index_;
            return tmp;
        }
        bool operator==(const const_iterator &other) const { return map_ == other.map_ && index_ == other.index_; }
        bool operator!=(const const_iterator &other) const { return !(*this == other); }
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size_); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size_); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, size_); }

    std::pair<iterator, bool> insert(const value_type &value) { return emplace(value.key, value.value); }
    std::pair<iterator, bool> insert(value_type &&value) { return emplace(std::move(value.key), std::move(value.value)); }

    std::pair<iterator, bool> insert(const std::pair<Key, Value> &p) { return emplace(p.first, p.second); }
    std::pair<iterator, bool> insert(std::pair<Key, Value> &&p) { return emplace(std::move(p.first), std::move(p.second)); }

    std::pair<iterator, bool> insert(const Key &key, const Value &value) { return emplace(key, value); }

    std::pair<iterator, bool> emplace(const Key &key, const Value &value)
    {
        return try_emplace(key, value);
    }

    std::pair<iterator, bool> emplace(Key &&key, Value &&value)
    {
        return try_emplace(std::move(key), std::move(value));
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const Key &key, Args &&...args);

    size_type erase(const Key &key);
    void clear()
    {
        buckets_.clear();
        entries_.clear();
        buckets_.resize(capacity_);
        size_ = 0;
    }

    iterator find(const Key &key);
    const_iterator find(const Key &key) const;
    size_type count(const Key &key) const { return find(key) != end() ? 1 : 0; }
    bool contains(const Key &key) const { return find(key) != end(); }

    template <typename InputIt>
    void batch_insert(InputIt first, InputIt last);

    template <typename InputIt, typename OutputIt>
    void batch_find(InputIt keys_first, InputIt keys_last, OutputIt results_first);

    template <typename InputIt>
    std::vector<bool> batch_contains(InputIt first, InputIt last);

private:
    void rehash(size_t new_capacity);
};

#include "unordered_dense_map_impl.hpp"