=========
Quickstart
=========

Basic Usage
===========

.. code-block:: cpp

   #include <consistent_hash/consistent_hash.hpp>
   #include <iostream>

   int main() {
     // Create a ring hasher with default settings
     consistent_hash::RingHasher<> hasher;

     // Add nodes with weights
     hasher.add_node("cache-1", 100);  // weight 100
     hasher.add_node("cache-2", 200);  // weight 200 (2x capacity)
     hasher.add_node("cache-3", 150);  // weight 150

     // Lookup a key
     std::string key = "user:12345";
     auto node = hasher.lookup(key);

     if (node) {
       std::cout << "Key '" << key << "' maps to node: " << *node << "\n";
     }

     return 0;
   }

Choosing an Algorithm
=====================

.. tabs::

   .. tab:: Ring (Default)

      Best for: General purpose, cache sharding, when you need virtual nodes and weights.

      .. code-block:: cpp

         consistent_hash::RingHasher<> hasher;
         hasher.add_node("node1", 100);

   .. tab:: Jump

      Best for: High performance, minimal memory, when node list is stable.

      .. code-block:: cpp

         consistent_hash::JumpHasher<> hasher;
         hasher.add_node("node1");

   .. tab:: Rendezvous (HRW)

      Best for: Weighted distributions, multi-tenant, when weights change frequently.

      .. code-block:: cpp

         consistent_hash::RendezvousHasher<> hasher;
         hasher.add_node("node1", 100);

   .. tab:: Maglev

      Best for: Network load balancing, consistent permutation, fast O(1) lookup.

      .. code-block:: cpp

         consistent_hash::MaglevHasher<> hasher;
         hasher.add_node("node1", 100);

   .. tab:: Multi-Probe

      Best for: Bounded load with probe limits, strict capacity enforcement.

      .. code-block:: cpp

         consistent_hash::MultiProbeHasher<> hasher;
         hasher.add_node("node1", 100);

   .. tab:: Bounded Loads

      Best for: Strict capacity limits per node, hard bounds.

      .. code-block:: cpp

         consistent_hash::BoundedLoadsHasher<> hasher;
         hasher.add_node("node1", 100);

Thread Safety
=============

All hashers are thread-safe for concurrent reads. Writes (add/remove node) require exclusive access:

.. code-block:: cpp

   #include <shared_mutex>

   consistent_hash::RingHasher<> hasher;
   std::shared_mutex mtx;

   // Readers (concurrent)
   auto read = [&](std::string key) {
     std::shared_lock lock(mtx);
     return hasher.lookup(key);
   };

   // Writers (exclusive)
   auto write = [&](std::string node, uint32_t weight) {
     std::unique_lock lock(mtx);
     hasher.add_node(node, weight);
   };

Custom Hasher
=============

Use your own hash function:

.. code-block:: cpp

   #include <consistent_hash/concepts.hpp>
   #include <xxhash.h>

   struct XXH3Hasher {
     using result_type = uint64_t;

     constexpr uint64_t operator()(std::string_view key) const noexcept {
       return XXH3_64bits(key.data(), key.size());
     }
   };

   // Use with any algorithm
   consistent_hash::RingHasher<std::string, XXH3Hasher> hasher;

Serialization
=============

Save and restore hasher state:

.. code-block:: cpp

   #include <consistent_hash/serialization.hpp>
   #include <fstream>

   consistent_hash::RingHasher<> hasher;
   hasher.add_node("node1", 100);

   // Save to JSON
   {
     std::ofstream ofs("state.json");
     consistent_hash::save_json(hasher, ofs);
   }

   // Load from JSON
   consistent_hash::RingHasher<> hasher2;
   {
     std::ifstream ifs("state.json");
     consistent_hash::load_json(hasher2, ifs);
   }

Error Handling
==============

Operations return ``std::optional`` or ``Result`` types:

.. code-block:: cpp

   // Returns std::optional<NodeId> (empty if no nodes)
   auto node = hasher.lookup("key");

   // Returns Result<NodeId, ErrorCode> with error details
   auto result = hasher.lookup_result("key");
   if (!result) {
     std::cerr << "Error: " << static_cast<int>(result.error()) << "\n";
   }

   // Bulk lookup
   std::vector<std::string> keys = {"key1", "key2", "key3"};
   auto results = hasher.lookup_bulk(keys);
   // results: std::vector<std::optional<NodeId>>

Next Steps
==========

* Read about specific :doc:`algorithms/ring`
* Learn about :doc:`concepts` for custom types
* See :doc:`examples/load_balancer` for a complete example
* Check :doc:`performance` for benchmarking tips