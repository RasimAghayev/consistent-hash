#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <consistent_hash/lockfree.hpp>
#include <consistent_hash/health.hpp>

namespace py = pybind11;
using namespace consistent_hash;

// Node wrapper
struct PyNode {
    std::string id;
    std::string address;
    uint32_t weight;
    std::string zone;
    std::map<std::string, std::string> metadata;
    
    PyNode(const std::string& id, const std::string& address, uint32_t weight = 100,
           const std::string& zone = "",
           const std::map<std::string, std::string>& metadata = {})
        : id(id), address(address), weight(weight), zone(zone), metadata(metadata) {}
    
    Node<> to_cpp() const {
        Node<> node;
        node.id = id;
        node.address = address;
        node.weight = weight;
        node.zone = zone;
        node.metadata = metadata;
        node.status = NodeStatus::Active;
        return node;
    }
};

// Migration wrapper
struct PyMigration {
    std::string from;
    std::string to;
    std::string key;
    
    PyMigration(const Migration<>& m) : from(m.from), to(m.to), key(m.key) {}
};

// Stats wrapper
struct PyStats {
    size_t total_keys;
    size_t unique_nodes_used;
    double mean_keys_per_node;
    double stddev_keys_per_node;
    double skew;
    double max_load;
    double min_load;
    uint64_t migration_count;
    double avg_lookup_ns;
    double p50_lookup_ns;
    double p90_lookup_ns;
    double p99_lookup_ns;
    double p999_lookup_ns;
    
    PyStats(const DistributionStats<>& s)
        : total_keys(s.total_keys),
          unique_nodes_used(s.unique_nodes_used),
          mean_keys_per_node(s.mean_keys_per_node),
          stddev_keys_per_node(s.stddev_keys_per_node),
          skew(s.skew),
          max_load(s.max_load),
          min_load(s.min_load),
          migration_count(s.migration_count),
          avg_lookup_ns(s.avg_lookup_ns),
          p50_lookup_ns(s.p50_lookup_ns),
          p90_lookup_ns(s.p90_lookup_ns),
          p99_lookup_ns(s.p99_lookup_ns),
          p999_lookup_ns(s.p999_lookup_ns) {}
};

// Health status enum
enum class PyHealthStatus {
    Unknown = 0,
    Healthy = 1,
    Degraded = 2,
    Unhealthy = 3,
    Draining = 4
};

// Health status wrapper
struct PyNodeHealth {
    std::string node_id;
    PyHealthStatus status;
    std::chrono::steady_clock::time_point last_check;
    std::chrono::steady_clock::time_point last_status_change;
    size_t consecutive_failures;
    size_t consecutive_successes;
    uint64_t total_checks;
    uint64_t total_failures;
    uint64_t total_passive_errors;
    std::chrono::steady_clock::time_point last_passive_error;
    std::string last_error_message;
    double latency_ms;
    
    PyNodeHealth() = default;
    PyNodeHealth(const health::NodeHealth& h) 
        : node_id(h.node_id),
          status(static_cast<PyHealthStatus>(static_cast<int>(h.status))),
          last_check(h.last_check),
          last_status_change(h.last_status_change),
          consecutive_failures(h.consecutive_failures),
          consecutive_successes(h.consecutive_successes),
          total_checks(h.total_checks),
          total_failures(h.total_failures),
          total_passive_errors(h.total_passive_errors),
          last_passive_error(h.last_passive_error),
          last_error_message(h.last_error_message),
          latency_ms(h.latency_ms) {}
};

// Generic hasher wrapper template
template <typename Hasher>
class PyHasher {
public:
    using NodeType = typename Hasher::NodeType;
    using KeyType = typename Hasher::key_type;
    
    PyHasher() : hasher_(std::make_unique<Hasher>()) {}
    explicit PyHasher(Hasher h) : hasher_(std::make_unique<Hasher>(std::move(h))) {}
    
