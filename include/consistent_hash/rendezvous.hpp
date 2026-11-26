#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "consistent_hash/consistent_hash.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/metrics.hpp"

namespace consistent_hash {

// Rendezvous (HRW) consistent hashing. Score(key, node) = hash(key ⊕ node_id).
// Select node with max score. O(n) lookup, no state beyond node list.
// Best for: small clusters, simplicity.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class RendezvousHasher : public ConsistentHasher<NodeId, Key, Hasher> {
public:
  using Base = ConsistentHasher<NodeId, Key, Hasher>;
  using NodeType = typename Base::NodeType;
  using MigrationType = typename Base::MigrationType;
  using StatsType = typename Base::StatsType;
  using Result = typename Base::Result;
  using ErrorCode = typename Base::ErrorCode;

  RendezvousHasher() : hasher_{} {
    probe_keys_.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
      probe_keys_.push_back(Key("probe:" + std::to_string(i)));
    }
  }

  void add_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    upsert_node_unlocked(node);
  }

  bool upsert_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return upsert_node_unlocked(node);
  }

  void remove_node(const NodeId &id) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    nodes_.erase(id);
  }

  Result update_weight(const NodeId &id, uint32_t weight) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
      return Result{ErrorCode::NodeNotFound, "Node not found: " + id};
    }
    if (weight == 0 || weight > 1000) {
      return Result{ErrorCode::InvalidWeight, "Weight must be 1..1000"};
    }
    it->second.weight = weight;
    return Result{};
  }

  bool has_node(const NodeId &id) const noexcept override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.find(id) != nodes_.end();
  }

  std::optional<NodeId> lookup(const Key &key) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return lookup_opt_unlocked(key);
  }

  NodeId lookup_or_throw(const Key &key) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (auto opt = lookup_opt_unlocked(key); opt) {
      return *opt;
    }
    throw std::runtime_error("RendezvousHasher: no active nodes");
  }

  std::vector<NodeId> lookup_n(const Key &key, size_t n) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<NodeId> result;
    if (nodes_.empty() || n == 0)
      return result;
    const size_t total = nodes_.size();
    const size_t want = std::min(n, total);

    std::vector<std::pair<uint64_t, NodeId>> scores;
    scores.reserve(nodes_.size());
    const uint64_t key_hash = hasher_(key);
    for (const auto &[id, node] : nodes_) {
      if (node.status != NodeStatus::Active)
        continue;
      const uint64_t node_hash = detail::mix64(detail::fnv1a_64(id));
      const uint64_t score = detail::mix64(key_hash ^ node_hash);
      scores.emplace_back(score, id);
    }
    if (scores.empty())
      return result;
    std::partial_sort(
        scores.begin(), scores.begin() + static_cast<long>(want), scores.end(),
        [](const auto &a, const auto &b) { return a.first > b.first; });
    result.reserve(want);
    for (size_t i = 0; i < want; ++i) {
      result.push_back(scores[i].second);
    }
    return result;
  }

  std::vector<std::optional<NodeId>>
  lookup_bulk(const std::vector<Key> &keys) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::optional<NodeId>> results;
    results.reserve(keys.size());
    for (const auto &k : keys) {
      results.push_back(lookup_opt_unlocked(k));
    }
    return results;
  }

  std::vector<MigrationType> rebalance() override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return rebalance_locked();
  }

  StatsType stats() const noexcept override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return compute_stats_locked();
  }

  size_t node_count() const noexcept override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.size();
  }

  void clear() noexcept override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    nodes_.clear();
    last_assignment_.clear();
    metrics_.clear();
  }

  std::string serialize() const override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::string result = "{";
    result += "\"nodes\":[";
    bool first = true;
    for (const auto &[id, node] : nodes_) {
      if (!first)
        result += ",";
      first = false;
      result += "{\"id\":\"" + id + "\",";
      result += "\"address\":\"" + node.address + "\",";
      result += "\"weight\":" + std::to_string(node.weight) + ",";
      result += "\"zone\":\"" + node.zone + "\",";
      result +=
          "\"status\":" + std::to_string(static_cast<int>(node.status)) + "}";
    }
    result += "]}";
    return result;
  }

  Result deserialize([[maybe_unused]] std::string_view data) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    clear();
    return Result{ErrorCode::InvalidArgument,
                  "Deserialization not fully implemented; use JSON library"};
  }

private:
  // Caller must hold the mutex.
  bool upsert_node_unlocked(const NodeType &node) {
    const bool is_new = nodes_.find(node.id) == nodes_.end();
    nodes_[node.id] = node;
    return is_new;
  }

  std::vector<MigrationType> rebalance_locked() {
    std::vector<MigrationType> migrations;
    migrations.reserve(probe_keys_.size());
    std::map<Key, NodeId> current;
    for (const auto &k : probe_keys_) {
      if (auto opt = lookup_opt_unlocked(k); opt) {
        current[k] = *opt;
      }
    }
    for (const auto &[k, node] : current) {
      auto pit = last_assignment_.find(k);
      const NodeId from =
          (pit == last_assignment_.end()) ? NodeId() : pit->second;
      if (from != node) {
        migrations.push_back(MigrationType{from, node, k});
      }
    }
    last_assignment_ = std::move(current);
    metrics_.record_migrations(static_cast<uint64_t>(migrations.size()));
    return migrations;
  }

  // Caller must hold the mutex.
  std::optional<NodeId> lookup_opt_unlocked(const Key &key) const {
    if (nodes_.empty())
      return std::nullopt;

    std::optional<NodeId> best_id;
    uint64_t best_score = 0;
    bool first = true;
    const uint64_t key_hash = hasher_(key);

    for (const auto &[id, node] : nodes_) {
      if (node.status != NodeStatus::Active)
        continue;
      const uint64_t node_hash = detail::mix64(detail::fnv1a_64(id));
      const uint64_t score = detail::mix64(key_hash ^ node_hash);
      if (first || score > best_score) {
        best_score = score;
        best_id = id;
        first = false;
      }
    }
    return best_id;
  }

  StatsType compute_stats_locked() const {
    StatsType s;
    auto snap = metrics_.snapshot();
    s.migration_count = snap.migration_count;
    s.avg_lookup_ns = snap.avg_lookup_ns;
    s.p50_lookup_ns = snap.p50_lookup_ns;
    s.p90_lookup_ns = snap.p90_lookup_ns;
    s.p99_lookup_ns = snap.p99_lookup_ns;
    s.p999_lookup_ns = snap.p999_lookup_ns;
    if (nodes_.empty())
      return s;
    try {
      auto dist = measure_distribution(*this, probe_keys_);
      s.total_keys = dist.total_keys;
      s.unique_nodes_used = dist.unique_nodes_used;
      s.mean_keys_per_node = dist.mean_keys_per_node;
      s.stddev_keys_per_node = dist.stddev_keys_per_node;
      s.skew = dist.skew;
      s.max_load = dist.max_load;
      s.min_load = dist.min_load;
    } catch (...) {
    }
    return s;
  }

  std::map<NodeId, NodeType> nodes_;
  std::map<Key, NodeId> last_assignment_;
  std::vector<Key> probe_keys_;
  Metrics metrics_;
  mutable std::shared_mutex mutex_;
  Hasher hasher_;
};

} // namespace consistent_hash