# Build and test image for ConsistentHash (C++17).
FROM gcc:13.5 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build clang-format git make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /consistent-hash
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCONSISTENT_HASH_BUILD_TESTS=ON \
    -DCONSISTENT_HASH_BUILD_BENCHMARKS=ON \
    && cmake --build build --parallel

# Run tests as a separate stage so a failed test fails the image build.
FROM builder AS tester
RUN ctest --test-dir build --output-on-failure

# Benchmark stage runs the benchmarks (non-failing by default).
FROM builder AS bench
CMD ["./build/benchmarks/consistent_hash_benchmarks"]
