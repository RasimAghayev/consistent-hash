#pragma once

#include <consistent_hash/consistent_hash.hpp>
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <consistent_hash/health.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <map>
#include <set>

namespace consistent_hash::chaos {

// Chaos experiment configuration
struct ChaosConfig {
    // Experiment duration
    std::chrono::seconds duration{300};
    
    // Failure injection rates (per second)
    double node_failure_rate{0.1};
    double node_recovery_rate{0.05};
    double network_partition_rate{0.02};
    double network_heal_rate{0.05};
    double latency_injection_rate{0.05};
    double packet_loss_rate{0.01};
    
    // Latency injection
    std::chrono::milliseconds min_added_latency{10};
    std::chrono::milliseconds max_added_latency{500};
    
    // Packet loss
    double min_packet_loss{0.01};
    double max_packet_loss{0.1};
    
    // Network partition
    size_t min_partition_size{1};
    size_t max_partition_size{3};
    
    // Random seed
    uint64_t seed{std::random_device{}()};
};

// Network partition representation
struct NetworkPartition {
    std::set<std::string> partition_a;
    std::set<std::string> partition_b;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::seconds duration;
};

// Chaos event types
enum class ChaosEventType {
    NodeFailure,
    NodeRecovery,
    NetworkPartition,
    NetworkHeal,
    LatencyInjection,
    PacketLoss
};

struct ChaosEvent {
    ChaosEventType type;
    std::string target_node;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::seconds duration;
    std::string details;
    
    // For network partition
    std::set<std::string> partition_a;
    std::set<std::string> partition_b;
    
    // For latency/packet loss
    std::chrono::milliseconds added_latency;
    double packet_loss_rate;
};

// Experiment results
struct ExperimentResult {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::vector<ChaosEvent> events;
    
    // Metrics
    size_t total_operations{0};
    size_t successful_operations{0};
    size_t failed_operations{0};
    double avg_latency_ns{0};
    double p99_latency_ns{0};
    
    // Distribution metrics
    double initial_skew{0};
    double final_skew{0};
    double max_skew_during_experiment{0};
    
    // Migration metrics
    size_t total_migrations{0};
    size_t migrations_during_chaos{0};
    
    // Availability
    double availability_percent{100.0};
};

// Network simulator
class NetworkSimulator {
public:
    struct LinkState {
        std::chrono::milliseconds added_latency{0};
        double packet_loss_rate{0.0};
        bool partitioned{false};
        std::set<std::string> partition_group;
    };

    explicit NetworkSimulator(const std::vector<std::string>& nodes) {
        for (const auto& node : nodes) {
            links_[node] = LinkState{};
        }
    }

    // Add latency to a node's connections
    void inject_latency(const std::string& node, std::chrono::milliseconds latency) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(node);
        if (it != links_.end()) {
            it->second.added_latency = latency;
        }
    }

    // Remove latency injection
    void remove_latency(const std::string& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(node);
        if (it != links_.end()) {
            it->second.added_latency = std::chrono::milliseconds{0};
        }
    }

    // Inject packet loss
    void inject_packet_loss(const std::string& node, double loss_rate) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(node);
        if (it != links_.end()) {
            it->second.packet_loss_rate = std::clamp(loss_rate, 0.0, 1.0);
        }
    }

    // Remove packet loss
    void remove_packet_loss(const std::string& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(node);
        if (it != links_.end()) {
            it->second.packet_loss_rate = 0.0;
        }
    }

    // Create network partition
    void create_partition(const std::set<std::string>& group_a, const std::set<std::string>& group_b) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& node : group_a) {
            auto it = links_.find(node);
            if (it != links_.end()) {
                it->second.partitioned = true;
                it->second.partition_group = group_a;
            }
        }
        for (const auto& node : group_b) {
            auto it = links_.find(node);
            if (it != links_.end()) {
                it->second.partitioned = true;
                it->second.partition_group = group_b;
            }
        }
    }

    // Heal network partition
    void heal_partition() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [node, state] : links_) {
            state.partitioned = false;
            state.partition_group.clear();
        }
    }

    // Check if two nodes can communicate
    bool can_communicate(const std::string& from, const std::string& to) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it_from = links_.find(from);
        auto it_to = links_.find(to);
        
        if (it_from == links_.end() || it_to == links_.end()) {
            return false;
        }
        
        // If either is partitioned, check if they're in the same partition
        if (it_from->second.partitioned || it_to->second.partitioned) {
            return it_from->second.partition_group == it_to->second.partition_group;
        }
        
        return true;
    }

    // Get link state for a node
    LinkState get_link_state(const std::string& node) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = links_.find(node);
        if (it != links_.end()) {
            return it->second;
        }
        return LinkState{};
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, LinkState> links_;
};

