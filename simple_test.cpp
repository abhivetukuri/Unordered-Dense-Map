#include "include/unordered_dense_map.hpp"
#include <iostream>

int main() {
    try {
        unordered_dense_map<int, int> map;
        map[1] = 10;
        map[2] = 20;
        
        auto it = map.find(1);
        if (it != map.end()) {
            std::cout << "Found key 1 with value: " << it->value << std::endl;
        }
        
        std::cout << "Simple test passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
}