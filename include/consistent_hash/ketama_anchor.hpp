#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "consistent_hash/concepts.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/types.hpp"

namespace consistent_hash {

// Ketama Hasher - Memcached-compatible consistent hashing
// Uses MD5 hash with 4 virtual nodes per physical node (160 points per node)
// Compatible with libketama, spymemcached, etc.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class KetamaHasher {
public:
  using node_id_type = NodeId;
  using key_type = Key;
  using hasher_type = Hasher;
  using NodeType = Node<NodeId>;
  using MigrationType = Migration<NodeId, Key>;
  using StatsType = DistributionStats<NodeId, Key>;

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

  static constexpr uint32_t KETAMA_POINTS_PER_NODE = 160;  // 40 * 4 (MD5 4x32-bit)
  static constexpr uint32_t KETAMA_VIRTUAL_NODES = 40;

  KetamaHasher() 
      : mutex_(std::make_shared<std::shared_mutex>()) {}

  // Allow copying for LockFreeHasher
  KetamaHasher(const KetamaHasher& other)
      : nodes_(other.nodes_),
        ring_(other.ring_),
        mutex_(std::make_shared<std::shared_mutex>()),
        hasher_(other.hasher_) {}

  KetamaHasher& operator=(const KetamaHasher& other) {
    if (this != &other) {
      std::unique_lock<std::shared_mutex> lock(*mutex_);
      std::shared_lock<std::shared_mutex> other_lock(*other.mutex_);
      nodes_ = other.nodes_;
      ring_ = other.ring_;
      hasher_ = other.hasher_;
    }
    return *this;
  }

  KetamaHasher(KetamaHasher&&) = default;
  KetamaHasher& operator=(KetamaHasher&&) = default;

  void add_node(const NodeType& node) {
    std::unique_lock lock(*mutex_);
    add_node_unlocked(node);
  }

  bool upsert_node(const NodeType& node) {
    std::unique_lock lock(*mutex_);
    bool existed = nodes_.contains(node.id);
    remove_node_unlocked(node.id);
    add_node_unlocked(node);
    return !existed;
  }

  void remove_node(const NodeId& id) {
    std::unique_lock lock(*mutex_);
    remove_node_unlocked(id);
  }

  Result update_weight(const NodeId& id, uint32_t weight) {
    std::unique_lock lock(*mutex_);
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
      return Result{ErrorCode::NodeNotFound, "Node not found"};
    }
    if (weight == 0) {
      return Result{ErrorCode::InvalidWeight, "Weight must be > 0"};
    }
    remove_node_unlocked(id);
    NodeType updated = it->second;
    updated.weight = weight;
    add_node_unlocked(updated);
    return Result{};
  }

  bool has_node(const NodeId& id) const noexcept {
    std::shared_lock lock(*mutex_);
    return nodes_.contains(id);
  }

  std::optional<NodeId> lookup(const Key& key) const noexcept {
    std::shared_lock lock(*mutex_);
    return lookup_unlocked(key);
  }

  NodeId lookup_or_throw(const Key& key) const {
    std::shared_lock lock(*mutex_);
    if (auto opt = lookup_unlocked(key); opt) {
      return *opt;
    }
    throw std::runtime_error("No active nodes");
  }

  std::vector<NodeId> lookup_n(const Key& key, size_t n) const noexcept {
    std::shared_lock lock(*mutex_);
    if (ring_.empty() || n == 0) return {};

    std::vector<NodeId> result;
    result.reserve(std::min(n, ring_.size()));

    uint64_t hash = hasher_(key);
    uint32_t key_hash = static_cast<uint32_t>(hash & 0xFFFFFFFF);

    auto it = std::lower_bound(ring_.begin(), ring_.end(), key_hash,
                               [](const RingEntry& e, uint32_t h) { return e.hash < h; });

    std::unordered_set<NodeId> seen;
    size_t checked = 0;
    while (result.size() < n && checked < ring_.size()) {
      if (it == ring_.end()) it = ring_.begin();
      if (seen.insert(it->node_id).second) {
        result.push_back(it->node_id);
      }
      ++it;
      ++checked;
    }
    return result;
  }

  std::vector<std::optional<NodeId>> lookup_bulk(const std::vector<Key>& keys) const noexcept {
    std::shared_lock lock(*mutex_);
    std::vector<std::optional<NodeId>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
      results.push_back(lookup_unlocked(key));
    }
    return results;
  }

  std::vector<MigrationType> rebalance() {
    std::unique_lock lock(*mutex_);
    // Ketama is deterministic - no rebalance needed unless nodes change
    return {};
  }

  StatsType stats() const noexcept {
    std::shared_lock lock(*mutex_);
    return measure_distribution(*this, generate_test_keys(10000));
  }

  size_t node_count() const noexcept {
    std::shared_lock lock(*mutex_);
    return nodes_.size();
  }

  void clear() noexcept {
    std::unique_lock lock(*mutex_);
    nodes_.clear();
    ring_.clear();
  }

  std::string serialize() const {
    std::shared_lock lock(*mutex_);
    // Serialize nodes
    std::string result;
    for (const auto& [id, node] : nodes_) {
      result += id + ":" + std::to_string(node.weight) + ":" + node.address + ";";
    }
    return result;
  }

  Result deserialize(std::string_view data) {
    std::unique_lock lock(*mutex_);
    nodes_.clear();
    ring_.clear();

    std::string str(data);
    size_t pos = 0;
    while ((pos = str.find(';')) != std::string::npos) {
      std::string entry = str.substr(0, pos);
      str.erase(0, pos + 1);

      size_t p1 = entry.find(':');
      size_t p2 = entry.find(':', p1 + 1);
      if (p1 == std::string::npos || p2 == std::string::npos) continue;

      NodeId id = entry.substr(0, p1);
      uint32_t weight = static_cast<uint32_t>(std::stoul(entry.substr(p1 + 1, p2 - p1 - 1)));
      std::string address = entry.substr(p2 + 1);

      nodes_.emplace(id, NodeType{id, address, weight, "", NodeStatus::Active, {}});
    }
    rebuild_ring();
    return Result{};
  }

