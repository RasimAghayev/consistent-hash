// Fuzzer for JumpHasher lookup
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "consistent_hash/jump.hpp"
#include "consistent_hash/hash.hpp"

using consistent_hash::JumpHasher;
using consistent_hash::DefaultHasher;
using consistent_hash::Node;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0) return 0;

  JumpHasher<std::string, std::string, DefaultHasher<std::string>> hasher;

  // Add some nodes
  hasher.add_node(Node{"node-1", "127.0.0.1:8001", 100, "", consistent_hash::NodeStatus::Active});
  hasher.add_node(Node{"node-2", "127.0.0.1:8002", 100, "", consistent_hash::NodeStatus::Active});
  hasher.add_node(Node{"node-3", "127.0.0.1:8003", 100, "", consistent_hash::NodeStatus::Active});
  hasher.add_node(Node{"node-4", "127.0.0.1:8004", 100, "", consistent_hash::NodeStatus::Active});

  // Use fuzz input as key
  std::string key(reinterpret_cast<const char*>(data), size);

  // Test lookup
  auto result = hasher.lookup(key);
  (void)result;

  // Test lookup_n
  auto results = hasher.lookup_n(key, 3);
  (void)results;

  // Test bulk lookup
  std::vector<std::string> keys = {key, key + "2", key + "3"};
  auto bulk_results = hasher.lookup_bulk(keys);
  (void)bulk_results;

  return 0;
}