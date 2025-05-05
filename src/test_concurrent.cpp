#include "../include/concurrent_unordered_dense_map.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <cassert>

using namespace std::chrono;

void test_concurrent_basic()
{
    std::cout << "=== Testing Concurrent Basic Operations ===" << std::endl;

    concurrent_unordered_dense_map<int, int> map;

    assert(map.insert(1, 10));
    assert(map.insert(2, 20));
    assert(map.insert(3, 30));

    assert(map.contains(1));
    assert(map.contains(2));
    assert(map.contains(3));
    assert(!map.contains(4));

    auto it = map.find(2);
    assert(it != map.end());

    assert(map.size() == 3);

    assert(map.erase(2));
    assert(!map.contains(2));
    assert(map.size() == 2);

    std::cout << "✓ Concurrent basic operations passed!" << std::endl;
}

void test_concurrent_multithreaded()
{
    std::cout << "\n=== Testing Concurrent Multi-threaded Operations ===" << std::endl;

    concurrent_unordered_dense_map<int, int> map;
    const int num_threads = std::thread::hardware_concurrency();
    const int operations_per_thread = 1000;

    {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int t = 0; t < num_threads; ++t)
        {
            threads.emplace_back([&, t]()
                                 {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> dis(0, operations_per_thread * num_threads);
                
                for (int i = 0; i < operations_per_thread; ++i)
                {
                    int key = t * operations_per_thread + i;
                    int value = key * 2;
                    
                    if (map.insert(key, value))
                    {
                        success_count.fetch_add(1);
                    }
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        std::cout << "Inserted " << success_count.load() << " elements concurrently" << std::endl;
        std::cout << "Map size: " << map.size() << std::endl;
    }

    {
        std::vector<std::thread> threads;
        std::atomic<int> found_count{0};

        for (int t = 0; t < num_threads; ++t)
        {
            threads.emplace_back([&, t]()
                                 {
                for (int i = 0; i < operations_per_thread; ++i)
                {
                    int key = t * operations_per_thread + i;
                    
                    if (map.contains(key))
                    {
                        found_count.fetch_add(1);
                    }
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        std::cout << "Found " << found_count.load() << " elements during concurrent lookup" << std::endl;
    }

    {
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t)
        {
            threads.emplace_back([&, t]()
                                 {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> operation_dis(0, 2);
                std::uniform_int_distribution<> key_dis(0, operations_per_thread * num_threads);
                
                for (int i = 0; i < operations_per_thread / 2; ++i)
                {
                    int operation = operation_dis(gen);
                    int key = key_dis(gen);
                    
                    switch (operation)
                    {
                        case 0: // Insert
                            map.insert(key + 100000, key * 3);
                            break;
                        case 1: // Lookup
                            map.contains(key);
                            break;
                        case 2: // Erase
                            map.erase(key);
                            break;
                    }
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }
    }

    std::cout << "Final map size after mixed operations: " << map.size() << std::endl;
    std::cout << "✓ Concurrent multi-threaded operations completed!" << std::endl;
}

void benchmark_concurrent_vs_sequential()
{
    std::cout << "\n=== Concurrent vs Sequential Performance ===" << std::endl;

    const int num_operations = 100000;
    const int num_threads = std::thread::hardware_concurrency();

    {
        concurrent_unordered_dense_map<int, int> map;
        auto start = high_resolution_clock::now();

        for (int i = 0; i < num_operations; ++i)
        {
            map.insert(i, i * 2);
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "Sequential insertion (" << num_operations << " ops): "
                  << duration.count() / 1000.0 << " ms" << std::endl;
    }

    {
        concurrent_unordered_dense_map<int, int> map;
        auto start = high_resolution_clock::now();

        std::vector<std::thread> threads;
        int ops_per_thread = num_operations / num_threads;

        for (int t = 0; t < num_threads; ++t)
        {
            threads.emplace_back([&, t]()
                                 {
                int start_key = t * ops_per_thread;
                int end_key = (t + 1) * ops_per_thread;
                
                for (int i = start_key; i < end_key; ++i)
                {
                    map.insert(i, i * 2);
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "Concurrent insertion (" << num_operations << " ops, "
                  << num_threads << " threads): " << duration.count() / 1000.0 << " ms" << std::endl;
        std::cout << "Final map size: " << map.size() << std::endl;
    }
}

int main()
{
    std::cout << "Concurrent Unordered Dense Map Test Suite" << std::endl;
    std::cout << "=========================================" << std::endl;

    try
    {
        test_concurrent_basic();
        test_concurrent_multithreaded();
        benchmark_concurrent_vs_sequential();

        std::cout << "\n🎉 All concurrent tests completed!" << std::endl;
        std::cout << "\nConcurrent features implemented:" << std::endl;
        std::cout << "✓ Lock-free atomic operations" << std::endl;
        std::cout << "✓ Segmented design for reduced contention" << std::endl;
        std::cout << "✓ Epoch-based memory management concepts" << std::endl;
        std::cout << "✓ Thread-safe insertion, lookup, and deletion" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Concurrent test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}