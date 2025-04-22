CXX = clang++
CXXFLAGS = -std=c++20 -O3 -march=native -Wall -Wextra -pthread
INCLUDES = -Iinclude

# Targets
TARGET = test_unordered_dense_map
CONCURRENT_TARGET = test_concurrent
BENCHMARK_TARGET = benchmark
LIB_SOURCES = src/unordered_dense_map.cpp
TEST_SOURCES = src/test_unordered_dense_map.cpp
CONCURRENT_SOURCES = src/test_concurrent.cpp
BENCHMARK_SOURCES = src/benchmark.cpp

# Default target
all: $(TARGET) $(CONCURRENT_TARGET) $(BENCHMARK_TARGET)

# Build the test executable
$(TARGET): $(TEST_SOURCES) $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_SOURCES) $(LIB_SOURCES) -o $(TARGET)

# Build the concurrent test executable
$(CONCURRENT_TARGET): $(CONCURRENT_SOURCES) $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CONCURRENT_SOURCES) $(LIB_SOURCES) -o $(CONCURRENT_TARGET)

# Build the benchmark executable
$(BENCHMARK_TARGET): $(BENCHMARK_SOURCES) $(LIB_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(BENCHMARK_SOURCES) $(LIB_SOURCES) -o $(BENCHMARK_TARGET)

# Run tests
test: $(TARGET) $(CONCURRENT_TARGET)
	@echo "Running sequential tests..."
	./$(TARGET)
	@echo "Running concurrent tests..."
	./$(CONCURRENT_TARGET)

# Run benchmarks
benchmark: $(BENCHMARK_TARGET)
	@echo "Running performance benchmarks..."
	./$(BENCHMARK_TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(CONCURRENT_TARGET) $(BENCHMARK_TARGET)
	rm -rf build/

# Install (optional)
install: $(TARGET)
	@echo "Installing headers to /usr/local/include..."
	@sudo mkdir -p /usr/local/include
	@sudo cp include/*.hpp /usr/local/include/
	@echo "Installation complete!"

# Uninstall
uninstall:
	@echo "Removing installed headers..."
	@sudo rm -f /usr/local/include/unordered_dense_map.hpp
	@sudo rm -f /usr/local/include/unordered_dense_map_impl.hpp
	@echo "Uninstallation complete!"

.PHONY: all test benchmark clean install uninstall 