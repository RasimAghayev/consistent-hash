#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "consistent_hash/consistent_hash.hpp"
#include "consistent_hash/hash.hpp"
#include "consistent_hash/metrics.hpp"

namespace consistent_hash {

// Ring consistent hashing with virtual nodes, optional weighting and
// zone-aware placement. Lookup is O(log V) via binary search on the sorted
// virtual-node ring.
template <typename NodeId = std::string, typename Key = std::string>
class RingHasher : public ConsistentHasher<NodeId, Key> {
public:
    using Base = ConsistentHasher<NodeId, Key>;
    using NodeType = typename Base::NodeType;
    using MigrationType = typename Base::MigrationType;
    using StatsType = typename Base::StatsType;

    explicit RingHasher(size_t virtual_nodes_per_node = 150)
        : vnodes_per_node_(virtual_nodes_per_node == 0 ? 1 : virtual_nodes_per_node) {
        probe_keys_.reserve(1000);
        for (size_t i = 0; i < 1000; ++i) {
            probe_keys_.push_back(Key("probe:" + std::to_string(i)));
        }
    }

    void add_node(const NodeType& node) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        add_node_unlocked(node);
    }

    void remove_node(const NodeId& id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        remove_node_locked(id);
    }

    bool has_node(const NodeId& id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return nodes_.find(id) != nodes_.end();
    }

    NodeId lookup(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return lookup_unlocked(key);
    }

    std::vector<NodeId> lookup_n(const Key& key, size_t n) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return lookup_n_unlocked(key, n);
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

    // Zone-aware: when set, lookup prefers a node in this zone.
    void preferred_zone(const std::string& zone) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        preferred_zone_ = zone;
    }

private:
    struct Entry {
        uint64_t hash;
        NodeId id;
    };

    // Caller must hold the mutex.
    void add_node_unlocked(const NodeType& node) {
        remove_node_locked(node.id);  // replace if re-added
        nodes_[node.id] = node;
        const size_t count =
            std::max<size_t>(1, (vnodes_per_node_ * node.weight) / 100);
        const uint64_t base = detail::fnv1a_64(node.id);
        uint64_t vh = base;
        for (size_t i = 0; i < count; ++i) {
            vh = hash_combine(vh, static_cast<uint64_t>(i));
            ring_.push_back(Entry{vh, node.id});
        }
        std::sort(ring_.begin(), ring_.end(),
                  [](const Entry& a, const Entry& b) { return a.hash < b.hash; });
    }

    // Caller must hold the mutex.
    void remove_node_locked(const NodeId& id) {
        nodes_.erase(id);
        ring_.erase(std::remove_if(ring_.begin(), ring_.end(),
                                   [&id](const Entry& e) { return e.id == id; }),
                    ring_.end());
    }

    // Caller must hold the mutex.
    NodeId lookup_unlocked(const Key& key) const {
        if (ring_.empty()) {
            throw std::runtime_error("RingHasher: no nodes available");
        }
        const uint64_t h = hasher_(key);
        auto it = std::lower_bound(ring_.begin(), ring_.end(), h,
                                   [](const Entry& e, uint64_t v) {
                                       return e.hash < v;
                                   });
        if (it == ring_.end()) it = ring_.begin();

        if (!preferred_zone_.empty()) {
            const size_t start = static_cast<size_t>(it - ring_.begin());
            const size_t m = ring_.size();
            for (size_t i = 0; i < m; ++i) {
                const Entry& e = ring_[(start + i) % m];
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
            const Entry& e = ring_[(start + i) % m];
            auto nit = nodes_.find(e.id);
            if (nit != nodes_.end() && nit->second.status == NodeStatus::Active) {
                return e.id;
            }
        }
        throw std::runtime_error("RingHasher: no active nodes available");
    }

    // Caller must hold the mutex.
    std::vector<NodeId> lookup_n_unlocked(const Key& key, size_t n) const {
        std::vector<NodeId> result;
        if (ring_.empty() || n == 0) return result;
        const size_t total = nodes_.size();
        const size_t want = std::min(n, total);
        const uint64_t h = hasher_(key);
        auto it = std::lower_bound(ring_.begin(), ring_.end(), h,
                                   [](const Entry& e, uint64_t v) {
                                       return e.hash < v;
                                   });
        if (it == ring_.end()) it = ring_.begin();
        const size_t m = ring_.size();
        const size_t start = static_cast<size_t>(it - ring_.begin());
        std::set<NodeId> seen;
        result.reserve(want);
        for (size_t i = 0; i < m && seen.size() < want; ++i) {
            const Entry& e = ring_[(start + i) % m];
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
        for (const auto& k : probe_keys_) {
            try {
                current[k] = lookup_unlocked(k);
            } catch (...) {
                continue;
            }
        }
        for (const auto& [k, node] : current) {
            auto pit = last_assignment_.find(k);
            const NodeId from = (pit == last_assignment_.end()) ? NodeId() : pit->second;
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
        s.migration_count = metrics_.migration_count();
        s.avg_lookup_ns = metrics_.avg_lookup_ns();
        s.p99_lookup_ns = metrics_.p99_lookup_ns();
        if (nodes_.empty()) return s;
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
    DefaultHasher<Key> hasher_;
};

}  // namespace consistent_hash
