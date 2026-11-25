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

namespace consistent_hash {

// Rendezvous / highest random weight (HRW) hashing.
// score(key, node) = hash(key || node); owner = argmax score.
// O(n) lookup, no state beyond the node list. Best for small clusters.
template <typename NodeId = std::string, typename Key = std::string>
class RendezvousHasher : public ConsistentHasher<NodeId, Key> {
public:
    using Base = ConsistentHasher<NodeId, Key>;
    using NodeType = typename Base::NodeType;
    using MigrationType = typename Base::MigrationType;
    using StatsType = typename Base::StatsType;

    RendezvousHasher() = default;

    void add_node(const NodeType& node) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = index_by_id_.find(node.id);
        if (it != index_by_id_.end()) {
            nodes_[it->second] = node;
            return;
        }
        index_by_id_[node.id] = nodes_.size();
        nodes_.push_back(node);
    }

    void remove_node(const NodeId& id) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = index_by_id_.find(id);
        if (it == index_by_id_.end()) return;
        const size_t pos = it->second;
        const size_t last = nodes_.size() - 1;
        if (pos != last) {
            nodes_[pos] = nodes_[last];
            index_by_id_[nodes_[pos].id] = pos;
        }
        nodes_.pop_back();
        index_by_id_.erase(it);
    }

    bool has_node(const NodeId& id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return index_by_id_.find(id) != index_by_id_.end();
    }

    NodeId lookup(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (nodes_.empty()) throw std::runtime_error("RendezvousHasher: no nodes");
        const uint64_t kh = hasher_(key);
        const NodeType* best = nullptr;
        uint64_t best_score = 0;
        bool first = true;
        for (const auto& node : nodes_) {
            if (node.status != NodeStatus::Active) continue;
            const uint64_t score = detail::hash_combine(kh,
                detail::fnv1a_64(std::string(node.id)));
            if (first || score > best_score) {
                best_score = score;
                best = &node;
                first = false;
            }
        }
        if (!best) throw std::runtime_error("RendezvousHasher: no active nodes");
        return best->id;
    }

    std::vector<NodeId> lookup_n(const Key& key, size_t n) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<NodeId> result;
        if (nodes_.empty() || n == 0) return result;
        const size_t total = nodes_.size();
        const size_t want = std::min(n, total);
        // Score all active nodes and take the top `want`.
        struct Scored { NodeId id; uint64_t score; };
        std::vector<Scored> scored;
        scored.reserve(total);
        const uint64_t kh = hasher_(key);
        for (const auto& node : nodes_) {
            if (node.status != NodeStatus::Active) continue;
            scored.push_back(Scored{node.id,
                detail::hash_combine(kh, detail::fnv1a_64(std::string(node.id)))});
        }
        std::sort(scored.begin(), scored.end(),
                  [](const Scored& a, const Scored& b) { return a.score > b.score; });
        for (size_t i = 0; i < want && i < scored.size(); ++i) {
            result.push_back(scored[i].id);
        }
        return result;
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

private:
    std::vector<MigrationType> rebalance_locked() {
        std::vector<MigrationType> migrations;
        migrations.reserve(probe_keys_.size());
        std::map<Key, NodeId> current;
        for (const auto& k : probe_keys_) {
            try { current[k] = lookup_locked_unlocked(k); } catch (...) {}
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

    NodeId lookup_locked_unlocked(const Key& k) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (nodes_.empty()) throw std::runtime_error("empty");
        const uint64_t kh = hasher_(k);
        const NodeType* best = nullptr;
        uint64_t best_score = 0;
        bool first = true;
        for (const auto& node : nodes_) {
            if (node.status != NodeStatus::Active) continue;
            const uint64_t score = detail::hash_combine(kh,
                detail::fnv1a_64(std::string(node.id)));
            if (first || score > best_score) {
                best_score = score; best = &node; first = false;
            }
        }
        if (!best) throw std::runtime_error("no active nodes");
        return best->id;
    }

    StatsType compute_stats_locked() const {
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
        } catch (...) {}
        return s;
    }

    std::vector<NodeType> nodes_;
    std::map<NodeId, size_t> index_by_id_;
    std::map<Key, NodeId> last_assignment_;
    std::vector<Key> probe_keys_;
    Metrics metrics_;
    mutable std::shared_mutex mutex_;
    DefaultHasher<Key> hasher_;
};

}  // namespace consistent_hash
