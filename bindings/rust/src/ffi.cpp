// ffi.cpp - C++ implementation for Rust bindings
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <consistent_hash/lockfree.hpp>
#include <consistent_hash/health.hpp>
#include <consistent_hash/simd_hash.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

using namespace consistent_hash;

// Helper to convert Rust Node to C++ Node
Node<> to_cpp_node(const rust::Node& node) {
    Node<> cpp_node;
    cpp_node.id = std::string(node.id.data(), node.id.size());
    cpp_node.address = std::string(node.address.data(), node.address.size());
    cpp_node.weight = node.weight;
    cpp_node.zone = std::string(node.zone.data(), node.zone.size());
    cpp_node.status = NodeStatus::Active;
    for (const auto& [k, v] : node.metadata) {
        cpp_node.metadata[std::string(k.data(), k.size())] = std::string(v.data(), v.size());
    }
    return cpp_node;
}

// RingHasher implementation
std::unique_ptr<RingHasher<>> new_ring_hasher() {
    return std::make_unique<RingHasher<>>();
}

void ring_add_node(RingHasher<>* h, const rust::Node& node) {
    h->add_node(to_cpp_node(node));
}

bool ring_upsert_node(RingHasher<>* h, const rust::Node& node) {
    return h->upsert_node(to_cpp_node(node));
}

void ring_remove_node(RingHasher<>* h, const char* id) {
    h->remove_node(std::string(id));
}

bool ring_update_weight(RingHasher<>* h, const char* id, uint32_t weight) {
    auto result = h->update_weight(std::string(id), weight);
    return static_cast<bool>(result);
}

bool ring_has_node(RingHasher<>* h, const char* id) {
    return h->has_node(std::string(id));
}

rust::Option<std::string> ring_lookup(RingHasher<>* h, const char* key) {
    auto opt = h->lookup(std::string(key));
    if (opt) {
        return rust::Some<std::string>(std::move(*opt));
    }
    return rust::None();
}

std::string ring_lookup_or_throw(RingHasher<>* h, const char* key) {
    return h->lookup_or_throw(std::string(key));
}

std::vector<std::string> ring_lookup_n(RingHasher<>* h, const char* key, size_t n) {
    return h->lookup_n(std::string(key), n);
}

std::vector<rust::Option<std::string>> ring_lookup_bulk(RingHasher<>* h, const rust::Slice<const std::string>& keys) {
    std::vector<std::string> cpp_keys;
    cpp_keys.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        cpp_keys.push_back(std::string(keys[i].data(), keys[i].size()));
    }
    auto results = h->lookup_bulk(cpp_keys);
    std::vector<rust::Option<std::string>> rust_results;
    rust_results.reserve(results.size());
    for (auto& opt : results) {
        if (opt) {
            rust_results.push_back(rust::Some<std::string>(std::move(*opt)));
        } else {
            rust_results.push_back(rust::None());
        }
    }
    return rust_results;
}

std::vector<rust::Migration> ring_rebalance(RingHasher<>* h) {
    auto migrations = h->rebalance();
    std::vector<rust::Migration> rust_migrations;
    rust_migrations.reserve(migrations.size());
    for (const auto& m : migrations) {
        rust_migrations.push_back({
            std::string(m.from),
            std::string(m.to),
            std::string(m.key)
        });
    }
    return rust_migrations;
}

rust::DistributionStats ring_stats(RingHasher<>* h) {
    auto stats = h->stats();
    return rust::DistributionStats{
        stats.total_keys,
        stats.unique_nodes_used,
        stats.mean_keys_per_node,
        stats.stddev_keys_per_node,
        stats.skew,
        stats.max_load,
        stats.min_load,
        stats.migration_count,
        stats.avg_lookup_ns,
        stats.p50_lookup_ns,
        stats.p90_lookup_ns,
        stats.p99_lookup_ns,
        stats.p999_lookup_ns
    };
}

size_t ring_node_count(RingHasher<>* h) {
    return h->node_count();
}

void ring_clear(RingHasher<>* h) {
    h->clear();
}

std::string ring_serialize(RingHasher<>* h) {
    return h->serialize();
}

bool ring_deserialize(RingHasher<>* h, const char* data) {
    auto result = h->deserialize(std::string(data));
    return static_cast<bool>(result);
}

// Similar implementations for other hashers would go here...
// (JumpHasher, MaglevHasher, RendezvousHasher, MultiProbeHasher, 
//  BoundedLoadsHasher, KetamaHasher, AnchorHasher)

// Lock-free hasher wrapper
template <typename Hasher>
class LockFreeHasherWrapper {
public:
    explicit LockFreeHasherWrapper(std::unique_ptr<Hasher> hasher)
        : lf_hasher_(std::make_unique<lockfree::LockFreeHasher<Hasher>>(std::move(hasher))) {}

    void add_node(const rust::Node& node) {
        lf_hasher_->add_node(to_cpp_node(node));
    }

    bool upsert_node(const rust::Node& node) {
        return lf_hasher_->upsert_node(to_cpp_node(node));
    }

