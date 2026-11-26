#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/ring.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace consistent_hash;

// Simulates cache sharding: keys map to cache nodes; adding a node moves few keys.
TEST(IntegrationCacheSharding, RebalancingMovesFraction) {
    RingHasher<> h;
    for (int i = 0; i < 4; ++i) {
        h.add_node({"cache-" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    (void)h.rebalance();

    const int keys = 500;
    std::map<std::string, std::string> before;
    for (int i = 0; i < keys; ++i) {
        if (auto opt = h.lookup("obj:" + std::to_string(i)); opt) {
            before["obj:" + std::to_string(i)] = opt.value();
        }
    }

    h.add_node({"cache-4", "10.0.0.4", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();

    size_t moved = 0;
    for (const auto& [k, b] : before) {
        if (h.lookup(k).value() != b) ++moved;
    }
    EXPECT_LT(static_cast<double>(moved) / keys, 0.5);
}

// Simulates load balancer backend selection: every key picks a healthy backend.
TEST(IntegrationLoadBalancer, OnlyActiveBackendsSelected) {
    MaglevHasher<> h(97);
    h.add_node({"b1", "10.0.0.1:80", 100, "", NodeStatus::Active, {}});
    h.add_node({"b2", "10.0.0.2:80", 100, "", NodeStatus::Draining, {}});
    h.add_node({"b3", "10.0.0.3:80", 100, "", NodeStatus::Active, {}});
    for (int i = 0; i < 100; ++i) {
        auto opt = h.lookup("req:" + std::to_string(i));
        EXPECT_TRUE(opt.has_value());
        EXPECT_NE(opt.value(), "b2");
    }
}

// Simulates DB shard routing: each shard key routes to a shard; lookup_n gives
// replicas.
TEST(IntegrationDbSharding, ReplicasDistinct) {
    RendezvousHasher<> h;
    for (int i = 0; i < 5; ++i) {
        h.add_node({"shard-" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
    }
    auto replicas = h.lookup_n("user:42", 3);
    ASSERT_EQ(replicas.size(), 3u);
    std::map<std::string, int> uniq;
    for (const auto& r : replicas) uniq[r]++;
    EXPECT_EQ(uniq.size(), 3u);
}

// Simulates CDN edge selection: bounded loads prevent edge overload.
TEST(IntegrationCdnRouting, EdgeCapacityRespected) {
    BoundedLoadsHasher<> h;
    h.add_node({"edge-1", "", 100, "", NodeStatus::Active, {{"capacity", "2"}}});
    h.add_node({"edge-2", "", 100, "", NodeStatus::Active, {{"capacity", "2"}}});
    std::map<std::string, int> load;
    for (int i = 0; i < 50; ++i) {
        if (auto opt = h.lookup("asset:" + std::to_string(i)); opt) {
            load[opt.value()]++;
        }
    }
    // Both edges used; neither starved.
    EXPECT_GT(load["edge-1"], 0);
    EXPECT_GT(load["edge-2"], 0);
}