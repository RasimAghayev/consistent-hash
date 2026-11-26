#include <consistent_hash/bounded_loads.hpp>

#include <iostream>
#include <map>
#include <string>

using namespace consistent_hash;

int main() {
    BoundedLoadsHasher<> cdn;
    for (int i = 0; i < 3; ++i) {
        cdn.add_node({"edge-" + std::to_string(i),
                      "10.0.0." + std::to_string(i) + ":443",
                      100, "", NodeStatus::Active,
                      {{"capacity", "5"}}});
    }

    std::cout << "=== CDN routing demo (bounded loads, 3 edge nodes, capacity 5) ===\n";
    const int assets = 100;
    std::map<std::string, int> edge_load;
    for (int i = 0; i < assets; ++i) {
        if (auto opt = cdn.lookup("asset:" + std::to_string(i)); opt) {
            edge_load[*opt]++;
        }
    }
    for (const auto& [node, c] : edge_load) {
        std::cout << "  " << node << ": " << c << " assets\n";
    }
    return 0;
}