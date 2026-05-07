---
id: 2026-05-07--repo--work-blocking-protocol
date_utc: 2026-05-07
agent: codex
scope: repo
type: docs
summary: Codify Work Blocking protocol and log the first two work blocks
files_changed:
  - CLAUDE.md
  - AGENTS.md
  - BACKLOG.md
  - CHANGELOG.md
  - instructions/work-blocking-protocol-v1.md
validation: Docs-only change; validated with git diff --check.
breaking_change: false
follow_ups:
  - WB-1 systemic naming, definition, and metric accountability audit
  - WB-2 FastLED/RMT transport visibility and ownership study
---

## Details

Adds the Work Blocking containment protocol so critical work discovered during an active mission is surfaced, scoped, logged, assigned out, and then the original mission resumes unless Captain explicitly re-scopes the session or an RBDO hard stop prevents continuation.

The first two Work Blocks record the naming/metric accountability audit and FastLED/RMT transport visibility study surfaced during K1 firmware-v3 effects characterisation.
