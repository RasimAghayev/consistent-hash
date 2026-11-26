cmake_minimum_required(VERSION 3.20)
project(consistent_hash LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(CONSISTENT_HASH_BUILD_TESTS "Build GoogleTest suite" ON)
option(CONSISTENT_HASH_BUILD_BENCHMARKS "Build Google Benchmark suite" ON)
option(CONSISTENT_HASH_BUILD_EXAMPLES "Build example demos" ON)

# Treat warnings as errors in release/CI.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

# Library target (header-only by default; non-inline bits in src/).
add_library(consistent_hash INTERFACE)
target_include_directories(consistent_hash INTERFACE ${CMAKE_SOURCE_DIR}/include)

if(TARGET consistent_hash AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(consistent_hash INTERFACE -pthread)
endif()

# Source file for non-inline implementations (NodeManager).
add_library(consistent_hash_impl STATIC src/consistent_hash.cpp)
target_include_directories(consistent_hash_impl PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(consistent_hash_impl PRIVATE -pthread)
target_link_libraries(consistent_hash_impl PUBLIC consistent_hash)

# Fetch GoogleTest and Google Benchmark.
include(FetchContent)

if(CONSISTENT_HASH_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2)
    FetchContent_MakeAvailable(googletest)

    add_executable(consistent_hash_tests
        tests/unit/test_ring.cpp
        tests/unit/test_jump.cpp
        tests/unit/test_maglev.cpp
        tests/unit/test_rendezvous.cpp
        tests/unit/test_multiprobe.cpp
        tests/unit/test_bounded_loads.cpp
        tests/unit/test_node_manager.cpp
        tests/property/test_distribution.cpp
        tests/integration/test_demos.cpp)
    target_link_libraries(consistent_hash_tests PRIVATE consistent_hash_impl GTest::gtest_main)
    target_compile_options(consistent_hash_tests PRIVATE -pthread)

    include(CTest)
    add_test(NAME consistent_hash_tests COMMAND consistent_hash_tests)
endif()

if(CONSISTENT_HASH_BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(consistent_hash_benchmarks
        benchmarks/bench_lookup.cpp)
    target_link_libraries(consistent_hash_benchmarks PRIVATE consistent_hash_impl benchmark::benchmark)
    target_compile_options(consistent_hash_benchmarks PRIVATE -pthread)
endif()

if(CONSISTENT_HASH_BUILD_EXAMPLES)
    add_executable(example_cache_sharding examples/cache_sharding/main.cpp)
    target_link_libraries(example_cache_sharding PRIVATE consistent_hash_impl)
    target_compile_options(example_cache_sharding PRIVATE -pthread)

    add_executable(example_load_balancer examples/load_balancer/main.cpp)
    target_link_libraries(example_load_balancer PRIVATE consistent_hash_impl)
    target_compile_options(example_load_balancer PRIVATE -pthread)

    add_executable(example_db_sharding examples/db_sharding/main.cpp)
    target_link_libraries(example_db_sharding PRIVATE consistent_hash_impl)
    target_compile_options(example_db_sharding PRIVATE -pthread)

    add_executable(example_cdn_routing examples/cdn_routing/main.cpp)
    target_link_libraries(example_cdn_routing PRIVATE consistent_hash_impl)
    target_compile_options(example_cdn_routing PRIVATE -pthread)
endif()
