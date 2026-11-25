# Changelog

All notable changes to ConsistentHash will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
* Initial release of the ConsistentHash C++ library.
* Consistent hashing algorithms: Ring (virtual nodes), Jump, Maglev,
  Rendezvous (HRW), Multi-probe, Bounded loads.
* Thread-safe node management with copy-on-write ring rebuilds.
* Metrics: distribution skew, migration count, lookup latency.
* Integration demos: cache sharding, load balancer, DB sharding, CDN routing.
* GoogleTest unit/property tests and Google Benchmark suites.
* CMake build, GitHub Actions CI (GCC, Clang, MSVC), sanitizers.

## [1.0.0] - 2026-11-25

### Added
* Initial project scaffolding (community files, CI, docs).
