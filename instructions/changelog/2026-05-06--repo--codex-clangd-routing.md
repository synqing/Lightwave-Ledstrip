---
id: 2026-05-06--repo--codex-clangd-routing
date_utc: 2026-05-06
agent: codex
scope: repo
type: docs
summary: Codify Codex clangd semantic routing and stale MCP child reset support
files_changed:
  - AGENTS.md
  - CLAUDE.md
  - docs/WORKFLOW_ROUTING.md
  - docs/tooling/cc-audit-phase-b1-inventory-report-2026-05-05.md
  - docs/tooling/cc-audit-phase-b2-correction-report-2026-05-05.md
  - docs/tooling/cc-audit-phase-b2-runtime-reduction-report-2026-05-05.md
  - firmware-v3/.clangd
  - tools/codex-clangd-mcp-reset.sh
  - CHANGELOG.md
validation: Documentation/tooling hygiene; validate with git diff --check.
breaking_change: false
follow_ups: []
---

## Details

Documents the Codex-specific clangd route for Lightwave firmware work, including the global MCP server expectations, Homebrew clangd compatibility requirements, the local `.clangd` parsing shim, and the scoped reset command for stale clangd MCP child transports.

Also corrects the Codex MCP inventory reports so `file-system` is no longer listed in the post-reduction Codex runtime surface.
