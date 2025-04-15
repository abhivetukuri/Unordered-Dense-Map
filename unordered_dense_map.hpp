#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <immintrin.h> // For SIMD intrinsics

namespace detail
{

    // WyHash implementation for fast hashing
    class WyHash
    {
    private:
        static constexpr uint64_t wyhash64_x = 0x60ea27eeadc3b5efULL;
        static constexpr uint64_t wyhash64_a = 0x3b3897599180e0c5ULL;
        static constexpr uint64_t wyhash64_b = 0x1b8735937b4aac63ULL;
        static constexpr uint64_t wyhash64_c = 0x96be6a03f93d9cd7ULL;
        static constexpr uint64_t wyhash64_d = 0xebd33483acc5ea64ULL;

    public:
        static uint64_t hash(const void *key, size_t len, uint64_t seed = 0)
        {
            const uint8_t *p = static_cast<const uint8_t *>(key);
            uint64_t a, b;
            size_t i;

            if (len <= 16)
            {
                if (len >= 4)
                {
                    a = (static_cast<uint64_t>(p[0]) << 32) | (static_cast<uint64_t>(p[len >> 1]) << 16) | (static_cast<uint64_t>(p[len - 2]) << 8) | p[len - 1];
                    b = (static_cast<uint64_t>(p[len - 4]) << 32) | (static_cast<uint64_t>(p[len - 3]) << 16) | (static_cast<uint64_t>(p[len - 2]) << 8) | p[len - 1];
                }
                else if (len > 0)
                {
                    a = p[0];
                    b = p[len - 1];
                }
                else
                {
                    a = b = 0;
                }
            }
            else
            {
                size_t i = len;
                if (i > 48)
                {
                    uint64_t see1 = seed, see2 = seed;
                    do
                    {
                        seed = wyhash64_mum(wyhash64_read(p, 8) ^ wyhash64_a, wyhash64_read(p + 8, 8) ^ seed);
                        see1 = wyhash64_mum(wyhash64_read(p + 16, 8) ^ wyhash64_b, wyhash64_read(p + 24, 8) ^ see1);
                        see2 = wyhash64_mum(wyhash64_read(p + 32, 8) ^ wyhash64_c, wyhash64_read(p + 40, 8) ^ see2);
                        p += 48;
                        i -= 48;
                    } while (i > 48);
                    seed ^= see1 ^ see2;
                }
                while (i > 16)
                {
                    seed = wyhash64_mum(wyhash64_read(p, 8) ^ wyhash64_a, wyhash64_read(p + 8, 8) ^ seed);
                    i -= 16;
                    p += 16;
                }
                a = wyhash64_read(p + i - 16, 8);
                b = wyhash64_read(p + i - 8, 8);
            }

            a ^= wyhash64_a;
            b ^= seed;
            a *= wyhash64_b;
            b *= wyhash64_c;
            a = wyhash64_mum(a, b);
            seed ^= a ^ b;
            return wyhash64_mum(seed, len ^ wyhash64_d);
        }

    private:
        static uint64_t wyhash64_mum(uint64_t A, uint64_t B)
        {
            uint64_t r = A * B;
            return r - (r >> 32);
        }

        static uint64_t wyhash64_read(const uint8_t *p, size_t k)
        {
            uint64_t r = 0;
            for (size_t i = 0; i < k; ++i)
            {
                r |= static_cast<uint64_t>(p[i]) << (i * 8);
            }
            return r;
        }
    };

