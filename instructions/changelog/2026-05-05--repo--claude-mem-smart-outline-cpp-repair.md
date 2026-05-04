---
id: 2026-05-05--repo--claude-mem-smart-outline-cpp-repair
date_utc: 2026-05-05
agent: codex
scope: repo
type: bugfix
summary: Record claude-mem Smart Explore C/C++ parser repair
files_changed:
  - CHANGELOG.md
  - /Users/spectrasynq/.claude/plugins/marketplaces/thedotmack/plugin/scripts/mcp-server.cjs
  - /Users/spectrasynq/.claude/plugins/cache/thedotmack/claude-mem/12.4.9/scripts/mcp-server.cjs
validation: Direct tree-sitter query extracts symbols from the four failing Lightwave C++ files; claude-mem worker health reports 12.4.9 ok and mcpReady=true.
breaking_change: false
follow_ups:
  - Restart or refresh the Codex/Claude MCP client session before retrying smart_outline through the tool transport.
---

## Details

claude-mem Smart Explore reported C++ files as unsupported or empty even though the files were present and parseable. The local repair adds the missing Bun prebuild path for the active tree-sitter binding and patches Smart Explore query routing so C/C++ no longer falls through to the invalid generic tree-sitter query.

The local claude-mem worker was restarted directly from the active 12.4.9 cache after the wrapper reported a stale startup failure. The worker is healthy, but the already-open Codex MCP transport remained closed after stale stdio processes were terminated.