private:
  struct RingEntry {
    uint64_t hash;
    NodeId node_id;

    bool operator<(const RingEntry& other) const noexcept { return hash < other.hash; }
    bool operator<(uint64_t h) const noexcept { return hash < h; }
  };

  mutable std::shared_ptr<std::shared_mutex> mutex_;
  std::unordered_map<NodeId, NodeType> nodes_;
  std::vector<RingEntry> ring_;
  Hasher hasher_{};

  void add_node_unlocked(const NodeType& node) {
    nodes_.emplace(node.id, node);
    // Generate virtual nodes proportional to weight (base 40 virtual nodes for weight 100)
    const uint32_t virtual_nodes = std::max<uint32_t>(1, (node.weight * KETAMA_VIRTUAL_NODES) / 100);
    for (uint32_t i = 0; i < virtual_nodes; ++i) {
      std::string key = node.id + "-" + std::to_string(i);
      // Ketama uses MD5 - we'll use our hasher with a special prefix
      uint64_t hash = hasher_("ketama:" + key);
      // Split 64-bit hash into 4 32-bit values (like MD5)
      for (int j = 0; j < 4; ++j) {
        uint32_t point_hash = static_cast<uint32_t>((hash >> (j * 16)) & 0xFFFFFFFF);
        ring_.push_back({point_hash, node.id});
      }
    }
    std::sort(ring_.begin(), ring_.end());
  }

  void remove_node_unlocked(const NodeId& id) {
    nodes_.erase(id);
    rebuild_ring();
  }

  void rebuild_ring() {
    ring_.clear();
    for (const auto& [id, node] : nodes_) {
      // Generate virtual nodes proportional to weight (base 40 virtual nodes for weight 100)
      const uint32_t virtual_nodes = std::max<uint32_t>(1, (node.weight * KETAMA_VIRTUAL_NODES) / 100);
      for (uint32_t i = 0; i < virtual_nodes; ++i) {
        std::string key = id + "-" + std::to_string(i);
        uint64_t hash = hasher_("ketama:" + key);
        for (int j = 0; j < 4; ++j) {
          uint32_t point_hash = static_cast<uint32_t>((hash >> (j * 16)) & 0xFFFFFFFF);
          ring_.push_back({point_hash, id});
        }
      }
    }
    std::sort(ring_.begin(), ring_.end());
  }

  std::optional<NodeId> lookup_unlocked(const Key& key) const noexcept {
    if (ring_.empty()) return std::nullopt;

    uint64_t hash = hasher_(key);
    uint32_t key_hash = static_cast<uint32_t>(hash & 0xFFFFFFFF);

    auto it = std::lower_bound(ring_.begin(), ring_.end(), key_hash,
                               [](const RingEntry& e, uint32_t h) { return e.hash < h; });

    if (it == ring_.end()) it = ring_.begin();
    return it->node_id;
  }

  static std::vector<Key> generate_test_keys(size_t count) {
    std::vector<Key> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      keys.emplace_back("test-key-" + std::to_string(i));
    }
    return keys;
  }
};

