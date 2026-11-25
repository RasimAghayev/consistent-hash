#include <consistent_hash/ring.hpp>

#include <iostream>
#include <map>
#include <string>

using namespace consistent_hash;

int main() {
    RingHasher<> cache(150);
    for (int i = 0; i < 4; ++i) {
        cache.add_node({"cache-" + std::to_string(i), "10.0.0." + std::to_string(i) + ":11211",
                        100, "", NodeStatus::Active, {}});
    }

    std::cout << "=== Cache sharding demo (ring, 4 cache nodes) ===\n";
    const int objects = 1000;
    std::map<std::string, int> shard_counts;
    for (int i = 0; i < objects; ++i) {
        std::string key = "object:" + std::to_string(i);
        shard_counts[cache.lookup(key)]++;
    }
    for (const auto& [node, c] : shard_counts) {
        std::cout << "  " << node << ": " << c << " objects\n";
    }

    // Rebalance when a cache node is added.
    auto before = shard_counts;
    cache.add_node({"cache-4", "10.0.0.4:11211", 100, "", NodeStatus::Active, {}});
    auto migrations = cache.rebalance();
    std::cout << "  rebalance migrations (probe keys): " << migrations.size() << "\n";
    std::cout << "  skew: " << cache.stats().skew << "\n";
    return 0;
}
