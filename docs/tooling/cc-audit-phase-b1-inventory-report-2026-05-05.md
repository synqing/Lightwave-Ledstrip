# CC Audit Phase B1 Inventory Report

**Label:** GROUNDED for local command output and redacted config inventory captured on 2026-05-05.

**Scope:** Phase B1 inventory, backup, classification, and proposed-diff planning only. No live config reduction was performed.

---

## Summary

Phase B1 created timestamped backups of authorised config files, created hook/skill manifests, generated checksums, and produced a redacted inventory plus proposed mutation table for later approval. The run did not edit global Claude/Codex config, repo MCP config, plugin config, hook config, skill folders, generated `.claude/CLAUDE.md`, transcript JSONL, or `claude-mem`.

Backup root:

`/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05-phase-b1-023133/`

---

## Preflight Results

| Check | Result | Evidence |
|---|---|---|
| `git status --short` | PASS with dirty tree | Existing dirty tree is not Phase B1-owned; no staging was changed by this run. |
| `git diff --check` | PASS | Exit 0. |
| `tools/cc-audit-collector.sh` | PASS | Completed; current audited instruction words: `12,095`. |
| `tools/claude-transcript-archive-prune.sh` | PASS with retention finding | Default dry-run only; `eligible_files=7`; no archive/prune performed. |

Transcript retention finding: the 7 eligible JSONL files remain a separate P0 retention item. Phase B1 did not archive, prune, move, delete, or read transcript contents.

---

## Backup Manifest

| Source | Backup file | Status |
|---|---|---|
| `~/.claude/settings.json` | `claude-settings.json` | copied |
| `.claude/settings.json` | `project-claude-settings.json` | copied |
| `.claude/mcp-config.json` | `project-claude-mcp-config.json` | copied |
| `.mcp.json` | `project-mcp.json` | copied |
| `~/.codex/config.toml` | `codex-config.toml` | copied |
| `~/.codex/mcp.json` | `codex-mcp.json` | copied |
| `~/.claude/plugins/installed_plugins.json` | `installed-plugins.json` | copied |
| Claude plugin hook paths | `claude-plugin-hooks.manifest` | manifest only; 19 paths |
| Claude skill roots | `claude-skill-roots.manifest` | manifest only |
| Codex/agent skill and plugin roots | `codex-skill-plugin-roots.manifest` | manifest only |

Forbidden backup path check:

| Check | Count |
|---|---:|
| Transcript JSONL copied | `0` |
| `~/.claude-mem` path copied | `0` |
| generated `.claude/CLAUDE.md` copied | `0` |
| skill source files copied | `0` |
| plugin cache/source package files copied | `0` |

---

## Checksum List

`shasum -a 256 -c SHA256SUMS` passed for all entries.

```text
f6d086b414a7c3fb10b5bbd4d34276ba270b40706aaaf7cb167d97b4fc33b4b3  claude-plugin-hooks.manifest
fc6a0133441e661e5ed04c9b9b5844c06cfec47f13206b4e3ae68e34ddd34018  claude-settings.json
d32069094ec6f1472b67a517e64e1e9f2efc8c8b26597050ce19c2e26ea313a0  claude-skill-roots.manifest
3d10a502cbf8546ee3abc86ff53d7daa2ff1ab34f964fd5631f5c41f82dcf66a  codex-config.toml
4f6665cfaa4f9376650c1e5a318c1a7042005b66cd3c71a4161d57dc20b8d083  codex-mcp.json
113e7042ecd2ef3f6d2a030c92720f01b6d28a05c488a229200eab29bf7e790b  codex-skill-plugin-roots.manifest
0bdce00a05a07869193d455c4d6f8b2b7504419954b97ac18490a30477e8e764  installed-plugins.json
a810d4724ae167b5140116b92149cb86a6f01a7f229c9c3024a193626c353e1e  project-claude-mcp-config.json
0fe84d03b8ed583db473f6815d63b80fc7dba01fe4fb064344bb2b731d3dee62  project-claude-settings.json
7b03229deea574aa661d19979b00bd795d1ec4a21aff2c1f64c6d26266672dfe  project-mcp.json
```

---

## Redacted Inventory

### Config Presence And Keys

| Config | Scope | Presence | Top-level keys only |
|---|---|---|---|
| `~/.claude/settings.json` | Claude-global | present | `agentPushNotifEnabled`, `autoCompactEnabled`, `awaySummaryEnabled`, `effortLevel`, `enabledPlugins`, `env`, `extraKnownMarketplaces`, `hooks`, `mcpServers`, `permissions`, `remoteControlAtStartup`, `skipDangerousModePermissionPrompt`, `statusLine`, `teammateMode`, `theme` |
| `.claude/settings.json` | repo-local | present | `enabledPlugins` |
| `.claude/mcp-config.json` | repo-local | present | `mcpServers` |
| `.mcp.json` | repo-local | present | `mcpServers` |
| `~/.codex/config.toml` | Codex-global | present | model fields, MCP server sections, plugin sections |
| `~/.codex/mcp.json` | Codex-global | present | `mcpServers` |
| `~/.claude/plugins/installed_plugins.json` | Claude-global | present | `plugins`, `version` |

