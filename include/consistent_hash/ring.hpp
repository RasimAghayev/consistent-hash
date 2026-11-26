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

// Ring consistent hashing with virtual nodes, optional weighting and
// zone-aware placement. Lookup is O(log V) via binary search on the sorted
// virtual-node ring.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class RingHasher : public ConsistentHasher<NodeId, Key, Hasher> {
public:
  using Base = ConsistentHasher<NodeId, Key, Hasher>;
  using NodeType = typename Base::NodeType;
  using MigrationType = typename Base::MigrationType;
  using StatsType = typename Base::StatsType;
  using Result = typename Base::Result;
  using ErrorCode = typename Base::ErrorCode;

  explicit RingHasher(size_t virtual_nodes_per_node = 150)
      : vnodes_per_node_(virtual_nodes_per_node == 0 ? 1
                                                     : virtual_nodes_per_node),
        hasher_{} {
    probe_keys_.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
      probe_keys_.push_back(Key("probe:" + std::to_string(i)));
    }
  }

  void add_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    add_node_unlocked(node);
  }

  bool upsert_node(const NodeType &node) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const bool is_new = nodes_.find(node.id) == nodes_.end();
    add_node_unlocked(node);
    return is_new;
  }

  void remove_node(const NodeId &id) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    remove_node_locked(id);
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
    rebuild_ring_unlocked();
    return Result{};
  }

  bool has_node(const NodeId &id) const override {
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
    throw std::runtime_error("RingHasher: no active nodes available");
  }

  std::vector<NodeId> lookup_n(const Key &key, size_t n) const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return lookup_n_unlocked(key, n);
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
    return rebalance_unlocked();
  }

  StatsType stats() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return compute_stats_unlocked();
  }

  size_t node_count() const override {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return nodes_.size();
  }

  void clear() override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    ring_.clear();
    nodes_.clear();
    last_assignment_.clear();
    metrics_.clear();
  }

  std::string serialize() const override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    // Simple JSON-like serialization
    std::string result = "{";
    result += "\"vnodes_per_node\":" + std::to_string(vnodes_per_node_) + ",";
    result += "\"preferred_zone\":\"" + preferred_zone_ + "\",";
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
    // Simple parser - in production use a proper JSON library
    std::unique_lock<std::shared_mutex> lock(mutex_);
    clear();
    // For now just return unsupported
    return Result{ErrorCode::InvalidArgument,
                  "Deserialization not fully implemented; use JSON library"};
  }

  // Zone-aware: when set, lookup prefers a node in this zone.
  void preferred_zone(const std::string &zone) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    preferred_zone_ = zone;
  }

private:
  struct Entry {
    uint64_t hash;
    NodeId id;
  };

  // Caller must hold the mutex.
  void add_node_unlocked(const NodeType &node) {
    remove_node_locked(node.id); // replace if re-added
    nodes_[node.id] = node;
    rebuild_ring_unlocked();
  }

  void rebuild_ring_unlocked() {
    ring_.clear();
    for (const auto &[id, node] : nodes_) {
      const size_t count =
          std::max<size_t>(1, (vnodes_per_node_ * node.weight) / 100);
      const uint64_t base = detail::fnv1a_64(node.id);
      uint64_t vh = base;
      for (size_t i = 0; i < count; ++i) {
        vh = hash_combine(vh, static_cast<uint64_t>(i));
        ring_.push_back(Entry{vh, node.id});
      }
    }
    std::sort(ring_.begin(), ring_.end(),
              [](const Entry &a, const Entry &b) { return a.hash < b.hash; });
  }

  // Caller must hold the mutex.
  void remove_node_locked(const NodeId &id) {
    nodes_.erase(id);
    ring_.erase(std::remove_if(ring_.begin(), ring_.end(),
                               [&id](const Entry &e) { return e.id == id; }),
                ring_.end());
  }

  // Caller must hold the mutex.
  std::optional<NodeId> lookup_opt_unlocked(const Key &key) const {
    if (ring_.empty()) {
      return std::nullopt;
    }
    const uint64_t h = hasher_(key);
    auto it =
        std::lower_bound(ring_.begin(), ring_.end(), h,
                         [](const Entry &e, uint64_t v) { return e.hash < v; });
    if (it == ring_.end())
      it = ring_.begin();

    if (!preferred_zone_.empty()) {
      const size_t start = static_cast<size_t>(it - ring_.begin());
      const size_t m = ring_.size();
      for (size_t i = 0; i < m; ++i) {
        const Entry &e = ring_[(start + i) % m];
        auto nit = nodes_.find(e.id);
        if (nit != nodes_.end() && nit->second.zone == preferred_zone_ &&
            nit->second.status == NodeStatus::Active) {
          return e.id;
        }
      }
    }

    const size_t m = ring_.size();
    const size_t start = static_cast<size_t>(it - ring_.begin());
    for (size_t i = 0; i < m; ++i) {
      const Entry &e = ring_[(start + i) % m];
      auto nit = nodes_.find(e.id);
      if (nit != nodes_.end() && nit->second.status == NodeStatus::Active) {
        return e.id;
      }
    }
    return std::nullopt;
  }

  // Caller must hold the mutex.
  std::vector<NodeId> lookup_n_unlocked(const Key &key, size_t n) const {
    std::vector<NodeId> result;
    if (ring_.empty() || n == 0)
      return result;
    const size_t total = nodes_.size();
    const size_t want = std::min(n, total);
    const uint64_t h = hasher_(key);
    auto it =
        std::lower_bound(ring_.begin(), ring_.end(), h,
                         [](const Entry &e, uint64_t v) { return e.hash < v; });
    if (it == ring_.end())
      it = ring_.begin();
    const size_t m = ring_.size();
    const size_t start = static_cast<size_t>(it - ring_.begin());
    std::set<NodeId> seen;
    result.reserve(want);
    for (size_t i = 0; i < m && seen.size() < want; ++i) {
      const Entry &e = ring_[(start + i) % m];
      auto nit = nodes_.find(e.id);
      if (nit != nodes_.end() && nit->second.status == NodeStatus::Active &&
          seen.insert(e.id).second) {
        result.push_back(e.id);
      }
    }
    return result;
  }

  // Caller must hold the mutex.
  std::vector<MigrationType> rebalance_unlocked() {
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

  StatsType compute_stats_unlocked() const {
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

  std::vector<Entry> ring_;
  std::map<NodeId, NodeType> nodes_;
  std::map<Key, NodeId> last_assignment_;
  std::vector<Key> probe_keys_;
  size_t vnodes_per_node_;
  std::string preferred_zone_;
  Metrics metrics_;
  mutable std::shared_mutex mutex_;
  Hasher hasher_;
};

} // namespace consistent_hash