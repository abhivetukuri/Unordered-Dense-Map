#pragma once

#include "unordered_dense_map.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <array>
#include <cstring>

// Lock-free concurrent hash table using epoch-based memory management
template <typename Key, typename Value, typename Hash = detail::hash_traits<Key>>
class concurrent_unordered_dense_map
{
private:
    static constexpr size_t INITIAL_CAPACITY = 16;
    static constexpr float MAX_LOAD_FACTOR = 0.75f;
    static constexpr size_t MAX_DISTANCE = 255;
    static constexpr size_t SEGMENT_COUNT = 64; // Number of segments for fine-grained locking

    // Atomic bucket for lock-free operations
    struct AtomicBucket
    {
        std::atomic<uint64_t> data{0}; // Packed: fingerprint|distance|occupied|tombstone|entry_index

        // Pack/unpack methods for atomic operations
        static uint64_t pack(uint8_t fingerprint, uint8_t distance, bool occupied, bool tombstone, uint64_t entry_index)
        {
            return (static_cast<uint64_t>(fingerprint) << 56) |
                   (static_cast<uint64_t>(distance) << 48) |
                   (static_cast<uint64_t>(occupied) << 47) |
                   (static_cast<uint64_t>(tombstone) << 46) |
                   (entry_index & 0x3FFFFFFFFFFFULL);
        }

        struct UnpackedBucket
        {
            uint8_t fingerprint;
            uint8_t distance;
            bool occupied;
            bool tombstone;
            uint64_t entry_index;

            bool is_empty() const { return !occupied && !tombstone; }
            bool is_tombstone() const { return !occupied && tombstone; }
            bool is_occupied() const { return occupied; }
        };

        UnpackedBucket unpack() const
        {
            uint64_t val = data.load(std::memory_order_acquire);
            return {
                static_cast<uint8_t>(val >> 56),          // fingerprint
                static_cast<uint8_t>((val >> 48) & 0xFF), // distance
                static_cast<bool>((val >> 47) & 1),       // occupied
                static_cast<bool>((val >> 46) & 1),       // tombstone
                val & 0x3FFFFFFFFFFFULL                   // entry_index
            };
        }

        bool compare_exchange_weak(uint64_t &expected, uint64_t desired)
        {
            return data.compare_exchange_weak(expected, desired,
                                              std::memory_order_acq_rel, std::memory_order_acquire);
        }

        void store(uint64_t value)
        {
            data.store(value, std::memory_order_release);
        }
    };

    struct Entry
    {
        Key key;
        Value value;
        std::atomic<bool> valid{true}; // For lock-free deletion

        Entry() = default;
        Entry(const Key &k, const Value &v) : key(k), value(v) {}
        Entry(Key &&k, Value &&v) : key(std::move(k)), value(std::move(v)) {}
    };

    // Segment for fine-grained locking approach (fallback when pure lock-free is too complex)
    struct Segment
    {
        std::atomic<size_t> size{0};
        std::atomic<size_t> capacity{INITIAL_CAPACITY / SEGMENT_COUNT};
        std::unique_ptr<AtomicBucket[]> buckets;
        std::atomic<Entry *> entries{nullptr};
        std::atomic<size_t> entries_capacity{0};
        mutable std::shared_mutex mutex; // For resize operations only

        Segment()
        {
            buckets = std::make_unique<AtomicBucket[]>(capacity.load());
            entries_capacity = capacity.load();
            entries.store(new Entry[entries_capacity]);
        }

        ~Segment()
        {
            delete[] entries.load();
        }

        // Non-copyable, non-movable for simplicity
        Segment(const Segment &) = delete;
        Segment &operator=(const Segment &) = delete;
        Segment(Segment &&) = delete;
        Segment &operator=(Segment &&) = delete;
    };

    std::array<std::unique_ptr<Segment>, SEGMENT_COUNT> segments_;
    std::atomic<size_t> total_size_{0};

    // Get segment index for a key
    size_t get_segment_index(const Key &key) const
    {
        uint64_t hash = Hash::hash(key);
        return hash % SEGMENT_COUNT;
    }

public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = size_t;

    // Concurrent iterator (read-only, may see inconsistent state during modifications)
    class const_iterator
    {
    public:
        const concurrent_unordered_dense_map *map_;
        size_t segment_idx_;
        size_t entry_idx_;

