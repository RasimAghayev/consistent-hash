.. Consistent Hash documentation master file, created by
   sphinx-quickstart on Thu Nov 26 2026.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. _consistent-hash-docs:

================================
Consistent Hash Documentation
================================

.. image:: https://img.shields.io/badge/C%2B%2B-20-blue.svg
   :target: https://en.cppreference.com/w/cpp/20
   :alt: C++20

.. image:: https://img.shields.io/badge/License-MIT-green.svg
   :target: https://opensource.org/licenses/MIT
   :alt: License

.. image:: https://github.com/RasimAghayev/consistent-hash/actions/workflows/ci.yml/badge.svg
   :target: https://github.com/RasimAghayev/consistent-hash/actions/workflows/ci.yml
   :alt: Build Status

.. image:: https://codecov.io/gh/RasimAghayev/consistent-hash/branch/main/graph/badge.svg
   :target: https://codecov.io/gh/RasimAghayev/consistent-hash
   :alt: Code Coverage

.. image:: https://github.com/RasimAghayev/consistent-hash/actions/workflows/codeql.yml/badge.svg
   :target: https://github.com/RasimAghayev/consistent-hash/actions/workflows/codeql.yml
   :alt: CodeQL

A modern, thread-safe, header-only C++20 consistent hashing library providing multiple algorithms for distributed systems.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   introduction
   installation
   quickstart

.. toctree::
   :maxdepth: 2
   :caption: Algorithms

   algorithms/ring
   algorithms/jump
   algorithms/rendezvous
   algorithms/maglev
   algorithms/multiprobe
   algorithms/bounded_loads

.. toctree::
   :maxdepth: 2
   :caption: Advanced Topics

   concepts
   custom_hashers
   serialization
   thread_safety
   performance

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/namespace
   api/ring
   api/jump
   api/rendezvous
   api/maglev
   api/multiprobe
   api/bounded_loads
   api/types
   api/factory

.. toctree::
   :maxdepth: 1
   :caption: Examples

   examples/load_balancer
   examples/cache_sharding
   examples/db_sharding
   examples/cdn_routing

.. toctree::
   :maxdepth: 1
   :caption: Development

   contributing
   changelog
   license

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`