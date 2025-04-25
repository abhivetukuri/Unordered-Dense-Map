#include "include/unordered_dense_map.hpp"
#include "include/unordered_dense_map_impl.hpp"
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    std::cout << "Minimal benchmark starting..." << std::endl;
    
    try {
        // Test 1: Basic creation
        std::cout << "Creating map..." << std::endl;
        unordered_dense_map<int, int> map;
        
        // Test 2: Single insertion
        std::cout << "Single insertion..." << std::endl;
        auto result = map.emplace(1, 10);
        std::cout << "Inserted: " << result.second << std::endl;
        
        // Test 3: Multiple insertions
        std::cout << "Multiple insertions..." << std::endl;
        for (int i = 2; i <= 10; ++i) {
            map.emplace(i, i * 10);
        }
        
        std::cout << "Map size: " << map.size() << std::endl;
        
        // Test 4: Lookup
        std::cout << "Testing lookup..." << std::endl;
        auto it = map.find(5);
        if (it != map.end()) {
            std::cout << "Found: " << it->key << " -> " << it->value << std::endl;
        }
        
        // Test 5: Iteration
        std::cout << "Testing iteration..." << std::endl;
        int count = 0;
        for (const auto& entry : map) {
            count++;
            if (count <= 3) {
                std::cout << "Entry: " << entry.key << " -> " << entry.value << std::endl;
            }
        }
        std::cout << "Iterated over " << count << " entries" << std::endl;
        
        // Test 6: Insert with operator[]
        std::cout << "Testing operator[]..." << std::endl;
        map[20] = 200;
        std::cout << "map[20] = " << map[20] << std::endl;
        
        // Test 7: Batch operations
        std::cout << "Testing batch insertion..." << std::endl;
        std::vector<std::pair<int, int>> batch = {{100, 1000}, {101, 1010}, {102, 1020}};
        map.batch_insert(batch.begin(), batch.end());
        
        std::cout << "Final map size: " << map.size() << std::endl;
        
        std::cout << "All tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
        return 1;
    }
    
    return 0;
}