#pragma once

#include <cstddef>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "consistent_hash/hash.hpp"
#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Abstract interface for every consistent hashing algorithm.
// NodeId and Key default to std::string but may be any copyable, hashable type.
template <typename NodeId = std::string, typename Key = std::string>
class ConsistentHasher {
public:
    using node_id_type = NodeId;
    using key_type = Key;
    using NodeType = Node<NodeId>;
    using MigrationType = Migration<NodeId, Key>;
    using StatsType = DistributionStats<NodeId, Key>;

    virtual ~ConsistentHasher() = default;

    // Add a node (thread-safe). Weight scales virtual-node count where applicable.
    virtual void add_node(const NodeType& node) = 0;
    // Remove a node by id (thread-safe).
    virtual void remove_node(const NodeId& id) = 0;
    virtual bool has_node(const NodeId& id) const = 0;
    // Owner node for a key (thread-safe). Throws std::runtime_error when empty.
    virtual NodeId lookup(const Key& key) const = 0;
    // Up to n distinct owner nodes for a key.
    virtual std::vector<NodeId> lookup_n(const Key& key, size_t n) const = 0;
    // Recompute key->node assignments vs. the last snapshot; return migrations.
    virtual std::vector<MigrationType> rebalance() = 0;
    // Distribution + latency + migration metrics.
    virtual StatsType stats() const = 0;
    virtual size_t node_count() const = 0;
    virtual void clear() = 0;
};

// Distribution of keys across nodes for a hasher instance.
template <typename Hasher, typename Key>
typename Hasher::StatsType measure_distribution(const Hasher& hasher,
                                                const std::vector<Key>& keys) {
    using NodeId = typename Hasher::node_id_type;
    using Stats = typename Hasher::StatsType;

    std::map<NodeId, size_t> counts;
    for (const auto& k : keys) {
        counts[hasher.lookup(k)]++;
    }

    const size_t total = keys.size();
    const size_t nodes_used = counts.size();
    const size_t divisor = nodes_used == 0 ? 1 : nodes_used;
    const double mean = static_cast<double>(total) / static_cast<double>(divisor);

    double sq = 0.0;
    double mn = 0.0;
    double mx = 0.0;
    bool first = true;
    for (const auto& [node, c] : counts) {
        const double v = static_cast<double>(c);
        const double d = v - mean;
        sq += d * d;
        if (first) { mn = v; mx = v; first = false; }
        else { if (v < mn) mn = v; if (v > mx) mx = v; }
    }

    Stats s;
    s.total_keys = total;
    s.unique_nodes_used = nodes_used;
    s.mean_keys_per_node = mean;
    s.stddev_keys_per_node =
        nodes_used == 0 ? 0.0 : std::sqrt(sq / static_cast<double>(divisor));
    s.skew = mean > 0.0 ? s.stddev_keys_per_node / mean : 0.0;
    s.max_load = mx;
    s.min_load = mn;
    return s;
}

}  // namespace consistent_hash
