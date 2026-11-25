#include <consistent_hash/node_manager.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace consistent_hash;

TEST(NodeManagerTest, AddGetRemove) {
    NodeManager mgr;
    mgr.add_node({"n1", "10.0.0.1:8080", 100, "zone-a", NodeStatus::Active, {}});
    mgr.add_node({"n2", "10.0.0.2:8080", 200, "zone-b", NodeStatus::Active, {}});
    EXPECT_EQ(mgr.size(), 2u);
    EXPECT_TRUE(mgr.has_node("n1"));
    EXPECT_TRUE(mgr.has_node("n2"));
    EXPECT_FALSE(mgr.has_node("n3"));

    auto n1 = mgr.get_node("n1");
    EXPECT_EQ(n1.address, "10.0.0.1:8080");
    EXPECT_EQ(n1.weight, 100);
    EXPECT_EQ(n1.zone, "zone-a");

    EXPECT_THROW(mgr.get_node("missing"), std::runtime_error);

    mgr.remove_node("n1");
    EXPECT_FALSE(mgr.has_node("n1"));
    EXPECT_EQ(mgr.size(), 1u);
}

TEST(NodeManagerTest, ListNodes) {
    NodeManager mgr;
    for (int i = 0; i < 5; ++i) {
        mgr.add_node({"n" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
    }
    auto nodes = mgr.list_nodes();
    EXPECT_EQ(nodes.size(), 5u);
}

TEST(NodeManagerTest, ReplaceNode) {
    NodeManager mgr;
    mgr.add_node({"n1", "old", 100, "", NodeStatus::Active, {}});
    mgr.add_node({"n1", "new", 200, "", NodeStatus::Active, {}});
    EXPECT_EQ(mgr.size(), 1u);
    auto n = mgr.get_node("n1");
    EXPECT_EQ(n.address, "new");
    EXPECT_EQ(n.weight, 200);
}
