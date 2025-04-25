#include "include/unordered_dense_map.hpp"
#include <iostream>

int main() {
    unordered_dense_map<int, int> map;
    map[1] = 10;
    std::cout << "Test passed" << std::endl;
    return 0;
}