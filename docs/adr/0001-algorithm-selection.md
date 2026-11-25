# ADR-0001: Consistent hashing algorithm selection

## Status
Accepted

## Context
The library must support several consistent hashing strategies because no
single algorithm is optimal for every deployment (cache sharding vs. load
balancing vs. DB sharding vs. CDN edge selection).

## Decision
Implement six algorithms behind a common `ConsistentHasher<NodeId, Key>`
interface: Ring (virtual nodes), Jump (Google), Maglev, Rendezvous (HRW),
Multi-probe, and Bounded loads.

## Rationale
- Ring: general purpose, weighted, zone-aware, O(log V) lookup.
- Jump: O(log n) lookup, O(1) memory, ideal for large node counts.
- Maglev: O(1) lookup, uniform distribution, ideal for load balancers.
- Rendezvous: O(n) lookup, no state beyond node list, simple/small clusters.
- Multi-probe: reduces hot-key collisions by selecting least-loaded candidate.
- Bounded loads: prevents node overload via capacity tracking.

## Consequences
Callers choose the algorithm at construction. A future ADR may add a
composite/router hasher that selects per use case.

## Hash function
Default: FNV-1a 64-bit over key content (strings) or bytes (arithmetic types),
with `std::hash` fallback for custom types. Users may inject a custom
`DefaultHasher<Key>` specialization.
