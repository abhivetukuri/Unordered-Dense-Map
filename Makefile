CXX = clang++
CXXFLAGS = -std=c++20 -O3 -march=native -Wall -Wextra
INCLUDES = -Iinclude

# Targets
TARGET = test_unordered_dense_map
SOURCES = src/test_unordered_dense_map.cpp

# Default target
all: $(TARGET)

# Build the test executable
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

# Run tests
test: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)
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

.PHONY: all test clean install uninstall 