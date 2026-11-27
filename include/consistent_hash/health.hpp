#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace consistent_hash::health {

// Health check result
enum class HealthStatus : uint8_t {
  Unknown = 0,
  Healthy = 1,
  Degraded = 2,
  Unhealthy = 3,
  Draining = 4  // Gracefully removing
};

// Health check configuration
struct HealthCheckConfig {
  // Interval between health checks
  std::chrono::milliseconds interval{5000};

  // Timeout for individual health check
  std::chrono::milliseconds timeout{2000};

  // Number of consecutive failures before marking unhealthy
  size_t failure_threshold{3};

  // Number of consecutive successes before marking healthy
  size_t success_threshold{2};

  // Initial delay before first check
  std::chrono::milliseconds initial_delay{1000};

  // Enable passive health checking (track errors from operations)
  bool passive_checks{true};

  // Passive check error threshold
  size_t passive_error_threshold{10};

  // Passive check time window
  std::chrono::seconds passive_window{60};
};

// Node health state
struct NodeHealth {
  std::string node_id;
  HealthStatus status = HealthStatus::Unknown;
  std::chrono::steady_clock::time_point last_check{};
  std::chrono::steady_clock::time_point last_status_change{};
  size_t consecutive_failures = 0;
  size_t consecutive_successes = 0;
  uint64_t total_checks = 0;
  uint64_t total_failures = 0;
  uint64_t total_passive_errors = 0;
  std::chrono::steady_clock::time_point last_passive_error{};
  std::string last_error_message{};
  double latency_ms = 0.0;

  bool is_healthy() const noexcept {
    return status == HealthStatus::Healthy;
  }

  bool is_available() const noexcept {
    return status == HealthStatus::Healthy || status == HealthStatus::Degraded;
  }
};

// Health check function type
using HealthCheckFn = std::function<bool(const std::string& node_id,
                                         std::string& error_out,
                                         std::chrono::milliseconds timeout)>;

// Passive error recorder
class PassiveErrorTracker {
public:
  struct Config {
    size_t error_threshold;
    std::chrono::seconds window;
    
    constexpr Config(size_t et = 10, std::chrono::seconds w = std::chrono::seconds(60))
        : error_threshold(et), window(w) {}
  };

  explicit PassiveErrorTracker(Config config = Config{}) : config_(config) {}

  void record_error(const std::string& node_id) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);
    auto& entry = errors_[node_id];
    entry.errors.push_back(now);
    cleanup_old_errors(entry, now);
  }

  void record_success(const std::string& node_id) {
    std::lock_guard lock(mutex_);
    auto it = errors_.find(node_id);
    if (it != errors_.end()) {
      it->second.errors.clear();
    }
  }

  bool is_unhealthy(const std::string& node_id) const {
    std::lock_guard lock(mutex_);
    auto it = errors_.find(node_id);
    if (it == errors_.end()) return false;
    cleanup_old_errors(const_cast<ErrorEntry&>(it->second), std::chrono::steady_clock::now());
    return it->second.errors.size() >= config_.error_threshold;
  }

  size_t error_count(const std::string& node_id) const {
    std::lock_guard lock(mutex_);
    auto it = errors_.find(node_id);
    if (it == errors_.end()) return 0;
    cleanup_old_errors(const_cast<ErrorEntry&>(it->second), std::chrono::steady_clock::now());
    return it->second.errors.size();
  }

private:
  struct ErrorEntry {
    std::vector<std::chrono::steady_clock::time_point> errors;
  };

  void cleanup_old_errors(ErrorEntry& entry, std::chrono::steady_clock::time_point now) const {
    auto cutoff = now - config_.window;
    entry.errors.erase(
      std::remove_if(entry.errors.begin(), entry.errors.end(),
                     [cutoff](auto t) { return t < cutoff; }),
      entry.errors.end());
  }

  Config config_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ErrorEntry> errors_;
};

// Active health checker
class HealthChecker {
public:
  using Callback = std::function<void(const std::string& node_id, HealthStatus old_status, HealthStatus new_status)>;

  explicit HealthChecker(HealthCheckConfig config = {})
      : config_(config),
        passive_tracker_(PassiveErrorTracker::Config{config.passive_error_threshold,
                                                     config.passive_window}) {
    start();
  }

  ~HealthChecker() {
    stop();
  }

  // Register a node for health checking
  void register_node(const std::string& node_id, HealthCheckFn check_fn) {
    {
      std::unique_lock lock(mutex_);
      nodes_[node_id] = NodeEntry{
        .node_id = node_id,
        .check_fn = check_fn,
        .health = {.node_id = node_id, .status = HealthStatus::Unknown}
      };
    }
    // Run an initial check synchronously so the node's status is available
    // immediately after registration (and to validate the check function).
    if (check_fn) {
      perform_check(node_id, check_fn);
    }
  }

