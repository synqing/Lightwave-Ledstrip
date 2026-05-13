# CC Audit MCP, Plugin, And Hook Inventory

**Label:** GROUNDED for inventory and classification. Args, env values, cookies, keys, and command payloads are redacted or summarised.

**Scope:** Read-only inventory of Claude/Codex MCP, plugin, and hook surfaces for Lightwave-Ledstrip.

Sources inspected:

- `~/.claude/settings.json`
- `.claude/settings.json`
- `.claude/mcp-config.json`
- `.mcp.json`
- `~/.codex/config.toml`
- `~/.codex/mcp.json`
- `~/.claude/plugins/installed_plugins.json`
- Active plugin hook JSON under `~/.claude/plugins/cache`, `~/.claude/plugins/marketplaces`, and `~/.claude/skills`
- Disabled/archive hook JSON under `~/.claude/plugins.disabled-20260122173309`

---

## Keep Mandatory

| Surface | Evidence | Reason |
|---|---|---|
| RBDO gate | `CLAUDE.md:3`, `CLAUDE.md:7`, `CLAUDE.md:11` | Mandatory tactical-output labelling and audit discipline. |
| claude-mem routing | `CLAUDE.md:82-94`, `docs/WORKFLOW_ROUTING.md:27-30` | Required session memory route; keep current `mcp-search` path. |
| clangd | `.mcp.json:7-10`, `CLAUDE.md:143-161`, `docs/WORKFLOW_ROUTING.md:47-57` | Mandatory for C++ symbol navigation. |
| QMD | `.mcp.json:3-5`, `CLAUDE.md:163-174`, `docs/WORKFLOW_ROUTING.md:67-69` | Mandatory documentation-search route when applicable. |
| claude-mem plugin 12.4.9 | `~/.claude/plugins/installed_plugins.json` | Installed current memory plugin. |
| claude-mem hooks | `~/.claude/plugins/cache/thedotmack/claude-mem/12.4.9/hooks/hooks.json` | Setup/session/prompt/tool/stop memory capture hooks. |
| claude-mem freshness hook | `~/.claude/settings.json` | SessionStart guardrail for memory version/health drift. |
| RTK Bash rewrite hook | `~/.claude/settings.json`, `docs/WORKFLOW_ROUTING.md:166` | Documented Bash output/compression layer. |

## Keep Useful

| Surface | Evidence | Reason |
|---|---|---|
| Playwright MCP | `~/.claude/settings.json`, `~/.codex/config.toml` | Useful for portal/UI validation. |
| GitHub Codex plugin/MCP | `~/.codex/config.toml` | Useful for PR/CI/repo operations. |
| Context7 | `~/.codex/config.toml` | Required by routing for external library API checks. |
| NotebookLM MCP | `~/.codex/config.toml`, `CLAUDE.md:85` | Useful architecture oracle; verify against live source. |
| Superpowers Codex plugin | `~/.codex/config.toml` | Useful for structured planning/review workflows. |
| Crispy health hook | `~/.claude/settings.json` | Useful session-start health sentinel. |
| GSD hooks | `~/.claude/settings.json` | Useful if GSD workflow is active. |

## Per-Task

| Surface | Evidence | Reason |
|---|---|---|
| Blender MCP/plugin hooks | Claude/Codex settings | Required for Blender/K1 marketing work, not normal Lightwave firmware tasks. |
| Swift/iOS plugin | Claude/Codex settings | Relevant for `lightwave-ios-v2`, not firmware-only tasks. |
| Vercel plugin/hooks | Claude plugin settings/hooks | Web/deploy-specific. |
| Build Web Apps plugin | Codex config | Portal/frontend tasks only. |
| Linear plugin | Codex config | Issue tracking only. |
| EasyEDA MCP | Codex config | PCB/hardware design only. |
| Obsidian vault MCP | Codex MCP config | Personal/reference knowledge only. |
| Stitch MCP | Claude/Codex MCP config | UI generation/design only. |

## Disable Candidates

These are recommendations only. No tools were disabled.

| Surface | Evidence | Reason |
|---|---|---|
| Twitter MCP | Claude settings | Secret-bearing cookie env in global settings; unrelated to Lightwave core workflow. |
| Pathmode MCP | Claude settings | Secret-bearing external API surface; no mandatory Lightwave route found. |
| Nogic MCP | Codex MCP config | Secret-bearing external API surface; no mandatory Lightwave route found. |
| Stripe Codex plugin | Codex config | Unrelated to Lightwave firmware/tooling unless payment work is active. |
| Puppeteer MCP | Codex config | Overlaps Playwright; keep only if a task specifically needs it. |
| Old active cache copies | Claude plugin cache | Older cache versions present beside selected current versions; do not delete without loader check. |
| Disabled plugin archive | `~/.claude/plugins.disabled-20260122173309` | Already outside active plugin path; cleanup candidate only after backup-retention decision. |
| Global bypass permission mode | Claude settings | High-trust mode; operationally convenient but audit-risky. Do not change without Captain decision. |

## Unknown

| Surface | Evidence | Reason |
|---|---|---|
| autocontext MCP | Claude settings | No mandatory Lightwave route established in inspected docs. |
| devkg MCP | Claude/Codex MCP config | Potentially useful graph surface; current operational role not proven. |
| code-review-graph MCP/hook | Claude settings | Hook updates graph after edits; keep only if graph is actively consumed. |
| mcp-agent-mail | Claude settings | Local coordination server; live need not proven. |
| detailspro MCP | Claude settings | Design-app integration; not Lightwave mandatory. |
| code-context MCP | `.claude/mcp-config.json` | Local code-context surface; not referenced by current routing table. |

## Active Hook Inventory

| Hook File | Events |
|---|---|
| `~/.claude/settings.json` | SessionStart, PostToolUse, PreToolUse |
| `~/.claude/plugins/cache/thedotmack/claude-mem/12.4.9/hooks/hooks.json` | Setup, SessionStart, UserPromptSubmit, PostToolUse, PreToolUse, Stop |
| `~/.claude/plugins/cache/superpowers-marketplace/episodic-memory/1.0.15/hooks/hooks.json` | SessionStart |
| `~/.claude/plugins/cache/hamel-review/review-loop/1.8.0/hooks/hooks.json` | Stop |
| `~/.claude/plugins/cache/claude-plugins-official/vercel/0.40.1/hooks/hooks.json` | SessionStart, SessionEnd |
| `~/.claude/plugins/marketplaces/thedotmack/plugin/hooks/hooks.json` | Setup, SessionStart, UserPromptSubmit, PostToolUse, PreToolUse, Stop |
| `~/.claude/plugins/marketplaces/thedotmack/cursor-hooks/hooks.json` | beforeSubmitPrompt, afterMCPExecution, afterShellExecution, afterFileEdit, stop |
| `~/.claude/skills/last30days/hooks/hooks.json` | SessionStart |

## Notes

- Project `.claude/settings.json` only contains an empty `enabledPlugins` object, so global plugin enablement is the active source for Claude plugin state.
- No hooks, plugins, MCP servers, or settings were changed.
