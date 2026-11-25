# Architecture Overview

ConsistentHash is a C++17 library implementing the major consistent hashing
algorithms. Each algorithm is a standalone concrete class deriving from the
`ConsistentHasher<NodeId, Key>` interface, so callers can swap strategies
without changing application code.

## Algorithms

| Algorithm | Lookup | Add node | Memory | Best for |
|-----------|--------|----------|--------|----------|
| Ring (virtual nodes) | O(log V) | O(V log V) | O(V) | General purpose, weighted, zone-aware |
| Jump (Google) | O(log n) | O(n) reindex | O(1) | Large node counts, minimal memory |
| Maglev | O(1) | O(M x n) table rebuild | O(M) | Load balancers, uniform distribution |
| Rendezvous (HRW) | O(n) | O(1) | O(n) | Small clusters, simplicity |
| Multi-probe | O(k) | O(1) | O(n) | Hot-key collision reduction |
| Bounded loads | O(log n) | O(log n) | O(n) | Overload prevention |

## Thread safety

All hashers guard node mutations with `std::shared_mutex`: lookups are
concurrent (shared lock), add/remove take an exclusive lock. Ring uses a
copy-on-write sorted vector; Maglev swaps an immutable lookup table.

## Node model

Each node carries an id, address, weight, zone, status (active/draining/
removed) and free-form metadata. Weight scales virtual-node count in Ring;
zone enables zone-aware placement; capacity metadata drives Bounded loads.

## Metrics

Each hasher tracks distribution skew, migration count, and lookup latency via
`Metrics`. `stats()` returns a `DistributionStats` snapshot computed over an
internal probe keyset.
