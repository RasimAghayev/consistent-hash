#pragma once

#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <vector>

namespace consistent_hash {

enum class NodeStatus : uint8_t { Active = 0, Draining = 1, Removed = 2 };

template <typename NodeId = std::string> struct Node {
  NodeId id;
  std::string address;
  uint32_t weight = 100; // Weighted hashing (1..1000)
  std::string zone;      // For zone-aware placement
  NodeStatus status = NodeStatus::Active;
  std::map<std::string, std::string> metadata{};
};

template <typename NodeId = std::string, typename Key = std::string>
struct Migration {
  NodeId from_node;
  NodeId to_node;
  Key key;
};

struct DistributionStatsBase {
    size_t total_keys = 0;
    size_t unique_nodes_used = 0;
    double mean_keys_per_node = 0.0;
    double stddev_keys_per_node = 0.0;
    double skew = 0.0; // stddev / mean (0 = perfectly uniform)
    double max_load = 0.0;
    double min_load = 0.0;
    uint64_t migration_count = 0;
    double avg_lookup_ns = 0.0;
    double p50_lookup_ns = 0.0;
    double p90_lookup_ns = 0.0;
    double p99_lookup_ns = 0.0;
    double p999_lookup_ns = 0.0;
    virtual ~DistributionStatsBase() = default;
  };

  template <typename NodeId = std::string, typename Key = std::string>
  struct DistributionStats : public DistributionStatsBase {
  };

} // namespace consistent_hash

// std::format specializations (must be in std namespace)
template <>
struct std::formatter<consistent_hash::NodeStatus>
    : std::formatter<std::string_view> {
  auto format(consistent_hash::NodeStatus s, auto &ctx) const {
    std::string_view sv;
    switch (s) {
    case consistent_hash::NodeStatus::Active:
      sv = "Active";
      break;
    case consistent_hash::NodeStatus::Draining:
      sv = "Draining";
      break;
    case consistent_hash::NodeStatus::Removed:
      sv = "Removed";
      break;
    }
    return std::formatter<std::string_view>::format(sv, ctx);
  }
};

template <typename NodeId>
struct std::formatter<consistent_hash::Node<NodeId>>
    : std::formatter<std::string> {
  template <typename FormatContext>
  auto format(const consistent_hash::Node<NodeId> &n,
              FormatContext &ctx) const {
    return std::format_to(
        ctx.out(),
        "Node{{id={}, address={}, weight={}, zone={}, status={}, metadata={}}}",
        n.id, n.address, n.weight, n.zone, n.status, n.metadata);
  }
};

template <typename NodeId, typename Key>
struct std::formatter<consistent_hash::Migration<NodeId, Key>>
    : std::formatter<std::string> {
  template <typename FormatContext>
  auto format(const consistent_hash::Migration<NodeId, Key> &m,
              FormatContext &ctx) const {
    return std::format_to(ctx.out(),
                          "Migration{{from={}, to={}, key={}}}",
                          m.from_node, m.to_node, m.key);
  }
};

template <typename NodeId, typename Key>
struct std::formatter<consistent_hash::DistributionStats<NodeId, Key>>
    : std::formatter<std::string> {
  template <typename FormatContext>
  auto format(const consistent_hash::DistributionStats<NodeId, Key> &s,
              FormatContext &ctx) const {
    return std::format_to(
        ctx.out(),
        "DistributionStats{{total_keys={}, unique_nodes={}, mean={:.2f}, "
        "stddev={:.2f}, "
        "skew={:.4f}, max_load={:.2f}, min_load={:.2f}, migrations={}, "
        "avg_ns={:.2f}, p50={:.2f}, p90={:.2f}, p99={:.2f}, p999={:.2f}}",
        s.total_keys, s.unique_nodes_used, s.mean_keys_per_node,
        s.stddev_keys_per_node, s.skew, s.max_load, s.min_load,
        s.migration_count, s.avg_lookup_ns, s.p50_lookup_ns,
        s.p90_lookup_ns, s.p99_lookup_ns, s.p999_lookup_ns);
  }
};