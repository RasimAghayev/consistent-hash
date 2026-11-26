#include <consistent_hash/multiprobe.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace consistent_hash;

TEST(MultiProbeTest, LookupReturnsValidNode) {
  MultiProbeHasher<> h;
  for (int i = 0; i < 6; ++i) {
    h.add_node({"n" + std::to_string(i),
                "10.0.0." + std::to_string(i),
                100,
                "",
                NodeStatus::Active,
                {}});
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

TEST(MultiProbeTest, ThrowsWhenEmpty) {
  MultiProbeHasher<> h;
  EXPECT_THROW(h.lookup_or_throw("key"), std::runtime_error);
}

TEST(MultiProbeTest, LookupReturnsNulloptWhenEmpty) {
  MultiProbeHasher<> h;
  EXPECT_FALSE(h.lookup("key").has_value());
}

TEST(MultiProbeTest, RemoveNode) {
  MultiProbeHasher<> h;
  h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
  h.remove_node("n1");
  EXPECT_FALSE(h.has_node("n1"));
  EXPECT_EQ(h.node_count(), 1u);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(h.lookup("k" + std::to_string(i)).value(), "n2");
  }
}

TEST(MultiProbeTest, LookupNDistinct) {
  MultiProbeHasher<> h;
  for (int i = 0; i < 6; ++i) {
    h.add_node({"n" + std::to_string(i), "", 100, "", NodeStatus::Active, {}});
  }
  auto owners = h.lookup_n("k", 4);
  ASSERT_EQ(owners.size(), 4u);
  std::set<std::string> seen(owners.begin(), owners.end());
  EXPECT_EQ(seen.size(), 4u);
}

TEST(MultiProbeTest, RebalanceTracksMigration) {
  MultiProbeHasher<> h;
  h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
  (void)h.rebalance();
  uint64_t a = h.stats().migration_count;
  h.add_node({"n3", "", 100, "", NodeStatus::Active, {}});
  (void)h.rebalance();
  EXPECT_GT(h.stats().migration_count, a);
}

TEST(MultiProbeTest, CustomProbeCount) {
  MultiProbeHasher<> h(4);
  h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
  h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
  auto opt = h.lookup("key");
  EXPECT_TRUE(opt.has_value());
  EXPECT_TRUE(opt.value() == "n1" || opt.value() == "n2");
}