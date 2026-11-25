#include <consistent_hash/ring.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

using namespace consistent_hash;

TEST(RingTest, SingleNodeLookup) {
    RingHasher<> h;
    h.add_node({"n1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    EXPECT_EQ(h.lookup("key:1"), "n1");
    EXPECT_EQ(h.lookup("key:2"), "n1");
    EXPECT_EQ(h.node_count(), 1);
    EXPECT_TRUE(h.has_node("n1"));
    EXPECT_FALSE(h.has_node("n2"));
}

TEST(RingTest, LookupThrowsWhenEmpty) {
    RingHasher<> h;
    EXPECT_THROW(h.lookup("key"), std::runtime_error);
}

TEST(RingTest, RemoveNode) {
    RingHasher<> h;
    h.add_node({"n1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "10.0.0.2:8080", 100, "", NodeStatus::Active, {}});
    h.remove_node("n1");
    EXPECT_FALSE(h.has_node("n1"));
    EXPECT_TRUE(h.has_node("n2"));
    EXPECT_EQ(h.node_count(), 1);
    // All remaining keys map to n2.
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(h.lookup("k" + std::to_string(i)), "n2");
    }
}

TEST(RingTest, LookupNReturnsDistinct) {
    RingHasher<> h;
    for (int i = 0; i < 5; ++i) {
        h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    auto owners = h.lookup_n("hotkey", 3);
    ASSERT_EQ(owners.size(), 3);
    EXPECT_NE(owners[0], owners[1]);
    EXPECT_NE(owners[1], owners[2]);
}

TEST(RingTest, DistributionSkewLow) {
    RingHasher<> h;
    const int N = 10;
    for (int i = 0; i < N; ++i) {
        h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    const int keys = 10000;
    std::map<std::string, int> counts;
    for (int i = 0; i < keys; ++i) {
        counts[h.lookup("key:" + std::to_string(i))]++;
    }
    double mean = static_cast<double>(keys) / N;
    double sq = 0.0;
    double mn = counts.begin()->second, mx = mn;
    for (const auto& [node, c] : counts) {
        double d = static_cast<double>(c) - mean;
        sq += d * d;
        if (c < mn) mn = c;
        if (c > mx) mx = c;
    }
    double stddev = std::sqrt(sq / N);
    double skew = stddev / mean;
    EXPECT_LT(skew, 0.3) << "skew=" << skew << " min=" << mn << " max=" << mx;
}

TEST(RingTest, RebalanceMovesFraction) {
    RingHasher<> h;
    h.add_node({"n1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "10.0.0.2:8080", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();  // snapshot

    const int keys = 200;
    std::map<std::string, std::string> before;
    for (int i = 0; i < keys; ++i) {
        before["k" + std::to_string(i)] = h.lookup("k" + std::to_string(i));
    }

    h.add_node({"n3", "10.0.0.3:8080", 100, "", NodeStatus::Active, {}});
    auto migrations = h.rebalance();
    EXPECT_GT(migrations.size(), 0u);

    size_t moved = 0;
    for (const auto& [k, b] : before) {
        if (h.lookup(k) != b) ++moved;
    }
    // Adding one node to two should move only a fraction of keys.
    EXPECT_LT(static_cast<double>(moved) / keys, 0.5);
}

TEST(RingTest, RebalanceTracksMigrationCount) {
    RingHasher<> h;
    h.add_node({"n1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "10.0.0.2:8080", 100, "", NodeStatus::Active, {}});
    auto m1 = h.rebalance();
    uint64_t after_two = h.stats().migration_count;

    h.add_node({"n3", "10.0.0.3:8080", 100, "", NodeStatus::Active, {}});
    auto m2 = h.rebalance();
    uint64_t after_three = h.stats().migration_count;

    EXPECT_GT(after_two, 0u);
    EXPECT_GT(after_three, after_two);
}

TEST(RingTest, ReaddNodeReplaces) {
    RingHasher<> h;
    h.add_node({"n1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    h.add_node({"n1", "10.0.0.9:9090", 200, "", NodeStatus::Active, {}});
    EXPECT_EQ(h.node_count(), 1u);
    auto node = h.stats();  // no throw
    (void)node;
}