    void remove_node(const char* id) {
        lf_hasher_->remove_node(std::string(id));
    }

    bool update_weight(const char* id, uint32_t weight) {
        auto result = lf_hasher_->update_weight(std::string(id), weight);
        return static_cast<bool>(result);
    }

    bool has_node(const char* id) {
        return lf_hasher_->has_node(std::string(id));
    }

    rust::Option<std::string> lookup(const char* key) {
        auto opt = lf_hasher_->lookup(std::string(key));
        if (opt) {
            return rust::Some<std::string>(std::move(*opt));
        }
        return rust::None();
    }

    std::string lookup_or_throw(const char* key) {
        return lf_hasher_->lookup_or_throw(std::string(key));
    }

    std::vector<std::string> lookup_n(const char* key, size_t n) {
        return lf_hasher_->lookup_n(std::string(key), n);
    }

    std::vector<rust::Option<std::string>> lookup_bulk(const rust::Slice<const std::string>& keys) {
        std::vector<std::string> cpp_keys;
        cpp_keys.reserve(keys.size());
        for (size_t i = 0; i < keys.size(); ++i) {
            cpp_keys.push_back(std::string(keys[i].data(), keys[i].size()));
        }
        auto results = lf_hasher_->lookup_bulk(cpp_keys);
        std::vector<rust::Option<std::string>> rust_results;
        rust_results.reserve(results.size());
        for (auto& opt : results) {
            if (opt) {
                rust_results.push_back(rust::Some<std::string>(std::move(*opt)));
            } else {
                rust_results.push_back(rust::None());
            }
        }
        return rust_results;
    }

    std::vector<rust::Migration> rebalance() {
        auto migrations = lf_hasher_->rebalance();
        std::vector<rust::Migration> rust_migrations;
        rust_migrations.reserve(migrations.size());
        for (const auto& m : migrations) {
            rust_migrations.push_back({
                std::string(m.from),
                std::string(m.to),
                std::string(m.key)
            });
        }
        return rust_migrations;
    }

    rust::DistributionStats stats() {
        auto stats = lf_hasher_->stats();
        return rust::DistributionStats{
            stats.total_keys,
            stats.unique_nodes_used,
            stats.mean_keys_per_node,
            stats.stddev_keys_per_node,
            stats.skew,
            stats.max_load,
            stats.min_load,
            stats.migration_count,
            stats.avg_lookup_ns,
            stats.p50_lookup_ns,
            stats.p90_lookup_ns,
            stats.p99_lookup_ns,
            stats.p999_lookup_ns
        };
    }

    size_t node_count() {
        return lf_hasher_->node_count();
    }

    void clear() {
        lf_hasher_->clear();
    }

    std::string serialize() {
        return lf_hasher_->serialize();
    }

    bool deserialize(const char* data) {
        auto result = lf_hasher_->deserialize(std::string(data));
        return static_cast<bool>(result);
    }

    uint64_t version() {
        return lf_hasher_->version();
    }

    void add_node_atomic(const rust::Node& node) {
        lf_hasher_->add_node_atomic(to_cpp_node(node));
    }

    void remove_node_atomic(const char* id) {
        lf_hasher_->remove_node_atomic(std::string(id));
    }

    bool update_weight_atomic(const char* id, uint32_t weight) {
        auto result = lf_hasher_->update_weight_atomic(std::string(id), weight);
        return static_cast<bool>(result);
    }

    rust::Snapshot create_snapshot() {
        auto snapshot = lf_hasher_->create_snapshot();
        return rust::Snapshot{
            snapshot.version,
            snapshot.node_count,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                snapshot.timestamp.time_since_epoch()).count()
        };
    }

    bool restore_snapshot(rust::Snapshot&& snapshot) {
        lockfree::LockFreeHasher<Hasher>::Snapshot cpp_snapshot;
        cpp_snapshot.version = snapshot.version;
        cpp_snapshot.node_count = snapshot.node_count;
        cpp_snapshot.timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(snapshot.timestamp));
        auto result = lf_hasher_->restore_snapshot(std::move(cpp_snapshot));
        return static_cast<bool>(result);
    }

private:
    std::unique_ptr<lockfree::LockFreeHasher<Hasher>> lf_hasher_;
};

// Health checker
class HealthCheckerWrapper {
public:
    explicit HealthCheckerWrapper(const rust::HealthCheckConfig& config) 
        : checker_(std::make_unique<health::HealthChecker>(to_cpp_config(config))) {}

    void register_node(const char* id, rust::Fn<bool(const char*, rust::String&, uint64_t)>& check_fn, const rust::HealthCheckConfig& config) {
        checker_->register_node(std::string(id), 
            [&check_fn](const std::string& node_id, std::string& error, std::chrono::milliseconds timeout) {
                rust::String rust_error;
                bool result = check_fn(node_id.c_str(), rust_error, timeout.count());
                error = std::string(rust_error.data(), rust_error.size());
                return result;
            }, to_cpp_config(config));
    }

    void unregister_node(const char* id) {
        checker_->unregister_node(std::string(id));
    }

