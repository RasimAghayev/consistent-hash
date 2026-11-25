# Security Policy

## Supported versions

| Version | Supported |
| ------- | --------- |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x: |

## Reporting a vulnerability

Please do **not** file a public issue for security vulnerabilities. Instead,
use GitHub private vulnerability reporting (Security advisories) or email the
maintainer at security@example.com.

Include: the affected version, a description of the issue, steps to reproduce,
and any suggested fix. Do not include secrets, tokens, or credentials.

## Response timeline

* P0 (critical, exploitable): acknowledge within 24 h, fix as soon as possible.
* P1 (high): acknowledge within 48 h.
* P2/P3 (medium/low): acknowledge within 5 business days.

After a report: acknowledge → triage → fix → coordinated disclosure → rotate any
exposed secrets if applicable.

## Scope

Code, CI workflows, Docker images, dependencies, and build scripts.

## Disclosure

We follow coordinated disclosure: the reporter is credited (unless they prefer
to remain anonymous), a CVE is requested when appropriate, and the fix is
released in a new version with a CHANGELOG entry.