    // SIMD-optimized hash mixing for poor-quality hashes
    inline uint64_t mix_hash(uint64_t hash)
    {
        // Use SIMD instructions for faster mixing
        __m256i h = _mm256_set1_epi64x(hash);
        __m256i mix = _mm256_set_epi64x(0x9e3779b97f4a7c15, 0xbf58476d1ce4e5b9, 0x94d049bb133111eb, 0x5ac635d8aa3a93e7);
        __m256i result = _mm256_xor_si256(h, mix);
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 13));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 7));
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 17));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 5));

        uint64_t mixed[4];
        _mm256_store_si256(reinterpret_cast<__m256i *>(mixed), result);
        return mixed[0] ^ mixed[1] ^ mixed[2] ^ mixed[3];
    }

    // 8-byte bucket structure with fingerprinting
    struct Bucket
    {
        uint64_t fingerprint : 8; // 8-bit fingerprint for quick comparison
        uint64_t distance : 8;    // Distance from ideal position (for robin-hood)
        uint64_t occupied : 1;    // Whether bucket is occupied
        uint64_t tombstone : 1;   // Whether bucket is a tombstone
        uint64_t reserved : 46;   // Reserved for future use

        Bucket() : fingerprint(0), distance(0), occupied(0), tombstone(0), reserved(0) {}

        bool is_empty() const { return !occupied && !tombstone; }
        bool is_tombstone() const { return !occupied && tombstone; }
        bool is_occupied() const { return occupied; }

        void clear()
        {
            fingerprint = 0;
            distance = 0;
            occupied = 0;
            tombstone = 0;
        }

        void set_occupied(uint8_t fp, uint8_t dist)
        {
            fingerprint = fp;
            distance = dist;
            occupied = 1;
            tombstone = 0;
        }

        void set_tombstone()
        {
            occupied = 0;
            tombstone = 1;
        }
    };

    // Hash traits for different key types
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

    // Specialization for string types
    template <>
    struct hash_traits<std::string>
    {
        static uint64_t hash(const std::string &key)
        {
            return WyHash::hash(key.data(), key.size());
        }

        static uint8_t fingerprint(const std::string &key)
        {
            uint64_t h = hash(key);
            return static_cast<uint8_t>(h & 0xFF);
        }
    };

} // namespace detail

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
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;

    // Constructors
    unordered_dense_map() : size_(0), capacity_(INITIAL_CAPACITY)
    {
        buckets_.resize(capacity_);
    }

    unordered_dense_map(const unordered_dense_map &other) = default;
    unordered_dense_map(unordered_dense_map &&other) = default;

    // Assignment operators
    unordered_dense_map &operator=(const unordered_dense_map &other) = default;
    unordered_dense_map &operator=(unordered_dense_map &&other) = default;

    // Capacity
    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type max_size() const { return std::numeric_limits<size_type>::max(); }

    // Element access
    Value &operator[](const Key &key)
    {
        auto [it, inserted] = try_emplace(key, Value{});
        return it->second;
    }

    Value &at(const Key &key)
    {
        auto it = find(key);
        if (it == end())
        {
            throw std::out_of_range("Key not found");
        }
        return it->second;
    }

    const Value &at(const Key &key) const
    {
        auto it = find(key);
        if (it == end())
        {
            throw std::out_of_range("Key not found");
        }
        return it->second;
    }

    // Iterators
    class iterator
    {
    private:
        unordered_dense_map *map_;
        size_t index_;

    public:
        iterator(unordered_dense_map *map, size_t index) : map_(map), index_(index) {}

        value_type &operator*()
        {
            return reinterpret_cast<value_type &>(map_->entries_[index_]);
        }

        value_type *operator->()
        {
            return reinterpret_cast<value_type *>(&map_->entries_[index_]);
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

        bool operator==(const iterator &other) const
        {
            return map_ == other.map_ && index_ == other.index_;
        }

        bool operator!=(const iterator &other) const
        {
            return !(*this == other);
        }
    };

    class const_iterator
    {
    private:
        const unordered_dense_map *map_;
        size_t index_;

    public:
        const_iterator(const unordered_dense_map *map, size_t index) : map_(map), index_(index) {}

        const value_type &operator*() const
        {
            return reinterpret_cast<const value_type &>(map_->entries_[index_]);
        }

        const value_type *operator->() const
        {
            return reinterpret_cast<const value_type *>(&map_->entries_[index_]);
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

        bool operator==(const const_iterator &other) const
        {
            return map_ == other.map_ && index_ == other.index_;
        }

        bool operator!=(const const_iterator &other) const
        {
            return !(*this == other);
        }
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size_); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size_); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, size_); }

    // Modifiers
    std::pair<iterator, bool> insert(const value_type &value)
    {
        return emplace(value.first, value.second);
    }

    std::pair<iterator, bool> insert(value_type &&value)
    {
        return emplace(std::move(value.first), std::move(value.second));
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args &&...args)
    {
        Entry entry(std::forward<Args>(args)...);
        return try_emplace(std::move(entry.key), std::move(entry.value));
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const Key &key, Args &&...args)
    {
        if (size_ >= capacity_ * MAX_LOAD_FACTOR)
        {
            rehash(capacity_ * 2);
        }

        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);

        // Mix poor-quality hashes
        if (fingerprint == 0)
        {
            hash = detail::mix_hash(hash);
            fingerprint = Hash::fingerprint(key);
        }

        size_t ideal_pos = hash % capacity_;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        // Find insertion position using robin-hood hashing
        while (distance < MAX_DISTANCE)
        {
            detail::Bucket &bucket = buckets_[current_pos];

            if (bucket.is_empty())
            {
                // Found empty slot
                bucket.set_occupied(fingerprint, distance);
                entries_.emplace_back(key, Value(std::forward<Args>(args)...));
                ++size_;
                return {iterator(this, size_ - 1), true};
            }

            if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
            {
                // Check if it's the same key
                size_t entry_index = current_pos - bucket.distance;
                if (entry_index >= entries_.size())
                {
                    entry_index += capacity_;
                }
                if (entries_[entry_index].key == key)
                {
                    return {iterator(this, entry_index), false};
                }
            }

            // Robin-hood: swap if current element has traveled less
            if (bucket.is_occupied() && bucket.distance < distance)
            {
                // Swap current element with the one we're trying to insert
                std::swap(bucket.fingerprint, fingerprint);
                std::swap(bucket.distance, distance);

                Entry temp_entry(std::move(entries_[current_pos - bucket.distance]));
                entries_[current_pos - bucket.distance] = Entry(key, Value(std::forward<Args>(args)...));
                key = std::move(temp_entry.key);
                // Continue with the swapped element
            }

            current_pos = (current_pos + 1) % capacity_;
            ++distance;
        }

        // If we get here, we need to rehash
        rehash(capacity_ * 2);
        return try_emplace(key, std::forward<Args>(args)...);
    }

    size_type erase(const Key &key)
    {
        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);

        if (fingerprint == 0)
        {
            hash = detail::mix_hash(hash);
            fingerprint = Hash::fingerprint(key);
        }

        size_t ideal_pos = hash % capacity_;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        while (distance < MAX_DISTANCE)
        {
            detail::Bucket &bucket = buckets_[current_pos];

            if (bucket.is_empty())
            {
                return 0; // Key not found
            }

            if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
            {
                size_t entry_index = current_pos - bucket.distance;
                if (entry_index >= entries_.size())
                {
                    entry_index += capacity_;
                }
                if (entries_[entry_index].key == key)
                {
                    // Found the key, perform backward-shift deletion
                    bucket.set_tombstone();

                    // Shift elements backward
                    size_t next_pos = (current_pos + 1) % capacity_;
                    while (!buckets_[next_pos].is_empty() && buckets_[next_pos].distance > 0)
                    {
                        detail::Bucket &next_bucket = buckets_[next_pos];
                        size_t next_entry_index = next_pos - next_bucket.distance;
                        if (next_entry_index >= entries_.size())
                        {
                            next_entry_index += capacity_;
                        }

                        // Move entry to previous position
                        size_t prev_entry_index = next_pos - 1 - (next_bucket.distance - 1);
                        if (prev_entry_index >= entries_.size())
                        {
                            prev_entry_index += capacity_;
                        }
                        entries_[prev_entry_index] = std::move(entries_[next_entry_index]);

                        // Update bucket
                        buckets_[next_pos - 1] = next_bucket;
                        --next_bucket.distance;
                        next_bucket.clear();

                        next_pos = (next_pos + 1) % capacity_;
                    }

                    // Remove the last entry
                    entries_.erase(entries_.begin() + entry_index);
                    --size_;
                    return 1;
                }
            }

            current_pos = (current_pos + 1) % capacity_;
            ++distance;
        }

        return 0; // Key not found
    }

    void clear()
    {
        buckets_.clear();
        entries_.clear();
        buckets_.resize(capacity_);
        size_ = 0;
    }

    // Lookup
    iterator find(const Key &key)
    {
        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);

        if (fingerprint == 0)
        {
            hash = detail::mix_hash(hash);
            fingerprint = Hash::fingerprint(key);
        }

        size_t ideal_pos = hash % capacity_;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        while (distance < MAX_DISTANCE)
        {
            detail::Bucket &bucket = buckets_[current_pos];

            if (bucket.is_empty())
            {
                return end();
            }

            if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
            {
                size_t entry_index = current_pos - bucket.distance;
                if (entry_index >= entries_.size())
                {
                    entry_index += capacity_;
                }
                if (entries_[entry_index].key == key)
                {
                    return iterator(this, entry_index);
                }
            }

            current_pos = (current_pos + 1) % capacity_;
            ++distance;
        }

        return end();
    }

    const_iterator find(const Key &key) const
    {
        return const_cast<unordered_dense_map *>(this)->find(key);
    }

    size_type count(const Key &key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    bool contains(const Key &key) const
    {
        return find(key) != end();
    }

private:
    void rehash(size_t new_capacity)
    {
        std::vector<detail::Bucket> old_buckets = std::move(buckets_);
        std::vector<Entry> old_entries = std::move(entries_);
        size_t old_size = size_;

        capacity_ = new_capacity;
        buckets_.resize(capacity_);
        entries_.clear();
        size_ = 0;

        // Reinsert all entries
        for (size_t i = 0; i < old_size; ++i)
        {
            try_emplace(std::move(old_entries[i].key), std::move(old_entries[i].value));
        }
    }
};

} // namespace detail