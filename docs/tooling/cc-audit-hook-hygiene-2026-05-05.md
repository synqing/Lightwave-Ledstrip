# CC Audit Hook Hygiene

**Label:** GROUNDED for names-only hook inventory and live startup check on 2026-05-05.

## Summary

Hook hygiene performed a names-only inventory and made no hook mutations. No non-essential enabled hook was clearly tied to a removed/default-off surface. Safety-critical and memory-critical hooks were preserved.

## Current Enabled Plugin Surface

```text
atomic-agents@claude-plugins-official
clangd-lsp@claude-plugins-official
claude-mem@thedotmack
review-loop@hamel-review
```

Current global settings hook event names:

```text
PostToolUse
PreToolUse
SessionStart
```

## Hook Event Inventory

| Hook source category | Event names |
|---|---|
| Vercel plugin cache, disabled/default-off | `SessionEnd`, `SessionStart` |
| review-loop cache/marketplace, enabled useful/safety | `Stop` |
| episodic-memory cache, memory-related | `SessionStart` |
| claude-mem cache/marketplace, mandatory | `PostToolUse`, `PreToolUse`, `SessionStart`, `Setup`, `Stop`, `UserPromptSubmit` |
| official output-style/security/hookify/Ralph marketplace sources, not enabled defaults | `SessionStart`, `PreToolUse`, `PostToolUse`, `Stop`, `UserPromptSubmit` depending on source |
| last30days skill hook, not changed | `SessionStart` |

Only event names were inspected. Hook bodies, args, env values, prompts, tool bodies, and transcript data were not printed.

## Decision

No hook file was edited. No hook backup was required because there was no approved mutable candidate. Vercel hook files remain present in plugin cache/source packages but Vercel is not an enabled Claude plugin/default surface. `claude-mem`, memory freshness, review-loop, and safety/status hooks remain untouched.

## Verification

| Check | Result | Evidence |
|---|---|---|
| Enabled plugin list | PASS | Vercel, Swift, and frontend plugins are not enabled defaults. |
| Names-only hook inventory | PASS | Event names only; no hook bodies or secret-bearing values printed. |
| Startup smoke | PASS with accepted warnings | `claude mcp list` completed; Google Calendar/Gmail auth warnings and Claude.ai Vercel remote connector remain accepted exceptions. |
| Hook mutation | PASS | No hook files changed. |

## No-Touch Statement

No `claude-mem` hooks, memory freshness hooks, review-loop hooks, safety hooks, hook source files, plugin cache/source packages, skill source folders, generated `.claude/CLAUDE.md`, transcript files, or `~/.claude-mem/**` were mutated.
