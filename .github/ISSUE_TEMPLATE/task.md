---
name: Task
title: "[Phase X] <Component> - <short description>"
labels: ["phase:X", "type:task", "priority:high", "agent:ready"]
assignees: []
milestone: "Phase X: <Name>"
---

## Objective
<One-sentence goal>

## Business Context
<Why this matters for the consistent hashing library>

## Technical Context
<Current state, relevant code, dependencies>

## Architecture Requirements
- [ ] Requirement 1
- [ ] Requirement 2

## Implementation Prompt
<Detailed, executable instructions for the agent — independently executable, no follow-up questions>

## Subtasks
- [ ] Subtask 1 → `feature/ISSUE-<n>-1-<slug>`
- [ ] Subtask 2 → `feature/ISSUE-<n>-2-<slug>`

## Files & Directories
- `include/consistent_hash/<component>.hpp`
- `tests/unit/test_<component>.cpp`
- `docs/adr/000N-<decision>.md` (if architectural)

## Test Requirements
- Unit: GoogleTest per algorithm (`ctest -R unit`)
- Property: distribution uniformity + monotonicity (`ctest -R property`)
- Integration: demo scenario (`ctest -R integration`)
- Benchmark: Google Benchmark (`make bench`)
- Sanitizer: ASan/UBSan clean in CI

## Security Requirements
- [ ] No secrets in code/config
- [ ] No AI signatures
- [ ] Deterministic hashing (no seed-dependent behavior)

## Acceptance Criteria
- [ ] AC1: Measurable, verifiable outcome
- [ ] AC2: ...

## Definition of Done
- [ ] All tests pass (unit, property, integration)
- [ ] CI green (lint, build, test, sanitizers, benchmarks)
- [ ] Documentation updated (API, ADR if architectural)
- [ ] Definition of Done met per issue

## PR Requirements
- Target branch: `test`
- Linked Issues: `Fixes #<epic>, #<task>, #<subtask...>`
- Conventional Commits: `feat(<component>): <description>`
- No AI signatures
