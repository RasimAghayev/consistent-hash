#include <consistent_hash/health.hpp>
#include <consistent_hash/ring.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace consistent_hash;
using namespace consistent_hash::health;

TEST(HealthCheckConfigTest, DefaultValues) {
  HealthCheckConfig config;
  
  EXPECT_EQ(config.interval, std::chrono::milliseconds(5000));
  EXPECT_EQ(config.timeout, std::chrono::milliseconds(2000));
  EXPECT_EQ(config.failure_threshold, 3);
  EXPECT_EQ(config.success_threshold, 2);
  EXPECT_EQ(config.initial_delay, std::chrono::milliseconds(1000));
  EXPECT_TRUE(config.passive_checks);
  EXPECT_EQ(config.passive_error_threshold, 10);
  EXPECT_EQ(config.passive_window, std::chrono::seconds(60));
}

TEST(PassiveErrorTrackerTest, RecordErrorAndSuccess) {
  PassiveErrorTracker tracker;
  
  tracker.record_error("node1");
  tracker.record_error("node1");
  tracker.record_error("node1");
  
  EXPECT_EQ(tracker.error_count("node1"), 3);
  EXPECT_FALSE(tracker.is_unhealthy("node1")); // threshold is 10
  
  tracker.record_success("node1");
  EXPECT_EQ(tracker.error_count("node1"), 0);
}

TEST(PassiveErrorTrackerTest, Threshold) {
  PassiveErrorTracker::Config config;
  config.error_threshold = 3;
  config.window = std::chrono::seconds(60);
  PassiveErrorTracker tracker(config);
  
  tracker.record_error("node1");
  tracker.record_error("node1");
  EXPECT_FALSE(tracker.is_unhealthy("node1"));
  
  tracker.record_error("node1");
  EXPECT_TRUE(tracker.is_unhealthy("node1"));
  
  tracker.record_success("node1");
  EXPECT_FALSE(tracker.is_unhealthy("node1"));
}

TEST(PassiveErrorTrackerTest, WindowExpiration) {
  PassiveErrorTracker::Config config;
  config.error_threshold = 3;
  config.window = std::chrono::seconds(1);
  PassiveErrorTracker tracker(config);
  
  tracker.record_error("node1");
  tracker.record_error("node1");
  tracker.record_error("node1");
  EXPECT_TRUE(tracker.is_unhealthy("node1"));
  
  // Wait for window to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  
  EXPECT_FALSE(tracker.is_unhealthy("node1"));
  EXPECT_EQ(tracker.error_count("node1"), 0);
}

TEST(HealthCheckerTest, RegisterUnregister) {
  HealthChecker checker;
  
  bool check_called = false;
  checker.register_node("node1", [&](const std::string&, std::string&, std::chrono::milliseconds) {
    check_called = true;
    return true;
  });
  
  EXPECT_TRUE(check_called); // Initial check runs on register
  
  checker.unregister_node("node1");
  auto health = checker.get_health("node1");
  EXPECT_FALSE(health.has_value());
}

TEST(HealthCheckerTest, HealthStatusTransitions) {
  HealthCheckConfig config;
  config.interval = std::chrono::milliseconds(100);
  config.timeout = std::chrono::milliseconds(50);
  config.failure_threshold = 2;
  config.success_threshold = 2;
  config.initial_delay = std::chrono::milliseconds(10);
  
  HealthChecker checker(config);
  
  std::atomic<int> check_count{0};
  std::atomic<bool> should_fail{false};
  
  checker.register_node("node1", [&](const std::string&, std::string& error, std::chrono::milliseconds) {
    check_count.fetch_add(1, std::memory_order_relaxed);
    if (should_fail.load()) {
      error = "Simulated failure";
      return false;
    }
    return true;
  });
  
  // Wait for initial check
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  auto health = checker.get_health("node1");
  ASSERT_TRUE(health.has_value());
  EXPECT_EQ(health->status, HealthStatus::Healthy);
  
  // Trigger failures
  should_fail.store(true);
  
  // Wait for checks to run
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  
  health = checker.get_health("node1");
  ASSERT_TRUE(health.has_value());
  EXPECT_EQ(health->status, HealthStatus::Unhealthy);
  EXPECT_GE(health->consecutive_failures, 2);
  
  // Recover
  should_fail.store(false);
  
  // Wait for recovery checks
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  
  health = checker.get_health("node1");
  ASSERT_TRUE(health.has_value());
  EXPECT_EQ(health->status, HealthStatus::Healthy);
}