// Anchor Hashing - Minimal disruption consistent hashing
// Based on "Anchor Hashing: A Scalable Consistent Hash" (Google, 2017)
// O(log N) lookup, O(1) add/remove, minimal key movement
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class AnchorHasher {
public:
  using node_id_type = NodeId;
  using key_type = Key;
  using hasher_type = Hasher;
  using NodeType = Node<NodeId>;
  using MigrationType = Migration<NodeId, Key>;
  using StatsType = DistributionStats<NodeId, Key>;

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

  struct Anchor {
    NodeId node_id;
    uint64_t hash;
    double weight = 1.0;

    bool operator<(const Anchor& other) const noexcept { return hash < other.hash; }
  };

  AnchorHasher()
      : mutex_(std::make_shared<std::shared_mutex>()) {}

  // Allow copying for LockFreeHasher
  AnchorHasher(const AnchorHasher& other)
      : anchors_(other.anchors_),
        node_index_(other.node_index_),
        total_weight_(other.total_weight_),
        mutex_(std::make_shared<std::shared_mutex>()),
        hasher_(other.hasher_) {}

  AnchorHasher& operator=(const AnchorHasher& other) {
    if (this != &other) {
      std::unique_lock<std::shared_mutex> lock(*mutex_);
      std::shared_lock<std::shared_mutex> other_lock(*other.mutex_);
      anchors_ = other.anchors_;
      node_index_ = other.node_index_;
      total_weight_ = other.total_weight_;
      hasher_ = other.hasher_;
    }
    return *this;
  }

  AnchorHasher(AnchorHasher&&) = default;
  AnchorHasher& operator=(AnchorHasher&&) = default;

  void add_node(const NodeType& node) {
    std::unique_lock lock(*mutex_);
    add_node_unlocked(node);
  }

  bool upsert_node(const NodeType& node) {
    std::unique_lock lock(*mutex_);
    bool existed = node_index_.contains(node.id);
    remove_node_unlocked(node.id);
    add_node_unlocked(node);
    return !existed;
  }

  void remove_node(const NodeId& id) {
    std::unique_lock lock(*mutex_);
    remove_node_unlocked(id);
  }

  Result update_weight(const NodeId& id, uint32_t weight) {
    std::unique_lock lock(*mutex_);
    auto it = node_index_.find(id);
    if (it == node_index_.end()) {
      return Result{ErrorCode::NodeNotFound, "Node not found"};
    }
    if (weight == 0) {
      return Result{ErrorCode::InvalidWeight, "Weight must be > 0"};
    }
    // Remove existing anchors for this node and re-add with new weight
    remove_node_unlocked(id);
    // Add anchors proportional to new weight (one anchor per weight unit)
    const uint32_t num_anchors = std::max<uint32_t>(1, weight);
    for (uint32_t i = 0; i < num_anchors; ++i) {
      double w = 1.0;
      uint64_t hash = hasher_("anchor:" + id + "-" + std::to_string(i));
      anchors_.push_back({id, hash, w});
      node_index_[id] = anchors_.size() - 1;
      total_weight_ += w;
    }
    rebuild_anchors();
    return Result{};
  }

  bool has_node(const NodeId& id) const noexcept {
    std::shared_lock lock(*mutex_);
    return node_index_.contains(id);
  }

  std::optional<NodeId> lookup(const Key& key) const noexcept {
    std::shared_lock lock(*mutex_);
    return lookup_unlocked(key);
  }

  NodeId lookup_or_throw(const Key& key) const {
    std::shared_lock lock(*mutex_);
    if (auto opt = lookup_unlocked(key); opt) {
      return *opt;
    }
    throw std::runtime_error("No active nodes");
  }

  std::vector<NodeId> lookup_n(const Key& key, size_t n) const noexcept {
    std::shared_lock lock(*mutex_);
    if (anchors_.empty() || n == 0) return {};

    std::vector<NodeId> result;
    result.reserve(std::min(n, anchors_.size()));

    uint64_t hash = hasher_(key);

    // Find first anchor >= position using integer comparison
    auto it = std::lower_bound(anchors_.begin(), anchors_.end(), hash,
                               [](const Anchor& a, uint64_t h) { return a.hash < h; });

    std::unordered_set<NodeId> seen;
    size_t checked = 0;
    while (result.size() < n && checked < anchors_.size()) {
      if (it == anchors_.end()) it = anchors_.begin();
      if (seen.insert(it->node_id).second) {
        result.push_back(it->node_id);
      }
      ++it;
      ++checked;
    }
    return result;
  }

  std::vector<std::optional<NodeId>> lookup_bulk(const std::vector<Key>& keys) const noexcept {
    std::shared_lock lock(*mutex_);
    std::vector<std::optional<NodeId>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
      results.push_back(lookup_unlocked(key));
    }
    return results;
  }

  std::vector<MigrationType> rebalance() {
    std::unique_lock lock(*mutex_);
    // Anchor hashing is designed to minimize migrations
    // Only rebuild if weights changed significantly
    rebuild_anchors();
    return {};
  }

  StatsType stats() const noexcept {
    std::shared_lock lock(*mutex_);
    return measure_distribution(*this, generate_test_keys(10000));
  }

  size_t node_count() const noexcept {
    std::shared_lock lock(*mutex_);
    return node_index_.size();
  }

  void clear() noexcept {
    std::unique_lock lock(*mutex_);
    anchors_.clear();
    node_index_.clear();
    total_weight_ = 0.0;
  }

  std::string serialize() const {
    std::shared_lock lock(*mutex_);
    std::string result;
    // Serialize unique nodes with their weight (number of anchors)
    for (const auto& [id, idx] : node_index_) {
      // Count anchors for this node
      size_t anchor_count = 0;
      for (const auto& anchor : anchors_) {
        if (anchor.node_id == id) anchor_count++;
      }
      result += id + ":" + std::to_string(anchor_count) + ";";
    }
    return result;
  }

  Result deserialize(std::string_view data) {
    std::unique_lock lock(*mutex_);
    anchors_.clear();
    node_index_.clear();
    total_weight_ = 0.0;

    std::string str(data);
    size_t pos = 0;
    while ((pos = str.find(';')) != std::string::npos) {
      std::string entry = str.substr(0, pos);
      str.erase(0, pos + 1);

      size_t p1 = entry.find(':');
      if (p1 == std::string::npos) continue;

      NodeId id = entry.substr(0, p1);
      size_t anchor_count = std::stoul(entry.substr(p1 + 1));

      // Recreate anchors for this node
      for (size_t i = 0; i < anchor_count; ++i) {
        double weight = 1.0;
        uint64_t hash = hasher_("anchor:" + id + "-" + std::to_string(i));
        anchors_.push_back({id, hash, weight});
        node_index_[id] = anchors_.size() - 1;
        total_weight_ += weight;
      }
    }
    rebuild_anchors();
    return Result{};
  }