### MCP Names

| Source | Scope | MCP names |
|---|---|---|
| `~/.claude/settings.json` | Claude-global | `Context-Engineer`, `autocontext`, `blender`, `code-review-graph`, `detailspro`, `devkg`, `mcp-agent-mail`, `pathmode`, `playwright`, `stitch`, `twitter` |
| `.claude/mcp-config.json` | repo-local | `code-context`, `context-engineer`, `mcp-search` |
| `.mcp.json` | repo-local | `clangd`, `qmd` |
| `~/.codex/mcp.json` | Codex-global | `devkg`, `nogic`, `obsidian-vault`, `stitch` |
| `~/.codex/config.toml` | Codex-global | `blender`, `claude-mem`, `context7`, `easyeda`, `fetch`, `filesystem-root`, `github`, `knowledge-graph`, `memory`, `playwright`, `puppeteer`, `sequential-thinking`, `notebooklm-mcp` |

### Plugin Names And Versions

| Plugin | Scope/version |
|---|---|
| `atomic-agents@claude-plugins-official` | `project:f849087b26bb` |
| `clangd-lsp@claude-plugins-official` | `project:1.0.0` |
| `claude-mem@thedotmack` | `project:12.4.9` |
| `episodic-memory@superpowers-marketplace` | `project:1.0.15` |
| `frontend-design@claude-plugins-official` | `project:unknown` |
| `review-loop@hamel-review` | `user:1.8.0` |
| `swift-lsp@claude-plugins-official` | `project:1.0.0`, `user:1.0.0` |
| `vercel@claude-plugins-official` | `project:0.40.1` |

Enabled Claude plugins from settings:

`atomic-agents@claude-plugins-official`, `clangd-lsp@claude-plugins-official`, `claude-mem@thedotmack`, `frontend-design@claude-plugins-official`, `review-loop@hamel-review`, `swift-lsp@claude-plugins-official`, `vercel@claude-plugins-official`

Codex plugin sections:

`build-ios-apps@openai-curated`, `github@openai-curated`, `stripe@openai-curated`, `vercel@claude-plugins-official`, `superpowers@openai-curated`, `build-web-apps@openai-curated`, `linear@claude-plugins-official`

### Hook Event Names

| Hook source | Scope | Event names |
|---|---|---|
| `~/.claude/settings.json` | Claude-global | `SessionStart`, `PostToolUse`, `PreToolUse` |
| `~/.claude/plugins/cache/thedotmack/claude-mem/12.4.9/hooks/hooks.json` | Claude-global plugin cache | `Setup`, `SessionStart`, `UserPromptSubmit`, `PostToolUse`, `PreToolUse`, `Stop` |
| `~/.claude/plugins/marketplaces/thedotmack/plugin/hooks/hooks.json` | Claude-global marketplace source | `Setup`, `SessionStart`, `UserPromptSubmit`, `PostToolUse`, `PreToolUse`, `Stop` |
| `~/.claude/plugins/cache/hamel-review/review-loop/1.8.0/hooks/hooks.json` | Claude-global plugin cache | `Stop` |
| `~/.claude/plugins/cache/superpowers-marketplace/episodic-memory/1.0.15/hooks/hooks.json` | Claude-global plugin cache | `SessionStart` |
| `~/.claude/plugins/cache/claude-plugins-official/vercel/0.40.1/hooks/hooks.json` | Claude-global plugin cache | `SessionStart`, `SessionEnd` |
| `~/.claude/skills/last30days/hooks/hooks.json` | Claude-global skill hook | `SessionStart` |
| Marketplace style/security/ralph/hookify hook files | Claude-global marketplace source | `SessionStart`, `PreToolUse`, `PostToolUse`, `UserPromptSubmit`, `Stop` depending on file |

### Skill Root Counts

| Root | Scope | `SKILL.md` count |
|---|---|---:|
| `~/.claude/skills` | Claude-global | `121` |
| `.claude/skills` | repo-local | `31` |
| `~/.codex/skills` | Codex-global | `49` |
| `~/.codex/plugins/cache` | Codex-global plugin cache | `78` |
| `~/.agents/skills` | agent-global | `120` |

---

## Classification Table

