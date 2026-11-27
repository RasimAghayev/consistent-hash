# Consistent Hash

[![Build Status](https://github.com/RasimAghayev/consistent-hash/actions/workflows/ci.yml/badge.svg)](https://github.com/RasimAghayev/consistent-hash/actions/workflows/ci.yml)
[![CodeQL](https://github.com/RasimAghayev/consistent-hash/actions/workflows/codeql.yml/badge.svg)](https://github.com/RasimAghayev/consistent-hash/actions/workflows/codeql.yml)
[![Mutation Testing](https://github.com/RasimAghayev/consistent-hash/actions/workflows/mutation.yml/badge.svg)](https://github.com/RasimAghayev/consistent-hash/actions/workflows/mutation.yml)
[![Documentation](https://github.com/RasimAghayev/consistent-hash/actions/workflows/docs.yml/badge.svg)](https://github.com/RasimAghayev/consistent-hash/actions/workflows/docs.yml)

[![codecov](https://codecov.io/gh/RasimAghayev/consistent-hash/branch/main/graph/badge.svg)](https://codecov.io/gh/RasimAghayev/consistent-hash)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Release](https://img.shields.io/github/v/release/RasimAghayev/consistent-hash)](https://github.com/RasimAghayev/consistent-hash/releases)
[![Docker](https://img.shields.io/docker/pulls/rasimaghayev/consistent-hash)](https://hub.docker.com/r/rasimaghayev/consistent-hash)

A modern, thread-safe, header-only C++20 consistent hashing library providing eight algorithms for distributed systems.

## Features

- **Six Algorithms**: Ring (virtual nodes), Jump (Google), Maglev, Rendezvous (HRW), Multi-Probe, Bounded Loads
- **C++20 Concepts**: Type-safe customization with `NodeIdType`, `KeyType`, `KeyHasher` concepts
- **Composable Hashing**: Algebraic hash composition with `operator^`, `hash_combine`, `PerfectHasher`
- **Thread-Safe**: `shared_mutex` for concurrent read access
- **Observability**: Built-in metrics, OpenTelemetry/Prometheus integration
- **Serialization**: JSON and MessagePack support for state persistence
- **Hardened**: ASan/UBSan, libFuzzer, CodeQL, mutation testing (mull)
- **Property-Based Testing**: RapidCheck for algorithmic invariants
- **Zero-Dependency**: Header-only core, optional features via FetchContent

## Algorithms

| Algorithm | Complexity | Best For | Key Features |
|-----------|------------|----------|--------------|
| **Ring** | O(log N) | General purpose, cache sharding | Virtual nodes, weights, bounded loads |
| **Jump** | O(1) | High-performance, stable clusters | No virtual nodes, deterministic |
| **Rendezvous** | O(N) | Weighted distributions, multi-tenant | Highest Random Weight (HRW) |
| **Maglev** | O(1) lookup | Network load balancing | Google Maglev, consistent permutation |
| **Multi-Probe** | O(log N + P) | Bounded load with probes | Multiple probe strategies |
| **Bounded Loads** | O(log N) | Strict capacity enforcement | Hard per-node limits |
| **Ketama** | O(log N) | Memcached compatibility | 160 points/node, MD5-based |
| **Anchor** | O(log N) | Minimal disruption | Google Anchor Hash, O(1) add/remove |

## Quick Start

### Header-Only Usage

```cpp
#include <consistent_hash/consistent_hash.hpp>

int main() {
    consistent_hash::RingHasher<> hasher;
    hasher.add_node("cache-1", 100);
    hasher.add_node("cache-2", 200);  // 2x capacity
    
    auto node = hasher.lookup("user:12345");
    if (node) {
        // Use *node
    }
    return 0;
}
```

### CMake Integration

```cmake
include(FetchContent)
FetchContent_Declare(
    consistent_hash
    GIT_REPOSITORY https://github.com/RasimAghayev/consistent-hash.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(consistent_hash)

target_link_libraries(your_target PRIVATE consistent_hash::consistent_hash)
```

### Package Managers

```bash
# vcpkg
vcpkg install consistent-hash

# Conan
conan install consistent-hash/1.0.0@
```

### Docker

```bash
docker run --rm rasimaghayev/consistent-hash:latest ./bin/example_load_balancer
```

## Documentation

- **API Reference**: [https://rasimaghayev.github.io/consistent-hash/](https://rasimaghayev.github.io/consistent-hash/)
- **Doxygen**: [https://rasimaghayev.github.io/consistent-hash/doxygen/](https://rasimaghayev.github.io/consistent-hash/doxygen/)
- **Examples**: See `examples/` directory

## Building

### Prerequisites

- CMake 3.25+
- C++20 compiler (GCC 11+, Clang 13+, MSVC 19.30+)

### Build Commands

```bash
# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# With tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCONSISTENT_HASH_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# With benchmarks
cmake -B build -DCONSISTENT_HASH_BUILD_BENCHMARKS=ON
cmake --build build --parallel
./build/benchmarks/consistent_hash_benchmark

# With sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCONSISTENT_HASH_ENABLE_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# With coverage
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCONSISTENT_HASH_ENABLE_COVERAGE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# With property-based tests
cmake -B build -DCONSISTENT_HASH_BUILD_PROPERTY_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# With fuzzers (Clang only)
cmake -B build -DCONSISTENT_HASH_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
./build/fuzz/fuzz_ring_lookup -runs=10000
```

## Testing

- **Unit Tests**: Algorithm correctness, edge cases
- **Integration Tests**: End-to-end scenarios
- **Property Tests**: RapidCheck for distribution, monotonicity invariants
- **Fuzz Tests**: libFuzzer targets for all algorithms
- **Mutation Tests**: mull for test quality assessment
- **Sanitizers**: ASan, UBSan, MSan in CI

## Observability

The library provides built-in metrics and integrates with:

- **Prometheus**: Built-in exporter at `/metrics` endpoint
- **OpenTelemetry**: Native OTLP export for traces and metrics
- **Custom**: Extensible `MetricsRegistry` for custom backends

```cpp
#include <consistent_hash/observability.hpp>

// Register with Prometheus
auto& registry = consistent_hash::MetricsRegistry::instance();
registry.set_prometheus_exporter(
    std::make_unique<consistent_hash::BuiltinPrometheusExporter>("my_app")
);

// Register your hasher
registry.prometheus_exporter()->register_metrics("ring_hasher", &hasher, {
    {"algorithm", "ring"},
    {"cluster", "cache-tier-1"}
});

// Export metrics
std::string metrics = registry.export_prometheus();
// Serve via HTTP endpoint
```

## Advanced Features

### SIMD-Accelerated Hashing

Runtime-dispatched SIMD hashing with automatic CPU feature detection:

```cpp
#include <consistent_hash/simd_hash.hpp>

// Automatic selection (HighwayHash on AVX2, xxHash3 on SSE4.2, FNV-1a fallback)
consistent_hash::simd::AdaptiveHasher hasher;
auto hash = hasher("my-key");

// Or explicit selection
consistent_hash::simd::HighwayHash hh;     // Requires AVX2
consistent_hash::simd::XxHash3 xxh3;       // Requires SSE4.2
consistent_hash::simd::Fnv1a fnv1a;        // Scalar fallback

// Batch hashing
std::vector<std::string_view> keys = {"key1", "key2", "key3"};
std::vector<uint64_t> hashes(3);
hasher.hash_batch(keys.data(), hashes.data(), 3);
```

### Lock-Free Read Path

Zero-lock concurrent reads using epoch-based reclamation:

```cpp
#include <consistent_hash/lockfree.hpp>

consistent_hash::RingHasher<> base_hasher;
base_hasher.add_node("node1", 100);
base_hasher.add_node("node2", 100);

// Wrap any hasher for lock-free reads
auto lf_hasher = consistent_hash::make_lockfree(std::move(base_hasher));

// Readers never block - zero mutex contention
auto node = lf_hasher.lookup("key");           // Lock-free
auto nodes = lf_hasher.lookup_n("key", 3);     // Lock-free
auto bulk = lf_hasher.lookup_bulk(keys);       // Lock-free

// Writers use copy-on-write with versioning
lf_hasher.add_node_atomic("node3", 100);       // Lock-free atomic add
lf_hasher.remove_node_atomic("node1");         // Lock-free atomic remove

// Versioned snapshots for rollback
auto snapshot = lf_hasher.create_snapshot();
lf_hasher.add_node_atomic("node4", 100);
lf_hasher.restore_snapshot(std::move(snapshot)); // Rollback
```

### Health Checking & Passive Failure Detection

```cpp
#include <consistent_hash/health.hpp>

// Configure health checks
consistent_hash::health::HealthChecker health;
health.register_node("node1", [](const std::string&, std::string&, auto timeout) {
    return http_health_check("http://node1/health", timeout);
}, 5s, 2s, 3, 2);

// Start background health checks
health.start();

// Health-aware hasher filters unhealthy nodes
auto healthy_hasher = consistent_hash::make_health_aware(
    std::move(ring_hasher), health);

// Passive error tracking from operations
healthy_hasher.record_lookup_result("node1", false, "timeout");
```

### Dynamic Reconfiguration

Atomic, lock-free node management with versioned snapshots:

```cpp
// Atomic operations with automatic versioning
lf_hasher.add_node_atomic("new-node", 100);
lf_hasher.remove_node_atomic("old-node");
lf_hasher.update_weight_atomic("node1", 200);

// Get current version
uint64_t version = lf_hasher.version();

// Create snapshot for backup/rollback
auto snapshot = lf_hasher.create_snapshot();
// ... perform risky operation ...
lf_hasher.restore_snapshot(std::move(snapshot)); // Automatic rollback
```

### Ketama & Anchor Hashing

```cpp
#include <consistent_hash/ketama_anchor.hpp>

// Memcached-compatible Ketama hashing
consistent_hash::KetamaHasher<> ketama;
ketama.add_node("cache-1", 100);  // 160 points/node

// Minimal-disruption Anchor hashing
consistent_hash::AnchorHasher<> anchor;
anchor.add_node("node1", 100);    // <20% key movement on changes
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

1. Fork the repository
2. Create a feature branch
3. Make changes with tests
4. Run `pre-commit run --all-files`
5. Submit a Pull Request

### Code Style

- **C++**: `clang-format` (Allman style, 100 col)
- **CMake**: `cmake-format`
- **Lint**: `clang-tidy`

## Security

See [SECURITY.md](SECURITY.md) for vulnerability reporting.

- **CodeQL**: Static analysis on every PR
- **Dependabot**: Weekly dependency updates
- **Fuzzing**: libFuzzer in CI
- **Mutation Testing**: mull weekly
- **SBOM**: SPDX JSON on releases
- **Signed Artifacts**: cosign signatures on releases

## License

MIT License - see [LICENSE](LICENSE) for details.

## Citation

If you use this library in research, please cite:

```bibtex
@software{consistent_hash,
  author = {Consistent Hash Contributors},
  title = {Consistent Hash: Modern C++20 Consistent Hashing Library},
  url = {https://github.com/RasimAghayev/consistent-hash},
  version = {1.0.0},
  year = {2024}
}
```