    void start() {
        checker_->start();
    }

    void stop() {
        checker_->stop();
    }

    rust::Option<rust::NodeHealth> get_health(const char* id) {
        auto health = checker_->get_health(std::string(id));
        if (!health) return rust::None();
        
        rust::NodeHealth rust_health;
        rust_health.node_id = std::move(health->node_id);
        rust_health.status = static_cast<rust::HealthStatus>(static_cast<int>(health->status));
        rust_health.last_check = std::chrono::duration_cast<std::chrono::milliseconds>(
            health->last_check.time_since_epoch()).count();
        rust_health.last_status_change = std::chrono::duration_cast<std::chrono::milliseconds>(
            health->last_status_change.time_since_epoch()).count();
        rust_health.consecutive_failures = health->consecutive_failures;
        rust_health.consecutive_successes = health->consecutive_successes;
        rust_health.total_checks = health->total_checks;
        rust_health.total_failures = health->total_failures;
        rust_health.total_passive_errors = health->total_passive_errors;
        rust_health.last_passive_error = std::chrono::duration_cast<std::chrono::milliseconds>(
            health->last_passive_error.time_since_epoch()).count();
        rust_health.last_error_message = std::move(health->last_error_message);
        rust_health.latency_ms = health->latency_ms;
        
        return rust::Some<rust::NodeHealth>(std::move(rust_health));
    }

    std::vector<std::string> get_healthy_nodes() {
        return checker_->get_healthy_nodes();
    }

    std::vector<std::string> get_available_nodes() {
        return checker_->get_available_nodes();
    }

    void set_draining(const char* id, bool draining) {
        checker_->set_draining(std::string(id), draining);
    }

    void record_passive_error(const char* id, const char* error) {
        checker_->record_passive_error(std::string(id), std::string(error));
    }

    void record_passive_success(const char* id) {
        checker_->record_passive_success(std::string(id));
    }

    size_t passive_error_count(const char* id) {
        return checker_->passive_error_count(std::string(id));
    }

    void set_callback(rust::Fn<void(const char*, rust::HealthStatus, rust::HealthStatus)>& callback) {
        checker_->set_callback([&callback](const std::string& id, health::HealthStatus old_s, health::HealthStatus new_s) {
            callback(id.c_str(), 
                     static_cast<rust::HealthStatus>(static_cast<int>(old_s)),
                     static_cast<rust::HealthStatus>(static_cast<int>(new_s)));
        });
    }

private:
    std::unique_ptr<health::HealthChecker> checker_;

    static health::HealthCheckConfig to_cpp_config(const rust::HealthCheckConfig& config) {
        return health::HealthCheckConfig{
            .interval = std::chrono::milliseconds(config.interval_ms),
            .timeout = std::chrono::milliseconds(config.timeout_ms),
            .failure_threshold = config.failure_threshold,
            .success_threshold = config.success_threshold,
            .initial_delay = std::chrono::milliseconds(config.initial_delay_ms),
            .passive_checks = config.passive_checks,
            .passive_error_threshold = config.passive_error_threshold,
            .passive_window = std::chrono::seconds(config.passive_window_secs)
        };
    }
};

// SIMD Adaptive Hasher
class AdaptiveHasherWrapper {
public:
    explicit AdaptiveHasherWrapper(rust::SIMDType preferred = rust::SIMDType::Fnv1a)
        : hasher_(static_cast<simd::AdaptiveHasher::Type>(static_cast<int>(preferred))) {}

    uint64_t hash(const char* data) const {
        return hasher_(std::string(data));
    }

    uint64_t hash_with_seed(const char* data, uint64_t seed) const {
        return hasher_(std::string(data), seed);
    }

    std::vector<uint64_t> hash_batch(const rust::Slice<const std::string>& inputs) {
        std::vector<std::string_view> views;
        views.reserve(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            views.emplace_back(inputs[i].data(), inputs[i].size());
        }
        std::vector<uint64_t> outputs(inputs.size());
        hasher_.hash_batch(views.data(), outputs.data(), inputs.size());
        std::vector<uint64_t> results;
        results.reserve(outputs.size());
        for (auto v : outputs) results.push_back(v);
        return results;
    }

    rust::SIMDType type() const {
        return static_cast<rust::SIMDType>(static_cast<int>(hasher_.type()));
    }

    std::string type_name() const {
        return hasher_.type_name();
    }

private:
    simd::AdaptiveHasher hasher_;
};

// Instantiate templates for all hasher types
template class LockFreeHasherWrapper<RingHasher<>>;
template class LockFreeHasherWrapper<JumpHasher<>>;
template class LockFreeHasherWrapper<MaglevHasher<>>;
template class LockFreeHasherWrapper<RendezvousHasher<>>;
template class LockFreeHasherWrapper<MultiProbeHasher<>>;
template class LockFreeHasherWrapper<BoundedLoadsHasher<>>;
template class LockFreeHasherWrapper<KetamaHasher<>>;
template class LockFreeHasherWrapper<AnchorHasher<>>;