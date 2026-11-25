#include <consistent_hash/maglev.hpp>

#include <iostream>
#include <map>
#include <string>

using namespace consistent_hash;

int main() {
    MaglevHasher<> lb(1021);  // prime table size
    lb.add_node({"backend-1", "10.0.0.1:8080", 100, "", NodeStatus::Active, {}});
    lb.add_node({"backend-2", "10.0.0.2:8080", 100, "", NodeStatus::Active, {}});
    lb.add_node({"backend-3", "10.0.0.3:8080", 100, "", NodeStatus::Draining, {}});
    lb.add_node({"backend-4", "10.0.0.4:8080", 100, "", NodeStatus::Active, {}});

    std::cout << "=== Load balancer demo (maglev, 3 active + 1 draining) ===\n";
    const int requests = 5000;
    std::map<std::string, int> counts;
    for (int i = 0; i < requests; ++i) {
        counts[lb.lookup("request:" + std::to_string(i))]++;
    }
    for (const auto& [node, c] : counts) {
        std::cout << "  " << node << ": " << c << " requests\n";
    }
    std::cout << "  drain node received 0: "
              << (counts["backend-3"] == 0 ? "yes" : "no") << "\n";
    return 0;
}
