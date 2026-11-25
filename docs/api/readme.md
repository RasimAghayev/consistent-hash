# API Reference

ConsistentHash is header-first. Include the umbrella header:

```cpp
#include <consistent_hash/consistent_hash.hpp>
```

## Core interface

```cpp
template <typename NodeId = std::string, typename Key = std::string>
class ConsistentHasher {
public:
    using NodeType = Node<NodeId>;
    using MigrationType = Migration<NodeId, Key>;
    using StatsType = DistributionStats<NodeId, Key>;

    virtual ~ConsistentHasher() = default;
    virtual void add_node(const NodeType& node) = 0;
    virtual void remove_node(const NodeId& id) = 0;
    virtual bool has_node(const NodeId& id) const = 0;
    virtual NodeId lookup(const Key& key) const = 0;              // throws if empty
    virtual std::vector<NodeId> lookup_n(const Key& key, size_t n) const = 0;
    virtual std::vector<MigrationType> rebalance() = 0;
    virtual StatsType stats() const = 0;
    virtual size_t node_count() const = 0;
    virtual void clear() = 0;
};
```

## Node

```cpp
template <typename NodeId = std::string>
struct Node {
    NodeId id;
    std::string address;
    uint32_t weight = 100;
    std::string zone;
    NodeStatus status = NodeStatus::Active;  // Active | Draining | Removed
    std::map<std::string, std::string> metadata;
};
```

## Algorithms

- `RingHasher<NodeId, Key>` — virtual nodes (default 150/node). Zone-aware via
  `preferred_zone(zone)`.
- `JumpHasher<NodeId, Key>` — Google Jump, O(1) memory.
- `MaglevHasher<NodeId, Key>` — fixed table (default 65537 slots).
- `RendezvousHasher<NodeId, Key>` — highest random weight.
- `MultiProbeHasher<NodeId, Key>` — k probes (default 16), least-loaded wins.
- `BoundedLoadsHasher<NodeId, Key>` — capacity-aware wrapper around Ring.

## Usage example

```cpp
RingHasher<> cache;
cache.add_node({"cache-1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}});
cache.add_node({"cache-2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}});

std::string owner = cache.lookup("user:123");   // cache-1 or cache-2
auto replicas = cache.lookup_n("user:123", 2);  // distinct owners
auto stats = cache.stats();                      // skew, migration count, latency
```

## Metrics

`DistributionStats` reports total keys, nodes used, mean/stddev load, skew
(stddev/mean), min/max load, migration count, and average/p99 lookup latency.
