cmake_minimum_required(VERSION 3.14)
project(razorfs_tests)

# C++17 for Google Test
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C11 for RAZORFS source
set(CMAKE_C_STANDARD 11)

# Enable testing
enable_testing()

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}
)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# RAZORFS source files (compiled as C)
set(RAZORFS_SOURCES
    ../src/string_table.c
    ../src/nary_tree_mt.c
    ../src/shm_persist.c
    ../src/compression.c
    ../src/numa_support.c
)

# Create library from RAZORFS sources
add_library(razorfs_lib STATIC ${RAZORFS_SOURCES})
target_link_libraries(razorfs_lib
    ${FUSE3_LIBRARIES}
    ZLIB::ZLIB
    Threads::Threads
    numa
    rt
)
target_include_directories(razorfs_lib PUBLIC ${FUSE3_INCLUDE_DIRS})

# Unit Tests
add_executable(string_table_test unit/string_table_test.cpp)
target_link_libraries(string_table_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

add_executable(nary_tree_test unit/nary_tree_test.cpp)
target_link_libraries(nary_tree_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

add_executable(shm_persist_test unit/shm_persist_test.cpp)
target_link_libraries(shm_persist_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

add_executable(compression_test unit/compression_test.cpp)
target_link_libraries(compression_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

add_executable(architecture_test unit/architecture_test.cpp)
target_link_libraries(architecture_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

# Integration Tests
add_executable(integration_test integration/filesystem_test.cpp)
target_link_libraries(integration_test
    razorfs_lib
    GTest::gtest_main
    GTest::gmock
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(string_table_test)
gtest_discover_tests(nary_tree_test)
gtest_discover_tests(shm_persist_test)
gtest_discover_tests(compression_test)
gtest_discover_tests(architecture_test)
gtest_discover_tests(integration_test)
