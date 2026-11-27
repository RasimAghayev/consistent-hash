// Example Prometheus metrics exporter for consistent_hash
// This shows how to integrate the library's metrics with Prometheus

#include <consistent_hash/observability.hpp>
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <consistent_hash/health.hpp>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>

#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>

using namespace consistent_hash;
using namespace prometheus;

// Custom collector that exports consistent_hash metrics
class ConsistentHashCollector : public Collector {
public:
  ConsistentHashCollector(std::shared_ptr<RingHasher<>> ring_hasher,
                          std::shared_ptr<JumpHasher<>> jump_hasher,
                          std::shared_ptr<MaglevHasher<>> maglev_hasher,
                          std::shared_ptr<KetamaHasher<>> ketama_hasher,
                          std::shared_ptr<AnchorHasher<>> anchor_hasher,
                          std::shared_ptr<health::HealthChecker> health_checker)
      : ring_hasher_(ring_hasher),
        jump_hasher_(jump_hasher),
        maglev_hasher_(maglev_hasher),
        ketama_hasher_(ketama_hasher),
        anchor_hasher_(anchor_hasher),
        health_checker_(health_checker) {
    // Create metrics
    auto& registry = Registry::Instance();
    
    // Lookup latency histogram
    auto& latency_family = BuildHistogram()
        .Name("consistent_hash_lookup_duration_seconds")
        .Help("Lookup latency in seconds")
        .Labels({{"algorithm", ""}})
        .Buckets({0.000001, 0.000005, 0.00001, 0.00005, 0.0001, 0.0005, 0.001, 0.005, 0.01})
        .Register(registry);
    latency_ = latency_family.Add({{"algorithm", "ring"}});
    
    // Lookups counter
    auto& lookups_family = BuildCounter()
        .Name("consistent_hash_lookups_total")
        .Help("Total number of lookups")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    lookups_ = lookups_family.Add({{"algorithm", "ring"}});
    
    // Errors counter
    auto& errors_family = BuildCounter()
        .Name("consistent_hash_lookup_errors_total")
        .Help("Total number of lookup errors")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    errors_ = errors_family.Add({{"algorithm", "ring"}});
    
    // Migrations counter
    auto& migrations_family = BuildCounter()
        .Name("consistent_hash_migrations_total")
        .Help("Total number of node migrations")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    migrations_ = migrations_family.Add({{"algorithm", "ring"}});
    
    // Node count gauge
    auto& nodes_family = BuildGauge()
        .Name("consistent_hash_nodes")
        .Help("Number of nodes in the cluster")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    nodes_ = nodes_family.Add({{"algorithm", "ring"}});
    
    // Distribution skew gauge
    auto& skew_family = BuildGauge()
        .Name("consistent_hash_distribution_skew")
        .Help("Distribution skew (stddev/mean)")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    skew_ = skew_family.Add({{"algorithm", "ring"}});
    
    // Hash quality metrics
    auto& avalanche_family = BuildGauge()
        .Name("consistent_hash_avalanche_effect")
        .Help("Hash avalanche effect (0-1, ideal ~0.5)")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    avalanche_ = avalanche_family.Add({{"algorithm", "ring"}});
    
    auto& collision_family = BuildGauge()
        .Name("consistent_hash_collision_rate")
        .Help("Hash collision rate")
        .Labels({{"algorithm", ""}})
        .Register(registry);
    collision_ = collision_family.Add({{"algorithm", "ring"}});
    
    // Node health
    auto& health_family = BuildGauge()
        .Name("consistent_hash_node_health")
        .Help("Node health status (0=unknown, 1=healthy, 2=degraded, 3=unhealthy, 4=draining)")
        .Labels({{"node_id", ""}, {"status", ""}})
        .Register(registry);
    health_ = &health_family;
  }

  void UpdateMetrics() {
    // Update RingHasher metrics
    if (ring_hasher_) {
      auto stats = ring_hasher_->stats();
      nodes_->Set(stats.unique_nodes_used);
      skew_->Set(stats.skew);
      migrations_->Increment(stats.migration_count);
    }
    
    // Update JumpHasher metrics
    if (jump_hasher_) {
      auto stats = jump_hasher_->stats();
      // ... similar updates
    }
    
    // Update health metrics
    if (health_checker_) {
      auto healthy_nodes = health_checker_->get_healthy_nodes();
      auto all_nodes = health_checker_->get_available_nodes();
      // Export health status for each node
      // This would require iterating over all known nodes
    }
  }

  std::vector<MetricFamily> Collect() override {
    // This is called by Prometheus when scraping
    // We return the metrics we've created
    return {};
  }

private:
  std::shared_ptr<RingHasher<>> ring_hasher_;
  std::shared_ptr<JumpHasher<>> jump_hasher_;
  std::shared_ptr<MaglevHasher<>> maglev_hasher_;
  std::shared_ptr<KetamaHasher<>> ketama_hasher_;
  std::shared_ptr<AnchorHasher<>> anchor_hasher_;
  std::shared_ptr<health::HealthChecker> health_checker_;
  
  Histogram& latency_;
  Counter& lookups_;
  Counter& errors_;
  Counter& migrations_;
  Gauge& nodes_;
  Gauge& skew_;
  Gauge& avalanche_;
  Gauge& collision_;
  GaugeFamily* health_;
};

int main() {
  // Create hashers
  auto ring_hasher = std::make_shared<RingHasher<>>();
  auto jump_hasher = std::make_shared<JumpHasher<>>();
  auto maglev_hasher = std::make_shared<MaglevHasher<>>(65537);
  auto ketama_hasher = std::make_shared<KetamaHasher<>>();
  auto anchor_hasher = std::make_shared<AnchorHasher<>>();
  
  // Add some nodes
  for (int i = 0; i < 10; ++i) {
    Node<> node{"node-" + std::to_string(i), "10.0.0." + std::to_string(i), 100, "", NodeStatus::Active, {}};
    ring_hasher->add_node(node);
    jump_hasher->add_node(node);
    maglev_hasher->add_node(node);
    ketama_hasher->add_node(node);
    anchor_hasher->add_node(node);
  }
  
  // Create health checker
  auto health_checker = std::make_shared<health::HealthChecker>();
  for (int i = 0; i < 10; ++i) {
    std::string node_id = "node-" + std::to_string(i);
    health_checker->register_node(node_id, [](const std::string&, std::string&, std::chrono::milliseconds) {
      return true; // Simulate healthy
    });
  }
  health_checker->start();
  
  // Create collector
  auto collector = std::make_shared<ConsistentHashCollector>(
      ring_hasher, jump_hasher, maglev_hasher, ketama_hasher, anchor_hasher, health_checker);
  
  // Register with Prometheus
  Registry registry;
  registry.RegisterCollectable(collector);
  
  // Start HTTP server
  Exposer exposer{"0.0.0.0:9090"};
  exposer.RegisterCollectable(collector);
  
  std::cout << "Prometheus metrics available at http://0.0.0.0:9090/metrics" << std::endl;
  
  // Periodically update metrics
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    collector->UpdateMetrics();
  }
  
  return 0;
}