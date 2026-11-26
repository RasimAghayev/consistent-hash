================
Contributing
================

Thank you for contributing to Consistent Hash!

Development Setup
=================

.. code-block:: bash

   git clone https://github.com/RasimAghayev/consistent-hash.git
   cd consistent-hash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCONSISTENT_HASH_BUILD_TESTS=ON
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure

Code Style
==========

* **C++**: ``clang-format`` (config: ``.clang-format``)
* **CMake**: ``cmake-format`` (config: ``.cmake-format.yaml``)
* **Lint**: ``clang-tidy`` (config: ``.clang-tidy``)

Run formatters:

.. code-block:: bash

   pre-commit run --all-files

Or individually:

.. code-block:: bash

   clang-format -i include/consistent_hash/*.hpp src/*.cpp
   cmake-format -i CMakeLists.txt cmake/*.cmake

Testing
=======

.. code-block:: bash

   # Unit tests
   ctest --test-dir build -R "test_.*" --output-on-failure

   # With sanitizers
   cmake -B build/sanitize -DCONSISTENT_HASH_ENABLE_SANITIZERS=ON
   cmake --build build/sanitize
   ctest --test-dir build/sanitize --output-on-failure

   # Coverage
   cmake -B build/coverage -DCONSISTENT_HASH_ENABLE_COVERAGE=ON
   cmake --build build/coverage
   ctest --test-dir build/coverage

Pull Request Process
====================

1. Fork and create a feature branch
2. Make changes with tests
3. Run ``pre-commit run --all-files``
4. Ensure all CI checks pass
5. Submit PR with conventional commit message

Commit Message Format
---------------------

.. code-block:: text

   <type>(<scope>): <description>

   [optional body]

   [optional footer]

Types: ``feat``, ``fix``, ``docs``, ``style``, ``refactor``, ``perf``, ``test``, ``chore``, ``ci``

Example:

.. code-block:: text

   feat(ring): add virtual node weight balancing

   - Implements dynamic virtual node allocation based on node weight
   - Adds set_virtual_nodes_per_weight() configuration

   Closes #123

Reporting Issues
================

* **Bugs**: Use the bug report template
* **Features**: Use the feature request template
* **Security**: Email security@consistent-hash.example.com

Code of Conduct
===============

See `CODE_OF_CONDUCT.md <https://github.com/RasimAghayev/consistent-hash/blob/main/CODE_OF_CONDUCT.md>`_.