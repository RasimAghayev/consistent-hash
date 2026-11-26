============
Introduction
============

What is Consistent Hashing?
===========================

Consistent hashing is a distributed hashing technique that minimizes reorganization when nodes are added or removed from a cluster. Unlike traditional modulo-based hashing, consistent hashing only requires **K/N** keys to be remapped when a node is added or removed (where K is the number of keys and N is the number of nodes).

Key Properties
==============

* **Minimal Disruption**: Only a small fraction of keys move when nodes change
* **Load Distribution**: Keys are distributed evenly across nodes
* **Scalability**: Works well with hundreds or thousands of nodes
* **Fault Tolerance**: Graceful degradation when nodes fail

Algorithms Provided
===================

This library implements six consistent hashing algorithms:

.. list-table::
   :header-rows: 1
   :widths: 20 20 30 30

   * - Algorithm
     - Complexity
     - Best For
     - Features
   * - :doc:`algorithms/ring`
     - O(log N)
     - General purpose, cache sharding
     - Virtual nodes, weights, bounded loads
   * - :doc:`algorithms/jump`
     - O(1)
     - High-performance, minimal memory
     - No virtual nodes needed, deterministic
   * - :doc:`algorithms/rendezvous`
     - O(N)
     - Weighted distributions, multi-tenant
     - Highest random weight (HRW)
   * - :doc:`algorithms/maglev`
     - O(N log N) build, O(1) lookup
     - Network load balancing (Google Maglev)
     - Consistent permutation, fast lookup
   * - :doc:`algorithms/multiprobe`
     - O(log N + P)
     - Bounded load with probes
     - Multiple probe strategies, load limits
   * - :doc:`algorithms/bounded_loads`
     - O(log N)
     - Strict capacity enforcement
     - Hard capacity limits per node

Features
========

* **Header-only**: Easy integration, no linking required
* **C++20**: Modern C++ with concepts, ranges, coroutines
* **Thread-safe**: Shared mutex for concurrent access
* **Custom Hashers**: Composable algebraic hashing with custom hash functions
* **Serialization**: JSON/MessagePack support for state persistence
* **Observability**: Built-in metrics, OpenTelemetry/Prometheus ready
* **Hardened**: AddressSanitizer, UBSan, libFuzzer, CodeQL tested

Requirements
============

* C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 19.30+)
* CMake 3.25+ (for building tests/examples)
* Optional: GoogleTest, GoogleBenchmark, nlohmann/json, msgpack-c

License
=======

MIT License - see :doc:`license` for details.