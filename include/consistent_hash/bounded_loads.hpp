#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "consistent_hash/consistent_hash.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/metrics.hpp"
#include "consistent_hash/ring.hpp"

namespace consistent_hash {

// Bounded-load consistent hashing. Wraps a ring; each node has a capacity.
// If the natural owner is at capacity, the next available node in ring order
// is used. O(log n) with capacity tracking. Best for overload prevention.
template <NodeIdType NodeId = std::string, KeyType Key = std::string,
          KeyHasher<Key> Hasher = DefaultHasher<Key>>
class BoundedLoadsHasher : public ConsistentHasher<NodeId, Key, Hasher> {
public:
    using Base = ConsistentHasher<NodeId, Key, Hasher>;
    using NodeType = typename Base::NodeType;
    using MigrationType = typename Base::MigrationType;
    using StatsType = typename Base::StatsType;
    using Result = typename Base::Result;
    using ErrorCode = typename Base::ErrorCode;

    BoundedLoadsHasher(size_t virtual_nodes_per_node = 150,
                       size_t default_capacity = 1000)
        : ring_(virtual_nodes_per_node),
          default_capacity_(default_capacity == 0 ? 1 : default_capacity) {}

    void add_node(const NodeType& node) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        capacities_[node.id] =
            node.metadata.count("capacity")
                ? parse_capacity(node.metadata.at("capacity"))
                : default_capacity_;
        ring_.add_node(node);
    }

    bool upsert_node(const NodeType& node) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        const bool is_new = !ring_.has_node(node.id);
        capacities_[node.id] =
            node.metadata.count("capacity")
                ? parse_capacity(node.metadata.at("capacity"))
                : default_capacity_;
        ring_.upsert_node(node);
        return is_new;
    }

    void remove_node(const NodeId& id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        capacities_.erase(id);
        load_.erase(id);
        ring_.remove_node(id);
    }

    Result update_weight(const NodeId& id, uint32_t weight) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return ring_.update_weight(id, weight);
    }

    bool has_node(const NodeId& id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.has_node(id);
    }

    std::optional<NodeId> lookup(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return lookup_opt_unlocked(key);
    }

    NodeId lookup_or_throw(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (auto opt = lookup_opt_unlocked(key); opt) {
            return *opt;
        }
        throw std::runtime_error("BoundedLoadsHasher: no nodes available");
    }

    std::vector<NodeId> lookup_n(const Key& key, size_t n) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.lookup_n(key, n);
    }

    std::vector<std::optional<NodeId>> lookup_bulk(
        const std::vector<Key>& keys) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::optional<NodeId>> results;
        results.reserve(keys.size());
        for (const auto& k : keys) {
            results.push_back(lookup_opt_unlocked(k));
        }
        return results;
    }

    std::vector<MigrationType> rebalance() override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return ring_.rebalance();
    }

    StatsType stats() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.stats();
    }

    size_t node_count() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.node_count();
    }

    void clear() override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        ring_.clear();
        capacities_.clear();
        load_.clear();
    }

    std::string serialize() const override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        std::string result = "{";
        result += "\"default_capacity\":" + std::to_string(default_capacity_) + ",";
        result += "\"ring\":" + ring_.serialize() + "}";
        return result;
    }

    Result deserialize([[maybe_unused]] std::string_view data) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        clear();
        return Result{ErrorCode::InvalidArgument,
                      "Deserialization not fully implemented; use JSON library"};
    }

    void set_capacity(const NodeId& id, size_t cap) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        capacities_[id] = cap == 0 ? 1 : cap;
    }

    size_t capacity(const NodeId& id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = capacities_.find(id);
        return it == capacities_.end() ? default_capacity_ : it->second;
    }

    const std::map<NodeId, size_t>& load() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return load_;
    }

    Result reset_load(const NodeId& id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = load_.find(id);
        if (it == load_.end()) {
            return Result{ErrorCode::NodeNotFound, "Node not found: " + id};
        }
        it->second = 0;
        return Result{};
    }

private:
    static size_t parse_capacity(const std::string& v) {
        try {
            size_t c = std::stoull(v);
            return c == 0 ? 1 : c;
        } catch (...) {
            return 1;
        }
    }

    // Caller must hold the mutex.
    std::optional<NodeId> lookup_opt_unlocked(const Key& key) const {
        if (ring_.node_count() == 0) {
            return std::nullopt;
        }
        auto owner_opt = ring_.lookup(key);
        if (!owner_opt) return std::nullopt;
        NodeId owner = *owner_opt;
        const size_t cap = capacity(owner);
        if (load_[owner] < cap) {
            load_[owner]++;
            return owner;
        }
        // Walk the ring from the owner to find the next node with free capacity.
        const auto candidates = ring_.lookup_n(key, ring_.node_count());
        for (const auto& id : candidates) {
            if (load_[id] < capacity(id)) {
                load_[id]++;
                return id;
            }
        }
        // All nodes at capacity: best effort, return the natural owner.
        load_[owner]++;
        return owner;
    }

    RingHasher<NodeId, Key, Hasher> ring_;
    mutable std::shared_mutex mutex_;
    size_t default_capacity_;
    mutable std::map<NodeId, size_t> capacities_;
    mutable std::map<NodeId, size_t> load_;
};

}  // namespace consistent_hash