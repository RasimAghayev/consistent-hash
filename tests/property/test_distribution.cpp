#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/ring.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>

using namespace consistent_hash;

// Builds N identical nodes and returns a vector of keys.
static std::vector<std::string> make_keys(int n) {
    std::vector<std::string> keys;
    keys.reserve(n);
    for (int i = 0; i < n; ++i) keys.push_back("key:" + std::to_string(i));
    return keys;
}

static std::map<std::string, int> histogram(ConsistentHasher<>& h,
                                            const std::vector<std::string>& keys) {
    std::map<std::string, int> counts;
    for (const auto& k : keys) {
        if (auto opt = h.lookup(k); opt) {
            counts[opt.value()]++;
        }
    }
    return counts;
}

static double skew_of(const std::map<std::string, int>& counts, int keys, int n) {
    if (counts.empty()) return 0.0;
    double mean = static_cast<double>(keys) / static_cast<double>(n);
    double sq = 0.0;
    for (const auto& [node, c] : counts) {
        double d = static_cast<double>(c) - mean;
        sq += d * d;
    }
    return std::sqrt(sq / static_cast<double>(n)) / mean;
}

TEST(PropertyDistribution, AllAlgorithmsUniform) {
    const int N = 8;
    const int keys = 4000;
    auto key_list = make_keys(keys);

    auto build_nodes = [&](auto& h) {
        for (int i = 0; i < N; ++i) {
            h.add_node({"n" + std::to_string(i), "10.0.0." + std::to_string(i),
                        100, "", NodeStatus::Active, {}});
        }
    };

    RingHasher<> ring;
    build_nodes(ring);
    double r = skew_of(histogram(ring, key_list), keys, N);

    JumpHasher<> jump;
    build_nodes(jump);
    double j = skew_of(histogram(jump, key_list), keys, N);

    MaglevHasher<> maglev(251);
    build_nodes(maglev);
    double m = skew_of(histogram(maglev, key_list), keys, N);

    RendezvousHasher<> rv;
    build_nodes(rv);
    double rv_s = skew_of(histogram(rv, key_list), keys, N);

    MultiProbeHasher<> mp;
    build_nodes(mp);
    double mp_s = skew_of(histogram(mp, key_list), keys, N);

    BoundedLoadsHasher<> bl;
    build_nodes(bl);
    double bl_s = skew_of(histogram(bl, key_list), keys, N);

    EXPECT_LT(r, 0.35);
    EXPECT_LT(j, 0.35);
    EXPECT_LT(m, 0.35);
    EXPECT_LT(rv_s, 0.35);
    EXPECT_LT(mp_s, 0.35);
    EXPECT_LT(bl_s, 0.35);
}

TEST(PropertyMonotonicity, AddingNodeMovesMinority) {
    RingHasher<> h;
    h.add_node({"n1", "", 100, "", NodeStatus::Active, {}});
    h.add_node({"n2", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();

    const int keys = 300;
    std::map<std::string, std::string> before;
    for (int i = 0; i < keys; ++i) {
        if (auto opt = h.lookup("k" + std::to_string(i)); opt) {
            before["k" + std::to_string(i)] = opt.value();
        }
    }

    h.add_node({"n3", "", 100, "", NodeStatus::Active, {}});
    (void)h.rebalance();

    size_t moved = 0;
    for (const auto& [k, b] : before) {
        if (h.lookup(k).value() != b) ++moved;
    }
    EXPECT_LT(static_cast<double>(moved) / keys, 0.5);
}