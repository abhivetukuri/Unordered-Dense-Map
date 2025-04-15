cmake_minimum_required(VERSION 3.16)
project(UnorderedDenseMap VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable SIMD optimizations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")

# Enable optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Wall -Wextra")

# Check for AVX2 support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if(COMPILER_SUPPORTS_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    add_definitions(-DUSE_AVX2)
endif()

# Check for SSE4.2 support
check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
if(COMPILER_SUPPORTS_SSE42)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
    add_definitions(-DUSE_SSE42)
endif()

# Create the test executable
add_executable(test_unordered_dense_map test_unordered_dense_map.cpp)

# Set target properties
set_target_properties(test_unordered_dense_map PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Add include directories
target_include_directories(test_unordered_dense_map PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable testing
enable_testing()

# Add test
add_test(NAME UnorderedDenseMapTest COMMAND test_unordered_dense_map)

# Create a library target for the header-only implementation
add_library(unordered_dense_map INTERFACE)
target_include_directories(unordered_dense_map INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(unordered_dense_map INTERFACE cxx_std_20)

# Install targets
install(TARGETS unordered_dense_map
    EXPORT UnorderedDenseMapTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(FILES unordered_dense_map.hpp
    DESTINATION include
)

# Install CMake config
install(EXPORT UnorderedDenseMapTargets
    FILE UnorderedDenseMapTargets.cmake
    NAMESPACE UnorderedDenseMap::
    DESTINATION lib/cmake/UnorderedDenseMap
)

# Create and install config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    UnorderedDenseMapConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/UnorderedDenseMapConfigVersion.cmake
    DESTINATION lib/cmake/UnorderedDenseMap
)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/UnorderedDenseMapConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/UnorderedDenseMapConfig.cmake
    INSTALL_DESTINATION lib/cmake/UnorderedDenseMap
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/UnorderedDenseMapConfig.cmake
    DESTINATION lib/cmake/UnorderedDenseMap
)

# Print configuration info
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(COMPILER_SUPPORTS_AVX2)
    message(STATUS "AVX2 support: Enabled")
else()
    message(STATUS "AVX2 support: Disabled")
endif()
if(COMPILER_SUPPORTS_SSE42)
    message(STATUS "SSE4.2 support: Enabled")
else()
    message(STATUS "SSE4.2 support: Disabled")
endif() 