  // Unregister a node
  void unregister_node(const std::string& node_id) {
    std::unique_lock lock(mutex_);
    nodes_.erase(node_id);
  }

  // Set status change callback
  void set_callback(Callback cb) {
    callback_ = std::move(cb);
  }

  // Start health checking
  void start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&HealthChecker::run, this);
  }

  // Stop health checking
  void stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
  }

  // Get node health
  std::optional<NodeHealth> get_health(const std::string& node_id) const {
    std::shared_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return std::nullopt;
    return it->second.health;
  }

  // Get all healthy nodes
  std::vector<std::string> get_healthy_nodes() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, entry] : nodes_) {
      if (entry.health.is_healthy()) result.push_back(id);
    }
    return result;
  }

  // Get all available nodes (healthy + degraded)
  std::vector<std::string> get_available_nodes() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, entry] : nodes_) {
      if (entry.health.is_available()) result.push_back(id);
    }
    return result;
  }

  // Manually trigger a check
  bool check_node(const std::string& node_id) {
    std::shared_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it == nodes_.end() || !it->second.check_fn) return false;
    auto fn = it->second.check_fn;
    lock.unlock();
    return perform_check(node_id, fn);
  }

  // Record passive error (for integration with hasher)
  void record_passive_error(const std::string& node_id, std::string_view error = "") {
    passive_tracker_.record_error(node_id);
    std::unique_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
      it->second.health.total_passive_errors++;
      it->second.health.last_passive_error = std::chrono::steady_clock::now();
      if (!error.empty()) it->second.health.last_error_message = error;

      if (config_.passive_checks && passive_tracker_.is_unhealthy(node_id)) {
        // Reuse the already-held lock; update_status() would re-lock mutex_.
        update_status_locked(it->second, HealthStatus::Unhealthy, lock);
      }
    }
  }

  void record_passive_success(const std::string& node_id) {
    passive_tracker_.record_success(node_id);
  }

  // Get passive error count
  size_t passive_error_count(const std::string& node_id) const {
    return passive_tracker_.error_count(node_id);
  }

// Set node to draining mode (graceful removal)
  void set_draining(const std::string& node_id, bool draining) {
    std::unique_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
      HealthStatus new_status = draining ? HealthStatus::Draining :
                                   (it->second.health.consecutive_failures > 0 ? HealthStatus::Unhealthy : HealthStatus::Healthy);
      update_status_locked(it->second, new_status, lock);
    }
  }

private:
  struct NodeEntry {
    std::string node_id;
    HealthCheckFn check_fn;
    NodeHealth health;
  };

  void run() {
    // Initial delay
    {
      std::unique_lock lock(mutex_);
      cv_.wait_for(lock, config_.initial_delay, [this] { return !running_.load(); });
    }

    while (running_.load()) {
      check_all_nodes();

      std::unique_lock lock(mutex_);
      cv_.wait_for(lock, config_.interval, [this] { return !running_.load(); });
    }
  }

  void check_all_nodes() {
    std::vector<std::pair<std::string, HealthCheckFn>> checks;
    {
      std::shared_lock lock(mutex_);
      checks.reserve(nodes_.size());
      for (const auto& [id, entry] : nodes_) {
        if (entry.check_fn && entry.health.status != HealthStatus::Draining) {
          checks.emplace_back(id, entry.check_fn);
        }
      }
    }

    for (auto& [id, fn] : checks) {
      if (!running_.load()) break;
      perform_check(id, fn);
    }
  }

  bool perform_check(const std::string& node_id, const HealthCheckFn& fn) {
    std::string error;
    auto start = std::chrono::steady_clock::now();
    bool healthy = fn(node_id, error, config_.timeout);
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

    std::unique_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return healthy;

    auto& entry = it->second;
    entry.health.total_checks++;
    entry.health.last_check = std::chrono::steady_clock::now();
    entry.health.latency_ms = static_cast<double>(latency.count());

    HealthStatus old_status = entry.health.status;
    HealthStatus new_status = old_status;

    if (healthy) {
      entry.health.consecutive_successes++;
      entry.health.consecutive_failures = 0;
      passive_tracker_.record_success(node_id);

      if (entry.health.status == HealthStatus::Unknown) {
        // First successful check promotes an unknown node to healthy.
        new_status = HealthStatus::Healthy;
      } else if (entry.health.status == HealthStatus::Unhealthy ||
                 entry.health.status == HealthStatus::Degraded) {
        if (entry.health.consecutive_successes >= config_.success_threshold) {
          new_status = HealthStatus::Healthy;
        } else if (entry.health.status == HealthStatus::Unhealthy) {
          new_status = HealthStatus::Degraded;
        }
      }
    } else {
      entry.health.consecutive_failures++;
      entry.health.consecutive_successes = 0;
      entry.health.total_failures++;
      entry.health.last_error_message = error;
      passive_tracker_.record_error(node_id);

      if (entry.health.status == HealthStatus::Unknown) {
        // A node that fails its first check is unhealthy.
        new_status = HealthStatus::Unhealthy;
      } else if (entry.health.status == HealthStatus::Healthy) {
        if (entry.health.consecutive_failures >= config_.failure_threshold) {
          new_status = HealthStatus::Unhealthy;
        }
      } else if (entry.health.status == HealthStatus::Degraded &&
                 entry.health.consecutive_failures >= config_.failure_threshold) {
        new_status = HealthStatus::Unhealthy;
      }
    }

    if (new_status != old_status) {
      update_status_locked(entry, new_status, lock);
    }

    return healthy;
  }

  void update_status(const std::string& node_id, HealthStatus new_status) {
    std::unique_lock lock(mutex_);
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
      update_status_locked(it->second, new_status, lock);
    }
  }

  void update_status_locked(NodeEntry& entry, HealthStatus new_status, std::unique_lock<std::shared_mutex>& lock) {
    HealthStatus old_status = entry.health.status;
    if (old_status == new_status) return;

    entry.health.status = new_status;
    entry.health.last_status_change = std::chrono::steady_clock::now();

    if (callback_) {
      // Release lock before calling callback
      lock.unlock();
      callback_(entry.node_id, old_status, new_status);
      lock.lock();
    }
  }

  HealthCheckConfig config_;
  std::atomic<bool> running_{false};
  std::thread thread_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, NodeEntry> nodes_;
  std::condition_variable_any cv_;
  Callback callback_;
  PassiveErrorTracker passive_tracker_;
};

