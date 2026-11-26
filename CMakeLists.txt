cmake_minimum_required(VERSION 3.25)
project(consistent_hash LANGUAGES CXX VERSION 1.0.0)

# C++ standard per target (modern approach)
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
option(CONSISTENT_HASH_ENABLE_SANITIZERS "Enable sanitizers in Debug builds" OFF)

# Base compile options for all targets
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX)
endif()

# Sanitizers in Debug
if(CONSISTENT_HASH_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Header-only interface target
add_library(consistent_hash INTERFACE)
target_include_directories(consistent_hash
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(consistent_hash INTERFACE cxx_std_20)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(consistent_hash INTERFACE -pthread)
    target_link_options(consistent_hash INTERFACE -pthread)
endif()

# Implementation target (NodeManager non-inline bits)
add_library(consistent_hash_impl STATIC src/consistent_hash.cpp)
target_include_directories(consistent_hash_impl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(consistent_hash_impl PRIVATE cxx_std_20)
target_link_libraries(consistent_hash_impl PUBLIC consistent_hash)

# Fetch GoogleTest and Google Benchmark
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
    target_compile_features(consistent_hash_tests PRIVATE cxx_std_20)

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
    target_compile_features(consistent_hash_benchmarks PRIVATE cxx_std_20)
endif()

if(CONSISTENT_HASH_BUILD_EXAMPLES)
    add_executable(example_cache_sharding examples/cache_sharding/main.cpp)
    target_link_libraries(example_cache_sharding PRIVATE consistent_hash_impl)
    target_compile_features(example_cache_sharding PRIVATE cxx_std_20)

    add_executable(example_load_balancer examples/load_balancer/main.cpp)
    target_link_libraries(example_load_balancer PRIVATE consistent_hash_impl)
    target_compile_features(example_load_balancer PRIVATE cxx_std_20)

    add_executable(example_db_sharding examples/db_sharding/main.cpp)
    target_link_libraries(example_db_sharding PRIVATE consistent_hash_impl)
    target_compile_features(example_db_sharding PRIVATE cxx_std_20)

    add_executable(example_cdn_routing examples/cdn_routing/main.cpp)
    target_link_libraries(example_cdn_routing PRIVATE consistent_hash_impl)
    target_compile_features(example_cdn_routing PRIVATE cxx_std_20)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS consistent_hash consistent_hash_impl
    EXPORT consistent_hashTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT consistent_hashTargets
    FILE consistent_hashConfig.cmake
    NAMESPACE consistent_hash::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/consistent_hash
)

# Config package for find_package support
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/consistent_hashConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/consistent_hashConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/consistent_hash
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/consistent_hashConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/consistent_hashConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/consistent_hashConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/consistent_hash
)

# CPack for packaging
include(CPack)