| Surface | Scope | Classification | Reason |
|---|---|---|---|
| RBDO/READBACK/protected repo routing | repo-local docs | `KEEP mandatory` | Tactical-output and workflow safety gates. |
| `claude-mem` routing, plugin, hooks | Claude-global + Codex-global references | `KEEP mandatory` | Production memory route; do not mutate in Phase B1. |
| `.mcp.json` `clangd`, `qmd` | repo-local | `KEEP mandatory` | Required by protected repo workflow. |
| Context7 | Codex-global | `KEEP mandatory` when external APIs matter | Required for unstable/current library API facts. |
| GitHub Codex plugin/MCP | Codex-global | `KEEP useful` | PR/CI/repo operations; not a disable candidate. |
| Playwright | Claude-global + Codex-global | `KEEP useful` / `PER-TASK candidate` | Useful for portal/UI verification; not firmware-only default. |
| Superpowers/review/GSD-related surfaces | mixed | `KEEP useful` | Useful when an explicit workflow invokes them. |
| NotebookLM runtime MCP | Codex-global | `PER-TASK candidate` | Preserve routing doctrine; reduce runtime load only in non-architecture sessions. |
| Blender | Claude-global + Codex-global | `PER-TASK candidate` | K1 Blender/3D only. |
| Vercel/build-web/frontend surfaces | Claude-global + Codex-global | `PER-TASK candidate` | Web/deploy/frontend only. |
| Swift/iOS plugin | Claude-global/Codex plugin | `PER-TASK candidate` | `lightwave-ios-v2` only, not firmware-only sessions. |
| EasyEDA | Codex-global | `PER-TASK candidate` | PCB/hardware design only. |
| Linear | Codex-global | `PER-TASK candidate` | Issue tracking only. |
| Obsidian/Stitch/detailspro | mixed | `PER-TASK candidate` | Personal knowledge or UI generation only. |
| Twitter, Pathmode, Nogic, Stripe, duplicate Puppeteer | mixed | `DISABLE candidate` | Not Lightwave mandatory; future mutation needs approval and rollback. |
| autocontext, devkg, code-review-graph, mcp-agent-mail, code-context | mixed | `UNKNOWN / leave unchanged` | Explicitly immutable in Phase B1. |

---

## Exact Proposed Mutation Table For Later Approval

No row below was executed in Phase B1.

