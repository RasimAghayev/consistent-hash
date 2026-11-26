# consistent-hash vcpkg portfile.cmake
# This file is used by vcpkg to build and install consistent-hash

cmake_minimum_required(VERSION 3.25)

# Package metadata
set(PORT_NAME consistent-hash)
set(PORT_VERSION 1.0.0)

# Download source
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO RasimAghayev/consistent-hash
    REF v${PORT_VERSION}
    SHA512 0000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF main
)

# Configure options
set(CONSISTENT_HASH_BUILD_TESTS ${VCPKG_FEATURE_TESTS} CACHE BOOL "" FORCE)
set(CONSISTENT_HASH_BUILD_BENCHMARKS ${VCPKG_FEATURE_BENCHMARKS} CACHE BOOL "" FORCE)
set(CONSISTENT_HASH_BUILD_EXAMPLES ${VCPKG_FEATURE_EXAMPLES} CACHE BOOL "" FORCE)
set(CONSISTENT_HASH_BUILD_FUZZERS ${VCPKG_FEATURE_FUZZERS} CACHE BOOL "" FORCE)

# Disable hardening for package consumers (they can enable it themselves)
set(CONSISTENT_HASH_ENABLE_HARDENING OFF CACHE BOOL "" FORCE)

# Build and install
vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_INSTALL_INCLUDEDIR=include
)

vcpkg_cmake_install()

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE
     DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT_NAME}
     RENAME copyright
)

# Create config version file
vcpkg_cmake_config_fixup(
    PACKAGE_NAME ConsistentHash
    CONFIG_NAMESPACE consistent_hash
)