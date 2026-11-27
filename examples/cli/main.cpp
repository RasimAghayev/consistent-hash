#include <consistent_hash/consistent_hash.hpp>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <cxxopts.hpp>

using namespace consistent_hash;

// Command-line tool for consistent hash cluster management
class ClusterCLI {
public:
    struct NodeConfig {
        std::string id;
        std::string address;
        uint32_t weight = 100;
        std::string zone;
    };

    ClusterCLI() = default;

    int run(int argc, char* argv[]) {
        try {
            cxxopts::Options options("chash", "Consistent Hash Cluster Management Tool");
            options.add_options()
                ("h,help", "Print help")
                ("v,verbose", "Verbose output")
                ("algorithm", "Hash algorithm (ring, jump, maglev, rendezvous, multiprobe, bounded, ketama, anchor)", cxxopts::value<std::string>()->default_value("ring"))
                ("nodes", "Comma-separated list of nodes (id:address:weight:zone)", cxxopts::value<std::string>())
                ("config", "Config file path", cxxopts::value<std::string>())
                ("output", "Output format (json, table, csv)", cxxopts::value<std::string>()->default_value("table"))
                ("keys", "Number of test keys for benchmarks", cxxopts::value<int>()->default_value("100000"))
                ("threads", "Number of threads for benchmarks", cxxopts::value<int>()->default_value("4"))
                ;

            options.add_options("inspect")
                ("inspect", "Inspect cluster state")
                ("distribution", "Show key distribution")
                ("stats", "Show statistics")
                ;

            options.add_options("rebalance")
                ("rebalance", "Perform rebalance")
                ("dry-run", "Show migrations without applying")
                ("threshold", "Migration threshold (0.0-1.0)", cxxopts::value<double>()->default_value("0.1"))
                ;

            options.add_options("migrate")
                ("migrate", "Migrate keys between nodes")
                ("from", "Source node ID", cxxopts::value<std::string>())
                ("to", "Target node ID", cxxopts::value<std::string>())
                ("keys", "Keys to migrate (comma-separated or 'all')", cxxopts::value<std::string>()->default_value("all"))
                ;

            options.add_options("benchmark")
                ("benchmark", "Run benchmark")
                ("duration", "Benchmark duration in seconds", cxxopts::value<int>()->default_value("10"))
                ("warmup", "Warmup duration in seconds", cxxopts::value<int>()->default_value("2"))
                ("operations", "Operations to benchmark (lookup,lookup_n,rebalance)", cxxopts::value<std::string>()->default_value("lookup"))
                ;

            options.add_options("health")
                ("health", "Health check operations")
                ("check", "Run health check on all nodes")
                ("interval", "Check interval in seconds", cxxopts::value<int>()->default_value("5"))
                ("timeout", "Check timeout in seconds", cxxopts::value<int>()->default_value("2"))
                ("failure-threshold", "Failure threshold", cxxopts::value<int>()->default_value("3"))
                ;

            options.add_options("simulate")
                ("simulate", "Simulate cluster changes")
                ("add", "Add node (id:address:weight:zone)", cxxopts::value<std::vector<std::string>>())
                ("remove", "Remove node ID", cxxopts::value<std::vector<std::string>>())
                ("fail", "Simulate node failure", cxxopts::value<std::vector<std::string>>())
                ("recover", "Simulate node recovery", cxxopts::value<std::vector<std::string>>())
                ("steps", "Number of simulation steps", cxxopts::value<int>()->default_value("100"))
                ;

            auto result = options.parse(argc, argv);

            if (result.count("help")) {
                std::cout << options.help() << std::endl;
                return 0;
            }

            verbose_ = result.count("verbose");
            algorithm_ = result["algorithm"].as<std::string>();
            output_format_ = result["output"].as<std::string>();

            // Parse nodes
            if (result.count("nodes")) {
                parse_nodes(result["nodes"].as<std::string>());
            }

            // Load config file
            if (result.count("config")) {
                load_config(result["config"].as<std::string>());
            }

            // Create hasher
            create_hasher();

            // Execute command
            if (result.count("inspect")) {
                return cmd_inspect(result);
            } else if (result.count("rebalance")) {
                return cmd_rebalance(result);
            } else if (result.count("migrate")) {
                return cmd_migrate(result);
            } else if (result.count("benchmark")) {
                return cmd_benchmark(result);
            } else if (result.count("health")) {
                return cmd_health(result);
            } else if (result.count("simulate")) {
                return cmd_simulate(result);
            } else {
                std::cout << options.help() << std::endl;
                return 1;
            }

        } catch (const cxxopts::OptionException& e) {
            std::cerr << "Error parsing options: " << e.what() << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

private:
    bool verbose_ = false;
    std::string algorithm_;
    std::string output_format_;
    std::vector<NodeConfig> nodes_;
    std::unique_ptr<ConsistentHasher<std::string, std::string>> hasher_;
    std::shared_ptr<health::HealthChecker> health_checker_;

    void parse_nodes(const std::string& nodes_str) {
        std::stringstream ss(nodes_str);
        std::string node_str;
        while (std::getline(ss, node_str, ',')) {
            std::stringstream ns(node_str);
            std::string id, address, weight_str, zone;
            std::getline(ns, id, ':');
            std::getline(ns, address, ':');
            std::getline(ns, weight_str, ':');
            std::getline(ns, zone, ':');
            
            NodeConfig node;
            node.id = id;
            node.address = address;
            node.weight = weight_str.empty() ? 100 : std::stoul(weight_str);
            node.zone = zone;
            nodes_.push_back(node);
        }
    }

    void load_config(const std::string& config_path) {
        // TODO: Implement config file loading (JSON/YAML)
        if (verbose_) {
            std::cout << "Loading config from: " << config_path << std::endl;
        }
    }

    void create_hasher() {
        if (algorithm_ == "ring") {
            hasher_ = std::make_unique<RingHasher<>>();
        } else if (algorithm_ == "jump") {
            hasher_ = std::make_unique<JumpHasher<>>();
        } else if (algorithm_ == "maglev") {
            hasher_ = std::make_unique<MaglevHasher<>>(65537);
        } else if (algorithm_ == "rendezvous") {
            hasher_ = std::make_unique<RendezvousHasher<>>();
        } else if (algorithm_ == "multiprobe") {
            hasher_ = std::make_unique<MultiProbeHasher<>>();
        } else if (algorithm_ == "bounded") {
            hasher_ = std::make_unique<BoundedLoadsHasher<>>();
        } else if (algorithm_ == "ketama") {
            hasher_ = std::make_unique<KetamaHasher<>>();
        } else if (algorithm_ == "anchor") {
            hasher_ = std::make_unique<AnchorHasher<>>();
        } else {
            throw std::runtime_error("Unknown algorithm: " + algorithm_);
        }

        // Add nodes
        for (const auto& node : nodes_) {
            Node<> n;
            n.id = node.id;
            n.address = node.address;
            n.weight = node.weight;
            n.zone = node.zone;
            n.status = NodeStatus::Active;
            hasher_->add_node(n);
        }
    }

    int cmd_inspect(const cxxopts::ParseResult& result) {
        bool show_dist = result.count("distribution");
        bool show_stats = result.count("stats");

        if (show_dist || (!show_dist && !show_stats)) {
            print_distribution();
        }

        if (show_stats || (!show_dist && !show_stats)) {
            print_stats();
        }

        print_nodes();
        return 0;
    }

    void print_nodes() {
        if (output_format_ == "json") {
            std::cout << "{\n  \"nodes\": [\n";
            for (size_t i = 0; i < nodes_.size(); ++i) {
                const auto& n = nodes_[i];
                std::cout << "    {\"id\": \"" << n.id << "\", "
                          << "\"address\": \"" << n.address << "\", "
                          << "\"weight\": " << n.weight << ", "
                          << "\"zone\": \"" << n.zone << "\"}"
                          << (i + 1 < nodes_.size() ? "," : "") << "\n";
            }
            std::cout << "  ]\n}\n";
        } else {
            std::cout << std::left << std::setw(20) << "Node ID"
                      << std::setw(30) << "Address"
                      << std::setw(10) << "Weight"
                      << std::setw(15) << "Zone" << "\n";
            std::cout << std::string(75, '-') << "\n";
            for (const auto& n : nodes_) {
                std::cout << std::left << std::setw(20) << n.id
                          << std::setw(30) << n.address
                          << std::setw(10) << n.weight
                          << std::setw(15) << n.zone << "\n";
            }
        }
    }

    void print_distribution() {
        if (!hasher_) return;

        // Generate test keys
        const int num_keys = 10000;
        std::vector<std::string> keys;
        keys.reserve(num_keys);
        for (int i = 0; i < num_keys; ++i) {
            keys.push_back("key-" + std::to_string(i));
        }

        std::map<std::string, int> counts;
        for (const auto& key : keys) {
            if (auto opt = hasher_->lookup(key); opt) {
                counts[*opt]++;
            }
        }

        if (output_format_ == "json") {
            std::cout << "{\n  \"distribution\": {\n";
            size_t i = 0;
            for (const auto& [node, count] : counts) {
                std::cout << "    \"" << node << "\": " << count
                          << (++i < counts.size() ? "," : "") << "\n";
            }
            std::cout << "  }\n}\n";
        } else {
            std::cout << "\nKey Distribution:\n";
            std::cout << std::left << std::setw(20) << "Node"
                      << std::setw(15) << "Keys"
                      << std::setw(15) << "Percentage" << "\n";
            std::cout << std::string(50, '-') << "\n";
            
            int total = 0;
            for (const auto& [_, count] : counts) total += count;
            
            for (const auto& [node, count] : counts) {
                double pct = total > 0 ? (100.0 * count / total) : 0;
                std::cout << std::left << std::setw(20) << node
                          << std::setw(15) << count
                          << std::setw(15) << std::fixed << std::setprecision(2) << pct << "%\n";
            }
        }
    }

    void print_stats() {
        if (!hasher_) return;

        auto stats = hasher_->stats();
        
        if (output_format_ == "json") {
            std::cout << "{\n  \"stats\": {\n"
                      << "    \"total_keys\": " << stats.total_keys << ",\n"
                      << "    \"unique_nodes_used\": " << stats.unique_nodes_used << ",\n"
                      << "    \"mean_keys_per_node\": " << stats.mean_keys_per_node << ",\n"
                      << "    \"stddev_keys_per_node\": " << stats.stddev_keys_per_node << ",\n"
                      << "    \"skew\": " << stats.skew << ",\n"
                      << "    \"max_load\": " << stats.max_load << ",\n"
                      << "    \"min_load\": " << stats.min_load << ",\n"
                      << "    \"migration_count\": " << stats.migration_count << ",\n"
                      << "    \"avg_lookup_ns\": " << stats.avg_lookup_ns << ",\n"
                      << "    \"p50_lookup_ns\": " << stats.p50_lookup_ns << ",\n"
                      << "    \"p90_lookup_ns\": " << stats.p90_lookup_ns << ",\n"
                      << "    \"p99_lookup_ns\": " << stats.p99_lookup_ns << ",\n"
                      << "    \"p999_lookup_ns\": " << stats.p999_lookup_ns << "\n"
                      << "  }\n}\n";
        } else {
            std::cout << "\nStatistics:\n";
            std::cout << std::left << std::setw(30) << "Metric" << "Value\n";
            std::cout << std::string(50, '-') << "\n";
            std::cout << std::left << std::setw(30) << "Total Keys" << stats.total_keys << "\n";
            std::cout << std::left << std::setw(30) << "Unique Nodes Used" << stats.unique_nodes_used << "\n";
            std::cout << std::left << std::setw(30) << "Mean Keys/Node" << std::fixed << std::setprecision(2) << stats.mean_keys_per_node << "\n";
            std::cout << std::left << std::setw(30) << "StdDev Keys/Node" << stats.stddev_keys_per_node << "\n";
            std::cout << std::left << std::setw(30) << "Skew" << stats.skew << "\n";
            std::cout << std::left << std::setw(30) << "Max Load" << stats.max_load << "\n";
            std::cout << std::left << std::setw(30) << "Min Load" << stats.min_load << "\n";
            std::cout << std::left << std::setw(30) << "Migration Count" << stats.migration_count << "\n";
            std::cout << std::left << std::setw(30) << "Avg Lookup (ns)" << stats.avg_lookup_ns << "\n";
            std::cout << std::left << std::setw(30) << "P50 Lookup (ns)" << stats.p50_lookup_ns << "\n";
            std::cout << std::left << std::setw(30) << "P90 Lookup (ns)" << stats.p90_lookup_ns << "\n";
            std::cout << std::left << std::setw(30) << "P99 Lookup (ns)" << stats.p99_lookup_ns << "\n";
            std::cout << std::left << std::setw(30) << "P999 Lookup (ns)" << stats.p999_lookup_ns << "\n";
        }
    }

    int cmd_rebalance(const cxxopts::ParseResult& result) {
        bool dry_run = result.count("dry-run");
        double threshold = result["threshold"].as<double>();

        if (!hasher_) {
            std::cerr << "No hasher available" << std::endl;
            return 1;
        }

        auto migrations = hasher_->rebalance();
        
        if (dry_run) {
            std::cout << "Dry run - would perform " << migrations.size() << " migrations:\n";
            for (const auto& m : migrations) {
                std::cout << "  " << m.from << " -> " << m.to << " (key: " << m.key << ")\n";
            }
        } else {
            std::cout << "Performed " << migrations.size() << " migrations:\n";
            for (const auto& m : migrations) {
                std::cout << "  " << m.from << " -> " << m.to << " (key: " << m.key << ")\n";
            }
        }
        return 0;
    }

    int cmd_migrate(const cxxopts::ParseResult& result) {
        std::string from = result["from"].as<std::string>();
        std::string to = result["to"].as<std::string>();
        std::string keys_str = result["keys"].as<std::string>();

        std::cout << "Migrating from " << from << " to " << to << "\n";
        // TODO: Implement migration logic
        return 0;
    }

    int cmd_benchmark(const cxxopts::ParseResult& result) {
        int duration = result["duration"].as<int>();
        int warmup = result["warmup"].as<int>();
        std::string ops = result["operations"].as<std::string>();
        int num_keys = result["keys"].as<int>();
        int threads = result["threads"].as<int>();

        if (!hasher_) {
            std::cerr << "No hasher available" << std::endl;
            return 1;
        }

        std::cout << "Running benchmark: " << algorithm_ << " hasher\n";
        std::cout << "  Duration: " << duration << "s\n";
        std::cout << "  Warmup: " << warmup << "s\n";
        std::cout << "  Threads: " << threads << "\n";
        std::cout << "  Operations: " << ops << "\n";

        // Generate test keys
        std::vector<std::string> keys;
        keys.reserve(num_keys);
        for (int i = 0; i < num_keys; ++i) {
            keys.push_back("bench-key-" + std::to_string(i));
        }

        // Warmup
        if (warmup > 0) {
            std::cout << "Warming up...\n";
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(warmup)) {
                for (const auto& key : keys) {
                    hasher_->lookup(key);
                }
            }
        }

        // Benchmark
        std::cout << "Benchmarking...\n";
        auto start = std::chrono::steady_clock::now();
        auto end_time = start + std::chrono::seconds(duration);
        
        std::atomic<uint64_t> total_ops{0};
        std::atomic<uint64_t> total_latency_ns{0};
        
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&, t]() {
                size_t idx = t;
                while (std::chrono::steady_clock::now() < end_time) {
                    auto key = keys[idx % keys.size()];
                    auto op_start = std::chrono::steady_clock::now();
                    hasher_->lookup(key);
                    auto op_end = std::chrono::steady_clock::now();
                    
                    total_ops.fetch_add(1, std::memory_order_relaxed);
                    total_latency_ns.fetch_add(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count(),
                        std::memory_order_relaxed);
                    
                    idx += threads;
                }
            });
        }

        for (auto& w : workers) w.join();

        auto elapsed = std::chrono::steady_clock::now() - start;
        double elapsed_sec = std::chrono::duration<double>(elapsed).count();
        double ops_per_sec = total_ops.load() / elapsed_sec;
        double avg_latency_ns = total_latency_ns.load() / std::max<uint64_t>(1, total_ops.load());

        std::cout << "\nResults:\n";
        std::cout << "  Total operations: " << total_ops.load() << "\n";
        std::cout << "  Duration: " << std::fixed << std::setprecision(2) << elapsed_sec << "s\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec\n";
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) << avg_latency_ns << " ns\n";
        
        return 0;
    }

    int cmd_health(const cxxopts::ParseResult& result) {
        bool check = result.count("check");
        int interval = result["interval"].as<int>();
        int timeout = result["timeout"].as<int>();
        int failure_threshold = result["failure-threshold"].as<int>();

        if (!hasher_) {
            std::cerr << "No hasher available" << std::endl;
            return 1;
        }

        health_checker_ = std::make_shared<health::HealthChecker>();
        
        health::HealthCheckConfig config;
        config.interval = std::chrono::seconds(interval);
        config.timeout = std::chrono::seconds(timeout);
        config.failure_threshold = failure_threshold;
        config.success_threshold = 2;
        config.initial_delay = std::chrono::seconds(1);
        config.passive_checks = true;
        config.passive_error_threshold = 10;
        config.passive_window = std::chrono::seconds(60);

        // Register health checks for all nodes
        for (const auto& node : nodes_) {
            health_checker_->register_node(node.id, 
                [&](const std::string& id, std::string& error, std::chrono::milliseconds timeout) {
                    // Simulate health check
                    return true;
                }, config);
        }

        health_checker_->set_callback([](const std::string& id, health::HealthStatus old_s, health::HealthStatus new_s) {
            std::cout << "Health change: " << id << " " << static_cast<int>(old_s) << " -> " << static_cast<int>(new_s) << "\n";
        });

        health_checker_->start();

        if (check) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto healthy = health_checker_->get_healthy_nodes();
            auto available = health_checker_->get_available_nodes();
            
            std::cout << "Healthy nodes: " << healthy.size() << "\n";
            for (const auto& id : healthy) {
                std::cout << "  " << id << "\n";
            }
            std::cout << "Available nodes: " << available.size() << "\n";
        } else {
            std::cout << "Health checker running. Press Ctrl+C to stop.\n";
            std::this_thread::sleep_for(std::chrono::hours(24)); // Run until interrupted
        }

        health_checker_->stop();
        return 0;
    }

    int cmd_simulate(const cxxopts::ParseResult& result) {
        auto add_nodes = result["add"].as<std::vector<std::string>>();
        auto remove_nodes = result["remove"].as<std::vector<std::string>>();
        auto fail_nodes = result["fail"].as<std::vector<std::string>>();
        auto recover_nodes = result["recover"].as<std::vector<std::string>>();
        int steps = result["steps"].as<int>();

        std::cout << "Simulating cluster changes for " << steps << " steps\n";

        for (int step = 0; step < steps; ++step) {
            // Add nodes
            for (const auto& node_str : add_nodes) {
                std::stringstream ss(node_str);
                std::string id, address, weight_str, zone;
                std::getline(ss, id, ':');
                std::getline(ss, address, ':');
                std::getline(ss, weight_str, ':');
                std::getline(ss, zone, ':');
                
                Node<> n;
                n.id = id;
                n.address = address;
                n.weight = weight_str.empty() ? 100 : std::stoul(weight_str);
                n.zone = zone;
                n.status = NodeStatus::Active;
                hasher_->add_node(n);
                std::cout << "Step " << step << ": Added node " << id << "\n";
            }

            // Remove nodes
            for (const auto& id : remove_nodes) {
                hasher_->remove_node(id);
                std::cout << "Step " << step << ": Removed node " << id << "\n";
            }

            // Simulate failures
            for (const auto& id : fail_nodes) {
                if (health_checker_) {
                    health_checker_->record_passive_error(id, "simulated failure");
                }
                std::cout << "Step " << step << ": Node " << id << " failed\n";
            }

            // Recover nodes
            for (const auto& id : recover_nodes) {
                if (health_checker_) {
                    health_checker_->record_passive_success(id);
                }
                std::cout << "Step " << step << ": Node " << id << " recovered\n";
            }

            // Show distribution every 10 steps
            if (step % 10 == 0) {
                print_distribution();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    ClusterCLI cli;
    return cli.run(argc, argv);
}