| surface name | config path | current default-loading mechanism | proposed action | exact key/block that would be changed | reason | risk rating | rollback source | rollback verification | smoke check | scope |
|---|---|---|---|---|---|---|---|---|---|---|
| GitHub MCP/plugin | `~/.codex/config.toml` | `[mcp_servers.github]` and `[plugins."github@openai-curated"]` | Keep useful; optionally per-task only for firmware-local sessions | none in B1; later candidate blocks are `[mcp_servers.github]`, `[mcp_servers.github.env]`, `[plugins."github@openai-curated"]` | Needed for PR/CI/repo operations; not a disable candidate | Medium | `codex-config.toml` | redacted section-header diff restores same GitHub blocks | `rg -n '^\\[mcp_servers\\.github\\]|^\\[plugins\\.\"github@openai-curated\"\\]' ~/.codex/config.toml` | Codex-global |
| NotebookLM runtime MCP | `~/.codex/config.toml` | `[mcp_servers.notebooklm-mcp]` | Per-task candidate; preserve routing docs | `[mcp_servers.notebooklm-mcp]` only | Architecture oracle is useful but not every runtime session needs schema load | Medium | `codex-config.toml` | redacted section-header diff restores block | `rg -n '^\\[mcp_servers\\.notebooklm-mcp\\]' ~/.codex/config.toml`; verify `CLAUDE.md` routing unchanged | Codex-global |
| Blender MCP | `~/.claude/settings.json`, `~/.codex/config.toml` | Claude `mcpServers.blender`; Codex `[mcp_servers.blender]` | Per-task candidate | `mcpServers.blender`, `[mcp_servers.blender]` | 3D/K1 marketing only | Medium | `claude-settings.json`, `codex-config.toml` | redacted MCP-name diff restores `blender` | run redacted MCP-name listing | Claude-global + Codex-global |
| Vercel/plugin hooks | `~/.claude/plugins/installed_plugins.json`, `~/.codex/config.toml`, hook manifests | enabled Claude plugin; Codex plugin section; plugin hook files | Per-task candidate; no hook mutation until hook backup plan | plugin key `vercel@claude-plugins-official`; `[plugins."vercel@claude-plugins-official"]`; hook files only in future hook phase | Web/deploy-specific | Medium/High | `installed-plugins.json`, `codex-config.toml`, future hook backup | plugin-name diff plus hook event diff | session start without Vercel warning; Vercel task re-enable smoke | Claude-global + Codex-global |
| Swift/iOS plugin | `~/.claude/plugins/installed_plugins.json`, `~/.codex/config.toml` | Claude `swift-lsp`; Codex `build-ios-apps` plugin | Per-task candidate | `swift-lsp@claude-plugins-official`; `[plugins."build-ios-apps@openai-curated"]` | iOS-only, not firmware-only sessions | Medium | `installed-plugins.json`, `codex-config.toml` | plugin-name diff restores blocks | Swift/iOS task can re-enable and list plugin | Claude-global + Codex-global |
| Playwright/Puppeteer | `~/.claude/settings.json`, `~/.codex/config.toml` | Claude `mcpServers.playwright`; Codex `[mcp_servers.playwright]`, `[mcp_servers.puppeteer]` | Keep Playwright useful; Puppeteer disable/per-task candidate after overlap check | `mcpServers.playwright`, `[mcp_servers.playwright]`, `[mcp_servers.puppeteer]` | Browser testing useful, but Puppeteer may duplicate Playwright | Medium | `claude-settings.json`, `codex-config.toml` | MCP-name diff restores browser surfaces | browser task re-enable smoke with MCP section listing | Claude-global + Codex-global |
| EasyEDA | `~/.codex/config.toml` | `[mcp_servers.easyeda]` | Per-task candidate | `[mcp_servers.easyeda]`, `[mcp_servers.easyeda.env]` | PCB-only surface | Medium | `codex-config.toml` | redacted section-header diff restores block | EasyEDA task re-enable smoke by section listing only | Codex-global |
| Linear | `~/.codex/config.toml` | `[plugins."linear@claude-plugins-official"]` | Per-task candidate | `[plugins."linear@claude-plugins-official"]` | Issue tracking only | Low/Medium | `codex-config.toml` | plugin section diff restores block | plugin section visible before Linear task | Codex-global |
| Twitter MCP | `~/.claude/settings.json` | Claude `mcpServers.twitter` | Disable candidate for later approval | `mcpServers.twitter` | Secret-bearing/social surface; not Lightwave mandatory | High | `claude-settings.json` | MCP-name diff restores `twitter` | redacted MCP listing; no secret output | Claude-global |
| Pathmode MCP | `~/.claude/settings.json` | Claude `mcpServers.pathmode` | Disable candidate for later approval | `mcpServers.pathmode` | External API surface; not mandatory | High | `claude-settings.json` | MCP-name diff restores `pathmode` | redacted MCP listing; no secret output | Claude-global |
| Nogic MCP | `~/.codex/mcp.json` | Codex JSON `mcpServers.nogic` | Disable candidate for later approval | `mcpServers.nogic` | External API surface; not mandatory | High | `codex-mcp.json` | MCP-name diff restores `nogic` | redacted MCP listing; no secret output | Codex-global |
| Stripe plugin | `~/.codex/config.toml` | `[plugins."stripe@openai-curated"]` | Disable/per-task candidate for later approval | `[plugins."stripe@openai-curated"]` | Payment-specific; not Lightwave firmware/tooling | Medium | `codex-config.toml` | plugin section diff restores block | plugin section visible before payment task | Codex-global |
| claude-mem hooks/plugin | `~/.claude/plugins/installed_plugins.json`, hook paths, `~/.claude/settings.json` | plugin + SessionStart/UserPromptSubmit/PreToolUse/PostToolUse/Stop hooks | Keep mandatory; no mutation | none | Memory capture/freshness safety-critical | High | copied configs only; hook files not copied in B1 | future hook phase requires actual hook backup and event diff | non-destructive health/version check only if separately approved | Claude-global |
| UNKNOWN surfaces | mixed | active or configured | Leave unchanged | none | User marked immutable in B1 | Unknown | copied configs where applicable | not applicable | redacted listing only | mixed |

---

## Final Git Status Separation

Phase B1 deliverables:

```text
?? docs/tooling/cc-audit-phase-b1-inventory-report-2026-05-05.md
?? instructions/changelog/2026-05-05--repo--phase-b1-config-inventory.md
```

Pre-existing dirty tree at final status:

```text
none
```

Note: preflight observed a larger dirty tree, including staged notebook bundle files and unrelated modified docs. Those items were not staged, unstaged, edited, or committed by Phase B1; they were no longer present in final `git status --short`.

---

## No-Mutation Statement

No live config reduction occurred. No global Claude config was edited. No global Codex config was edited. No MCP/plugin/hook/skill config was edited. No plugin cache/source package was edited. No skill source folder was edited. No transcript JSONL was archived, pruned, moved, deleted, or read for content. Generated `.claude/CLAUDE.md` was not touched. `claude-mem` and `~/.claude-mem/**` were not touched.
