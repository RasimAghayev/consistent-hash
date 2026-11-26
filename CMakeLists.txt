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
option(CONSISTENT_HASH_ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(CONSISTENT_HASH_BUILD_FUZZERS "Build libFuzzer targets" OFF)
option(CONSISTENT_HASH_BUILD_PROPERTY_TESTS "Build RapidCheck property-based tests" OFF)

# Base compile options for all targets
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wformat=2 -Wformat-security
        -Wcast-align -Wcast-qual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
        -Wshadow -Wold-style-cast
        -Wnon-virtual-dtor -Woverloaded-virtual
        -Wsuggest-override -Wsuggest-final-types
        -Wduplicated-cond -Wduplicated-branches
        -Wlogical-op -Wuseless-cast
    )
    # Hardening flags (not in Debug to avoid performance impact)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
            -fstack-clash-protection
            -fcf-protection=full
        )
        add_link_options(
            -Wl,-z,relro,-z,now
            -Wl,-z,noexecstack
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX /permissive- /guard:cf /sdl)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_link_options(/DYNAMICBASE:NO /NXCOMPAT /SAFESEH)
    endif()
endif()

# Sanitizers in Debug
if(CONSISTENT_HASH_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Code coverage instrumentation
if(CONSISTENT_HASH_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# libFuzzer support
if(CONSISTENT_HASH_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fsanitize=fuzzer,address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=fuzzer,address)
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

if(CONSISTENT_HASH_BUILD_PROPERTY_TESTS)
    FetchContent_Declare(
        rapidcheck
        GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(rapidcheck)

    add_executable(consistent_hash_property_tests
        tests/property/test_ring_properties.cpp
        tests/property/test_jump_properties.cpp
        tests/property/test_rendezvous_properties.cpp
        tests/property/test_maglev_properties.cpp
        tests/property/test_multiprobe_properties.cpp
        tests/property/test_bounded_loads_properties.cpp
    )
    target_link_libraries(consistent_hash_property_tests PRIVATE consistent_hash_impl RapidCheck::rapidcheck)
    target_compile_features(consistent_hash_property_tests PRIVATE cxx_std_20)
    
    include(CTest)
    add_test(NAME consistent_hash_property_tests COMMAND consistent_hash_property_tests)
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

# libFuzzer targets (Clang only)
if(CONSISTENT_HASH_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Building libFuzzer targets")

    # Fuzzer for RingHasher lookup
    add_executable(fuzz_ring_lookup fuzz/fuzz_ring_lookup.cpp)
    target_link_libraries(fuzz_ring_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_ring_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_ring_lookup PROPERTIES
        OUTPUT_NAME "fuzz_ring_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for JumpHasher
    add_executable(fuzz_jump_lookup fuzz/fuzz_jump_lookup.cpp)
    target_link_libraries(fuzz_jump_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_jump_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_jump_lookup PROPERTIES
        OUTPUT_NAME "fuzz_jump_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for RendezvousHasher
    add_executable(fuzz_rendezvous_lookup fuzz/fuzz_rendezvous_lookup.cpp)
    target_link_libraries(fuzz_rendezvous_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_rendezvous_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_rendezvous_lookup PROPERTIES
        OUTPUT_NAME "fuzz_rendezvous_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for MaglevHasher
    add_executable(fuzz_maglev_lookup fuzz/fuzz_maglev_lookup.cpp)
    target_link_libraries(fuzz_maglev_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_maglev_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_maglev_lookup PROPERTIES
        OUTPUT_NAME "fuzz_maglev_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for MultiProbeHasher
    add_executable(fuzz_multiprobe_lookup fuzz/fuzz_multiprobe_lookup.cpp)
    target_link_libraries(fuzz_multiprobe_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_multiprobe_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_multiprobe_lookup PROPERTIES
        OUTPUT_NAME "fuzz_multiprobe_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for BoundedLoadsHasher
    add_executable(fuzz_bounded_loads_lookup fuzz/fuzz_bounded_loads_lookup.cpp)
    target_link_libraries(fuzz_bounded_loads_lookup PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_bounded_loads_lookup PRIVATE cxx_std_20)
    set_target_properties(fuzz_bounded_loads_lookup PROPERTIES
        OUTPUT_NAME "fuzz_bounded_loads_lookup"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Fuzzer for NodeManager (deserialize)
    add_executable(fuzz_node_manager fuzz/fuzz_node_manager.cpp)
    target_link_libraries(fuzz_node_manager PRIVATE consistent_hash_impl)
    target_compile_features(fuzz_node_manager PRIVATE cxx_std_20)
    set_target_properties(fuzz_node_manager PROPERTIES
        OUTPUT_NAME "fuzz_node_manager"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )

    # Custom target to run all fuzzers for a short time (CI smoke test)
    add_custom_target(fuzz-smoke
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_ring_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_jump_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_rendezvous_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_maglev_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_multiprobe_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_bounded_loads_lookup -runs=1000 -max_len=100 -timeout=30
        COMMAND ${CMAKE_BINARY_DIR}/fuzz/fuzz_node_manager -runs=1000 -max_len=100 -timeout=30
        COMMENT "Running libFuzzer smoke tests"
        DEPENDS fuzz_ring_lookup fuzz_jump_lookup fuzz_rendezvous_lookup fuzz_maglev_lookup fuzz_multiprobe_lookup fuzz_bounded_loads_lookup fuzz_node_manager
    )
endif()