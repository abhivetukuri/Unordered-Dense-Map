#include "include/unordered_dense_map.hpp"
#include "include/unordered_dense_map_impl.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>

using namespace std::chrono;

int main() {
    std::cout << "Safe benchmark starting..." << std::endl;
    
    try {
        // Small test first
        const size_t small_size = 100;
        
        std::cout << "Testing with " << small_size << " elements..." << std::endl;
        
        unordered_dense_map<int, int> map;
        
        // Test insertions
        auto start = high_resolution_clock::now();
        for (size_t i = 0; i < small_size; ++i) {
            map.emplace(i, i * 2);
        }
        auto end = high_resolution_clock::now();
        
        auto duration = duration_cast<microseconds>(end - start);
        std::cout << "Insertion of " << small_size << " elements took: " 
                  << duration.count() << " microseconds" << std::endl;
        
        std::cout << "Map size: " << map.size() << std::endl;
        
        // Test lookups
        start = high_resolution_clock::now();
        int found_count = 0;
        for (size_t i = 0; i < small_size; ++i) {
            auto it = map.find(i);
            if (it != map.end()) {
                found_count++;
            }
        }
        end = high_resolution_clock::now();
        
        duration = duration_cast<microseconds>(end - start);
        std::cout << "Lookup of " << small_size << " elements took: " 
                  << duration.count() << " microseconds" << std::endl;
        std::cout << "Found " << found_count << " elements" << std::endl;
        
        // Test iteration
        start = high_resolution_clock::now();
        int iter_count = 0;
        for (const auto& entry : map) {
            iter_count++;
            (void)entry; // Suppress unused variable warning
        }
        end = high_resolution_clock::now();
        
        duration = duration_cast<microseconds>(end - start);
        std::cout << "Iteration over " << iter_count << " elements took: " 
                  << duration.count() << " microseconds" << std::endl;
        
        // Test batch operations
        std::cout << "Testing batch operations..." << std::endl;
        std::vector<std::pair<int, int>> batch;
        for (int i = small_size; i < small_size + 10; ++i) {
            batch.emplace_back(i, i * 3);
        }
        
        start = high_resolution_clock::now();
        map.batch_insert(batch.begin(), batch.end());
        end = high_resolution_clock::now();
        
        duration = duration_cast<microseconds>(end - start);
        std::cout << "Batch insertion of " << batch.size() << " elements took: " 
                  << duration.count() << " microseconds" << std::endl;
        std::cout << "Final map size: " << map.size() << std::endl;
        
        std::cout << "All tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
        return 1;
    }
    
    return 0;
}