// Chaos experiment runner
template <typename Hasher>
class ChaosExperiment {
public:
    using HasherType = Hasher;
    using NodeType = typename Hasher::NodeType;

    ChaosExperiment(std::shared_ptr<Hasher> hasher, const std::vector<std::string>& node_ids, ChaosConfig config = {})
        : hasher_(hasher), node_ids_(node_ids), config_(config), 
          network_(node_ids), rng_(config.seed),
          prob_dist_(0.0, 1.0),
          node_failure_dist_(config.node_failure_rate),
          node_recovery_dist_(config.node_recovery_rate),
          partition_dist_(config.network_partition_rate),
          heal_dist_(config.network_heal_rate),
          latency_dist_(config.latency_injection_rate),
          loss_dist_(config.packet_loss_rate),
          node_dist_(0, node_ids.empty() ? 0 : node_ids.size() - 1),
          partition_size_dist_(config.min_partition_size, config.max_partition_size),
          latency_ms_dist_(config.min_added_latency.count(), config.max_added_latency.count()),
          loss_rate_dist_(config.min_packet_loss, config.max_packet_loss) {}

    ExperimentResult run() {
        ExperimentResult result;
        result.start_time = std::chrono::steady_clock::now();
        
        // Calculate initial distribution
        result.initial_skew = calculate_skew();
        
        auto end_time = std::chrono::steady_clock::now() + config_.duration;
        std::chrono::steady_clock::time_point last_metrics = std::chrono::steady_clock::now();
        
        // Statistics
        std::atomic<size_t> total_ops{0};
        std::atomic<size_t> successful_ops{0};
        std::atomic<size_t> failed_ops{0};
        std::mutex latency_mutex;
        std::vector<uint64_t> latencies;
        
        // Operation worker
        std::atomic<bool> running{true};
        std::vector<std::thread> workers;
        const int num_workers = 4;
        
        for (int i = 0; i < num_workers; ++i) {
            workers.emplace_back([&, i]() {
                std::uniform_int_distribution<size_t> key_dist(0, 9999);
                while (running.load()) {
                    auto op_start = std::chrono::steady_clock::now();
                    
                    // Simulate operation with network effects
                    bool success = simulate_operation(key_dist(rng_));
                    
                    auto op_end = std::chrono::steady_clock::now();
                    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
                    
                    total_ops.fetch_add(1, std::memory_order_relaxed);
                    if (success) {
                        successful_ops.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        failed_ops.fetch_add(1, std::memory_order_relaxed);
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(latency_mutex);
                        latencies.push_back(latency);
                    }
                    
                    // Small delay to prevent CPU saturation
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        // Chaos injection loop
        std::chrono::steady_clock::time_point next_chaos = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() < end_time) {
            auto now = std::chrono::steady_clock::now();
            
            // Collect metrics periodically
            if (now - last_metrics >= std::chrono::seconds(5)) {
                double current_skew = calculate_skew();
                result.max_skew_during_experiment = std::max(result.max_skew_during_experiment, current_skew);
                last_metrics = now;
            }
            
            // Inject chaos events
            if (now >= next_chaos) {
                double rand_val = prob_dist_(rng_);
                
                // Node failure
                if (rand_val < config_.node_failure_rate * 5.0) { // Scaled for 5-second intervals
                    inject_node_failure(result);
                }
                
                // Node recovery
                if (rand_val < config_.node_recovery_rate * 5.0) {
                    inject_node_recovery(result);
                }
                
                // Network partition
                if (rand_val < config_.network_partition_rate * 5.0) {
                    inject_network_partition(result);
                }
                
                // Network heal
                if (rand_val < config_.network_heal_rate * 5.0) {
                    inject_network_heal(result);
                }
                
                // Latency injection
                if (rand_val < config_.latency_injection_rate * 5.0) {
                    inject_latency(result);
                }
                
                // Packet loss
                if (rand_val < config_.packet_loss_rate * 5.0) {
                    inject_packet_loss(result);
                }
                
                // Schedule next chaos check
                next_chaos = now + std::chrono::seconds(5);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop workers
        running.store(false);
        for (auto& w : workers) {
            w.join();
        }
        
        // Final metrics
        result.end_time = std::chrono::steady_clock::now();
        result.total_operations = total_ops.load();
        result.successful_operations = successful_ops.load();
        result.failed_operations = failed_ops.load();
        result.availability_percent = total_ops.load() > 0 ? 
            (100.0 * successful_ops.load() / total_ops.load()) : 100.0;
        
        // Calculate latency percentiles
        {
            std::lock_guard<std::mutex> lock(latency_mutex);
            if (!latencies.empty()) {
                std::sort(latencies.begin(), latencies.end());
                result.avg_latency_ns = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
                size_t p99_idx = latencies.size() * 99 / 100;
                result.p99_latency_ns = latencies[std::min(p99_idx, latencies.size() - 1)];
            }
        }
        
        result.final_skew = calculate_skew();
        
        return result;
    }

private:
    std::shared_ptr<Hasher> hasher_;
    std::vector<std::string> node_ids_;
    ChaosConfig config_;
    NetworkSimulator network_;
    std::mt19937_64 rng_;
    
    // Random distributions as member variables
    std::uniform_real_distribution<double> prob_dist_;
    std::exponential_distribution<double> node_failure_dist_;
    std::exponential_distribution<double> node_recovery_dist_;
    std::exponential_distribution<double> partition_dist_;
    std::exponential_distribution<double> heal_dist_;
    std::exponential_distribution<double> latency_dist_;
    std::exponential_distribution<double> loss_dist_;
    std::uniform_int_distribution<size_t> node_dist_;
    std::uniform_int_distribution<size_t> partition_size_dist_;
    std::uniform_int_distribution<int> latency_ms_dist_;
    std::uniform_real_distribution<double> loss_rate_dist_;
    std::atomic<bool> running_{true};

    bool simulate_operation(size_t key_idx) {
        std::string key = "key-" + std::to_string(key_idx);
        
        // Check network connectivity (simplified)
        if (!network_.can_communicate("client", "server")) {
            return false;
        }
        
        // Simulate latency
        auto link_state = network_.get_link_state("server");
        if (link_state.added_latency.count() > 0) {
            std::this_thread::sleep_for(link_state.added_latency);
        }
        
        // Simulate packet loss
        if (link_state.packet_loss_rate > 0.0) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            if (dist(rng_) < link_state.packet_loss_rate) {
                return false;
            }
        }
        
        // Perform actual operation
        try {
            auto result = hasher_->lookup(key);
            return result.has_value();
        } catch (...) {
            return false;
        }
    }

    void inject_node_failure(ExperimentResult& result) {
        if (node_ids_.empty()) return;
        
        std::uniform_int_distribution<size_t> dist(0, node_ids_.size() - 1);
        std::string node = node_ids_[dist(rng_)];
        
        // Mark as failed in health checker if available
        // For now, just add latency to simulate failure
        network_.inject_latency(node, std::chrono::seconds(30));
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::NodeFailure,
            node,
            std::chrono::steady_clock::now(),
            std::chrono::seconds(30),
            "Simulated node failure"
        });
    }

    void inject_node_recovery(ExperimentResult& result) {
        if (node_ids_.empty()) return;
        
        std::uniform_int_distribution<size_t> dist(0, node_ids_.size() - 1);
        std::string node = node_ids_[dist(rng_)];
        
        network_.remove_latency(node);
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::NodeRecovery,
            node,
            std::chrono::steady_clock::now(),
            std::chrono::seconds(0),
            "Node recovered"
        });
    }

