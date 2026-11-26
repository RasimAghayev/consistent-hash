name: Pull Request Template
description: Standard PR template
labels: []
body:
  - type: markdown
    attributes:
      value: |
        ## Description
        Brief description of changes. Link to related issue(s).
        
        Fixes #(issue number)
  - type: checkboxes
    id: type
    attributes:
      label: Type of Change
      options:
        - label: 🐛 Bug fix
        - label: ✨ New feature
        - label: 📝 Documentation
        - label: 🔧 Refactoring
        - label: ⚡ Performance
        - label: 🧪 Tests
        - label: 🔨 CI/CD
        - label: 📦 Dependencies
  - type: checkboxes
    id: checklist
    attributes:
      label: Checklist
      options:
        - label: Code compiles with `-Wall -Wextra -Wpedantic -Werror` (or `/W4 /WX` on MSVC)
        - label: `clang-format` applied (run `pre-commit run clang-format`)
        - label: `cmake-format` applied (run `pre-commit run cmake-format`)
        - label: `clang-tidy` passes with no new warnings
        - label: All existing tests pass (`ctest --test-dir build/release --output-on-failure`)
        - label: New tests added for changes (unit/integration/property)
        - label: Documentation updated (README, comments, CHANGELOG.md)
        - label: CHANGELOG.md updated (for user-facing changes)
        - label: No breaking changes without migration guide
  - type: textarea
    id: testing
    attributes:
      label: Testing Performed
      description: How did you test these changes?
      render: text
      placeholder: |
        - Ran full test suite: all 76 tests pass
        - Added unit tests for new feature in tests/unit/test_new_feature.cpp
        - Verified with sanitizers (ASan/UBSan): no issues
        - Benchmarked: no regression in lookup latency
  - type: textarea
    id: breaking
    attributes:
      label: Breaking Changes
      description: List any breaking changes and migration steps
      render: text
      placeholder: "None" or "Changed API: `lookup()` now returns `std::optional` instead of throwing..."
  - type: textarea
    id: screenshots
    attributes:
      label: Screenshots/Logs (if applicable)
      description: For UI changes or benchmark results
      render: text