    void add_node(const PyNode& node) {
        hasher_->add_node(node.to_cpp());
    }
    
    bool upsert_node(const PyNode& node) {
        return hasher_->upsert_node(node.to_cpp());
    }
    
    void remove_node(const std::string& id) {
        hasher_->remove_node(id);
    }
    
    bool update_weight(const std::string& id, uint32_t weight) {
        auto result = hasher_->update_weight(id, weight);
        return static_cast<bool>(result);
    }
    
    bool has_node(const std::string& id) const {
        return hasher_->has_node(id);
    }
    
    std::optional<std::string> lookup(const std::string& key) const {
        return hasher_->lookup(key);
    }
    
    std::string lookup_or_throw(const std::string& key) const {
        return hasher_->lookup_or_throw(key);
    }
    
    std::vector<std::string> lookup_n(const std::string& key, size_t n) const {
        return hasher_->lookup_n(key, n);
    }
    
    std::vector<std::optional<std::string>> lookup_bulk(const std::vector<std::string>& keys) const {
        return hasher_->lookup_bulk(keys);
    }
    
    std::vector<PyMigration> rebalance() {
        auto migrations = hasher_->rebalance();
        std::vector<PyMigration> result;
        result.reserve(migrations.size());
        for (const auto& m : migrations) {
            result.emplace_back(m);
        }
        return result;
    }
    
    PyStats stats() const {
        return PyStats(hasher_->stats());
    }
    
    size_t node_count() const {
        return hasher_->node_count();
    }
    
    void clear() {
        hasher_->clear();
    }
    
    std::string serialize() const {
        return hasher_->serialize();
    }
    
    bool deserialize(const std::string& data) {
        auto result = hasher_->deserialize(data);
        return static_cast<bool>(result);
    }
    
private:
    std::unique_ptr<Hasher> hasher_;
};

// Lock-free hasher wrapper
template <typename Hasher>
class PyLockFreeHasher {
public:
    using NodeType = typename Hasher::NodeType;
    using KeyType = typename Hasher::key_type;
    
    PyLockFreeHasher() : lf_hasher_(std::make_unique<lockfree::LockFreeHasher<Hasher>>()) {}
    explicit PyLockFreeHasher(Hasher h) : lf_hasher_(std::make_unique<lockfree::LockFreeHasher<Hasher>>(std::move(h))) {}
    
    void add_node(const PyNode& node) {
        lf_hasher_->add_node(node.to_cpp());
    }
    
    bool upsert_node(const PyNode& node) {
        return lf_hasher_->upsert_node(node.to_cpp());
    }
    
    void remove_node(const std::string& id) {
        lf_hasher_->remove_node(id);
    }
    
    bool update_weight(const std::string& id, uint32_t weight) {
        auto result = lf_hasher_->update_weight(id, weight);
        return static_cast<bool>(result);
    }
    
    bool has_node(const std::string& id) const {
        return lf_hasher_->has_node(id);
    }
    
    std::optional<std::string> lookup(const std::string& key) const {
        return lf_hasher_->lookup(key);
    }
    
    std::string lookup_or_throw(const std::string& key) const {
        return lf_hasher_->lookup_or_throw(key);
    }
    
    std::vector<std::string> lookup_n(const std::string& key, size_t n) const {
        return lf_hasher_->lookup_n(key, n);
    }
    
    std::vector<std::optional<std::string>> lookup_bulk(const std::vector<std::string>& keys) const {
        return lf_hasher_->lookup_bulk(keys);
    }
    
    std::vector<PyMigration> rebalance() {
        auto migrations = lf_hasher_->rebalance();
        std::vector<PyMigration> result;
        result.reserve(migrations.size());
        for (const auto& m : migrations) {
            result.emplace_back(m);
        }
        return result;
    }
    
    PyStats stats() const {
        return PyStats(lf_hasher_->stats());
    }
    
    size_t node_count() const {
        return lf_hasher_->node_count();
    }
    
