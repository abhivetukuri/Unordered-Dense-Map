#include "include/unordered_dense_map.hpp"
#include "include/unordered_dense_map_impl.hpp"
#include <iostream>

int main() {
    std::cout << "Testing compilation..." << std::endl;
    
    unordered_dense_map<int, int> map;
    
    // Test the insert method we just added
    auto result = map.insert(1, 10);
    std::cout << "Insert result: " << result.second << std::endl;
    
    // Test emplace
    map.emplace(2, 20);
    
    // Test find
    auto it = map.find(1);
    if (it != map.end()) {
        std::cout << "Found: " << it->key << " -> " << it->value << std::endl;
    }
    
    std::cout << "Compilation test passed!" << std::endl;
    return 0;
}