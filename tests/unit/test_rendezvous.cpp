#include <consistent_hash/rendezvous.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace consistent_hash;

TEST(RendezvousTest, LookupReturnsValidNode) {
    RendezvousHasher<> h;
    for (int i = 0; i < 8; ++i) {
        h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i) {
        std::string owner = h.lookup("key:" + std::to_string(i));
        EXPECT_TRUE(owner.rfind("n", 0) == 0);
        seen.insert(owner);
    }
    EXPECT_EQ(seen.size(), 8u);
}

TEST(RendezvousTest, Deterministic) {
    RendezvousHasher<> h;
    h.add_node({"a", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"b", "", 100, "", NodeStatus::Active, {}});
    std::string first = h.lookup("rv-key");
    RendezvousHasher<> h2;
    h2.add_node({"a", "", 100, "", NodeStatus::Active, {}});
    h2.add_node({"b", "", 100, "", NodeStatus::Active, {}});
    EXPECT_EQ(h2.lookup("rv-key"), first);
}

TEST(RendezvousTest, ThrowsWhenEmpty) {
    RendezvousHasher<> h;
    EXPECT_THROW(h.lookup("key"), std::runtime_error);
}

TEST(RendezvousTest, RemoveNode) {
    RendezvousHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
    h.remove_node("n1");
    EXPECT_FALSE(h.has_node("n1"));
    EXPECT_EQ(h.node_count(), 1u);
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(h.lookup("k" + std::to_string(i)), "n2");
    }
}

TEST(RendezvousTest, LookupNDistinct) {
    RendezvousHasher<> h;
    for (int i = 0; i < 6; ++i) {
        h.add_node({"n" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
    }
    auto owners = h.lookup_n("k", 4);
    ASSERT_EQ(owners.size(), 4u);
    std::set<std::string> seen(owners.begin(), owners.end());
    EXPECT_EQ(seen.size(), 4u);
}

TEST(RendezvousTest, RebalanceTracksMigration) {
    RendezvousHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();
    uint64_t a = h.stats().migration_count;
    h.add_node({"n3", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();
    EXPECT_GT(h.stats().migration_count, a);
}