    void inject_network_partition(ExperimentResult& result) {
        if (node_ids_.size() < 2) return;
        
        size_t partition_size = partition_size_dist_(rng_);
        std::set<std::string> group_a;
        std::set<std::string> group_b;
        
        std::vector<std::string> shuffled = node_ids_;
        std::shuffle(shuffled.begin(), shuffled.end(), rng_);
        
        for (size_t i = 0; i < partition_size && i < shuffled.size(); ++i) {
            group_a.insert(shuffled[i]);
        }
        for (size_t i = partition_size; i < shuffled.size(); ++i) {
            group_b.insert(shuffled[i]);
        }
        
        if (group_a.empty() || group_b.empty()) return;
        
        network_.create_partition(group_a, group_b);
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::NetworkPartition,
            "",
            std::chrono::steady_clock::now(),
            std::chrono::seconds(30),
            "Network partition created",
            group_a,
            group_b
        });
    }

    void inject_network_heal(ExperimentResult& result) {
        network_.heal_partition();
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::NetworkHeal,
            "",
            std::chrono::steady_clock::now(),
            std::chrono::seconds(0),
            "Network partition healed"
        });
    }

    void inject_latency(ExperimentResult& result) {
        if (node_ids_.empty()) return;
        
        std::uniform_int_distribution<size_t> dist(0, node_ids_.size() - 1);
        std::string node = node_ids_[dist(rng_)];
        
        std::chrono::milliseconds latency(latency_ms_dist_(rng_));
        network_.inject_latency(node, latency);
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::LatencyInjection,
            node,
            std::chrono::steady_clock::now(),
            std::chrono::seconds(10),
            "Latency injected",
            {},
            {},
            latency,
            0.0
        });
    }

    void inject_packet_loss(ExperimentResult& result) {
        if (node_ids_.empty()) return;
        
        std::uniform_int_distribution<size_t> dist(0, node_ids_.size() - 1);
        std::string node = node_ids_[dist(rng_)];
        
        double loss_rate = loss_rate_dist_(rng_);
        network_.inject_packet_loss(node, loss_rate);
        
        result.events.push_back(ChaosEvent{
            ChaosEventType::PacketLoss,
            node,
            std::chrono::steady_clock::now(),
            std::chrono::seconds(10),
            "Packet loss injected",
            {},
            {},
            std::chrono::milliseconds{0},
            loss_rate
        });
    }

    double calculate_skew() {
        if (!hasher_) return 0.0;
        
        const int num_keys = 10000;
        std::map<std::string, int> counts;
        
        for (int i = 0; i < num_keys; ++i) {
            std::string key = "key-" + std::to_string(i);
            if (auto opt = hasher_->lookup(key); opt) {
                counts[*opt]++;
            }
        }
        
        if (counts.empty()) return 0.0;
        
        double mean = 0;
        for (const auto& [_, count] : counts) mean += count;
        mean /= counts.size();
        
        double variance = 0;
        for (const auto& [_, count] : counts) {
            double diff = count - mean;
            variance += diff * diff;
        }
        variance /= counts.size();
        
        return mean > 0 ? std::sqrt(variance) / mean : 0.0;
    }
};