    void clear() {
        lf_hasher_->clear();
    }
    
    std::string serialize() const {
        return lf_hasher_->serialize();
    }
    
    bool deserialize(const std::string& data) {
        auto result = lf_hasher_->deserialize(data);
        return static_cast<bool>(result);
    }
    
    // Lock-free specific
    uint64_t version() const {
        return lf_hasher_->version();
    }
    
    void add_node_atomic(const PyNode& node) {
        lf_hasher_->add_node_atomic(node.to_cpp());
    }
    
    void remove_node_atomic(const std::string& id) {
        lf_hasher_->remove_node_atomic(id);
    }
    
    bool update_weight_atomic(const std::string& id, uint32_t weight) {
        auto result = lf_hasher_->update_weight_atomic(id, weight);
        return static_cast<bool>(result);
    }
    
    struct PySnapshot {
        uint64_t version;
        size_t node_count;
        std::chrono::steady_clock::time_point timestamp;
        
        PySnapshot(const lockfree::LockFreeHasher<Hasher>::Snapshot& s)
            : version(s.version), node_count(s.node_count), timestamp(s.timestamp) {}
    };
    
    PySnapshot create_snapshot() const {
        return PySnapshot(lf_hasher_->create_snapshot());
    }
    
    bool restore_snapshot(PySnapshot&& snapshot) {
        auto cpp_snapshot = lockfree::LockFreeHasher<Hasher>::Snapshot{};
        cpp_snapshot.version = snapshot.version;
        cpp_snapshot.node_count = snapshot.node_count;
        cpp_snapshot.timestamp = snapshot.timestamp;
        auto result = lf_hasher_->restore_snapshot(std::move(cpp_snapshot));
        return static_cast<bool>(result);
    }
    
private:
    std::unique_ptr<lockfree::LockFreeHasher<Hasher>> lf_hasher_;
};

// Health checker wrapper
class PyHealthChecker {
public:
    PyHealthChecker() : checker_(std::make_unique<health::HealthChecker>()) {}
    
    struct PyConfig {
        std::chrono::milliseconds interval = std::chrono::milliseconds(5000);
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000);
        size_t failure_threshold = 3;
        size_t success_threshold = 2;
        std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000);
        bool passive_checks = true;
        size_t passive_error_threshold = 10;
        std::chrono::seconds passive_window = std::chrono::seconds(60);
    };
    
    void register_node(const std::string& id, py::function<bool(const std::string&, std::string&, std::chrono::milliseconds)> check_fn,
                       const PyConfig& config = PyConfig()) {
        health::HealthCheckConfig cfg;
        cfg.interval = config.interval;
        cfg.timeout = config.timeout;
        cfg.failure_threshold = config.failure_threshold;
        cfg.success_threshold = config.success_threshold;
        cfg.initial_delay = config.initial_delay;
        cfg.passive_checks = config.passive_checks;
        cfg.passive_error_threshold = config.passive_error_threshold;
        cfg.passive_window = config.passive_window;
        
        checker_->register_node(id, [check_fn](const std::string& node_id, std::string& error, std::chrono::milliseconds timeout) {
            std::string py_error;
            bool result = check_fn(node_id, py_error, timeout);
            error = py_error;
            return result;
        });
        
        // Apply config - would need to store per-node config
        (void)cfg;
    }
    
    void unregister_node(const std::string& id) {
        checker_->unregister_node(id);
    }
    
    void start() {
        checker_->start();
    }
    
    void stop() {
        checker_->stop();
    }
    
    std::optional<PyNodeHealth> get_health(const std::string& id) const {
        auto health = checker_->get_health(id);
        if (!health) return std::nullopt;
        return PyNodeHealth(*health);
    }
    
    std::vector<std::string> get_healthy_nodes() const {
        return checker_->get_healthy_nodes();
    }
    
    std::vector<std::string> get_available_nodes() const {
        return checker_->get_available_nodes();
    }
    
    void set_draining(const std::string& id, bool draining) {
        checker_->set_draining(id, draining);
    }
    
    void record_passive_error(const std::string& id, const std::string& error = "") {
        checker_->record_passive_error(id, error);
    }
    
    void record_passive_success(const std::string& id) {
        checker_->record_passive_success(id);
    }
    
    size_t passive_error_count(const std::string& id) const {
        return checker_->passive_error_count(id);
    }
    
    void set_callback(py::function<void(const std::string&, PyHealthStatus, PyHealthStatus)> callback) {
        checker_->set_callback([callback](const std::string& id, health::HealthStatus old_s, health::HealthStatus new_s) {
            callback(id, static_cast<PyHealthStatus>(static_cast<int>(old_s)), 
                     static_cast<PyHealthStatus>(static_cast<int>(new_s)));
        });
    }
    
