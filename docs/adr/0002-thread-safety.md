# ADR-0002: Thread safety and memory model

## Status
Accepted

## Context
Hashers may be accessed concurrently by multiple threads (looks ups) while
nodes are added or removed (writes). Data races would corrupt ring/table
state.

## Decision
Guard all mutations with `std::shared_mutex`. Lookups take a shared lock;
add/remove/clear take an exclusive lock. Ring keeps a sorted vector rebuilt
on mutation (copy-on-write semantics). Maglev rebuilds its immutable lookup
table on mutation and readers snapshot the table pointer via the shared lock.

## Consequences
- Lookups are concurrent and cheap.
- Add/remove serialize with each other and block concurrent lookups briefly.
- No lock-free path in this version; revisit if lookup latency becomes a
  bottleneck (TSan in CI guards against races).
