#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace consistent_hash {

enum class NodeStatus : uint8_t {
    Active = 0,
    Draining = 1,
    Removed = 2
};

template <typename NodeId = std::string>
struct Node {
    NodeId id;
    std::string address;
    uint32_t weight = 100;          // Weighted hashing (1..1000)
    std::string zone;               // For zone-aware placement
    NodeStatus status = NodeStatus::Active;
    std::map<std::string, std::string> metadata;
};

template <typename NodeId = std::string, typename Key = std::string>
struct Migration {
    NodeId from_node;
    NodeId to_node;
    Key key;
};

template <typename NodeId = std::string, typename Key = std::string>
struct DistributionStats {
    size_t total_keys = 0;
    size_t unique_nodes_used = 0;
    double mean_keys_per_node = 0.0;
    double stddev_keys_per_node = 0.0;
    double skew = 0.0;              // stddev / mean (0 = perfectly uniform)
    double max_load = 0.0;
    double min_load = 0.0;
    uint64_t migration_count = 0;
    double avg_lookup_ns = 0.0;
    double p50_lookup_ns = 0.0;
    double p90_lookup_ns = 0.0;
    double p99_lookup_ns = 0.0;
    double p999_lookup_ns = 0.0;
};

}  // namespace consistent_hash
