#include "include/unordered_dense_map.hpp"
#include "include/unordered_dense_map_impl.hpp"
#include "include/concurrent_unordered_dense_map.hpp"
#include <iostream>

int main() {
    std::cout << "Testing basic functionality..." << std::endl;
    
    try {
        // Test 1: Basic sequential map
        std::cout << "Test 1: Creating sequential map..." << std::endl;
        unordered_dense_map<int, int> map;
        
        std::cout << "Test 2: Inserting elements..." << std::endl;
        map.emplace(1, 10);
        map.emplace(2, 20);
        
        std::cout << "Test 3: Looking up elements..." << std::endl;
        auto it = map.find(1);
        if (it != map.end()) {
            std::cout << "Found: " << it->key << " -> " << it->value << std::endl;
        }
        
        std::cout << "Test 4: Using operator[]..." << std::endl;
        map[3] = 30;
        std::cout << "map[3] = " << map[3] << std::endl;
        
        std::cout << "Test 5: Creating concurrent map..." << std::endl;
        concurrent_unordered_dense_map<int, int> cmap;
        
        std::cout << "Test 6: Concurrent insert..." << std::endl;
        cmap.insert(1, 10);
        cmap.insert(2, 20);
        
        std::cout << "Test 7: Concurrent find..." << std::endl;
        if (cmap.contains(1)) {
            std::cout << "Found 1 in concurrent map" << std::endl;
        }
        
        std::cout << "All tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}