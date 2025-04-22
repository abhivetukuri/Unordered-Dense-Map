#pragma once

#include "unordered_dense_map.hpp"

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

        // Robin-hood: if current element has traveled less distance, swap them
        if (bucket.is_occupied() && bucket.distance < distance)
        {
            // Swap the key-value data in place in the entries array
            size_t entry_index = bucket.entry_index;
            std::swap(kcopy, entries_[entry_index].key);
            std::swap(vcopy, entries_[entry_index].value);
            
            // Swap metadata (with proper type conversions)
            uint8_t tmp_fp = fingerprint;
            fingerprint = static_cast<uint8_t>(bucket.fingerprint);
            bucket.fingerprint = tmp_fp;
            
            uint8_t tmp_dist = static_cast<uint8_t>(distance);
            distance = static_cast<size_t>(bucket.distance);
            bucket.distance = tmp_dist;
        }

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

// Batch operations implementation
template <typename Key, typename Value, typename Hash>
template<typename InputIt>
void unordered_dense_map<Key, Value, Hash>::batch_insert(InputIt first, InputIt last)
{
    size_t count = std::distance(first, last);
    
    // Reserve space to minimize reallocations
    if (size_ + count >= capacity_ * MAX_LOAD_FACTOR)
    {
        size_t new_capacity = capacity_;
        while (size_ + count >= new_capacity * MAX_LOAD_FACTOR)
        {
            new_capacity *= 2;
        }
        rehash(new_capacity);
    }
    
    // For small batches or non-integer keys, use regular insertion
    if constexpr (!std::is_same_v<Key, int> || count < 16)
    {
        for (auto it = first; it != last; ++it)
        {
            if constexpr (std::is_same_v<typename std::iterator_traits<InputIt>::value_type, std::pair<Key, Value>>)
            {
                insert(*it);
            }
            else
            {
                emplace(*it, Value{});
            }
        }
        return;
    }
    
    // SIMD-optimized path for integer keys
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    if constexpr (std::is_same_v<Key, int>)
    {
        std::vector<int> keys;
        std::vector<Value> values;
        keys.reserve(count);
        values.reserve(count);
        
        for (auto it = first; it != last; ++it)
        {
            if constexpr (std::is_same_v<typename std::iterator_traits<InputIt>::value_type, std::pair<Key, Value>>)
            {
                keys.push_back(it->first);
                values.push_back(it->second);
            }
            else
            {
                keys.push_back(*it);
                values.emplace_back();
            }
        }
        
        // Use vectorized hashing (implementation in .cpp file)
        for (size_t i = 0; i < count; ++i)
        {
            emplace(std::move(keys[i]), std::move(values[i]));
        }
    }
#endif
}

template <typename Key, typename Value, typename Hash>
template<typename InputIt, typename OutputIt>
void unordered_dense_map<Key, Value, Hash>::batch_find(InputIt keys_first, InputIt keys_last, OutputIt results_first)
{
    // For now, implement using regular find
    // TODO: Add SIMD optimization for probe sequence
    for (auto it = keys_first; it != keys_last; ++it, ++results_first)
    {
        *results_first = find(*it);
    }
}

template <typename Key, typename Value, typename Hash>
template<typename InputIt>
std::vector<bool> unordered_dense_map<Key, Value, Hash>::batch_contains(InputIt first, InputIt last)
{
    size_t count = std::distance(first, last);
    std::vector<bool> results;
    results.reserve(count);
    
    for (auto it = first; it != last; ++it)
    {
        results.push_back(contains(*it));
    }
    
    return results;
}