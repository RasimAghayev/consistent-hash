# ConsistentHash

ConsistentHash is a header-friendly C++17/20 library implementing the major
consistent hashing algorithms — Ring (virtual nodes), Jump (Google), Maglev,
Rendezvous (HRW), Multi-probe, and Bounded loads — with thread-safe node
management, metrics (distribution skew, migration count, lookup latency), and
integration demos for cache sharding, load balancing, DB sharding, and CDN
routing.

## Technology stack

| Layer | Choice |
|-------|--------|
| Language | C++17 (C++20 where beneficial) |
| Build | CMake 3.20+ + Ninja |
| Testing | GoogleTest + Google Benchmark |
| CI | GitHub Actions (GCC, Clang, MSVC), ASan/UBSan/TSan |
| Docs | Doxygen + Markdown |

## Repository structure

```
consistent-hash/
├── include/consistent_hash/  # Public headers
├── src/                      # Non-inline implementations
├── tests/                    # Unit, property, integration tests
├── benchmarks/               # Google Benchmark suites
├── examples/                 # cache_sharding, load_balancer, db_sharding, cdn_routing
├── docs/                     # Architecture, ADRs, API
├── cmake/                    # CMake helper modules
└── .github/workflows/        # CI/CD
```

## Getting started

### With Docker (recommended)

```bash
make build        # build the library and tests
make test         # run the test suite
make lint         # clang-format check
```

### With a local toolchain

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Header-only usage

```cpp
#include <consistent_hash/consistent_hash.hpp>

using namespace consistent_hash;

Node node{"node-1", "10.0.0.1:8080"};
RingHasher<> hasher;
hasher.add_node(node);

auto owner = hasher.lookup("user:123");   // -> "node-1"
```

## Examples

See `examples/` for cache sharding, load balancer, DB sharding, and CDN
routing demos, each with its own README.

## Running tests and benchmarks

```bash
make test          # GoogleTest suite
make bench         # Google Benchmark
```

## License

MIT License. See [LICENSE](LICENSE).

## Security

See [SECURITY.md](SECURITY.md) for the vulnerability reporting policy.