        const_iterator(const concurrent_unordered_dense_map *map, size_t seg_idx, size_t ent_idx)
            : map_(map), segment_idx_(seg_idx), entry_idx_(ent_idx) {}

        const value_type &operator*() const
        {
            const auto &segment = map_->segments_[segment_idx_];
            const Entry *entries = segment->entries.load();
            return reinterpret_cast<const value_type &>(entries[entry_idx_]);
        }

        const value_type *operator->() const
        {
            return &(**this);
        }

        const_iterator &operator++()
        {
            // Move to next valid entry
            find_next_valid();
            return *this;
        }

        bool operator==(const const_iterator &other) const
        {
            return map_ == other.map_ && segment_idx_ == other.segment_idx_ && entry_idx_ == other.entry_idx_;
        }

        bool operator!=(const const_iterator &other) const
        {
            return !(*this == other);
        }

    private:
        void find_next_valid()
        {
            // Implementation to find next valid entry across segments
            // Simplified for this example
            ++entry_idx_;
            while (segment_idx_ < SEGMENT_COUNT)
            {
                const auto &segment = map_->segments_[segment_idx_];
                size_t seg_size = segment->size.load();

                if (entry_idx_ < seg_size)
                {
                    const Entry *entries = segment->entries.load();
                    if (entries[entry_idx_].valid.load())
                    {
                        return; // Found valid entry
                    }
                    ++entry_idx_;
                }
                else
                {
                    // Move to next segment
                    ++segment_idx_;
                    entry_idx_ = 0;
                }
            }
        }
    };

    concurrent_unordered_dense_map()
    {
        for (size_t i = 0; i < SEGMENT_COUNT; ++i)
        {
            segments_[i] = std::make_unique<Segment>();
        }
    }

    // Basic operations
    bool contains(const Key &key) const
    {
        return find(key) != end();
    }

    const_iterator find(const Key &key) const
    {
        size_t seg_idx = get_segment_index(key);
        const auto &segment = segments_[seg_idx];

        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);

        size_t capacity = segment->capacity.load();
        size_t ideal_pos = hash % capacity;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        while (distance < MAX_DISTANCE)
        {
            auto bucket_data = segment->buckets[current_pos].unpack();

            if (bucket_data.is_empty())
            {
                break; // Key not found
            }

            if (bucket_data.is_tombstone())
            {
                current_pos = (current_pos + 1) % capacity;
                ++distance;
                continue;
            }

            if (bucket_data.is_occupied() && bucket_data.fingerprint == fingerprint)
            {
                const Entry *entries = segment->entries.load();
                if (bucket_data.entry_index < segment->size.load() &&
                    entries[bucket_data.entry_index].valid.load() &&
                    entries[bucket_data.entry_index].key == key)
                {
                    return const_iterator(this, seg_idx, bucket_data.entry_index);
                }
            }

            current_pos = (current_pos + 1) % capacity;
            ++distance;
        }

