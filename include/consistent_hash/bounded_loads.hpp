#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "consistent_hash/consistent_hash.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/metrics.hpp"
#include "consistent_hash/ring.hpp"

namespace consistent_hash {

// Bounded-load consistent hashing. Wraps a ring; each node has a capacity.
// If the natural owner is at capacity, the next available node in ring order
// is used. O(log n) with capacity tracking. Best for overload prevention.
template <typename NodeId = std::string, typename Key = std::string>
class BoundedLoadsHasher : public ConsistentHasher<NodeId, Key> {
public:
    using Base = ConsistentHasher<NodeId, Key>;
    using NodeType = typename Base::NodeType;
    using MigrationType = typename Base::MigrationType;
    using StatsType = typename Base::StatsType;

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

    void remove_node(const NodeId& id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        capacities_.erase(id);
        ring_.remove_node(id);
    }

    bool has_node(const NodeId& id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.has_node(id);
    }

    NodeId lookup(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (ring_.node_count() == 0) {
            throw std::runtime_error("BoundedLoadsHasher: no nodes");
        }
        NodeId owner = ring_.lookup(key);
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

    std::vector<NodeId> lookup_n(const Key& key, size_t n) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ring_.lookup_n(key, n);
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

    void set_capacity(const NodeId& id, size_t cap) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        capacities_[id] = cap == 0 ? 1 : cap;
    }

    size_t capacity(const NodeId& id) const {
        auto it = capacities_.find(id);
        return it == capacities_.end() ? default_capacity_ : it->second;
    }

    const std::map<NodeId, size_t>& load() const { return load_; }

private:
    static size_t parse_capacity(const std::string& v) {
        try {
            size_t c = std::stoull(v);
            return c == 0 ? 1 : c;
        } catch (...) {
            return 1;
        }
    }

    mutable RingHasher<NodeId, Key> ring_;
    mutable std::shared_mutex mutex_;
    size_t default_capacity_;
    mutable std::map<NodeId, size_t> capacities_;
    mutable std::map<NodeId, size_t> load_;
};

}  // namespace consistent_hash
