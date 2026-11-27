#include <consistent_hash/chaos.hpp>
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace consistent_hash;
using namespace consistent_hash::chaos;

TEST(ChaosTest, NetworkSimulatorBasic) {
    std::vector<std::string> nodes = {"node1", "node2", "node3"};
    NetworkSimulator sim(nodes);
    
    // Initially all can communicate
    EXPECT_TRUE(sim.can_communicate("node1", "node2"));
    EXPECT_TRUE(sim.can_communicate("node2", "node3"));
    
    // Create partition
    sim.create_partition({"node1"}, {"node2", "node3"});
    
    // node1 can't communicate with node2/node3
    EXPECT_FALSE(sim.can_communicate("node1", "node2"));
    EXPECT_FALSE(sim.can_communicate("node1", "node3"));
    
    // node2 can communicate with node3
    EXPECT_TRUE(sim.can_communicate("node2", "node3"));
    
    // Heal partition
    sim.heal_partition();
    EXPECT_TRUE(sim.can_communicate("node1", "node2"));
    EXPECT_TRUE(sim.can_communicate("node1", "node3"));
}

TEST(ChaosTest, NetworkSimulatorLatency) {
    std::vector<std::string> nodes = {"node1", "node2"};
    NetworkSimulator sim(nodes);
    
    // Inject latency
    sim.inject_latency("node1", std::chrono::milliseconds(100));
    auto state = sim.get_link_state("node1");
    EXPECT_EQ(state.added_latency, std::chrono::milliseconds(100));
    
    // Remove latency
    sim.remove_latency("node1");
    state = sim.get_link_state("node1");
    EXPECT_EQ(state.added_latency, std::chrono::milliseconds(0));
}

TEST(ChaosTest, NetworkSimulatorPacketLoss) {
    std::vector<std::string> nodes = {"node1", "node2"};
    NetworkSimulator sim(nodes);
    
    // Inject packet loss
    sim.inject_packet_loss("node1", 0.5);
    auto state = sim.get_link_state("node1");
    EXPECT_DOUBLE_EQ(state.packet_loss_rate, 0.5);
    
    // Remove packet loss
    sim.remove_packet_loss("node1");
    state = sim.get_link_state("node1");
    EXPECT_DOUBLE_EQ(state.packet_loss_rate, 0.0);
}

TEST(ChaosTest, ChaosConfigScenarios) {
    auto single = scenarios::single_node_failure();
    EXPECT_EQ(single.duration, std::chrono::seconds(60));
    
    auto partition = scenarios::network_partition();
    EXPECT_EQ(partition.duration, std::chrono::seconds(120));
    
    auto latency = scenarios::latency_spikes();
    EXPECT_EQ(latency.duration, std::chrono::seconds(60));
    
    auto combined = scenarios::combined_chaos();
    EXPECT_EQ(combined.duration, std::chrono::seconds(300));
    
    auto prod = scenarios::production_like();
    EXPECT_EQ(prod.duration, std::chrono::seconds(600));
}

TEST(ChaosTest, RingHasherChaosExperiment) {
    // Create a simple hasher
    auto hasher = std::make_shared<RingHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node3", "10.0.0.3:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2", "node3"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(5);  // Short for test
    config.node_failure_rate = 0.0;  // Disable random failures for deterministic test
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.latency_injection_rate = 0.0;
    config.packet_loss_rate = 0.0;
    
    ChaosExperiment<RingHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_GE(result.availability_percent, 0.0);
    EXPECT_LE(result.availability_percent, 100.0);
    EXPECT_GE(result.final_skew, 0.0);
}

TEST(ChaosTest, JumpHasherChaosExperiment) {
    auto hasher = std::make_shared<JumpHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(3);
    config.node_failure_rate = 0.0;
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.latency_injection_rate = 0.0;
    config.packet_loss_rate = 0.0;
    
    ChaosExperiment<JumpHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_GE(result.availability_percent, 0.0);
}

TEST(ChaosTest, KetamaHasherChaosExperiment) {
    auto hasher = std::make_shared<KetamaHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(3);
    config.node_failure_rate = 0.0;
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.latency_injection_rate = 0.0;
    config.packet_loss_rate = 0.0;
    
    ChaosExperiment<KetamaHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_GE(result.availability_percent, 0.0);
}

TEST(ChaosTest, AnchorHasherChaosExperiment) {
    auto hasher = std::make_shared<AnchorHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(3);
    config.node_failure_rate = 0.0;
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.latency_injection_rate = 0.0;
    config.packet_loss_rate = 0.0;
    
    ChaosExperiment<AnchorHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_GE(result.availability_percent, 0.0);
}

TEST(ChaosTest, ChaosExperimentWithLatency) {
    auto hasher = std::make_shared<RingHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(3);
    config.latency_injection_rate = 1.0;  // High rate for test
    config.min_added_latency = std::chrono::milliseconds(10);
    config.max_added_latency = std::chrono::milliseconds(50);
    config.node_failure_rate = 0.0;
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.packet_loss_rate = 0.0;
    
    ChaosExperiment<RingHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_GT(result.avg_latency_ns, 0);
}

TEST(ChaosTest, ChaosExperimentWithPacketLoss) {
    auto hasher = std::make_shared<RingHasher<>>();
    hasher->add_node(Node<>("node1", "10.0.0.1:11211", 100, "", NodeStatus::Active, {}));
    hasher->add_node(Node<>("node2", "10.0.0.2:11211", 100, "", NodeStatus::Active, {}));
    
    std::vector<std::string> nodes = {"node1", "node2"};
    
    ChaosConfig config;
    config.duration = std::chrono::seconds(3);
    config.packet_loss_rate = 1.0;  // High rate for test
    config.min_packet_loss = 0.1;
    config.max_packet_loss = 0.5;
    config.node_failure_rate = 0.0;
    config.node_recovery_rate = 0.0;
    config.network_partition_rate = 0.0;
    config.network_heal_rate = 0.0;
    config.latency_injection_rate = 0.0;
    
    ChaosExperiment<RingHasher<>> experiment(hasher, nodes, config);
    auto result = experiment.run();
    
    EXPECT_GT(result.total_operations, 0);
    EXPECT_LT(result.availability_percent, 100.0);  // Some operations should fail due to packet loss
}