// Pre-defined chaos scenarios
namespace scenarios {
    
    // Single node failure
    inline ChaosConfig single_node_failure() {
        ChaosConfig config;
        config.duration = std::chrono::seconds(60);
        config.node_failure_rate = 0.02;  // One failure every ~50 seconds
        config.node_recovery_rate = 0.01; // Recovery every ~100 seconds
        return config;
    }
    
    // Network partition
    inline ChaosConfig network_partition() {
        ChaosConfig config;
        config.duration = std::chrono::seconds(120);
        config.network_partition_rate = 0.01; // Partition every ~100 seconds
        config.network_heal_rate = 0.02;      // Heal every ~50 seconds
        config.min_partition_size = 1;
        config.max_partition_size = 3;
        return config;
    }
    
    // Latency spikes
    inline ChaosConfig latency_spikes() {
        ChaosConfig config;
        config.duration = std::chrono::seconds(60);
        config.latency_injection_rate = 0.1; // Latency every 10 seconds
        config.min_added_latency = std::chrono::milliseconds(50);
        config.max_added_latency = std::chrono::milliseconds(1000);
        return config;
    }
    
    // Combined chaos
    inline ChaosConfig combined_chaos() {
        ChaosConfig config;
        config.duration = std::chrono::seconds(300);
        config.node_failure_rate = 0.01;
        config.node_recovery_rate = 0.01;
        config.network_partition_rate = 0.005;
        config.network_heal_rate = 0.01;
        config.latency_injection_rate = 0.05;
        config.packet_loss_rate = 0.02;
        config.min_added_latency = std::chrono::milliseconds(10);
        config.max_added_latency = std::chrono::milliseconds(500);
        config.min_packet_loss = 0.001;
        config.max_packet_loss = 0.05;
        config.min_partition_size = 1;
        config.max_partition_size = 2;
        return config;
    }
    
