# CC Audit Phase B Inventory Plan

**Label:** GROUNDED for paths and current classifications from the Phase A audit artefacts. This is a plan only; no global config, MCP, plugin, hook, skill, generated `.claude/CLAUDE.md`, or `claude-mem` mutation is authorised by this document.

**Rollback root for any future approved mutation:** `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/`

---

## Execution Boundary

Phase B must begin with backup creation and redacted inventory output. It must not print raw settings values, environment values, cookies, tokens, prompt bodies, or transcript rows. Any future mutation must be limited to manifest-listed paths that were backed up first.

`claude-mem` remains out of scope for pruning, deletion, disabling, or direct database changes.

---

## Inventory Plan

| surface | exact_config_path | current_status | proposed_action | reason | rollback_path | verification_command | risk_rating |
|---|---|---|---|---|---|---|---|
| Claude global settings | `~/.claude/settings.json` | Global hooks, MCPs, permissions, and plugin state are active. | Future approved run: back up, produce redacted key/count diff, then classify global-only surfaces as keep/per-task/disable-candidate. | High blast radius; contains hook and MCP defaults. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/claude-settings.json` | `jq 'keys' ~/.claude/settings.json` | High |
| Project Claude settings | `.claude/settings.json` | Audit found empty `enabledPlugins`; no project hooks reported. | Keep minimal unless a project-local override is explicitly needed. | Low current surface; avoid adding project-local drift. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/project-claude-settings.json` | `jq '{enabledPlugins, hooks}' .claude/settings.json` | Low |
| Project Claude MCP config | `.claude/mcp-config.json` | Project code-context/devkg-style surfaces require classification before mutation. | Inventory keys only, then decide keep/per-task in a separate approval. | Project-scoped but may affect source navigation. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/project-claude-mcp-config.json` | `jq '.mcpServers // {} | keys' .claude/mcp-config.json` | Medium |
| Project MCP config | `.mcp.json` | QMD and clangd are mandatory repo routes. | Keep; no disable action proposed. | Required by protected workflow gates. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/project-mcp.json` | `jq '.mcpServers // {} | keys' .mcp.json` | High |
| Codex config | `~/.codex/config.toml` | Contains Codex MCP/plugin sections including GitHub, Context7, NotebookLM, Playwright, EasyEDA, and Vercel-style surfaces. | Future approved run: back up, list section headers only, classify per-task candidates. | Useful but broad cross-domain surface. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/codex-config.toml` | `rg -n '^\\[mcp_servers\\.|^\\[plugins\\.|^model_reasoning_effort|^model = ' ~/.codex/config.toml` | Medium/High |
| Codex MCP JSON | `~/.codex/mcp.json` | Additional Codex MCP JSON surfaces present. | Inventory keys only; classify later. | May overlap with TOML-defined MCPs. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/codex-mcp.json` | `jq '.mcpServers // {} | keys' ~/.codex/mcp.json` | Medium |
| Claude installed plugins | `~/.claude/plugins/installed_plugins.json` | Enabled Claude plugins are active globally. | Future approved run: back up, list plugin names/versions only, classify per-task or keep. | Plugin changes can alter skills, hooks, and MCPs. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/installed-plugins.json` | `jq 'keys' ~/.claude/plugins/installed_plugins.json` | High |
| Claude plugin hook configs | `~/.claude/plugins/cache/**/hooks/hooks.json` | Active plugin hooks include claude-mem, Vercel, review-loop, episodic-memory, and related hook files. | Keep claude-mem safety hooks; classify domain hooks after backup. | Hook changes can break memory capture or inject unwanted context. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/plugin-hooks-manifest.txt` | `find ~/.claude/plugins/cache ~/.claude/plugins/marketplaces ~/.claude/skills -path '*/hooks/hooks.json' -print` | High for claude-mem; Medium for domain hooks |
| Skill roots | `~/.claude/skills`, `.claude/skills`, `~/.codex/skills`, `~/.agents/skills`, `~/.claude/plugins/cache`, `~/.codex/plugins/cache` | Audit found 508 physical `SKILL.md` files across searched roots. | Inventory only; no deletion. Classify daily Lightwave skills vs per-task domains. | Skill deletion can remove expected workflows; per-task activation is safer. | `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05/skill-roots-manifest.txt` | `find ~/.claude/skills .claude/skills ~/.codex/skills ~/.agents/skills ~/.claude/plugins/cache ~/.codex/plugins/cache -name SKILL.md -print | wc -l` | Medium |
| claude-mem state | `~/.claude-mem` | Production memory system; explicitly out of Phase B mutation scope. | No proposed mutation. Verify health only if a separate claude-mem task is approved. | Memory DB and worker state are high-risk operational infrastructure. | Not applicable; no mutation proposed. | `test -d ~/.claude-mem && echo claude-mem-present` | High |

---

## Required Future Gate

Before any Phase B mutation, produce a redacted before/after diff plan and verify backup manifests exist under the rollback root. Stop immediately if a command would expose secrets, raw transcript content, or unredacted settings values.
