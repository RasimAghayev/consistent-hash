from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, get, rmdir
from conan.tools.scm import Version
import os


class ConsistentHashConan(ConanFile):
    name = "consistent-hash"
    version = "1.0.0"
    license = "MIT"
    author = "Consistent Hash Contributors"
    url = "https://github.com/RasimAghayev/consistent-hash"
    description = "Modern C++20 Consistent Hashing Library"
    topics = ("consistent-hashing", "distributed-systems", "load-balancing", "c++20")
    homepage = "https://github.com/RasimAghayev/consistent-hash"
    
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_tests": [True, False],
        "build_benchmarks": [True, False],
        "build_examples": [True, False],
        "build_fuzzers": [True, False],
        "enable_hardening": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_tests": False,
        "build_benchmarks": False,
        "build_examples": True,
        "build_fuzzers": False,
        "enable_hardening": True,
    }
    
    exports_sources = "CMakeLists.txt", "include/*", "src/*", "cmake/*", "LICENSE"
    
    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
    
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
    
    def layout(self):
        cmake_layout(self)
    
    def requirements(self):
        # Optional dependencies for features
        if self.options.build_tests:
            self.requires("gtest/1.15.0")
        if self.options.build_benchmarks:
            self.requires("benchmark/1.8.3")
        # nlohmann/json and msgpack-c for serialization
        self.requires("nlohmann_json/3.12.0")
        self.requires("msgpack-c/6.1.0")
    
    def build_requirements(self):
        self.tool_requires("cmake/3.28.0")
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        tc.variables["CONSISTENT_HASH_BUILD_TESTS"] = self.options.build_tests
        tc.variables["CONSISTENT_HASH_BUILD_BENCHMARKS"] = self.options.build_benchmarks
        tc.variables["CONSISTENT_HASH_BUILD_EXAMPLES"] = self.options.build_examples
        tc.variables["CONSISTENT_HASH_BUILD_FUZZERS"] = self.options.build_fuzzers
        tc.variables["CONSISTENT_HASH_ENABLE_HARDENING"] = self.options.enable_hardening
        tc.variables["CMAKE_CXX_STANDARD"] = "20"
        tc.variables["CMAKE_CXX_STANDARD_REQUIRED"] = "ON"
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        
        # Remove unnecessary files
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "share"))
    
    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ConsistentHash")
        self.cpp_info.set_property("cmake_target_name", "ConsistentHash::ConsistentHash")
        self.cpp_info.set_property("pkg_config_name", "consistent-hash")
        
        # Header-only library
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        
        self.cpp_info.includedirs = ["include"]
        
        # System libraries
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread", "m", "rt"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = []
        
        # CMake config
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.set_property("cmake_config_version", self.version)
        
        # Components
        self.cpp_info.components["consistent_hash"].set_property("cmake_target_name", "ConsistentHash::ConsistentHash")
        self.cpp_info.components["consistent_hash"].includedirs = ["include"]
        self.cpp_info.components["consistent_hash"].requires = ["nlohmann_json::nlohmann_json", "msgpack-c::msgpack-c"]
        
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["consistent_hash"].system_libs = ["pthread", "m", "rt"]