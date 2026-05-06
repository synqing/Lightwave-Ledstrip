---
id: 2026-05-05--repo--claude-mem-search-routing-fallbacks
date_utc: 2026-05-05
agent: codex
scope: repo
type: docs
summary: Codify claude-mem search fallbacks when Smart Explore transport is degraded
files_changed:
  - AGENTS.md
  - CLAUDE.md
  - docs/WORKFLOW_ROUTING.md
  - docs/tooling/claude-mem-usage-optimisation-2026-05-02.md
  - CHANGELOG.md
validation: Documentation-only change; verified with markdown lint-oriented grep and git diff review.
breaking_change: false
follow_ups: []
---

## Details

Document the operational distinction between claude-mem memory search and Smart Explore code navigation. Agents must no longer treat `smart_search`, `smart_outline`, or `smart_unfold` `Transport closed` failures as proof that claude-mem memory search or the worker is down.

The routing docs now direct agents to verify worker `/api/health`, use worker `GET /api/search` or SQLite FTS when the client MCP transport is closed, and use `$RECALL_CLI`, `rg`, or clangd according to whether the task needs prior-session memory or current source truth.
