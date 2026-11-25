---
name: Pull Request
about: Standard PR template for ConsistentHash
title: "[<type>(<component>)] <short description>"
labels: []
assignees: []
---

## Summary
<One-sentence summary of the change>

## Changes
- <bullet 1>
- <bullet 2>

## Linked Issues
Fixes #<issue>
Refs: #<issue>

## Testing
- [ ] Unit tests pass (`ctest`)
- [ ] Property tests pass
- [ ] Benchmarks run (`make bench`)
- [ ] `clang-format --dry-run --Werror` clean
- [ ] Sanitizers clean (ASan/UBSan)

## Security
- [ ] No secrets introduced
- [ ] No AI signatures

## Documentation
- [ ] API docs updated (`docs/api/`)
- [ ] ADR added if architectural decision

## Checklist
- [ ] Target branch is `test`
- [ ] Conventional Commits used
- [ ] No AI signatures
- [ ] Linked to epic/task/subtask issues
