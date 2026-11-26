# Contributing to Consistent Hash

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Table of Contents
- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Style](#code-style)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)
- [Security](#security)

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## Getting Started

### Prerequisites
- CMake 3.25+
- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 19.30+)
- Ninja (recommended) or Make
- Git

### Building
```bash
# Clone the repository
git clone https://github.com/RasimAghayev/consistent-hash.git
cd consistent-hash

# Configure with CMake presets (recommended)
cmake --preset release

# Build
cmake --build build/release --parallel

# Run tests
ctest --test-dir build/release --output-on-failure
```

### Using Docker
```bash
# Build the Docker image
docker build -t consistent-hash .

# Run examples
docker run --rm consistent-hash ./bin/example_load_balancer
```

## Development Workflow

1. **Fork** the repository
2. **Create a branch** for your feature/fix: `git checkout -b feature/my-feature`
3. **Make changes** following the code style guidelines
4. **Run pre-commit hooks**: `pre-commit run --all-files`
5. **Run tests**: `ctest --test-dir build/release --output-on-failure`
6. **Commit** with conventional commit messages
7. **Push** and create a Pull Request

### Conventional Commits
Use the format: `<type>(<scope>): <description>`

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Formatting, missing semicolons, etc.
- `refactor`: Code change that neither fixes a bug nor adds a feature
- `perf`: Performance improvement
- `test`: Adding missing tests
- `chore`: Maintenance tasks
- `ci`: CI/CD changes

Examples:
```
feat(ring): add virtual node weight balancing
fix(multiprobe): resolve load mutation race condition
docs(readme): update installation instructions
ci(github-actions): add CodeQL analysis
```

## Code Style

### C++
- **Standard**: C++20
- **Formatting**: `clang-format` (config: `.clang-format`)
- **Static Analysis**: `clang-tidy` (config: `.clang-tidy`)
- **Target-based CMake**: Modern CMake practices
- **Naming**: 
  - Types: `PascalCase` (e.g., `RingHasher`, `Node`)
  - Functions/Methods: `snake_case` (e.g., `add_node`, `lookup_opt_unlocked`)
  - Variables: `snake_case` (e.g., `node_count`, `virtual_nodes`)
  - Constants: `kPascalCase` (e.g., `kDefaultProbes`)
  - Template parameters: `PascalCase` (e.g., `NodeId`, `Key`)

### CMake
- **Format**: `cmake-format` (config: `.cmake-format.yaml`)
- **Minimum version**: 3.25
- **Modern practices**: Target-based, generator expressions, presets

### Git
- **Line endings**: LF (enforced by `.gitattributes`)
- **Commit messages**: Conventional Commits
- **Branch naming**: `type/short-description` (e.g., `feat/ring-virtual-nodes`)

## Testing

### Running Tests
```bash
# All tests
ctest --test-dir build/release --output-on-failure

# Specific test
ctest --test-dir build/release -R test_ring --output-on-failure

# With coverage
cmake --preset coverage
cmake --build build/coverage
ctest --test-dir build/coverage
```

### Test Categories
- **Unit tests**: `tests/unit/` - Individual algorithm correctness
- **Integration tests**: `tests/integration/` - End-to-end scenarios
- **Property tests**: `tests/property/` - Distribution, monotonicity invariants
- **Fuzz tests**: `fuzz/` - libFuzzer targets for security

### Writing Tests
- Use GoogleTest framework
- Follow AAA pattern (Arrange, Act, Assert)
- Test edge cases: empty ring, single node, weight changes, concurrent access
- Property-based tests for distribution guarantees

## Pull Request Process

1. **Ensure CI passes**: All GitHub Actions checks must pass
2. **Update documentation**: README, comments, CHANGELOG if applicable
3. **Add tests**: For new features or bug fixes
4. **Request review**: Assign maintainers
5. **Address feedback**: Respond to review comments
6. **Squash commits**: Maintainers will squash on merge

### PR Checklist
- [ ] Code compiles with `-Wall -Wextra -Wpedantic -Werror`
- [ ] `clang-format` and `cmake-format` applied
- [ ] `clang-tidy` passes (no new warnings)
- [ ] All existing tests pass
- [ ] New tests added for changes
- [ ] Documentation updated
- [ ] CHANGELOG.md updated (for user-facing changes)

## Issue Reporting

### Bug Reports
Use the bug report template and include:
- **Environment**: OS, compiler, CMake version
- **Steps to reproduce**: Minimal code example
- **Expected behavior**: What should happen
- **Actual behavior**: What happens
- **Logs**: Compiler output, test failures, sanitizer reports

### Feature Requests
Use the feature request template and include:
- **Problem statement**: What problem does this solve?
- **Proposed solution**: API design, algorithm choice
- **Alternatives considered**: Why this approach?
- **Impact**: Performance, API breaking changes, migration path

## Security

### Reporting Vulnerabilities
**Do not** create public issues for security vulnerabilities. Instead:
1. Email: security@consistent-hash.example.com
2. Use GitHub Security Advisories (private)
3. Include: Description, impact, reproduction steps, suggested fix

### Supported Versions
| Version | Supported |
|---------|-----------|
| 1.x.x   | ✅ Yes    |
| < 1.0   | ❌ No     |

### Security Features
- AddressSanitizer / UndefinedBehaviorSanitizer in CI
- libFuzzer targets for all algorithms
- CodeQL static analysis
- Hardened compiler flags in Release
- Dependency scanning via Dependabot

## License

By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).