#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "consistent_hash/concepts.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/types.hpp"
#include "consistent_hash/observability.hpp"
#include "consistent_hash/lockfree.hpp"
#include "consistent_hash/ketama_anchor.hpp"
#include "consistent_hash/health.hpp"

namespace consistent_hash {

// Abstract interface for every consistent hashing algorithm.
// NodeId and Key default to std::string but may be any copyable, hashable type.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class ConsistentHasher {
public:
  // Error codes for operations that can fail.
  enum class ErrorCode {
    None = 0,
    EmptyRing,
    NoActiveNodes,
    NodeNotFound,
    InvalidWeight,
    InvalidArgument
  };

  struct Result {
    ErrorCode code = ErrorCode::None;
    std::string message;

    explicit operator bool() const noexcept { return code == ErrorCode::None; }
  };

  using node_id_type = NodeId;
  using key_type = Key;
  using hasher_type = Hasher;
  using NodeType = Node<NodeId>;
  using MigrationType = Migration<NodeId, Key>;
  using StatsType = DistributionStats<NodeId, Key>;

  virtual ~ConsistentHasher() = default;

  // Add a node (thread-safe). Weight scales virtual-node count where
  // applicable.
  virtual void add_node(const NodeType &node) = 0;
  // Add or update a node (thread-safe). Returns true if new, false if updated.
  virtual bool upsert_node(const NodeType &node) = 0;
  // Remove a node by id (thread-safe).
  virtual void remove_node(const NodeId &id) = 0;
  // Update node weight without removal (thread-safe).
  virtual Result update_weight(const NodeId &id, uint32_t weight) = 0;
  virtual bool has_node(const NodeId &id) const noexcept = 0;

  // Owner node for a key (thread-safe). Returns nullopt when empty/no active
  // nodes.
  virtual std::optional<NodeId> lookup(const Key &key) const = 0;
  // Owner node for a key (thread-safe). Throws on error (legacy compatibility).
  virtual NodeId lookup_or_throw(const Key &key) const = 0;
  // Up to n distinct owner nodes for a key.
  virtual std::vector<NodeId> lookup_n(const Key &key, size_t n) const = 0;
  // Bulk lookup for multiple keys (efficient for batch operations).
  virtual std::vector<std::optional<NodeId>>
  lookup_bulk(const std::vector<Key> &keys) const = 0;

  // Recompute key->node assignments vs. the last snapshot; return migrations.
  virtual std::vector<MigrationType> rebalance() = 0;
  // Distribution + latency + migration metrics.
  virtual StatsType stats() const noexcept = 0;
  virtual size_t node_count() const noexcept = 0;
  virtual void clear() noexcept = 0;

  // Serialize current state for persistence.
  virtual std::string serialize() const = 0;
  // Deserialize and restore state.
  virtual Result deserialize(std::string_view data) = 0;
};

// C++23 monadic operations for std::optional<NodeId>.
// These provide functional composition for chaining optional operations.

// and_then: chain a function that returns optional<NodeId>
template <typename NodeId, typename F>
requires std::invocable<F, NodeId>
auto and_then(std::optional<NodeId> opt, F &&f) {
  if (opt)
    return std::invoke(std::forward<F>(f), *opt);
  using ResultType = std::invoke_result_t<F, NodeId>;
  return std::optional<ResultType>{};
}

// transform: apply a function to the contained value
template <typename NodeId, typename F>
requires std::invocable<F, NodeId>
auto transform(std::optional<NodeId> opt, F &&f) {
  if (opt)
    return std::optional<std::invoke_result_t<F, NodeId>>{
        std::invoke(std::forward<F>(f), *opt)};
  return std::optional<std::invoke_result_t<F, NodeId>>{};
}

// or_else: provide fallback when optional is empty
template <typename NodeId, typename F>
requires std::invocable<F>
auto or_else(std::optional<NodeId> opt, F &&f) {
  if (opt)
    return opt;
  return std::invoke(std::forward<F>(f));
}

