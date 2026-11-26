#include <consistent_hash/rendezvous.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace consistent_hash;

int main() {
    RendezvousHasher<> shards;
    for (int i = 0; i < 6; ++i) {
        shards.add_node({"shard-" + std::to_string(i),
                         "10.0.0." + std::to_string(i) + ":5432",
                         100, "", NodeStatus::Active, {}});
    }

    std::cout << "=== DB sharding demo (rendezvous/HRW, 6 shards) ===\n";
    for (int i = 0; i < 10; ++i) {
        std::string key = "user:" + std::to_string(1000 + i);
        auto replicas = shards.lookup_n(key, 3);
        std::cout << "  " << key << " -> primary=" << replicas[0]
                  << " replicas=";
        for (size_t j = 1; j < replicas.size(); ++j) {
            std::cout << replicas[j] << (j + 1 < replicas.size() ? "," : "");
        }
        std::cout << "\n";
    }
    return 0;
}