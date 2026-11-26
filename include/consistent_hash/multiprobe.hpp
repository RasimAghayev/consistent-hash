#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
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

// Multi-probe hashing. Uses k independent hash functions; each probe yields a
// candidate node; the candidate with the current lowest load wins.
// O(k) lookup. Best for reducing hot-key collisions.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class MultiProbeHasher : public ConsistentHasher<NodeId, Key, Hasher> {
public:
  using Base = ConsistentHasher<NodeId, Key, Hasher>;
  using NodeType = typename Base::NodeType;
  using MigrationType = typename Base::MigrationType;
  using StatsType = typename Base::StatsType;
  using Result = typename Base::Result;
  using ErrorCode = typename Base::ErrorCode;

  static constexpr size_t kDefaultProbes = 16;

  explicit MultiProbeHasher(size_t probes = kDefaultProbes)
      : probes_(probes == 0 ? 1 : probes), hasher_{} {
    probe_keys_.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
      probe_keys_.push_back(Key("probe:" + std::to_string(i)));
    }
  }

  void add_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = index_by_id_.find(node.id);
    if (it != index_by_id_.end()) {
      nodes_[it->second] = node;
      return;
    }
    index_by_id_[node.id] = nodes_.size();
    nodes_.push_back(node);
    load_[node.id] = 0;
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
    load_[node.id] = 0;
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
      load_[nodes_[pos].id] = load_[id];
    }
    nodes_.pop_back();
    index_by_id_.erase(it);
    load_.erase(id);
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
    std::unique_lock<std::shared_mutex> lock(mutex_); // mutates load
    return lookup_opt_unlocked(key);
  }

  NodeId lookup_or_throw(const Key &key) const override {
    std::unique_lock<std::shared_mutex> lock(mutex_); // mutates load
    if (auto opt = lookup_opt_unlocked(key); opt) {
      return *opt;
    }
    throw std::runtime_error("MultiProbeHasher: no nodes available");
  }

  std::vector<NodeId> lookup_n(const Key &key, size_t n) const override {
    std::unique_lock<std::shared_mutex> lock(mutex_); // mutates load
    std::vector<NodeId> result;
    if (nodes_.empty() || n == 0)
      return result;
    const size_t total = nodes_.size();
    const size_t want = std::min(n, total);
    const uint64_t kh = hasher_(key);
    std::set<NodeId> seen;
    result.reserve(want);
    for (size_t round = 0; round < want && seen.size() < total; ++round) {
      size_t best_index = 0;
      size_t best_load = std::numeric_limits<size_t>::max();
      bool found = false;
      for (size_t p = 0; p < probes_; ++p) {
        const size_t idx = static_cast<size_t>(
            hash_combine(kh, static_cast<uint64_t>(p) +
                                 static_cast<uint64_t>(round) * 1000003ULL) %
            total);
        const NodeId &id = nodes_[idx].id;
        if (seen.count(id))
          continue;
        auto lit = load_.find(id);
        const size_t l = (lit == load_.end()) ? 0 : lit->second;
        if (l < best_load) {
          best_load = l;
          best_index = idx;
          found = true;
        }
      }
      if (!found)
        break;
      const NodeId chosen = nodes_[best_index].id;
      seen.insert(chosen);
      result.push_back(chosen);
      load_[chosen]++;
    }
    return result;
  }

  std::vector<std::optional<NodeId>>
  lookup_bulk(const std::vector<Key> &keys) const override {
    std::vector<std::optional<NodeId>> results;
    results.reserve(keys.size());
    for (const auto &k : keys) {
      results.push_back(lookup(k));
    }
    return results;
  }

  std::vector<MigrationType> rebalance() override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return rebalance_locked();
  }

  StatsType stats() const override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
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
    load_.clear();
    last_assignment_.clear();
    metrics_.clear();
  }

  std::string serialize() const override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::string result = "{";
    result += "\"probes\":" + std::to_string(probes_) + ",";
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

  const std::map<NodeId, size_t> &load() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return load_;
  }

private:
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

  // Caller must hold the mutex; mutates load_.
  std::optional<NodeId> lookup_opt_unlocked(const Key &k) const {
    if (nodes_.empty())
      return std::nullopt;
    const size_t total = nodes_.size();
    const uint64_t kh = hasher_(k);
    size_t best_index = 0;
    size_t best_load = std::numeric_limits<size_t>::max();
    for (size_t p = 0; p < probes_; ++p) {
      const size_t idx = static_cast<size_t>(
          hash_combine(kh, static_cast<uint64_t>(p)) % total);
      const NodeId &id = nodes_[idx].id;
      auto lit = load_.find(id);
      const size_t l = (lit == load_.end()) ? 0 : lit->second;
      if (l < best_load) {
        best_load = l;
        best_index = idx;
      }
    }
    const NodeId chosen = nodes_[best_index].id;
    load_[chosen] = best_load + 1;
    return chosen;
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

  size_t probes_;
  std::vector<NodeType> nodes_;
  std::map<NodeId, size_t> index_by_id_;
  mutable std::map<NodeId, size_t> load_;
  std::map<Key, NodeId> last_assignment_;
  std::vector<Key> probe_keys_;
  Metrics metrics_;
  mutable std::shared_mutex mutex_;
  Hasher hasher_;
};

} // namespace consistent_hash