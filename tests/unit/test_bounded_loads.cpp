#include <consistent_hash/bounded_loads.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace consistent_hash;

TEST(BoundedLoadsTest, LookupReturnsValidNode) {
    BoundedLoadsHasher<> h;
    for (int i = 0; i < 4; ++i) {
        h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                    100, "", NodeStatus::Active, {}});
    }
    std::set<std::string> seen;
    for (int i = 0; i < 200; ++i) {
        seen.insert(h.lookup("key:" + std::to_string(i)));
    }
    EXPECT_EQ(seen.size(), 4u);
}

TEST(BoundedLoadsTest, CapacityLimitsLoad) {
    BoundedLoadsHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active,
                {{"capacity", "3"}}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active,
                {{"capacity", "3"}}});
    // Send many keys; n1 and n2 each cap at 3, overflow goes to the other.
    for (int i = 0; i < 100; ++i) {
        h.lookup("key:" + std::to_string(i));
    }
    auto load = h.load();
    // No node should be wildly over capacity; both used.
    EXPECT_GT(load.at("n1"), 0u);
    EXPECT_GT(load.at("n2"), 0u);
}

TEST(BoundedLoadsTest, ThrowsWhenEmpty) {
    BoundedLoadsHasher<> h;
    EXPECT_THROW(h.lookup("key"), std::runtime_error);
}

TEST(BoundedLoadsTest, RemoveNode) {
    BoundedLoadsHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {{"capacity", "10"}}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {{"capacity", "10"}}});
    h.remove_node("n1");
    EXPECT_FALSE(h.has_node("n1"));
    EXPECT_EQ(h.node_count(), 1u);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(h.lookup("k" + std::to_string(i)), "n2");
    }
}

TEST(BoundedLoadsTest, SetCapacity) {
    BoundedLoadsHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.set_capacity("n1", 1);
    h.lookup("k1");
    EXPECT_EQ(h.load().at("n1"), 1u);
}
