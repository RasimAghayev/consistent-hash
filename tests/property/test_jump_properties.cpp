#include <consistent_hash/jump.hpp>
#include <rapidcheck/rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace consistent_hash;
using namespace rc;

namespace {

// Property: Jump hashing is deterministic
RC_GTEST_PROP(JumpHasher, DeterministicLookup, ()) {
  JumpHasher<> hasher;
  
  const auto nodes = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 20)),
      gen::inRange(1, 10)
  );
  
  for (const auto& node : nodes) {
    hasher.add_node(node);
  }
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  // Multiple lookups should return same result
  for (const auto& key : keys) {
    auto opt1 = hasher.lookup(key);
    auto opt2 = hasher.lookup(key);
    auto opt3 = hasher.lookup(key);
    RC_ASSERT(opt1 == opt2 && opt2 == opt3);
  }
}

// Property: Adding node only affects new keys (consistency)
RC_GTEST_PROP(JumpHasher, AddingNodeConsistency, ()) {
  JumpHasher<> hasher;
  
  const auto initial_nodes = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 20)),
      gen::inRange(1, 5)
  );
  
  for (const auto& node : initial_nodes) {
    hasher.add_node(node);
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
  
  // Add a new node
  hasher.add_node("new_node");
  
  // Check mappings for existing keys - some will move, but only to new node
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      // Either same as before or new node
      RC_ASSERT(opt == before[key] || *opt == "new_node");
    }
  }
}

// Property: Removing node redistributes its keys
RC_GTEST_PROP(JumpHasher, RemovingNodeRedistributes, ()) {
  JumpHasher<> hasher;
  
  hasher.add_node("node1");
  hasher.add_node("node2");
  hasher.add_node("node3");
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(100, 500)
  );
  
  // Record mappings before removal
  std::map<std::string, std::string> before;
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      before[key] = *opt;
    }
  }
  
  // Remove node2
  hasher.remove_node("node2");
  
  // Keys that mapped to node2 should now map to node1 or node3
  // Others should stay the same
  for (const auto& key : keys) {
    if (auto opt = hasher.lookup(key); opt) {
      if (before[key] == "node2") {
        RC_ASSERT(*opt == "node1" || *opt == "node3");
      } else {
        RC_ASSERT(*opt == before[key]);
      }
    }
  }
}

// Property: Jump hashing with single node
RC_GTEST_PROP(JumpHasher, SingleNode, ()) {
  JumpHasher<> hasher;
  hasher.add_node("only_node");
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  for (const auto& key : keys) {
    auto opt = hasher.lookup(key);
    RC_ASSERT(opt && *opt == "only_node");
  }
}

// Property: Empty ring returns nullopt
RC_GTEST_PROP(JumpHasher, EmptyRingReturnsNullopt, ()) {
  JumpHasher<> hasher;
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  for (const auto& key : keys) {
    auto opt = hasher.lookup(key);
    RC_ASSERT(!opt);
  }
}

// Property: Lookup_n returns distinct nodes
RC_GTEST_PROP(JumpHasher, LookupNDistinct, ()) {
  JumpHasher<> hasher;
  
  for (int i = 0; i < 5; ++i) {
    hasher.add_node("node" + std::to_string(i));
  }
  
  const auto keys = *gen::container<std::vector<std::string>>(
      gen::string(gen::printableAscii(), gen::inRange(1, 50)),
      gen::inRange(10, 100)
  );
  
  for (const auto& key : keys) {
    auto nodes = hasher.lookup_n(key, 3);
    RC_ASSERT(nodes.size() <= 3);
    
    // All nodes should be distinct
    std::set<std::string> unique(nodes.begin(), nodes.end());
    RC_ASSERT(unique.size() == nodes.size());
    
    // All should be valid
    for (const auto& n : nodes) {
      RC_ASSERT(n.rfind("node", 0) == 0);  // starts with "node"
    }
  }
}

} // namespace