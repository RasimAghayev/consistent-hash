==========
Changelog
==========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog <https://keepachangelog.com/en/1.0.0/>`_,
and this project adheres to `Semantic Versioning <https://semver.org/spec/v2.0.0.html>`_.

[Unreleased]
============

Added
-----

* Initial release of Consistent Hash library
* Six algorithms: Ring, Jump, Rendezvous, Maglev, Multi-Probe, Bounded Loads
* C++20 concepts for type-safe customization
* Composable algebraic hashing with custom hashers
* Thread-safe implementation with shared_mutex
* JSON and MessagePack serialization
* Comprehensive test suite (unit, integration, property-based)
* libFuzzer targets for all algorithms
* GitHub Actions CI/CD with CodeQL, sanitizers, coverage
* Docker multi-arch builds (amd64/arm64)
* Doxygen and Sphinx documentation
* vcpkg and Conan package recipes
* OpenTelemetry/Prometheus metrics integration
* Hardening compiler flags

Changed
-------

* N/A

Deprecated
----------

* N/A

Removed
-------

* N/A

Fixed
-----

* N/A

Security
--------

* N/A

[v1.0.0] - 2024-11-26
=====================

Added
-----

* First stable release
* All six consistent hashing algorithms
* Full C++20 support with concepts
* Modern CMake build system
* Comprehensive documentation

[v0.1.0] - 2024-10-15
=====================

Added
-----

* Initial prototype with Ring and Jump hashing
* Basic test infrastructure
* CMake build system

[Unreleased]: https://github.com/RasimAghayev/consistent-hash/compare/v1.0.0...HEAD
[v1.0.0]: https://github.com/RasimAghayev/consistent-hash/releases/tag/v1.0.0
[v0.1.0]: https://github.com/RasimAghayev/consistent-hash/releases/tag/v0.1.0