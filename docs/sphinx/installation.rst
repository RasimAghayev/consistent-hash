============
Installation
============

Package Managers
================

vcpkg
-----

.. code-block:: bash

   vcpkg install consistent-hash

Conan
-----

.. code-block:: bash

   conan install consistent-hash/1.0.0@

CMake FetchContent
------------------

.. code-block:: cmake

   include(FetchContent)
   FetchContent_Declare(
     consistent_hash
     GIT_REPOSITORY https://github.com/RasimAghayev/consistent-hash.git
     GIT_TAG        v1.0.0  # or main for latest
   )
   FetchContent_MakeAvailable(consistent_hash)

   target_link_libraries(your_target PRIVATE consistent_hash::consistent_hash)

Manual Installation
===================

Single Header (Recommended)
---------------------------

Copy ``include/consistent_hash/consistent_hash.hpp`` to your project:

.. code-block:: cpp

   #include "consistent_hash/consistent_hash.hpp"

   int main() {
     consistent_hash::RingHasher<> hasher;
     hasher.add_node("node1", 1);
     auto node = hasher.lookup("my-key");
   }

Full Source Tree
----------------

.. code-block:: bash

   git clone https://github.com/RasimAghayev/consistent-hash.git
   cd consistent-hash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   sudo cmake --install build

CMake Options
=============

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Option
     - Default
     - Description
   * - ``CONSISTENT_HASH_BUILD_TESTS``
     - ``ON``
     - Build unit/integration tests
   * - ``CONSISTENT_HASH_BUILD_BENCHMARKS``
     - ``OFF``
     - Build GoogleBenchmark benchmarks
   * - ``CONSISTENT_HASH_BUILD_EXAMPLES``
     - ``ON``
     - Build example applications
   * - ``CONSISTENT_HASH_BUILD_FUZZERS``
     - ``OFF``
     - Build libFuzzer targets
   * - ``CONSISTENT_HASH_ENABLE_COVERAGE``
     - ``OFF``
     - Enable code coverage (gcov/lcov)
   * - ``CONSISTENT_HASH_ENABLE_SANITIZERS``
     - ``OFF``
     - Enable ASan/UBSan/MSan
   * - ``CONSISTENT_HASH_ENABLE_HARDENING``
     - ``ON``
     - Enable hardening flags (Release)

Example with all options:

.. code-block:: bash

   cmake -B build \
     -DCMAKE_BUILD_TYPE=Release \
     -DCONSISTENT_HASH_BUILD_TESTS=ON \
     -DCONSISTENT_HASH_BUILD_BENCHMARKS=ON \
     -DCONSISTENT_HASH_BUILD_EXAMPLES=ON \
     -DCONSISTENT_HASH_ENABLE_HARDENING=ON

Docker
======

.. code-block:: bash

   # Build image
   docker build -t consistent-hash .

   # Run examples
   docker run --rm consistent-hash ./bin/example_load_balancer

   # Development container
   docker run -it -v $(pwd):/src consistent-hash bash

Dependencies
============

Required
--------

* C++20 standard library

Optional (auto-fetched via FetchContent)
----------------------------------------

* `GoogleTest <https://github.com/google/googletest>`_ v1.15.2 (tests)
* `GoogleBenchmark <https://github.com/google/benchmark>`_ v1.8.3 (benchmarks)
* `nlohmann/json <https://github.com/nlohmann/json>`_ v3.11.2 (JSON serialization)
* `msgpack-c <https://github.com/msgpack/msgpack-c>`_ v6.1.0 (MessagePack serialization)
* `RapidCheck <https://github.com/emil-e/rapidcheck>`_ (property-based testing)

Building Tests
==============

.. code-block:: bash

   cmake -B build -DCONSISTENT_HASH_BUILD_TESTS=ON
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure

Building Benchmarks
===================

.. code-block:: bash

   cmake -B build -DCONSISTENT_HASH_BUILD_BENCHMARKS=ON
   cmake --build build --parallel
   ./build/benchmarks/consistent_hash_benchmark --benchmark_format=json

Building Examples
=================

.. code-block:: bash

   cmake -B build -DCONSISTENT_HASH_BUILD_EXAMPLES=ON
   cmake --build build --parallel
   ./build/examples/example_load_balancer