    // Production-like chaos
    inline ChaosConfig production_like() {
        ChaosConfig config;
        config.duration = std::chrono::seconds(600);
        config.node_failure_rate = 0.005;   // Very rare failures
        config.node_recovery_rate = 0.01;   // Quick recovery
        config.network_partition_rate = 0.002; // Rare partitions
        config.network_heal_rate = 0.02;    // Fast healing
        config.latency_injection_rate = 0.02;
        config.packet_loss_rate = 0.005;
        config.min_added_latency = std::chrono::milliseconds(5);
        config.max_added_latency = std::chrono::milliseconds(200);
        config.min_packet_loss = 0.0001;
        config.max_packet_loss = 0.01;
        return config;
    }
}

// Helper to run experiment and print results
template <typename Hasher>
void run_chaos_experiment(std::shared_ptr<Hasher> hasher, const std::vector<std::string>& nodes, const ChaosConfig& config) {
    ChaosExperiment<Hasher> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    std::cout << "=== Chaos Experiment Results ===\n";
    std::cout << "Duration: " << std::chrono::duration<double>(result.end_time - result.start_time).count() << "s\n";
    std::cout << "Total Operations: " << result.total_operations << "\n";
    std::cout << "Successful: " << result.successful_operations << "\n";
    std::cout << "Failed: " << result.failed_operations << "\n";
    std::cout << "Availability: " << std::fixed << std::setprecision(2) << result.availability_percent << "%\n";
    std::cout << "Avg Latency: " << result.avg_latency_ns << " ns\n";
    std::cout << "P99 Latency: " << result.p99_latency_ns << " ns\n";
    std::cout << "Initial Skew: " << result.initial_skew << "\n";
    std::cout << "Final Skew: " << result.final_skew << "\n";
    std::cout << "Max Skew: " << result.max_skew_during_experiment << "\n";
    std::cout << "Events: " << result.events.size() << "\n";
    
    for (const auto& event : result.events) {
        std::cout << "  [" << std::chrono::duration<double>(event.timestamp.time_since_epoch()).count() << "s] "
                  << static_cast<int>(event.type) << ": " << event.target_node << " - " << event.details << "\n";
    }
}

} // namespace consistent_hash::chaos