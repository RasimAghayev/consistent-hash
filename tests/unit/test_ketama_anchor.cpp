#include <consistent_hash/consistent_hash.hpp>
#include <gtest/gtest.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

using namespace consistent_hash;

TEST(KetamaHasherTest, BasicOperations) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node3", "host3:11211", 100, "", {}));
  
  EXPECT_EQ(hasher.node_count(), 3);
  EXPECT_TRUE(hasher.has_node("node1"));
  EXPECT_TRUE(hasher.has_node("node2"));
  EXPECT_TRUE(hasher.has_node("node3"));
  EXPECT_FALSE(hasher.has_node("node4"));
}

TEST(KetamaHasherTest, Lookup) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  // Test lookup
  auto result = hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value() == "node1" || result.value() == "node2");
  
  // Test lookup_or_throw
  EXPECT_NO_THROW({
    auto node = hasher.lookup_or_throw("test-key");
    EXPECT_TRUE(node == "node1" || node == "node2");
  });
  
  // Test lookup_n
  auto results = hasher.lookup_n("test-key", 2);
  EXPECT_EQ(results.size(), 2);
  EXPECT_NE(results[0], results[1]);
  
  // Test bulk lookup
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  auto bulk = hasher.lookup_bulk(keys);
  EXPECT_EQ(bulk.size(), 3);
  for (const auto& opt : bulk) {
    EXPECT_TRUE(opt.has_value());
  }
}

TEST(KetamaHasherTest, WeightedDistribution) {
  KetamaHasher<std::string, std::string> hasher;
  
  // node1 has 3x weight of node2
  hasher.add_node(Node<std::string>("node1", "host1:11211", 300, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  // Measure distribution
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, int> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  // node1 should get roughly 3x the keys of node2
  double ratio = static_cast<double>(counts["node1"]) / counts["node2"];
  EXPECT_GT(ratio, 2.0);
  EXPECT_LT(ratio, 4.0);
}

TEST(KetamaHasherTest, UpdateWeight) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  // Update node1 weight
  auto result = hasher.update_weight("node1", 300);
  EXPECT_TRUE(result);
  
  // Verify distribution changed
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, int> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  double ratio = static_cast<double>(counts["node1"]) / counts["node2"];
  EXPECT_GT(ratio, 2.0);
  EXPECT_LT(ratio, 4.0);
}

TEST(KetamaHasherTest, UpdateWeightErrors) {
  KetamaHasher<std::string, std::string> hasher;
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  
  // Update non-existent node
  auto result = hasher.update_weight("nonexistent", 200);
  EXPECT_FALSE(result);
  EXPECT_EQ(static_cast<int>(result.code), static_cast<int>(KetamaHasher<std::string, std::string>::ErrorCode::NodeNotFound));
  
  // Update with invalid weight
  result = hasher.update_weight("node1", 0);
  EXPECT_FALSE(result);
  EXPECT_EQ(static_cast<int>(result.code), static_cast<int>(KetamaHasher<std::string, std::string>::ErrorCode::InvalidWeight));
}

TEST(KetamaHasherTest, RemoveNode) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  hasher.remove_node("node1");
  EXPECT_EQ(hasher.node_count(), 1);
  EXPECT_FALSE(hasher.has_node("node1"));
  EXPECT_TRUE(hasher.has_node("node2"));
  
  // Lookup should still work
  auto result = hasher.lookup("test-key");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "node2");
}

TEST(KetamaHasherTest, UpsertNode) {
  KetamaHasher<std::string, std::string> hasher;
  
  // Insert new node
  bool inserted = hasher.upsert_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  EXPECT_TRUE(inserted);
  
  // Update existing node
  inserted = hasher.upsert_node(Node<std::string>("node1", "host1:11211", 200, "", {}));
  EXPECT_FALSE(inserted);
  
  // Verify weight was updated
  std::vector<std::string> keys;
  for (int i = 0; i < 1000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, int> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  // With single node, all keys should go to node1
  EXPECT_EQ(counts["node1"], 1000);
}

TEST(KetamaHasherTest, Rebalance) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  // Ketama is deterministic - rebalance should return empty
  auto migrations = hasher.rebalance();
  EXPECT_TRUE(migrations.empty());
}

TEST(KetamaHasherTest, Serialization) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 200, "", {}));
  
  std::string serialized = hasher.serialize();
  EXPECT_FALSE(serialized.empty());
  
  // Deserialize into new hasher
  KetamaHasher<std::string, std::string> hasher2;
  auto result = hasher2.deserialize(serialized);
  EXPECT_TRUE(result);
  
  EXPECT_EQ(hasher2.node_count(), 2);
  EXPECT_TRUE(hasher2.has_node("node1"));
  EXPECT_TRUE(hasher2.has_node("node2"));
  
  // Verify lookups match
  for (int i = 0; i < 1000; ++i) {
    std::string key = "key-" + std::to_string(i);
    auto r1 = hasher.lookup(key);
    auto r2 = hasher2.lookup(key);
    EXPECT_EQ(r1, r2);
  }
}

TEST(KetamaHasherTest, Clear) {
  KetamaHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  hasher.clear();
  EXPECT_EQ(hasher.node_count(), 0);
  EXPECT_FALSE(hasher.lookup("key").has_value());
}

