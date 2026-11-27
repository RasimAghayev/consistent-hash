cmake_minimum_required(VERSION 3.25)
project(prometheus_exporter LANGUAGES CXX)

find_package(consistent_hash REQUIRED)
find_package(prometheus-cpp REQUIRED)

add_executable(prometheus_exporter main.cpp)
target_link_libraries(prometheus_exporter PRIVATE consistent_hash::consistent_hash prometheus-cpp-push prometheus-cpp-core)
target_compile_features(prometheus_exporter PRIVATE cxx_std_20)