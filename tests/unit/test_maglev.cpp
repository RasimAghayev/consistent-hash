#include <consistent_hash/maglev.hpp>

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

using namespace consistent_hash;

TEST(MaglevTest, LookupReturnsValidNode) {
    MaglevHasher<> h(97);  // small prime table for fast tests
    for (int i = 0; i < 6; ++i) {
        h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    std::set<std::string> seen;
    for (int i = 0; i < 500; ++i) {
        auto opt = h.lookup("key:" + std::to_string(i));
        EXPECT_TRUE(opt.has_value());
        EXPECT_TRUE(opt.value().rfind("n", 0) == 0);
        seen.insert(opt.value());
    }
    EXPECT_EQ(seen.size(), 6u);
}

TEST(MaglevTest, Deterministic) {
    MaglevHasher<> h(97);
    h.add_node({"a", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"b", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"c", "", 100, "", NodeStatus::Active, {}});
    std::string first = h.lookup("stable-key").value();
    MaglevHasher<> h2(97);
    h2.add_node({"a", "", 100, "", NodeStatus::Active, {}});
    h2.add_node({"b", "", 100, "", NodeStatus::Active, {}});
    h2.add_node({"c", "", 100, "", NodeStatus::Active, {}});
    EXPECT_EQ(h2.lookup("stable-key").value(), first);
}

TEST(MaglevTest, ThrowsWhenEmpty) {
    MaglevHasher<> h(97);
    EXPECT_THROW(h.lookup_or_throw("key"), std::runtime_error);
}

TEST(MaglevTest, LookupReturnsNulloptWhenEmpty) {
    MaglevHasher<> h(97);
    EXPECT_FALSE(h.lookup("key").has_value());
}

TEST(MaglevTest, RemoveNode) {
    MaglevHasher<> h(97);
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
    h.remove_node("n1");
    EXPECT_FALSE(h.has_node("n1"));
    EXPECT_EQ(h.node_count(), 1u);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(h.lookup("k" + std::to_string(i)).value(), "n2");
    }
}

TEST(MaglevTest, DistributionSkewLow) {
    MaglevHasher<> h(251);  // prime
    const int N = 8;
    for (int i = 0; i < N; ++i) {
        h.add_node({"n" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
    }
    const int keys = 5000;
    std::map<std::string, int> counts;
    for (int i = 0; i < keys; ++i) {
        if (auto opt = h.lookup("key:" + std::to_string(i)); opt) {
            counts[opt.value()]++;
        }
    }
    double mean = static_cast<double>(keys) / N;
    double sq = 0.0;
    for (const auto& [node, c] : counts) {
        double d = static_cast<double>(c) - mean;
        sq += d * d;
    }
    double stddev = std::sqrt(sq / N);
    double skew = stddev / mean;
    EXPECT_LT(skew, 0.35);
}

TEST(MaglevTest, RebalanceTracksMigration) {
    MaglevHasher<> h(97);
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();
    uint64_t a = h.stats().migration_count;
    h.add_node({"n3", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();
    EXPECT_GT(h.stats().migration_count, a);
}