private:
    std::unique_ptr<health::HealthChecker> checker_;
};

// Health-aware hasher wrapper
template <typename Hasher>
class PyHealthAwareHasher {
public:
    using NodeType = typename Hasher::NodeType;
    using KeyType = typename Hasher::key_type;
    
    PyHealthAwareHasher(Hasher h, PyHealthChecker& health) 
        : hasher_(std::make_unique<health::HealthAwareHasher<Hasher>>(std::move(h), health.checker_)) {}
    
    void add_node(const PyNode& node) {
        hasher_->add_node(node.to_cpp());
    }
    
    bool upsert_node(const PyNode& node) {
        return hasher_->upsert_node(node.to_cpp());
    }
    
    void remove_node(const std::string& id) {
        hasher_->remove_node(id);
    }
    
    bool update_weight(const std::string& id, uint32_t weight) {
        auto result = hasher_->update_weight(id, weight);
        return static_cast<bool>(result);
    }
    
    bool has_node(const std::string& id) const {
        return hasher_->has_node(id);
    }
    
    std::optional<std::string> lookup(const std::string& key) const {
        return hasher_->lookup(key);
    }
    
    std::string lookup_or_throw(const std::string& key) const {
        return hasher_->lookup_or_throw(key);
    }
    
    std::vector<std::string> lookup_n(const std::string& key, size_t n) const {
        return hasher_->lookup_n(key, n);
    }
    
    std::vector<std::optional<std::string>> lookup_bulk(const std::vector<std::string>& keys) const {
        return hasher_->lookup_bulk(keys);
    }
    
    std::vector<PyMigration> rebalance() {
        auto migrations = hasher_->rebalance();
        std::vector<PyMigration> result;
        result.reserve(migrations.size());
        for (const auto& m : migrations) {
            result.emplace_back(m);
        }
        return result;
    }
    
    PyStats stats() const {
        return PyStats(hasher_->stats());
    }
    
    size_t node_count() const {
        return hasher_->node_count();
    }
    
    void clear() {
        hasher_->clear();
    }
    
    std::string serialize() const {
        return hasher_->serialize();
    }
    
    bool deserialize(const std::string& data) {
        auto result = hasher_->deserialize(data);
        return static_cast<bool>(result);
    }
    
    void record_lookup_result(const std::string& node_id, bool success, const std::string& error = "") {
        hasher_->record_lookup_result(node_id, success, error);
    }
    
private:
    std::unique_ptr<health::HealthAwareHasher<Hasher>> hasher_;
};

// SIMD hasher bindings
class PyAdaptiveHasher {
public:
    enum class Type { HighwayHash, XxHash3, Fnv1a };
    
    PyAdaptiveHasher() = default;
    explicit PyAdaptiveHasher(Type preferred) : hasher_(preferred) {}
    
    uint64_t hash(const std::string& data) const {
        return hasher_(data);
    }
    
    uint64_t hash(const std::string& data, uint64_t seed) const {
        return hasher_(data, seed);
    }
    