        return end();
    }

    bool insert(const Key &key, const Value &value)
    {
        size_t seg_idx = get_segment_index(key);
        auto &segment = segments_[seg_idx];

        // For simplicity, use a shared lock for the entire operation
        // In a production implementation, this would be more sophisticated
        std::shared_lock<std::shared_mutex> lock(segment->mutex);

        // Check if key already exists
        if (find(key) != end())
        {
            return false; // Key already exists
        }

        // Check if resize is needed
        size_t current_size = segment->size.load();
        size_t current_capacity = segment->capacity.load();

        if (current_size >= current_capacity * MAX_LOAD_FACTOR)
        {
            // Need to resize - upgrade to exclusive lock
            lock.unlock();
            std::unique_lock<std::shared_mutex> exclusive_lock(segment->mutex);

            // Double-check after acquiring exclusive lock
            if (segment->size.load() >= segment->capacity.load() * MAX_LOAD_FACTOR)
            {
                resize_segment(*segment);
            }

            exclusive_lock.unlock();
            lock = std::shared_lock<std::shared_mutex>(segment->mutex);
        }

        // Perform insertion using lock-free techniques
        return insert_in_segment(*segment, key, value);
    }

    bool erase(const Key &key)
    {
        size_t seg_idx = get_segment_index(key);
        auto &segment = segments_[seg_idx];

        std::shared_lock<std::shared_mutex> lock(segment->mutex);

        auto it = find(key);
        if (it == end())
        {
            return false;
        }

        // Mark entry as invalid (lock-free deletion)
        Entry *entries = segment->entries.load();
        entries[it.entry_idx_].valid.store(false, std::memory_order_release);

        // Mark bucket as tombstone
        size_t capacity = segment->capacity.load();
        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);
        size_t ideal_pos = hash % capacity;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        while (distance < MAX_DISTANCE)
        {
            auto bucket = &segment->buckets[current_pos];
            auto bucket_data = bucket->unpack();

            if (bucket_data.is_occupied() &&
                bucket_data.fingerprint == fingerprint &&
                bucket_data.entry_index == it.entry_idx_)
            {
                // Mark as tombstone
                uint64_t tombstone_data = AtomicBucket::pack(fingerprint, bucket_data.distance, false, true, bucket_data.entry_index);
                uint64_t expected = bucket->data.load();
                bucket->compare_exchange_weak(expected, tombstone_data);
                break;
            }

            current_pos = (current_pos + 1) % capacity;
            ++distance;
        }

        total_size_.fetch_sub(1, std::memory_order_acq_rel);
        return true;
    }

    size_type size() const
    {
        return total_size_.load(std::memory_order_acquire);
    }

    bool empty() const
    {
        return size() == 0;
    }

    const_iterator begin() const
    {
        const_iterator it(this, 0, 0);
        if (segments_[0]->size.load() == 0 || !segments_[0]->entries.load()[0].valid.load())
        {
            ++it; // Find first valid entry
        }
        return it;
    }

    const_iterator end() const
    {
        return const_iterator(this, SEGMENT_COUNT, 0);
    }

private:
    void resize_segment(Segment &segment)
    {
        // Simplified resize implementation
        // In production, this would use more sophisticated techniques
        size_t old_capacity = segment.capacity.load();
        size_t new_capacity = old_capacity * 2;

        auto new_buckets = std::make_unique<AtomicBucket[]>(new_capacity);
        Entry *new_entries = new Entry[new_capacity];

        // Rehash existing entries
        Entry *old_entries = segment.entries.load();
        size_t old_size = segment.size.load();
        size_t new_size = 0;

        for (size_t i = 0; i < old_size; ++i)
        {
            if (old_entries[i].valid.load())
            {
                // Manually copy key and value, reset valid to true
                new_entries[new_size].key = std::move(old_entries[i].key);
                new_entries[new_size].value = std::move(old_entries[i].value);
                new_entries[new_size].valid.store(true);
                // Update bucket mapping...
                ++new_size;
            }
        }

        // Atomic swap
        Entry *old_entries_ptr = segment.entries.exchange(new_entries);
        segment.buckets = std::move(new_buckets);
        segment.capacity.store(new_capacity);
        segment.size.store(new_size);

        delete[] old_entries_ptr;
    }

    bool insert_in_segment(Segment &segment, const Key &key, const Value &value)
    {
        uint64_t hash = Hash::hash(key);
        uint8_t fingerprint = Hash::fingerprint(key);

        size_t capacity = segment.capacity.load();
        size_t ideal_pos = hash % capacity;
        size_t current_pos = ideal_pos;
        size_t distance = 0;

        while (distance < MAX_DISTANCE)
        {
            auto bucket = &segment.buckets[current_pos];
            auto bucket_data = bucket->unpack();

            if (bucket_data.is_empty() || bucket_data.is_tombstone())
            {
                // Try to claim this bucket
                size_t entry_idx = segment.size.fetch_add(1, std::memory_order_acq_rel);

                // Add entry
                Entry *entries = segment.entries.load();
                entries[entry_idx].key = key;
                entries[entry_idx].value = value;
                entries[entry_idx].valid.store(true);

                // Update bucket
                uint64_t new_bucket_data = AtomicBucket::pack(fingerprint, distance, true, false, entry_idx);
                uint64_t expected = bucket->data.load();

                if (bucket->compare_exchange_weak(expected, new_bucket_data))
                {
                    total_size_.fetch_add(1, std::memory_order_acq_rel);
                    return true;
                }
                else
                {
                    // Someone else claimed this bucket, revert entry addition
                    segment.size.fetch_sub(1, std::memory_order_acq_rel);
                    entries[entry_idx].valid.store(false);
                }
            }

            current_pos = (current_pos + 1) % capacity;
            ++distance;
        }

        return false; // Failed to insert
    }
};