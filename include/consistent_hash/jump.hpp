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

// Jump consistent hashing (Google). O(log n) lookup, O(1) memory, no ring.
// Buckets are node indices; buckets map to nodes in insertion order.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class JumpHasher : public ConsistentHasher<NodeId, Key, Hasher> {
public:
  using Base = ConsistentHasher<NodeId, Key, Hasher>;
  using NodeType = typename Base::NodeType;
  using MigrationType = typename Base::MigrationType;
  using StatsType = typename Base::StatsType;
  using Result = typename Base::Result;
  using ErrorCode = typename Base::ErrorCode;

  JumpHasher() : hasher_{} {
    probe_keys_.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
      probe_keys_.push_back(Key("probe:" + std::to_string(i)));
    }
  }

  void add_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = index_by_id_.find(node.id);
    if (it != index_by_id_.end()) {
      nodes_[it->second] = node; // update in place
      return;
    }
    index_by_id_[node.id] = nodes_.size();
    nodes_.push_back(node);
  }

  bool upsert_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = index_by_id_.find(node.id);
    if (it != index_by_id_.end()) {
      nodes_[it->second] = node;
      return false;
    }
    index_by_id_[node.id] = nodes_.size();
    nodes_.push_back(node);
    return true;
  }

  void remove_node(const NodeId &id) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = index_by_id_.find(id);
    if (it == index_by_id_.end())
      return;
    const size_t pos = it->second;
    const size_t last = nodes_.size() - 1;
    if (pos != last) {
      nodes_[pos] = nodes_[last];
      index_by_id_[nodes_[pos].id] = pos;
    }
    nodes_.pop_back();
    index_by_id_.erase(it);
  }

  Result update_weight(const NodeId &id, uint32_t weight) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = index_by_id_.find(id);
    if (it == index_by_id_.end()) {
      return Result{ErrorCode::NodeNotFound, "Node not found: " + id};
    }
    if (weight == 0 || weight > 1000) {
      return Result{ErrorCode::InvalidWeight, "Weight must be 1..1000"};
    }
    nodes_[it->second].weight = weight;
    return Result{};
  }

  bool has_node(const NodeId &id) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_by_id_.find(id) != index_by_id_.end();
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
    throw std::runtime_error("JumpHasher: no nodes available");
  }

  std::vector<NodeId> lookup_n(const Key &key, size_t n) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<NodeId> result;
    if (nodes_.empty() || n == 0)
      return result;
    const size_t total = nodes_.size();
    const size_t want = std::min(n, total);
    const size_t b = jump_bucket(hasher_(key), total);
    std::set<NodeId> seen;
    result.reserve(want);
    for (size_t i = 0; i < total && seen.size() < want; ++i) {
      const NodeId id = nodes_[(b + i) % total].id;
      if (seen.insert(id).second)
        result.push_back(id);
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

  StatsType stats() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return compute_stats_locked();
  }

  size_t node_count() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.size();
  }

  void clear() override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    nodes_.clear();
    index_by_id_.clear();
    last_assignment_.clear();
    metrics_.clear();
  }

  std::string serialize() const override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::string result = "{";
    result += "\"nodes\":[";
    bool first = true;
    for (size_t i = 0; i < nodes_.size(); ++i) {
      if (!first)
        result += ",";
      first = false;
      const auto &node = nodes_[i];
      result += "{\"id\":\"" + node.id + "\",";
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
  static int64_t jump_bucket(uint64_t key, int32_t num_buckets) {
    int64_t b = -1;
    int64_t j = 0;
    while (j < num_buckets) {
      b = j;
      key = key * 2862933555777941757ULL + 1ULL;
      j = static_cast<int64_t>((static_cast<double>(b + 1) * (1LL << 31)) /
                               (static_cast<double>((key >> 33) + 1ULL)));
    }
    return b;
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
  std::optional<NodeId> lookup_opt_unlocked(const Key &k) const {
    if (nodes_.empty())
      return std::nullopt;
    return nodes_[jump_bucket(hasher_(k), nodes_.size())].id;
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

  std::vector<NodeType> nodes_;
  std::map<NodeId, size_t> index_by_id_;
  std::map<Key, NodeId> last_assignment_;
  std::vector<Key> probe_keys_;
  Metrics metrics_;
  mutable std::shared_mutex mutex_;
  Hasher hasher_;
};

} // namespace consistent_hash