cmake_minimum_required(VERSION 3.25)
project(chash LANGUAGES CXX)

find_package(consistent_hash REQUIRED)
find_package(cxxopts REQUIRED)

add_executable(chash main.cpp)

target_link_libraries(chash PRIVATE consistent_hash::consistent_hash cxxopts)
target_compile_features(chash PRIVATE cxx_std_20)

# Install
install(TARGETS chash
    RUNTIME DESTINATION bin
)