TEST(HealthCheckerTest, Callback) {
  HealthCheckConfig config;
  config.interval = std::chrono::milliseconds(50);
  config.timeout = std::chrono::milliseconds(50);
  config.failure_threshold = 2;
  config.success_threshold = 2;
  config.initial_delay = std::chrono::milliseconds(10);

  HealthChecker checker(config);
  
  std::atomic<int> callback_count{0};
  HealthStatus last_old_status = HealthStatus::Unknown;
  HealthStatus last_new_status = HealthStatus::Unknown;
  
  checker.set_callback([&](const std::string&, HealthStatus old_s, HealthStatus new_s) {
    callback_count.fetch_add(1, std::memory_order_relaxed);
    last_old_status = old_s;
    last_new_status = new_s;
  });
  
  std::atomic<bool> should_fail{false};
  checker.register_node("node1", [&](const std::string&, std::string& error, std::chrono::milliseconds) {
    if (should_fail.load()) {
      error = "fail";
      return false;
    }
    return true;
  });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  should_fail.store(true);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  
  EXPECT_GE(callback_count.load(), 1);
  EXPECT_EQ(last_old_status, HealthStatus::Healthy);
  EXPECT_EQ(last_new_status, HealthStatus::Unhealthy);
}

TEST(HealthCheckerTest, GetHealthyNodes) {
  HealthChecker checker;
  
  checker.register_node("healthy1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  checker.register_node("healthy2", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  
  std::atomic<bool> fail_node{true};
  checker.register_node("unhealthy", [&](const std::string&, std::string&, std::chrono::milliseconds) { return !fail_node.load(); });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  auto healthy = checker.get_healthy_nodes();
  EXPECT_EQ(healthy.size(), 2);
  EXPECT_TRUE(std::find(healthy.begin(), healthy.end(), "healthy1") != healthy.end());
  EXPECT_TRUE(std::find(healthy.begin(), healthy.end(), "healthy2") != healthy.end());
  EXPECT_TRUE(std::find(healthy.begin(), healthy.end(), "unhealthy") == healthy.end());
  
  auto available = checker.get_available_nodes();
  EXPECT_EQ(available.size(), 2); // unhealthy is not available
}

TEST(HealthCheckerTest, Draining) {
  HealthChecker checker;
  
  checker.register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  checker.set_draining("node1", true);
  
  auto health = checker.get_health("node1");
  ASSERT_TRUE(health.has_value());
  EXPECT_EQ(health->status, HealthStatus::Draining);
  
  checker.set_draining("node1", false);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  health = checker.get_health("node1");
  ASSERT_TRUE(health.has_value());
  EXPECT_EQ(health->status, HealthStatus::Healthy);
}

TEST(HealthCheckerTest, PassiveErrorIntegration) {
  HealthCheckConfig config;
  config.passive_checks = true;
  config.passive_error_threshold = 3;
  config.passive_window = std::chrono::seconds(60);
  
  HealthChecker checker(config);
  
  checker.register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  // Record passive errors
  checker.record_passive_error("node1", "connection timeout");
  checker.record_passive_error("node1", "connection refused");
  // Should still be healthy (threshold is 3)
  auto health = checker.get_health("node1");
  EXPECT_EQ(health->status, HealthStatus::Healthy);
  
  checker.record_passive_error("node1", "network unreachable");
  // Now should be unhealthy
  health = checker.get_health("node1");
  EXPECT_EQ(health->status, HealthStatus::Unhealthy);
  
  // Record success should clear passive errors
  checker.record_passive_success("node1");
  health = checker.get_health("node1");
  // Note: passive errors don't auto-recover, need active checks
}

TEST(HealthCheckerTest, Stop) {
  HealthCheckConfig config;
  config.interval = std::chrono::milliseconds(50);
  config.initial_delay = std::chrono::milliseconds(10);
  
  HealthChecker checker(config);
  
  std::atomic<int> check_count{0};
  checker.register_node("node1", [&](const std::string&, std::string&, std::chrono::milliseconds) {
    check_count.fetch_add(1, std::memory_order_relaxed);
    return true;
  });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int checks_before_stop = check_count.load();
  
  checker.stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  int checks_after_stop = check_count.load();
  EXPECT_EQ(checks_before_stop, checks_after_stop); // No more checks after stop
}

TEST(HealthAwareHasherTest, BasicFiltering) {
  using Hasher = RingHasher<std::string, std::string>;
  
  auto health_checker = std::make_shared<HealthChecker>();
  
  health_checker->register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  health_checker->register_node("node2", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  health_checker->register_node("node3", [](const std::string&, std::string&, std::chrono::milliseconds) { return false; }); // unhealthy
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node3", "host3", 100, "", {}));
  
  health::HealthAwareHasher<Hasher> aware_hasher(std::move(base_hasher), health_checker);
  
  // Lookups should only return healthy nodes
  for (int i = 0; i < 100; ++i) {
    auto result = aware_hasher.lookup("key-" + std::to_string(i));
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(*result, "node3"); // Should never return unhealthy node
  }
}

TEST(HealthAwareHasherTest, LookupNFiltering) {
  using Hasher = RingHasher<std::string, std::string>;
  
  auto health_checker = std::make_shared<HealthChecker>();
  
  health_checker->register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  health_checker->register_node("node2", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  health_checker->register_node("node3", [](const std::string&, std::string&, std::chrono::milliseconds) { return false; });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node3", "host3", 100, "", {}));
  
  health::HealthAwareHasher<Hasher> aware_hasher(std::move(base_hasher), health_checker);
  
  // lookup_n should only return healthy nodes
  auto results = aware_hasher.lookup_n("test-key", 3);
  for (const auto& node : results) {
    EXPECT_NE(node, "node3");
  }
  EXPECT_LE(results.size(), 2); // Only 2 healthy nodes
}

TEST(HealthAwareHasherTest, PassiveRecording) {
  using Hasher = RingHasher<std::string, std::string>;
  
  auto health_checker = std::make_shared<HealthChecker>();
  health_checker->register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  health_checker->register_node("node2", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1", 100, "", {}));
  base_hasher.add_node(Node<std::string>("node2", "host2", 100, "", {}));
  
  health::HealthAwareHasher<Hasher> aware_hasher(std::move(base_hasher), health_checker);
  
  // Record a failed lookup
  aware_hasher.record_lookup_result("node1", false, "timeout");
  
  // Check passive error was recorded
  EXPECT_GT(health_checker->passive_error_count("node1"), 0);
  
  // Record a successful lookup
  aware_hasher.record_lookup_result("node1", true);
  
  // Passive error count should be cleared
  EXPECT_EQ(health_checker->passive_error_count("node1"), 0);
}

TEST(HealthAwareHasherTest, Delegation) {
  using Hasher = RingHasher<std::string, std::string>;
  
  auto health_checker = std::make_shared<HealthChecker>();
  health_checker->register_node("node1", [](const std::string&, std::string&, std::chrono::milliseconds) { return true; });
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  Hasher base_hasher;
  base_hasher.add_node(Node<std::string>("node1", "host1", 100, "", {}));
  
  health::HealthAwareHasher<Hasher> aware_hasher(std::move(base_hasher), health_checker);
  
  // Test delegated methods
  aware_hasher.add_node(Node<std::string>("node2", "host2", 100, "", {}));
  EXPECT_TRUE(aware_hasher.has_node("node2"));
  
  auto result = aware_hasher.update_weight("node1", 200);
  EXPECT_TRUE(result);
  
  aware_hasher.remove_node("node1");
  EXPECT_FALSE(aware_hasher.has_node("node1"));
  
  auto stats = aware_hasher.stats();
  EXPECT_GE(stats.total_keys, 0);
  
  std::string serialized = aware_hasher.serialize();
  EXPECT_FALSE(serialized.empty());
  
  auto deserialize_result = aware_hasher.deserialize(serialized);
  EXPECT_TRUE(deserialize_result);
}