#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "UnorderedDenseMap::unordered_dense_map" for configuration ""
set_property(TARGET UnorderedDenseMap::unordered_dense_map APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(UnorderedDenseMap::unordered_dense_map PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libunordered_dense_map.a"
  )

list(APPEND _cmake_import_check_targets UnorderedDenseMap::unordered_dense_map )
list(APPEND _cmake_import_check_files_for_UnorderedDenseMap::unordered_dense_map "${_IMPORT_PREFIX}/lib/libunordered_dense_map.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
