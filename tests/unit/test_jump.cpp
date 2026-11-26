#include <consistent_hash/jump.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace consistent_hash;

TEST(JumpTest, LookupReturnsValidNode) {
  JumpHasher<> h;
  for (int i = 0; i < 10; ++i) {
    h.add_node({"n" + std::to_string(i),
                "10.0.0." + std::to_string(i),
                100,
                "",
                NodeStatus::Active,
                {}});
  }
  std::set<std::string> seen;
  for (int i = 0; i < 1000; ++i) {
    auto opt = h.lookup("key:" + std::to_string(i));
    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(opt.value().rfind("n", 0) == 0);
    seen.insert(opt.value());
  }
  EXPECT_EQ(seen.size(), 10u);
}

TEST(JumpTest, Deterministic) {
  JumpHasher<> h;
  h.add_node({"a", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"b", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"c", "", 100, "", NodeStatus::Active, {}});
  std::string first = h.lookup("stable-key").value();
  JumpHasher<> h2;
  h2.add_node({"a", "", 100, "", NodeStatus::Active, {}});
  h2.add_node({"b", "", 100, "", NodeStatus::Active, {}});
  h2.add_node({"c", "", 100, "", NodeStatus::Active, {}});
  EXPECT_EQ(h2.lookup("stable-key").value(), first);
}

TEST(JumpTest, ThrowsWhenEmpty) {
  JumpHasher<> h;
  EXPECT_THROW(h.lookup_or_throw("key"), std::runtime_error);
}

TEST(JumpTest, LookupReturnsNulloptWhenEmpty) {
  JumpHasher<> h;
  EXPECT_FALSE(h.lookup("key").has_value());
}

TEST(JumpTest, RemoveNode) {
  JumpHasher<> h;
  h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
  h.remove_node("n1");
  EXPECT_FALSE(h.has_node("n1"));
  EXPECT_EQ(h.node_count(), 1u);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(h.lookup("k" + std::to_string(i)).value(), "n2");
  }
}

TEST(JumpTest, LookupNDistinct) {
  JumpHasher<> h;
  for (int i = 0; i < 6; ++i) {
    h.add_node({"n" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
  }
  auto owners = h.lookup_n("k", 4);
  ASSERT_EQ(owners.size(), 4u);
  std::set<std::string> seen(owners.begin(), owners.end());
  EXPECT_EQ(seen.size(), 4u);
}

TEST(JumpTest, RebalanceTracksMigration) {
  JumpHasher<> h;
  h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
  (void)h.rebalance();
  uint64_t a = h.stats().migration_count;
  h.add_node({"n3", "", 100, "", NodeStatus::Active, {}});
  (void)h.rebalance();
  EXPECT_GT(h.stats().migration_count, a);
}