// Distribution of keys across nodes for a hasher instance.
template <typename Hasher, typename Key>
typename Hasher::StatsType measure_distribution(const Hasher &hasher,
                                                const std::vector<Key> &keys) {
  using NodeId = typename Hasher::node_id_type;
  using Stats = typename Hasher::StatsType;

  std::map<NodeId, size_t> counts;
  for (const auto &k : keys) {
    if (auto opt = hasher.lookup(k); opt) {
      counts[*opt]++;
    }
  }

  const size_t total = keys.size();
  const size_t nodes_used = counts.size();
  const size_t divisor = nodes_used == 0 ? 1 : nodes_used;
  const double mean = static_cast<double>(total) / static_cast<double>(divisor);

  double sq = 0.0;
  double mn = 0.0;
  double mx = 0.0;
  bool first = true;
  for (const auto &[node, c] : counts) {
    const double v = static_cast<double>(c);
    const double d = v - mean;
    sq += d * d;
    if (first) {
      mn = v;
      mx = v;
      first = false;
    } else {
      if (v < mn)
        mn = v;
      if (v > mx)
        mx = v;
    }
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

// Range-based measure_distribution (C++23 views compatible).
template <typename Hasher, RangeCompatible Range>
requires KeyType<std::ranges::range_value_t<Range>>
typename Hasher::StatsType measure_distribution_range(const Hasher &hasher,
                                                      const Range &keys) {
  using NodeId = typename Hasher::node_id_type;
  using Stats = typename Hasher::StatsType;

  std::map<NodeId, size_t> counts;
  for (const auto &k : keys) {
    if (auto opt = hasher.lookup(k); opt) {
      counts[*opt]++;
    }
  }

  const size_t total = std::ranges::distance(keys);
  const size_t nodes_used = counts.size();
  const size_t divisor = nodes_used == 0 ? 1 : nodes_used;
  const double mean = static_cast<double>(total) / static_cast<double>(divisor);

  double sq = 0.0;
  double mn = 0.0;
  double mx = 0.0;
  bool first = true;
  for (const auto &[node, c] : counts) {
    const double v = static_cast<double>(c);
    const double d = v - mean;
    sq += d * d;
    if (first) {
      mn = v;
      mx = v;
      first = false;
    } else {
      if (v < mn)
        mn = v;
      if (v > mx)
        mx = v;
    }
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

// Compute per-node load for a set of keys.
template <typename Hasher, typename Key>
std::map<typename Hasher::node_id_type, size_t>
compute_loads(const Hasher &hasher, const std::vector<Key> &keys) {
  using NodeId = typename Hasher::node_id_type;
  std::map<NodeId, size_t> loads;
  for (const auto &k : keys) {
    if (auto opt = hasher.lookup(k); opt) {
      loads[*opt]++;
    }
  }
  return loads;
}

// Range-based compute_loads.
template <typename Hasher, RangeCompatible Range>
requires KeyType<std::ranges::range_value_t<Range>>
    std::map<typename Hasher::node_id_type, size_t>
    compute_loads_range(const Hasher &hasher, const Range &keys) {
  using NodeId = typename Hasher::node_id_type;
  std::map<NodeId, size_t> loads;
  for (const auto &k : keys) {
    if (auto opt = hasher.lookup(k); opt) {
      loads[*opt]++;
    }
  }
  return loads;
}

// Find the most/least loaded nodes.
template <typename Hasher, typename Key>
std::pair<typename Hasher::node_id_type, typename Hasher::node_id_type>
find_load_extremes(const Hasher &hasher, const std::vector<Key> &keys) {
  using NodeId = typename Hasher::node_id_type;
  auto loads = compute_loads(hasher, keys);
  if (loads.empty())
    return {NodeId{}, NodeId{}};

  auto [min_it, max_it] = std::minmax_element(
      loads.begin(), loads.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });

  return {min_it->first, max_it->first};
}

// Range-based find_load_extremes.
template <typename Hasher, RangeCompatible Range>
requires KeyType<std::ranges::range_value_t<Range>>
    std::pair<typename Hasher::node_id_type, typename Hasher::node_id_type>
    find_load_extremes_range(const Hasher &hasher, const Range &keys) {
  using NodeId = typename Hasher::node_id_type;
  auto loads = compute_loads_range(hasher, keys);
  if (loads.empty())
    return {NodeId{}, NodeId{}};

  auto [min_it, max_it] = std::minmax_element(
      loads.begin(), loads.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });

  return {min_it->first, max_it->first};
}

} // namespace consistent_hash

// Factory functions for creating enhanced hashers
namespace consistent_hash {

// Create a lock-free read-optimized hasher from any hasher type
template <typename Hasher>
lockfree::LockFreeHasher<Hasher> make_lockfree(Hasher hasher) {
  return lockfree::LockFreeHasher<Hasher>(std::move(hasher));
}

// Create a health-aware hasher from any hasher type
template <typename Hasher>
health::HealthAwareHasher<Hasher> make_health_aware(Hasher hasher,
                                                     std::shared_ptr<health::HealthChecker> health) {
  return health::HealthAwareHasher<Hasher>(std::move(hasher), std::move(health));
}

// Create a health-aware lock-free hasher (both optimizations)
template <typename Hasher>
lockfree::LockFreeHasher<health::HealthAwareHasher<Hasher>>
make_lockfree_health_aware(Hasher hasher,
                           std::shared_ptr<health::HealthChecker> health) {
  return lockfree::LockFreeHasher<health::HealthAwareHasher<Hasher>>(
      health::HealthAwareHasher<Hasher>(std::move(hasher), std::move(health)));
}

} // namespace consistent_hash