#!/bin/bash

echo "=== Compiling minimal benchmark ==="
cd /Users/abhiv/Desktop/Unordered-Dense-Map

# Try to compile with debug flags
clang++ -std=c++20 -O0 -g -Iinclude minimal_benchmark.cpp src/unordered_dense_map.cpp -o minimal_benchmark -pthread 2>&1

if [ $? -eq 0 ]; then
    echo "=== Compilation successful ==="
    echo "=== Running minimal benchmark ==="
    ./minimal_benchmark
    echo "=== Exit code: $? ==="
else
    echo "=== Compilation failed ==="
fi