    std::vector<uint64_t> hash_batch(const std::vector<std::string>& inputs) const {
        std::vector<std::string_view> views;
        views.reserve(inputs.size());
        for (const auto& s : inputs) views.emplace_back(s);
        std::vector<uint64_t> outputs(inputs.size());
        hasher_.hash_batch(views.data(), outputs.data(), inputs.size());
        return outputs;
    }
    
    Type type() const {
        return static_cast<Type>(static_cast<int>(hasher_.type()));
    }
    
    std::string type_name() const {
        return hasher_.type_name();
    }
    
private:
    simd::AdaptiveHasher hasher_;
};

// Factory functions
template <typename Hasher>
PyHasher<Hasher> make_hasher() {
    return PyHasher<Hasher>();
}

template <typename Hasher>
PyLockFreeHasher<Hasher> make_lockfree_hasher(Hasher h) {
    return PyLockFreeHasher<Hasher>(std::move(h));
}

template <typename Hasher>
PyHealthAwareHasher<Hasher> make_health_aware_hasher(Hasher h, PyHealthChecker& health) {
    return PyHealthAwareHasher<Hasher>(std::move(h), health);
}

PYBIND11_MODULE(consistent_hash, m) {
    m.doc() = "Modern C++20 Consistent Hashing Library - Python Bindings";
    
    // Node
    py::class_<PyNode>(m, "Node")
        .def(py::init<const std::string&, const std::string&, uint32_t, const std::string&, const std::map<std::string, std::string>&>(),
             py::arg("id"), py::arg("address"), py::arg("weight") = 100,
             py::arg("zone") = "", py::arg("metadata") = std::map<std::string, std::string>{})
        .def_readwrite("id", &PyNode::id)
        .def_readwrite("address", &PyNode::address)
        .def_readwrite("weight", &PyNode::weight)
        .def_readwrite("zone", &PyNode::zone)
        .def_readwrite("metadata", &PyNode::metadata);
    
    // Migration
    py::class_<PyMigration>(m, "Migration")
        .def_readwrite("from", &PyMigration::from)
        .def_readwrite("to", &PyMigration::to)
        .def_readwrite("key", &PyMigration::key);
    
    // Stats
    py::class_<PyStats>(m, "DistributionStats")
        .def_readwrite("total_keys", &PyStats::total_keys)
        .def_readwrite("unique_nodes_used", &PyStats::unique_nodes_used)
        .def_readwrite("mean_keys_per_node", &PyStats::mean_keys_per_node)
        .def_readwrite("stddev_keys_per_node", &PyStats::stddev_keys_per_node)
        .def_readwrite("skew", &PyStats::skew)
        .def_readwrite("max_load", &PyStats::max_load)
        .def_readwrite("min_load", &PyStats::min_load)
        .def_readwrite("migration_count", &PyStats::migration_count)
        .def_readwrite("avg_lookup_ns", &PyStats::avg_lookup_ns)
        .def_readwrite("p50_lookup_ns", &PyStats::p50_lookup_ns)
        .def_readwrite("p90_lookup_ns", &PyStats::p90_lookup_ns)
        .def_readwrite("p99_lookup_ns", &PyStats::p99_lookup_ns)
        .def_readwrite("p999_lookup_ns", &PyStats::p999_lookup_ns);
    
    // Health status
    py::enum_<PyHealthStatus>(m, "HealthStatus")
        .value("Unknown", PyHealthStatus::Unknown)
        .value("Healthy", PyHealthStatus::Healthy)
        .value("Degraded", PyHealthStatus::Degraded)
        .value("Unhealthy", PyHealthStatus::Unhealthy)
        .value("Draining", PyHealthStatus::Draining);
    
    // Node Health
    py::class_<PyNodeHealth>(m, "NodeHealth")
        .def_readwrite("node_id", &PyNodeHealth::node_id)
        .def_readwrite("status", &PyNodeHealth::status)
        .def_readwrite("last_check", &PyNodeHealth::last_check)
        .def_readwrite("last_status_change", &PyNodeHealth::last_status_change)
        .def_readwrite("consecutive_failures", &PyNodeHealth::consecutive_failures)
        .def_readwrite("consecutive_successes", &PyNodeHealth::consecutive_successes)
        .def_readwrite("total_checks", &PyNodeHealth::total_checks)
        .def_readwrite("total_failures", &PyNodeHealth::total_failures)
        .def_readwrite("total_passive_errors", &PyNodeHealth::total_passive_errors)
        .def_readwrite("last_passive_error", &PyNodeHealth::last_passive_error)
        .def_readwrite("last_error_message", &PyNodeHealth::last_error_message)
        .def_readwrite("latency_ms", &PyNodeHealth::latency_ms);
    
    // Hasher base class template
    auto register_hasher = [&](auto name, auto make) {
        using Hasher = decltype(make());
        py::class_<PyHasher<Hasher>>(m, name.c_str())
            .def(py::init<>())
            .def("add_node", &PyHasher<Hasher>::add_node)
            .def("upsert_node", &PyHasher<Hasher>::upsert_node)
            .def("remove_node", &PyHasher<Hasher>::remove_node)
            .def("update_weight", &PyHasher<Hasher>::update_weight)
            .def("has_node", &PyHasher<Hasher>::has_node)
            .def("lookup", &PyHasher<Hasher>::lookup)
            .def("lookup_or_throw", &PyHasher<Hasher>::lookup_or_throw)
            .def("lookup_n", &PyHasher<Hasher>::lookup_n)
            .def("lookup_bulk", &PyHasher<Hasher>::lookup_bulk)
            .def("rebalance", &PyHasher<Hasher>::rebalance)
            .def("stats", &PyHasher<Hasher>::stats)
            .def("node_count", &PyHasher<Hasher>::node_count)
            .def("clear", &PyHasher<Hasher>::clear)
            .def("serialize", &PyHasher<Hasher>::serialize)
            .def("deserialize", &PyHasher<Hasher>::deserialize);
    };
    
    auto register_lockfree_hasher = [&](auto name, auto make) {
        using Hasher = decltype(make());
        py::class_<PyLockFreeHasher<decltype(make())>>(m, name.c_str())
            .def(py::init<>())
            .def("add_node", &PyLockFreeHasher<decltype(make())>::add_node)
            .def("upsert_node", &PyLockFreeHasher<decltype(make())>::upsert_node)
            .def("remove_node", &PyLockFreeHasher<decltype(make())>::remove_node)
            .def("update_weight", &PyLockFreeHasher<decltype(make())>::update_weight)
            .def("has_node", &PyLockFreeHasher<decltype(make())>::has_node)
            .def("lookup", &PyLockFreeHasher<decltype(make())>::lookup)
            .def("lookup_or_throw", &PyLockFreeHasher<decltype(make())>::lookup_or_throw)
            .def("lookup_n", &PyLockFreeHasher<decltype(make())>::lookup_n)
            .def("lookup_bulk", &PyLockFreeHasher<decltype(make())>::lookup_bulk)
            .def("rebalance", &PyLockFreeHasher<decltype(make())>::rebalance)
            .def("stats", &PyLockFreeHasher<decltype(make())>::stats)
            .def("node_count", &PyLockFreeHasher<decltype(make())>::node_count)
            .def("clear", &PyLockFreeHasher<decltype(make())>::clear)
            .def("serialize", &PyLockFreeHasher<decltype(make())>::serialize)
            .def("deserialize", &PyLockFreeHasher<decltype(make())>::deserialize)
            .def("version", &PyLockFreeHasher<decltype(make())>::version)
            .def("add_node_atomic", &PyLockFreeHasher<decltype(make())>::add_node_atomic)
            .def("remove_node_atomic", &PyLockFreeHasher<decltype(make())>::remove_node_atomic)
            .def("update_weight_atomic", &PyLockFreeHasher<decltype(make())>::update_weight_atomic)
            .def("create_snapshot", &PyLockFreeHasher<decltype(make())>::create_snapshot)
            .def("restore_snapshot", &PyLockFreeHasher<decltype(make())>::restore_snapshot);
    };
    
    // Register all hashers
    register_hasher("RingHasher", []{ return RingHasher<>(); });
    register_hasher("JumpHasher", []{ return JumpHasher<>(); });
    register_hasher("MaglevHasher", []{ return MaglevHasher<>(65537); });
    register_hasher("RendezvousHasher", []{ return RendezvousHasher<>(); });
    register_hasher("MultiProbeHasher", []{ return MultiProbeHasher<>(); });
    register_hasher("BoundedLoadsHasher", []{ return BoundedLoadsHasher<>(); });
    register_hasher("KetamaHasher", []{ return KetamaHasher<>(); });
    register_hasher("AnchorHasher", []{ return AnchorHasher<>(); });
    
    // Lock-free hashers
    register_lockfree_hasher("LockFreeRingHasher", []{ return RingHasher<>(); });
    register_lockfree_hasher("LockFreeJumpHasher", []{ return JumpHasher<>(); });
    register_lockfree_hasher("LockFreeMaglevHasher", []{ return MaglevHasher<>(65537); });
    register_lockfree_hasher("LockFreeRendezvousHasher", []{ return RendezvousHasher<>(); });
    register_lockfree_hasher("LockFreeMultiProbeHasher", []{ return MultiProbeHasher<>(); });
    register_lockfree_hasher("LockFreeBoundedLoadsHasher", []{ return BoundedLoadsHasher<>(); });
    register_lockfree_hasher("LockFreeKetamaHasher", []{ return KetamaHasher<>(); });
    register_lockfree_hasher("LockFreeAnchorHasher", []{ return AnchorHasher<>(); });
    
    // Health checker
    py::class_<PyHealthChecker>(m, "HealthChecker")
        .def(py::init<>())
        .def("register_node", &PyHealthChecker::register_node, 
             py::arg("id"), py::arg("check_fn"), py::arg("config") = PyHealthChecker::PyConfig())
        .def("unregister_node", &PyHealthChecker::unregister_node)
        .def("start", &PyHealthChecker::start)
        .def("stop", &PyHealthChecker::stop)
        .def("get_health", &PyHealthChecker::get_health)
        .def("get_healthy_nodes", &PyHealthChecker::get_healthy_nodes)
        .def("get_available_nodes", &PyHealthChecker::get_available_nodes)
        .def("set_draining", &PyHealthChecker::set_draining)
        .def("record_passive_error", &PyHealthChecker::record_passive_error)
        .def("record_passive_success", &PyHealthChecker::record_passive_success)
        .def("passive_error_count", &PyHealthChecker::passive_error_count)
        .def("set_callback", &PyHealthChecker::set_callback);
    
    // Health config
    py::class_<PyHealthChecker::PyConfig>(m, "HealthCheckConfig")
        .def(py::init<>())
        .def_readwrite("interval", &PyHealthChecker::PyConfig::interval)
        .def_readwrite("timeout", &PyHealthChecker::PyConfig::timeout)
        .def_readwrite("failure_threshold", &PyHealthChecker::PyConfig::failure_threshold)
        .def_readwrite("success_threshold", &PyHealthChecker::PyConfig::success_threshold)
        .def_readwrite("initial_delay", &PyHealthChecker::PyConfig::initial_delay)
        .def_readwrite("passive_checks", &PyHealthChecker::PyConfig::passive_checks)
        .def_readwrite("passive_error_threshold", &PyHealthChecker::PyConfig::passive_error_threshold)
        .def_readwrite("passive_window", &PyHealthChecker::PyConfig::passive_window);
    
    // Health-aware hashers
    auto register_health_aware = [&](auto name, auto make) {
        using Hasher = decltype(make());
        py::class_<PyHealthAwareHasher<Hasher>>(m, name.c_str())
            .def(py::init<decltype(make()), PyHealthChecker&>())
            .def("add_node", &PyHealthAwareHasher<Hasher>::add_node)
            .def("upsert_node", &PyHealthAwareHasher<Hasher>::upsert_node)
            .def("remove_node", &PyHealthAwareHasher<Hasher>::remove_node)
            .def("update_weight", &PyHealthAwareHasher<Hasher>::update_weight)
            .def("has_node", &PyHealthAwareHasher<Hasher>::has_node)
            .def("lookup", &PyHealthAwareHasher<Hasher>::lookup)
            .def("lookup_or_throw", &PyHealthAwareHasher<Hasher>::lookup_or_throw)
            .def("lookup_n", &PyHealthAwareHasher<Hasher>::lookup_n)
            .def("lookup_bulk", &PyHealthAwareHasher<Hasher>::lookup_bulk)
            .def("rebalance", &PyHealthAwareHasher<Hasher>::rebalance)
            .def("stats", &PyHealthAwareHasher<Hasher>::stats)
            .def("node_count", &PyHealthAwareHasher<Hasher>::node_count)
            .def("clear", &PyHealthAwareHasher<Hasher>::clear)
            .def("serialize", &PyHealthAwareHasher<Hasher>::serialize)
            .def("deserialize", &PyHealthAwareHasher<Hasher>::deserialize)
            .def("record_lookup_result", &PyHealthAwareHasher<Hasher>::record_lookup_result);
    };
    
    register_health_aware("HealthAwareRingHasher", []{ return RingHasher<>(); });
    register_health_aware("HealthAwareJumpHasher", []{ return JumpHasher<>(); });
    register_health_aware("HealthAwareMaglevHasher", []{ return MaglevHasher<>(65537); });
    register_health_aware("HealthAwareRendezvousHasher", []{ return RendezvousHasher<>(); });
    register_health_aware("HealthAwareMultiProbeHasher", []{ return MultiProbeHasher<>(); });
    register_health_aware("HealthAwareBoundedLoadsHasher", []{ return BoundedLoadsHasher<>(); });
    register_health_aware("HealthAwareKetamaHasher", []{ return KetamaHasher<>(); });
    register_health_aware("HealthAwareAnchorHasher", []{ return AnchorHasher<>(); });
    
    // SIMD hasher
    py::class_<PyAdaptiveHasher>(m, "AdaptiveHasher")
        .def(py::init<>())
        .def(py::init<PyAdaptiveHasher::Type>())
        .def("hash", py::overload_cast<const std::string&>(&PyAdaptiveHasher::hash, py::const_))
        .def("hash", py::overload_cast<const std::string&, uint64_t>(&PyAdaptiveHasher::hash, py::const_))
        .def("hash_batch", &PyAdaptiveHasher::hash_batch)
        .def("type", &PyAdaptiveHasher::type)
        .def("type_name", &PyAdaptiveHasher::type_name);
    
    py::enum_<PyAdaptiveHasher::Type>(m, "SIMDType")
        .value("HighwayHash", PyAdaptiveHasher::Type::HighwayHash)
        .value("XxHash3", PyAdaptiveHasher::Type::XxHash3)
        .value("Fnv1a", PyAdaptiveHasher::Type::Fnv1a);
    
    // Factory functions
    m.def("make_lockfree", [](auto h) { return make_lockfree_hasher(std::move(h)); });
    m.def("make_health_aware", [](auto h, PyHealthChecker& health) { return make_health_aware_hasher(std::move(h), health); });
    m.def("make_health_aware_lockfree", [](auto h, PyHealthChecker& health) {
        auto ha = make_health_aware_hasher(std::move(h), health);
        return PyLockFreeHasher<health::HealthAwareHasher<decltype(make_hasher())>>(std::move(ha.hasher_));
    });
}