// Health-aware hasher wrapper
template <typename Hasher>
class HealthAwareHasher {
public:
  using node_id_type = typename Hasher::node_id_type;
  using key_type = typename Hasher::key_type;

  HealthAwareHasher(Hasher hasher, std::shared_ptr<HealthChecker> health)
      : hasher_(std::move(hasher)), health_(std::move(health)) {}

  std::optional<node_id_type> lookup(const key_type& key) const noexcept {
    auto nodes = health_->get_available_nodes();
    if (nodes.empty()) return std::nullopt;

    // Try primary node
    auto primary = hasher_.lookup(key);
    if (primary && std::find(nodes.begin(), nodes.end(), *primary) != nodes.end()) {
      return primary;
    }

    // Fallback: find next healthy node
    auto candidates = hasher_.lookup_n(key, nodes.size());
    for (const auto& candidate : candidates) {
      if (std::find(nodes.begin(), nodes.end(), candidate) != nodes.end()) {
        return candidate;
      }
    }

    return std::nullopt;
  }

  std::vector<node_id_type> lookup_n(const key_type& key, size_t n) const noexcept {
    auto available = health_->get_available_nodes();
    if (available.empty()) return {};

    auto candidates = hasher_.lookup_n(key, n * 2);  // Get extra to filter
    std::vector<node_id_type> result;
    result.reserve(n);

    for (const auto& c : candidates) {
      if (std::find(available.begin(), available.end(), c) != available.end()) {
        result.push_back(c);
        if (result.size() >= n) break;
      }
    }
    return result;
  }

  std::vector<std::optional<node_id_type>> lookup_bulk(const std::vector<key_type>& keys) const noexcept {
    std::vector<std::optional<node_id_type>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
      results.push_back(lookup(key));
    }
    return results;
  }

  // Delegate other operations to underlying hasher
  void add_node(const typename Hasher::NodeType& node) { hasher_.add_node(node); }
  bool upsert_node(const typename Hasher::NodeType& node) { return hasher_.upsert_node(node); }
  void remove_node(const node_id_type& id) { hasher_.remove_node(id); }
  typename Hasher::Result update_weight(const node_id_type& id, uint32_t w) { return hasher_.update_weight(id, w); }
  bool has_node(const node_id_type& id) const noexcept { return hasher_.has_node(id); }
  std::vector<typename Hasher::MigrationType> rebalance() { return hasher_.rebalance(); }
  typename Hasher::StatsType stats() const noexcept { return hasher_.stats(); }
  size_t node_count() const noexcept { return hasher_.node_count(); }
  void clear() noexcept { hasher_.clear(); }
  std::string serialize() const { return hasher_.serialize(); }
  typename Hasher::Result deserialize(std::string_view data) { return hasher_.deserialize(data); }

  // Record operation result for passive health checking
  void record_lookup_result(const node_id_type& node_id, bool success, std::string_view error = "") {
    if (success) {
      health_->record_passive_success(node_id);
    } else {
      health_->record_passive_error(node_id, error);
    }
  }

  HealthChecker* health_checker() const { return health_.get(); }

private:
  Hasher hasher_;
  std::shared_ptr<HealthChecker> health_;
};

} // namespace consistent_hash::health