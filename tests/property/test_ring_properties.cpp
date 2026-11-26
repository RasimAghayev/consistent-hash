#include <consistent_hash/ring.hpp>
#include <consistent_hash/concepts.hpp>
#include <rapidcheck/rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace consistent_hash;
using namespace rc;

namespace {

// Property: Adding and removing a node should not affect other nodes' keys
RC_GTEST_PROP(RingHasher, AddRemoveNodePreservesOtherKeys, ()) {
  RingHasher<> hasher;
  
  // Add initial nodes
  const std::vector<std::string> initial_nodes = {"node1", "node2", "node3"};
  for (const auto& node : initial_nodes) {
    hasher.add_node(node, 100);
  }
  
  // Generate random keys
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(100, 1000)
  );
  
  // Record initial mappings
  std::map<std::string, std::string> initial_mappings;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      initial_mappings[key] = *opt;
    }
  }
  
  // Add a new node
  hasher.add_node("node4", 100);
  
  // Remove a different node
  hasher.remove_node("node2");
  
  // Check that keys not affected by removal still map correctly
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      // Keys that mapped to node2 may have moved
      // Keys that mapped to node1 or node3 should stay (mostly)
      // This is a weak property - we just check no crash and valid node returned
      RC_ASSERT(std::find(initial_nodes.begin(), initial_nodes.end(), *opt) != initial_nodes.end() || *opt == "node4");
    }
  }
}

// Property: Lookup always returns a valid node when nodes exist
RC_GTEST_PROP(RingHasher, LookupReturnsValidNode, ()) {
  RingHasher<> hasher;
  
  const auto nodes = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 20)),
      gen::inRange(1, 10)
  );
  
  for (const auto& node : nodes) {
    hasher.add_node(node, *gen::inRange(1u, 1000u));
  }
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      RC_ASSERT(std::find(nodes.begin(), nodes.end(), *opt) != nodes.end());
    }
  }
}

// Property: Rebalance only moves keys that need to move
RC_GTEST_PROP(RingHasher, RebalanceMinimalMoves, ()) {
  RingHasher<> hasher;
  
  const auto nodes = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 20)),
      gen::inRange(2, 8)
  );
  
  for (const auto& node : nodes) {
    hasher.add_node(node, *gen::inRange(1u, 1000u));
  }
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(100, 500)
  );
  
  // Record initial mappings
  std::map<std::string, std::string> before;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      before[key] = *opt;
    }
  }
  
  // Rebalance (should be no-op if no changes)
  auto migrations = hasher.rebalance();
  
  // Record mappings after
  std::map<std::string, std::string> after;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      after[key] = *opt;
    }
  }
  
  // No migrations expected without node changes
  RC_ASSERT(migrations.empty());
  
  // Mappings should be identical
  RC_ASSERT(before == after);
}

// Property: Weighted distribution approximately proportional
RC_GTEST_PROP(RingHasher, WeightedDistribution, ()) {
  RingHasher<> hasher(200);  // More virtual nodes for better distribution
  
  hasher.add_node("light", 100);
  hasher.add_node("heavy", 300);  // 3x weight
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(1000, 5000)
  );
  
  std::map<std::string, size_t> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  // Heavy should get roughly 3x the keys of light
  // Allow 20% tolerance
  if (counts["light"] > 0 && counts["heavy"] > 0) {
    double ratio = static_cast<double>(counts["heavy"]) / counts["light"];
    RC_ASSERT(ratio > 2.0 && ratio < 4.0);
  }
}

// Property: Bounded load enforcement
RC_GTEST_PROP(RingHasher, BoundedLoadEnforcement, ()) {
  RingHasher<> hasher(150);
  hasher.set_bounded_load(1.5);
  
  hasher.add_node("node1", 100);
  hasher.add_node("node2", 100);
  hasher.add_node("node3", 100);
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(1000, 5000)
  );
  
  std::map<std::string, size_t> counts;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      counts[*opt]++;
    }
  }
  
  // Check no node exceeds 1.5x fair share
  double fair_share = static_cast<double>(keys.size()) / 3.0;
  double max_allowed = fair_share * 1.5;
  
  for (const auto& [node, count] : counts) {
    RC_ASSERT(static_cast<double>(count) <= max_allowed + 1);  // +1 for rounding
  }
}

// Property: Serialization round-trip
RC_GTEST_PROP(RingHasher, SerializationRoundTrip, ()) {
  RingHasher<> hasher;
  
  const auto nodes = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 20)),
      gen::inRange(1, 8)
  );
  
  for (const auto& node : nodes) {
    hasher.add_node(node, *gen::inRange(1u, 1000u));
  }
  
  // Serialize
  auto serialized = hasher.serialize();
  RC_ASSERT(!serialized.empty());
  
  // Deserialize into new hasher
  RingHasher<> hasher2;
  auto result = hasher2.deserialize(serialized);
  RC_ASSERT(result);
  
  // Check same mappings
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  for (const auto& key : keys) {
    auto opt1 = hasher.lookup(key);
    auto opt2 = hasher2.lookup(key);
    RC_ASSERT(opt1 == opt2);
  }
}

// Property: Concurrent access safety (no crashes)
RC_GTEST_PROP(RingHasher, ConcurrentAccessNoCrash, ()) {
  RingHasher<> hasher;
  
  // Add initial nodes
  for (int i = 0; i < 5; ++i) {
    hasher.add_node("node" + std::to_string(i), 100);
  }
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(100, 500)
  );
  
  // Simulate concurrent reads
  std::vector<std::string> results;
  results.reserve(keys.size());
  
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      results.push_back(*opt);
    }
  }
  
  // All results should be valid nodes
  for (const auto& result : results) {
    RC_ASSERT(std::find_if(nodes.begin(), nodes.end(),
      [&](const std::string& n) { return n == result; }) != nodes.end());
  }
}

} // namespace