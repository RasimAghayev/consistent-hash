# Multi-stage Dockerfile for consistent_hash
# Stage 1: Build stage with full toolchain
FROM gcc:13 AS builder

# Install CMake and Ninja
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy CMake files first for better layer caching
COPY CMakeLists.txt .
COPY CMakePresets.json .
COPY cmake/ cmake/

# Copy source code
COPY include/ include/
COPY src/ src/
COPY examples/ examples/
COPY tests/ tests/
COPY benchmarks/ benchmarks/

# Configure and build with Release preset
RUN cmake --preset release && cmake --build build/release --parallel $(nproc)

# Stage 2: Runtime stage with minimal image
FROM ubuntu:22.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN useradd --system --no-create-home --uid 1000 appuser

WORKDIR /app

# Copy only the compiled binaries from builder stage
COPY --from=builder /src/build/release/bin/example_load_balancer ./example_load_balancer
COPY --from=builder /src/build/release/bin/example_cache_sharding ./example_cache_sharding
COPY --from=builder /src/build/release/bin/example_db_sharding ./example_db_sharding
COPY --from=builder /src/build/release/bin/example_cdn_routing ./example_cdn_routing
COPY --from=builder /src/build/release/bin/consistent_hash_tests ./consistent_hash_tests
COPY --from=builder /src/build/release/bin/consistent_hash_benchmarks ./consistent_hash_benchmarks

# Copy library for dynamic linking
COPY --from=builder /src/build/release/lib/libconsistent_hash_impl.a /usr/local/lib/

# Switch to non-root user
USER appuser

# Default command runs the load balancer demo
ENTRYPOINT ["./example_load_balancer"]