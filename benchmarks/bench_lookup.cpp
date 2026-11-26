#include <benchmark/benchmark.h>

#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/ring.hpp>

#include <string>
#include <vector>

using namespace consistent_hash;

template <typename Hasher> static void bench_lookup(benchmark::State &state) {
  const int n = static_cast<int>(state.range(0));
  Hasher h;
  for (int i = 0; i < n; ++i) {
    h.add_node({"node-" + std::to_string(i),
                "10.0.0." + std::to_string(i),
                100,
                "",
                NodeStatus::Active,
                {}});
  }
  size_t idx = 0;
  for (auto _ : state) {
    auto owner = h.lookup("key:" + std::to_string(idx));
    benchmark::DoNotOptimize(owner);
    idx = (idx + 1) % 1000;
  }
  state.SetItemsProcessed(state.iterations());
}

static void CustomRanges(benchmark::internal::Benchmark *b) {
  b->Args({1})->Args({10})->Args({100})->Args({1000});
}

BENCHMARK_TEMPLATE(bench_lookup, RingHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, JumpHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, MaglevHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, RendezvousHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, MultiProbeHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, BoundedLoadsHasher<>)->Apply(CustomRanges);

BENCHMARK_MAIN();
