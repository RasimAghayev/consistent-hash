#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// Maglev consistent hashing. Fixed-size lookup table (default M = 65537,
// prime). O(1) lookup, O(M * n) table build. Best for load balancers.
template <typename NodeId = std::string, typename Key = std::string>
class MaglevHasher : public ConsistentHasher<NodeId, Key> {
public:
    using Base = ConsistentHasher<NodeId, Key>;
    using NodeType = typename Base::NodeType;
    using MigrationType = typename Base::MigrationType;
    using StatsType = typename Base::StatsType;

    static constexpr size_t kDefaultTableSize = 65537;

    explicit MaglevHasher(size_t table_size = kDefaultTableSize)
        : table_size_(table_size == 0 ? kDefaultTableSize : table_size),
          empty_slot_(std::numeric_limits<size_t>::max()) {
        probe_keys_.reserve(1000);
        for (size_t i = 0; i < 1000; ++i) {
            probe_keys_.push_back(Key("probe:" + std::to_string(i)));
        }
        rebuild_table_locked();
    }

    void add_node(const NodeType& node) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = index_by_id_.find(node.id);
        if (it != index_by_id_.end()) {
            nodes_[it->second] = node;
            rebuild_table_locked();
            return;
        }
        index_by_id_[node.id] = nodes_.size();
        nodes_.push_back(node);
        rebuild_table_locked();
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
        rebuild_table_locked();
    }

    bool has_node(const NodeId& id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return index_by_id_.find(id) != index_by_id_.end();
    }

    NodeId lookup(const Key& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return lookup_unlocked(key);
    }

    std::vector<NodeId> lookup_n(const Key& key, size_t n) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<NodeId> result;
        if (nodes_.empty() || n == 0) return result;
        const size_t total = nodes_.size();
        const size_t want = std::min(n, total);
        const size_t start = static_cast<size_t>(hasher_(key) % table_size_);
        std::set<NodeId> seen;
        result.reserve(want);
        size_t idx = start;
        for (size_t i = 0; i < table_size_ && seen.size() < want; ++i) {
            const size_t s = table_[idx];
            if (s != empty_slot_) {
                const NodeType& node = nodes_[s];
                if (node.status == NodeStatus::Active && seen.insert(node.id).second) {
                    result.push_back(node.id);
                }
            }
            idx = (idx + 1) % table_size_;
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
        table_.assign(table_size_, empty_slot_);
        last_assignment_.clear();
        metrics_.clear();
    }

private:
    static size_t slot_for(const NodeId& id, size_t j, size_t table_size) {
        const uint64_t start = detail::fnv1a_64(id) % table_size;
        uint64_t skip = detail::fnv1a_64(std::string(id) + "|skip") % table_size;
        if (skip == 0) skip = 1;
        // table_size is prime; any nonzero skip yields a full permutation.
        return static_cast<size_t>((start + static_cast<uint64_t>(j) * skip) %
                                   table_size);
    }

    void rebuild_table_locked() {
        table_.assign(table_size_, empty_slot_);
        const size_t n = nodes_.size();
        if (n == 0) return;
        for (size_t j = 0; j < table_size_; ++j) {
            for (size_t s = 0; s < n; ++s) {
                const size_t slot = slot_for(nodes_[s].id, j, table_size_);
                if (table_[slot] == empty_slot_) {
                    table_[slot] = s;
                }
            }
        }
    }

    std::vector<MigrationType> rebalance_locked() {
        std::vector<MigrationType> migrations;
        migrations.reserve(probe_keys_.size());
        std::map<Key, NodeId> current;
        for (const auto& k : probe_keys_) {
            try { current[k] = lookup_unlocked(k); } catch (...) {}
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

    // Caller must hold the mutex.
    NodeId lookup_unlocked(const Key& k) const {
        if (nodes_.empty()) throw std::runtime_error("empty");
        const size_t start = static_cast<size_t>(hasher_(k) % table_size_);
        size_t idx = start;
        for (size_t i = 0; i < table_size_; ++i) {
            const size_t s = table_[idx];
            if (s != empty_slot_ && nodes_[s].status == NodeStatus::Active) {
                return nodes_[s].id;
            }
            idx = (idx + 1) % table_size_;
        }
        throw std::runtime_error("no active nodes");
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

    size_t table_size_;
    size_t empty_slot_;
    std::vector<size_t> table_;             // node index or empty_slot_
    std::vector<NodeType> nodes_;
    std::map<NodeId, size_t> index_by_id_;
    std::map<Key, NodeId> last_assignment_;
    std::vector<Key> probe_keys_;
    Metrics metrics_;
    mutable std::shared_mutex mutex_;
    DefaultHasher<Key> hasher_;
};

}  // namespace consistent_hash
