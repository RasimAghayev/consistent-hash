// Fuzzer for NodeManager deserialization
#include <cstddef>
#include <cstdint>
#include <string>

#include "consistent_hash/node_manager.hpp"

using consistent_hash::NodeManager;
using consistent_hash::Node;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0) return 0;

  NodeManager<std::string> manager;

  // Use fuzz input as serialized data
  std::string serialized(reinterpret_cast<const char*>(data), size);

  // Test deserialize - should handle malformed input gracefully
  auto result = manager.deserialize(serialized);
  (void)result;

  return 0;
}