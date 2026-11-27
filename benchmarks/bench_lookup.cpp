#include <benchmark/benchmark.h>

#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/ketama_anchor.hpp>
#include <consistent_hash/lockfree.hpp>
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

template <typename Hasher> static void bench_lookup_n(benchmark::State &state) {
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
    auto owners = h.lookup_n("key:" + std::to_string(idx), 3);
    benchmark::DoNotOptimize(owners);
    idx = (idx + 1) % 1000;
  }
  state.SetItemsProcessed(state.iterations());
}

template <typename Hasher> static void bench_lockfree_lookup(benchmark::State &state) {
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
  lockfree::LockFreeHasher<Hasher> lf_h(std::move(h));
  size_t idx = 0;
  for (auto _ : state) {
    auto owner = lf_h.lookup("key:" + std::to_string(idx));
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
BENCHMARK_TEMPLATE(bench_lookup, KetamaHasher<>)->Apply(CustomRanges);
BENCHMARK_TEMPLATE(bench_lookup, AnchorHasher<>)->Apply(CustomRanges);

BENCHMARK_TEMPLATE(bench_lookup_n, RingHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lookup_n, JumpHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lookup_n, MaglevHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lookup_n, KetamaHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lookup_n, AnchorHasher<>)->Args({100});

BENCHMARK_TEMPLATE(bench_lockfree_lookup, RingHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lockfree_lookup, JumpHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lockfree_lookup, MaglevHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lockfree_lookup, KetamaHasher<>)->Args({100});
BENCHMARK_TEMPLATE(bench_lockfree_lookup, AnchorHasher<>)->Args({100});

BENCHMARK_MAIN();
