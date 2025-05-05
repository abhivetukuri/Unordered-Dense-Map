#include "../include/unordered_dense_map.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <unordered_map>
#include <iomanip>
#include <string>
#include <iomanip>
#include <cassert>

using namespace std::chrono;

template <typename MapType>
double benchmark_insertion(const std::vector<std::pair<int, int>> &data)
{
    MapType map;
    auto start = high_resolution_clock::now();

    for (const auto &pair : data)
    {
        map[pair.first] = pair.second;
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    return duration.count() / 1000000.0;
}

template <typename MapType>
double benchmark_lookup(const MapType &map, const std::vector<int> &keys)
{
    auto start = std::chrono::high_resolution_clock::now();

    const int num_passes = 10;
    for (int pass = 0; pass < num_passes; ++pass)
    {
        for (int key : keys)
        {
            auto it = map.find(key);
            if (it != map.end())
            {
                volatile int dummy = it->value;
                (void)dummy;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count() / num_passes;
}

void test_basic_functionality()
{
    std::cout << "=== Testing Basic Functionality ===" << std::endl;

    unordered_dense_map<int, std::string> map;

    map[1] = "one";
    map[2] = "two";
    map[3] = "three";

    assert(map.size() == 3);
    assert(map[1] == "one");
    assert(map[2] == "two");
    assert(map[3] == "three");

    auto it = map.find(2);
    assert(it != map.end());
    assert(it->value == "two");

    assert(map.contains(1));
    assert(!map.contains(4));

    assert(map.count(1) == 1);
    assert(map.count(4) == 0);

    int count = 0;
    for (const auto &entry : map)
    {
        count++;
        assert(entry.key >= 1 && entry.key <= 3);
    }
    assert(count == 3);

    assert(map.erase(2) == 1);
    assert(map.size() == 2);
    assert(!map.contains(2));

    map.clear();
    assert(map.empty());
    assert(map.size() == 0);

    std::cout << "✓ Basic functionality tests passed!" << std::endl;
}

void test_string_keys()
{
    std::cout << "\n=== Testing String Keys ===" << std::endl;

    unordered_dense_map<std::string, int> map;

    map["apple"] = 1;
    map["banana"] = 2;
    map["cherry"] = 3;

    assert(map.size() == 3);
    assert(map["apple"] == 1);
    assert(map["banana"] == 2);
    assert(map["cherry"] == 3);

    auto it = map.find("banana");
    assert(it != map.end());
    assert(it->value == 2);

    assert(map.erase("apple") == 1);
    assert(map.size() == 2);
    assert(!map.contains("apple"));

    std::cout << "✓ String key tests passed!" << std::endl;
}

void test_robin_hood_hashing()
{
    std::cout << "\n=== Testing Robin-Hood Hashing ===" << std::endl;

    unordered_dense_map<int, int> map;

    for (int i = 0; i < 100; ++i)
    {
        map[i] = i * 2;
    }

    assert(map.size() == 100);

    auto it12 = map.find(12);
    assert(it12 != map.end());
    assert(it12->value == 24);

    for (int i = 0; i < 100; ++i)
    {
        auto it = map.find(i);
        if (it == map.end())
        {
            std::cout << "ERROR: Key " << i << " not found!" << std::endl;
            assert(false);
        }
        if (it->value != i * 2)
        {
            std::cout << "ERROR: Key " << i << " has wrong value. Expected: " << (i * 2) << ", Got: " << it->value << std::endl;
            assert(false);
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 99);

    for (int i = 0; i < 20; ++i)
    {
        int key = dis(gen);
        auto it = map.find(key);
        assert(it != map.end());
        assert(it->value == key * 2);
    }

    std::cout << "✓ Robin-hood hashing tests passed!" << std::endl;
}

void test_backward_shift_deletion()
{
    std::cout << "\n=== Testing Backward-Shift Deletion ===" << std::endl;

    unordered_dense_map<int, int> map;

    for (int i = 0; i < 100; ++i)
    {
        map[i] = i * 2;
    }

    assert(map.size() == 100);

    for (int i = 25; i < 30; ++i)
    {
        assert(map.erase(i) == 1);
    }

    assert(map.size() == 95);

    for (int i = 0; i < 25; ++i)
    {
        auto it = map.find(i);
        assert(it != map.end());
        assert(it->value == i * 2);
    }

    for (int i = 30; i < 100; ++i)
    {
        auto it = map.find(i);
        if (it == map.end())
        {
            std::cout << "ERROR: Could not find key " << i << std::endl;
            assert(false);
        }
        if (it->value != i * 2)
        {
            std::cout << "ERROR: Key " << i << " has wrong value. Expected: " << (i * 2) << ", Got: " << it->value << std::endl;
            assert(false);
        }
    }

    for (int i = 25; i < 30; ++i)
    {
        assert(!map.contains(i));
    }

    std::cout << "✓ Backward-shift deletion tests passed!" << std::endl;
}

void performance_comparison()
{
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    const int num_elements = 1000000;

    std::vector<std::pair<int, int>> data;
    data.reserve(num_elements);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_elements * 10);

    for (int i = 0; i < num_elements; ++i)
    {
        data.emplace_back(dis(gen), i);
    }

    std::vector<int> lookup_keys;
    lookup_keys.reserve(num_elements);
    for (int i = 0; i < num_elements; ++i)
    {
        if (i % 3 == 0)
        {
            lookup_keys.push_back(data[i % data.size()].first);
        }
        else
        {
            lookup_keys.push_back(dis(gen));
        }
    }

    double dense_insert_time = benchmark_insertion<unordered_dense_map<int, int>>(data);
    double std_insert_time = benchmark_insertion<std::unordered_map<int, int>>(data);

    unordered_dense_map<int, int> dense_map;
    std::unordered_map<int, int> std_map;

    for (const auto &pair : data)
    {
        dense_map[pair.first] = pair.second;
        std_map[pair.first] = pair.second;
    }

    double dense_lookup_time = benchmark_lookup(dense_map, lookup_keys);
    double std_lookup_time = benchmark_lookup(std_map, lookup_keys);

    size_t dense_memory = sizeof(unordered_dense_map<int, int>) +
                          dense_map.size() * (sizeof(int) + sizeof(int)) +
                          dense_map.size() * sizeof(detail::Bucket);
    size_t std_memory = sizeof(std::unordered_map<int, int>) +
                        std_map.size() * (sizeof(int) + sizeof(int) + sizeof(void *));

    std::cout << "Insertion Performance (1M elements):" << std::endl;
    std::cout << "  Unordered Dense Map: " << std::fixed << std::setprecision(3) << dense_insert_time << "s" << std::endl;
    std::cout << "  std::unordered_map:  " << std::fixed << std::setprecision(3) << std_insert_time << "s" << std::endl;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << (std_insert_time / dense_insert_time) << "x" << std::endl;
    std::cout << std::endl;

    std::cout << "Lookup Performance (1M lookups):" << std::endl;
    std::cout << "  Unordered Dense Map: " << std::fixed << std::setprecision(3) << dense_lookup_time << "s" << std::endl;
    std::cout << "  std::unordered_map:  " << std::fixed << std::setprecision(3) << std_lookup_time << "s" << std::endl;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) << (std_lookup_time / dense_lookup_time) << "x" << std::endl;
    std::cout << std::endl;

    std::cout << "Memory Usage (approximate):" << std::endl;
    std::cout << "  Unordered Dense Map: " << (dense_memory / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  std::unordered_map:  " << (std_memory / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Memory ratio: " << std::fixed << std::setprecision(2) << (double)dense_memory / std_memory << "x" << std::endl;
}

void test_simd_optimizations()
{
    std::cout << "\n=== Testing SIMD Optimizations ===" << std::endl;

    unordered_dense_map<int, int> map;

    for (int i = 0; i < 1000; ++i)
    {
        int key = i * 256;
        map[key] = i;
    }

    assert(map.size() == 1000);

    for (int i = 0; i < 1000; ++i)
    {
        int key = i * 256;
        auto it = map.find(key);
        assert(it != map.end());
        assert(it->value == i);
    }

    std::cout << "✓ SIMD optimization tests passed!" << std::endl;
}

void test_edge_cases()
{
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;

    unordered_dense_map<int, int> map;

    map[0] = 42;
    assert(map[0] == 42);
    assert(map.contains(0));

    map[-1] = -42;
    map[-1000] = -2000;
    assert(map[-1] == -42);
    assert(map[-1000] == -2000);

    map[std::numeric_limits<int>::max()] = 999;
    map[std::numeric_limits<int>::min()] = -999;
    assert(map[std::numeric_limits<int>::max()] == 999);
    assert(map[std::numeric_limits<int>::min()] == -999);

    assert(map.erase(999999) == 0);

    assert(map.find(999999) == map.end());

    unordered_dense_map<int, int> empty_map;
    assert(empty_map.empty());
    assert(empty_map.size() == 0);
    assert(empty_map.find(1) == empty_map.end());
    assert(empty_map.erase(1) == 0);

    std::cout << "✓ Edge case tests passed!" << std::endl;
}

int main()
{
    std::cout << "Unordered Dense Map Test Suite" << std::endl;
    std::cout << "==============================" << std::endl;

    try
    {
        test_basic_functionality();
        test_string_keys();
        test_robin_hood_hashing();
        test_backward_shift_deletion();
        test_simd_optimizations();
        test_edge_cases();
        performance_comparison();

        std::cout << "\n🎉 All tests passed successfully!" << std::endl;
        std::cout << "\nFeatures implemented:" << std::endl;
        std::cout << "✓ Robin-hood hash table with backward-shift deletion" << std::endl;
        std::cout << "✓ Densely packed storage" << std::endl;
        std::cout << "✓ 8-byte bucket structure with fingerprinting" << std::endl;
        std::cout << "✓ WyHash SIMD intrinsics" << std::endl;
        std::cout << "✓ Automatic mixing for poor-quality hashes" << std::endl;
        std::cout << "✓ Cache-efficient design" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}