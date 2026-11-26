===============
Ring Hasher
===============

.. doxygenclass:: consistent_hash::RingHasher
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

Overview
========

The Ring Hasher implements classic consistent hashing using a hash ring with virtual nodes. Each physical node is mapped to multiple virtual nodes on the ring, providing better load distribution.

Algorithm
=========

1. Hash each node (with virtual node suffix) to a position on the ring [0, 2^64-1]
2. Sort virtual nodes by position
3. For lookup, hash the key and find the first virtual node clockwise
4. Return the physical node owning that virtual node

Complexity
==========

* **Add Node**: O(V log V) where V = virtual nodes
* **Remove Node**: O(V)
* **Lookup**: O(log V) via binary search
* **Memory**: O(V)

Configuration
=============

.. code-block:: cpp

   // Default: 150 virtual nodes per physical node
   consistent_hash::RingHasher<> hasher;

   // Custom virtual node count
   consistent_hash::RingHasher<> hasher(200);

   // Custom hasher
   consistent_hash::RingHasher<std::string, MyHasher> hasher;

Virtual Nodes
=============

Virtual nodes improve load distribution by giving each physical node multiple positions on the ring. More virtual nodes = better distribution but more memory.

.. list-table::
   :header-rows: 1

   * - Virtual Nodes
     - Distribution Quality
     - Memory (1000 nodes)
   * - 50
     - Fair
     - ~40 KB
   * - 150 (default)
     - Good
     - ~120 KB
   * - 500
     - Excellent
     - ~400 KB

Weighted Nodes
==============

Nodes can have weights to handle heterogeneous capacities:

.. code-block:: cpp

   hasher.add_node("small-cache", 50);    // 1x capacity
   hasher.add_node("medium-cache", 100);  // 2x capacity
   hasher.add_node("large-cache", 200);   // 4x capacity

Weight affects the number of virtual nodes assigned:
``virtual_nodes = base_virtual_nodes * weight / 100``

Bounded Loads
=============

Enable bounded loads to prevent hot spots:

.. code-block:: cpp

   // Max load factor: 1.5x average
   hasher.set_bounded_load(1.5);

   // Or use the factory for bounded loads
   auto hasher = consistent_hash::make_hasher<consistent_hash::Algorithm::Ring>(
     consistent_hash::Config{.bounded_load = 1.5}
   );

When a node exceeds its fair share, the hasher probes to the next node.

Thread Safety
=============

* **Concurrent lookups**: Safe (shared lock)
* **Concurrent add/remove**: Requires exclusive lock
* **Lookup during modification**: Safe (readers don't block, but may see intermediate state)

Example
=======

.. code-block:: cpp

   #include <consistent_hash/ring.hpp>
   #include <iostream>
   #include <vector>

   int main() {
     consistent_hash::RingHasher<> hasher(150);  // 150 virtual nodes

     // Add nodes with different weights
     hasher.add_node("cache-us-east-1", 200);
     hasher.add_node("cache-us-west-2", 150);
     hasher.add_node("cache-eu-west-1", 180);
     hasher.add_node("cache-ap-southeast-1", 100);

     // Enable bounded load (max 1.25x fair share)
     hasher.set_bounded_load(1.25);

     // Simulate requests
     std::vector<std::string> keys = {
       "user:1001", "user:1002", "session:abc", "session:def",
       "cache:image:123", "cache:video:456"
     };

     for (const auto& key : keys) {
       if (auto node = hasher.lookup(key)) {
         std::cout << key << " -> " << *node << "\n";
       }
     }

     // Check distribution
     auto stats = hasher.get_distribution_stats(10000);
     std::cout << "Load balance: " << stats.load_balance_ratio << "\n";
     std::cout << "Max load: " << stats.max_load << "\n";
     std::cout << "Min load: " << stats.min_load << "\n";

     return 0;
   }

API Reference
=============

.. doxygenclass:: consistent_hash::RingHasher
   :members:
   :outline:

See Also
========

* :doc:`/algorithms/jump` - Jump consistent hashing
* :doc:`/algorithms/rendezvous` - Rendezvous (HRW) hashing
* :doc:`/concepts` - C++20 concepts for custom types
* :doc:`/custom_hashers` - Using custom hash functions