TEST(KetamaHasherTest, EmptyHasher) {
  KetamaHasher<std::string, std::string> hasher;
  
  auto result = hasher.lookup("key");
  EXPECT_FALSE(result.has_value());
  
  auto results = hasher.lookup_n("key", 3);
  EXPECT_TRUE(results.empty());
  
  auto bulk = hasher.lookup_bulk({"key1", "key2"});
  EXPECT_EQ(bulk.size(), 2);
  EXPECT_FALSE(bulk[0].has_value());
  EXPECT_FALSE(bulk[1].has_value());
  
  EXPECT_THROW(hasher.lookup_or_throw("key"), std::runtime_error);
}

// ============ AnchorHasher Tests ============

TEST(AnchorHasherTest, BasicOperations) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node3", "host3:11211", 100, "", {}));
  
  EXPECT_EQ(hasher.node_count(), 3);
  EXPECT_TRUE(hasher.has_node("node1"));
  EXPECT_TRUE(hasher.has_node("node2"));
  EXPECT_TRUE(hasher.has_node("node3"));
  EXPECT_FALSE(hasher.has_node("node4"));
}

TEST(AnchorHasherTest, Lookup) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  auto result = hasher.lookup("test-key");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value() == "node1" || result.value() == "node2");
  
  auto results = hasher.lookup_n("test-key", 2);
  EXPECT_EQ(results.size(), 2);
  EXPECT_NE(results[0], results[1]);
  
  std::vector<std::string> keys = {"key1", "key2", "key3"};
  auto bulk = hasher.lookup_bulk(keys);
  EXPECT_EQ(bulk.size(), 3);
}

TEST(AnchorHasherTest, WeightedDistribution) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 300, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, int> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  double ratio = static_cast<double>(counts["node1"]) / counts["node2"];
  EXPECT_GT(ratio, 2.0);
  EXPECT_LT(ratio, 4.0);
}

TEST(AnchorHasherTest, UpdateWeight) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  auto result = hasher.update_weight("node1", 300);
  EXPECT_TRUE(result);
  
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, int> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  double ratio = static_cast<double>(counts["node1"]) / counts["node2"];
  EXPECT_GT(ratio, 2.0);
  EXPECT_LT(ratio, 4.0);
}

TEST(AnchorHasherTest, MinimalDisruption) {
  AnchorHasher<std::string, std::string> hasher;
  
  // Add initial nodes
  for (int i = 0; i < 10; ++i) {
    hasher.add_node(Node<std::string>("node" + std::to_string(i), "host" + std::to_string(i), 100, "", {}));
  }
  
  // Record initial assignments
  std::vector<std::string> keys;
  for (int i = 0; i < 10000; ++i) {
    keys.push_back("key-" + std::to_string(i));
  }
  
  std::unordered_map<std::string, std::string> initial_assignments;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      initial_assignments[key] = *opt;
    }
  }
  
  // Add a new node
  hasher.add_node(Node<std::string>("node10", "host10", 100, "", {}));
  
  // Count how many keys moved
  int moved = 0;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      if (initial_assignments[key] != *opt) {
        moved++;
      }
    }
  }
  
  // Anchor hashing should minimize disruption
  // With 11 nodes, ideal would be ~1/11 of keys move (~909)
  // Anchor hashing typically does better than consistent hashing
  double move_rate = static_cast<double>(moved) / static_cast<double>(keys.size());
  EXPECT_LT(move_rate, 0.2); // Less than 20% should move
}

TEST(AnchorHasherTest, RemoveNode) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  hasher.remove_node("node1");
  EXPECT_EQ(hasher.node_count(), 1);
  EXPECT_FALSE(hasher.has_node("node1"));
  
  auto result = hasher.lookup("test-key");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "node2");
}

TEST(AnchorHasherTest, Serialization) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 200, "", {}));
  
  std::string serialized = hasher.serialize();
  EXPECT_FALSE(serialized.empty());
  
  AnchorHasher<std::string, std::string> hasher2;
  auto result = hasher2.deserialize(serialized);
  EXPECT_TRUE(result);
  
  EXPECT_EQ(hasher2.node_count(), 2);
  EXPECT_TRUE(hasher2.has_node("node1"));
  EXPECT_TRUE(hasher2.has_node("node2"));
  
  for (int i = 0; i < 1000; ++i) {
    std::string key = "key-" + std::to_string(i);
    auto r1 = hasher.lookup(key);
    auto r2 = hasher2.lookup(key);
    EXPECT_EQ(r1, r2);
  }
}

TEST(AnchorHasherTest, Clear) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  hasher.clear();
  EXPECT_EQ(hasher.node_count(), 0);
  EXPECT_FALSE(hasher.lookup("key").has_value());
}

TEST(AnchorHasherTest, EmptyHasher) {
  AnchorHasher<std::string, std::string> hasher;
  
  auto result = hasher.lookup("key");
  EXPECT_FALSE(result.has_value());
  
  auto results = hasher.lookup_n("key", 3);
  EXPECT_TRUE(results.empty());
  
  auto bulk = hasher.lookup_bulk({"key1", "key2"});
  EXPECT_EQ(bulk.size(), 2);
  EXPECT_FALSE(bulk[0].has_value());
  EXPECT_FALSE(bulk[1].has_value());
  
  EXPECT_THROW(hasher.lookup_or_throw("key"), std::runtime_error);
}

TEST(AnchorHasherTest, Stats) {
  AnchorHasher<std::string, std::string> hasher;
  
  hasher.add_node(Node<std::string>("node1", "host1:11211", 100, "", {}));
  hasher.add_node(Node<std::string>("node2", "host2:11211", 100, "", {}));
  
  auto stats = hasher.stats();
  EXPECT_EQ(stats.unique_nodes_used, 2);
  EXPECT_GT(stats.total_keys, 0);
}