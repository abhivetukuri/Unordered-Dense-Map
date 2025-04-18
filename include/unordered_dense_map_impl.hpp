#pragma once

#include "unordered_dense_map.hpp"
#include <iostream>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <immintrin.h>
#define UNORDERED_DENSE_MAP_SIMD 1
#else
#define UNORDERED_DENSE_MAP_SIMD 0
#endif

namespace detail
{

// SIMD-optimized hash mixing for poor-quality hashes
#if UNORDERED_DENSE_MAP_SIMD
    inline uint64_t mix_hash(uint64_t hash)
    {
        __m256i h = _mm256_set1_epi64x(hash);
        __m256i mix = _mm256_set_epi64x(0x9e3779b97f4a7c15, 0xbf58476d1ce4e5b9, 0x94d049bb133111eb, 0x5ac635d8aa3a93e7);
        __m256i result = _mm256_xor_si256(h, mix);
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 13));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 7));
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 17));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 5));
        uint64_t mixed[4];
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(mixed), result);
        return mixed[0] ^ mixed[1] ^ mixed[2] ^ mixed[3];
    }
#else
    inline uint64_t mix_hash(uint64_t hash)
    {
        hash ^= (hash >> 33);
        hash *= 0xff51afd7ed558ccdULL;
        hash ^= (hash >> 33);
        hash *= 0xc4ceb9fe1a85ec53ULL;
        hash ^= (hash >> 33);
        return hash;
    }
#endif

} // namespace detail

// Template method implementations
template <typename Key, typename Value, typename Hash>
template <typename... Args>
std::pair<typename unordered_dense_map<Key, Value, Hash>::iterator, bool>
unordered_dense_map<Key, Value, Hash>::try_emplace(const Key &key, Args &&...args)
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

    // Create copies for potential swapping
    Key kcopy = key;
    Value vcopy = Value(std::forward<Args>(args)...);

    // Find insertion position using robin-hood hashing
    while (distance < MAX_DISTANCE)
    {
        detail::Bucket &bucket = buckets_[current_pos];

        if (bucket.is_empty() || bucket.is_tombstone())
        {
            // Found empty slot or tombstone
            size_t entry_idx = entries_.size();
            bucket.set_occupied(fingerprint, distance, entry_idx);
            entries_.emplace_back(std::move(kcopy), std::move(vcopy));
            ++size_;


            return {iterator(this, entry_idx), true};
        }

        if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
        {
            // Check if it's the same key
            size_t entry_index = bucket.entry_index;
            if (entries_[entry_index].key == kcopy)
            {
                return {iterator(this, entry_index), false};
            }
        }

        // Robin-hood hashing disabled for now to avoid complex entry index management
        // TODO: Implement proper robin-hood with entry index tracking

        current_pos = (current_pos + 1) % capacity_;
        ++distance;
    }

    // If we get here, we need to rehash
    rehash(capacity_ * 2);
    return try_emplace(kcopy, std::move(vcopy));
}

template <typename Key, typename Value, typename Hash>
typename unordered_dense_map<Key, Value, Hash>::size_type
unordered_dense_map<Key, Value, Hash>::erase(const Key &key)
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
        
        if (bucket.is_tombstone())
        {
            // Skip tombstones and continue probing
            current_pos = (current_pos + 1) % capacity_;
            ++distance;
            continue;
        }

        if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
        {
            size_t entry_index = bucket.entry_index;
            if (entries_[entry_index].key == key)
            {
                // Found the key to delete

                // Move the last entry to this position to maintain dense packing
                if (entry_index != size_ - 1)
                {
                    Key moved_key = entries_[size_ - 1].key; // Save key before move
                    
                    // Move the last entry to fill the gap
                    entries_[entry_index] = std::move(entries_[size_ - 1]);

                    // Find the bucket that points to the last entry and update its index
                    for (size_t i = 0; i < capacity_; ++i)
                    {
                        if (buckets_[i].is_occupied() && buckets_[i].entry_index == size_ - 1)
                        {
                            buckets_[i].entry_index = entry_index;
                            break;
                        }
                    }
                }

                // Use tombstone instead of backward-shift for now
                bucket.set_tombstone();
                
                // Remove the last entry (which is now either the deleted entry or empty after move)
                entries_.pop_back();
                --size_;

                return 1;
            }
        }

        current_pos = (current_pos + 1) % capacity_;
        ++distance;
    }

    return 0; // Key not found
}

template <typename Key, typename Value, typename Hash>
typename unordered_dense_map<Key, Value, Hash>::iterator
unordered_dense_map<Key, Value, Hash>::find(const Key &key)
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
        
        if (bucket.is_tombstone())
        {
            // Skip tombstones and continue probing
            current_pos = (current_pos + 1) % capacity_;
            ++distance;
            continue;
        }

        if (bucket.is_occupied() && bucket.fingerprint == fingerprint)
        {
            size_t entry_index = bucket.entry_index;


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

template <typename Key, typename Value, typename Hash>
typename unordered_dense_map<Key, Value, Hash>::const_iterator
unordered_dense_map<Key, Value, Hash>::find(const Key &key) const
{
    auto it = const_cast<unordered_dense_map *>(this)->find(key);
    return const_iterator(this, it.index_);
}

template <typename Key, typename Value, typename Hash>
void unordered_dense_map<Key, Value, Hash>::rehash(size_t new_capacity)
{
    std::vector<Entry> old_entries = std::move(entries_);
    size_t old_size = size_;

    capacity_ = new_capacity;
    buckets_.clear();
    buckets_.resize(capacity_);
    entries_.clear();
    size_ = 0;

    // Reinsert all entries
    for (size_t i = 0; i < old_size; ++i)
    {
        emplace(std::move(old_entries[i].key), std::move(old_entries[i].value));
    }
}