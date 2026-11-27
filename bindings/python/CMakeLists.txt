cmake_minimum_required(VERSION 3.25)
project(consistent_hash_python LANGUAGES CXX)

find_package(pybind11 REQUIRED)
find_package(consistent_hash REQUIRED)

pybind11_add_module(consistent_hash consistent_hash.cpp)

target_link_libraries(consistent_hash PRIVATE consistent_hash::consistent_hash)
target_compile_features(consistent_hash PRIVATE cxx_std_20)

set_target_properties(consistent_hash PROPERTIES
    PREFIX ""
    SUFFIX "${PYTHON_MODULE_EXTENSION}"
)

# Install
install(TARGETS consistent_hash
    LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}
)