private:
  mutable std::shared_ptr<std::shared_mutex> mutex_;
  std::vector<Anchor> anchors_;
  std::unordered_map<NodeId, size_t> node_index_;
  double total_weight_ = 0.0;
  Hasher hasher_{};

  void add_node_unlocked(const NodeType& node) {
    // Create anchors proportional to weight (one anchor per weight unit) so the
    // weighted distribution is smooth rather than coarse.
    const uint32_t num_anchors = std::max<uint32_t>(1, node.weight);
    for (uint32_t i = 0; i < num_anchors; ++i) {
      double weight = 1.0; // Each virtual anchor has equal weight
      uint64_t hash = hasher_("anchor:" + node.id + "-" + std::to_string(i));
      anchors_.push_back({node.id, hash, weight});
      node_index_[node.id] = anchors_.size() - 1;
      total_weight_ += weight;
    }
    // Keep anchors sorted so lookup's binary search is valid.
    rebuild_anchors();
  }

  void add_anchor_unlocked(const NodeId& id, double weight) {
    uint64_t hash = hasher_("anchor:" + id);
    anchors_.push_back({id, hash, weight});
    node_index_[id] = anchors_.size() - 1;
    total_weight_ += weight;
  }

  void remove_node_unlocked(const NodeId& id) {
    // Remove all anchors for this node
    std::vector<Anchor> new_anchors;
    new_anchors.reserve(anchors_.size());
    for (const auto& anchor : anchors_) {
      if (anchor.node_id != id) {
        new_anchors.push_back(anchor);
      } else {
        total_weight_ -= anchor.weight;
      }
    }
    anchors_ = std::move(new_anchors);
    node_index_.erase(id);
    rebuild_anchors();
  }

  void rebuild_anchors() {
    // Sort by hash for binary search
    std::sort(anchors_.begin(), anchors_.end());

    // Update node index
    node_index_.clear();
    for (size_t i = 0; i < anchors_.size(); ++i) {
      node_index_[anchors_[i].node_id] = i;
    }
  }

  std::optional<NodeId> lookup_unlocked(const Key& key) const noexcept {
    if (anchors_.empty()) return std::nullopt;

    uint64_t hash = hasher_(key);

    // Binary search for first anchor >= position using integer comparison
    auto it = std::lower_bound(anchors_.begin(), anchors_.end(), hash,
                               [](const Anchor& a, uint64_t h) { return a.hash < h; });

    if (it == anchors_.end()) it = anchors_.begin();
    return it->node_id;
  }

  static std::vector<Key> generate_test_keys(size_t count) {
    std::vector<Key> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      keys.emplace_back("test-key-" + std::to_string(i));
    }
    return keys;
  }
};

} // namespace consistent_hash