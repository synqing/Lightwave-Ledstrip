# CC Audit Phase B2 Correction Report

**Label:** GROUNDED for local command output and direct config state, with accepted exceptions recorded.

## Summary

Phase B2 correction restored NotebookLM runtime loading for Codex and removed failed or unproven default MCP surfaces from local Claude/Codex/repo configuration. The correction is accepted with two explicit exceptions: Claude.ai Vercel remains as an out-of-scope remote Claude.ai connector, and the single transcript no-read breach is preserved as an exception record. Local/default-managed runtime reduction succeeded.

The correction used the B1 backup root:

`/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05-phase-b1-023133/`

Claude.ai Vercel still appears in `claude mcp list` as a remote Claude.ai connector that `claude mcp remove` cannot manage. It is out of scope for local CLI-managed MCP cleanup and must not be chased through local files. One over-broad Vercel source search matched Claude project transcript content; that breach is recorded here. No transcript archive, prune, move, or delete occurred.

## Commands Used

```bash
git status --short
cd /Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05-phase-b1-023133 && shasum -a 256 -c SHA256SUMS
claude mcp remove --help
claude mcp list
codex mcp list
which qmd || true
qmd --version || true
claude mcp list | grep qmd || true
jq '.mcpServers.qmd' .mcp.json
which code-context || true
code-context --help || true
claude mcp remove blender || true
claude mcp remove easyeda || true
claude mcp remove nogic || true
claude mcp remove stitch || true
claude mcp remove detailspro || true
claude mcp remove Vercel || true
claude mcp remove "claude.ai Vercel" || true
codex mcp list
git diff --check
```

An additional over-broad `rg` command searched under `/Users/spectrasynq/.claude` for Vercel registry state and matched transcript content. That command should not be repeated; future searches must exclude `/Users/spectrasynq/.claude/projects`.

## Config Files Edited

| Path | Scope | Change |
|---|---|---|
| `/Users/spectrasynq/.claude.json` | Claude live registry | Removed CLI-managed Blender, EasyEDA, Nogic, Stitch, DetailsPro; attempted Vercel removal. |
| `/Users/spectrasynq/.claude/settings.json` | Claude-global | Removed `autocontext`, `devkg`, and `mcp-agent-mail` from default MCP config. |
| `/Users/spectrasynq/.codex/config.toml` | Codex-global | Restored `[mcp_servers.notebooklm-mcp]`; removed Context7 default sections. |
| `/Users/spectrasynq/.codex/mcp.json` | Codex-global | Removed `devkg`; no MCPs remain in that JSON file. |
| `.mcp.json` | Repo-local tracked | Removed `qmd`; preserved `clangd`. |
| `.claude/mcp-config.json` | Repo-local ignored | Removed `code-context`; preserved `mcp-search` and `context-engineer`. |
| `CLAUDE.md` | Repo instructions | Demoted QMD/Context7 from protected mandatory gates; removed autocontext default guidance. |
| `docs/WORKFLOW_ROUTING.md` | Repo routing registry | Demoted QMD/Context7 and updated routing to NotebookLM plus current-source verification. |

## Before Live MCP List

Claude before correction listed these relevant surfaces:

```text
claude.ai Vercel - connected
plugin:claude-mem:mcp-search - connected
easyeda - failed
nogic - failed
blender - connected
stitch - connected
notebooklm-mcp - connected
qmd - failed
clangd - connected
detailspro - failed
Google Calendar - needs authentication
Gmail - needs authentication
```

Codex before correction listed:

```text
claude-mem
context7
github
playwright
```

## After Live MCP List

Claude after correction lists:

```text
claude.ai Vercel - connected
claude.ai Google Drive - connected
claude.ai Google Calendar - needs authentication
claude.ai Gmail - needs authentication
claude.ai PDF Viewer - connected
claude.ai Three.js 3D Viewer - connected
plugin:claude-mem:mcp-search - connected
auggie - connected
aidesigner - needs authentication
cinema4d - connected
notebooklm-mcp - connected
taskmaster-ai - connected
figma - connected
clangd - connected
```

Codex after correction lists:

```text
claude-mem
fetch
file-system
filesystem-root
github
knowledge-graph
memory
notebooklm-mcp
playwright
sequential-thinking
```

## Decisions

| Surface | Decision | Evidence |
|---|---|---|
| NotebookLM | Restored/preserved | `[mcp_servers.notebooklm-mcp]` restored in Codex; Claude and Codex list it. |
| qmd | Removed | `which qmd` and `qmd --version` fail; Claude health failed; `.mcp.json` now lists only `clangd`. |
| Context7 | Removed/demoted | Codex list showed it before, but no live health/usefulness proof was established; Codex config no longer lists it. |
| autocontext | Removed | Present only in Claude settings; no live list/consumer proof. |
| devkg | Removed | Present in Claude settings and Codex MCP JSON; no active consumer proved. |
| mcp-agent-mail | Removed | Present only in Claude settings; no active consumer proved. |
| code-context | Removed | `.claude/mcp-config.json` used `code-context`, but `code-context` is not on PATH. |
| Vercel | Accepted exception | Local CLI-managed Vercel plugin/default config is removed. Claude.ai Vercel still appears as a remote connector and is out of scope for this local cleanup. |

## Final Preserved Surfaces

```text
claude-mem
GitHub
Playwright
NotebookLM
clangd
Google Calendar
Gmail
generated .claude/CLAUDE.md
hooks
plugin cache/source packages
skill source folders
~/.claude-mem/**
```

## Final Removed Default Config Surfaces

```text
qmd
Context7
autocontext
devkg
mcp-agent-mail
code-context
Blender
EasyEDA
Nogic
Stitch
DetailsPro
Twitter
Pathmode
Puppeteer
Stripe
Linear
Swift/iOS
Codex Vercel plugin/default section
```

## Verification Results

| Check | Result | Evidence |
|---|---|---|
| B1 backup root exists | PASS | Backup root present. |
| B1 checksums | PASS | `shasum -a 256 -c SHA256SUMS` passed. |
| NotebookLM restored | PASS | Codex and Claude both list NotebookLM. |
| claude-mem preserved | PASS | Codex and Claude both list claude-mem. |
| GitHub preserved | PASS | Codex lists GitHub. |
| Playwright preserved | PASS | Codex lists Playwright and Claude config preserves Playwright. |
| qmd either healthy or removed | PASS | qmd removed from `.mcp.json` and docs demoted it. |
| Context7 healthy or removed/demoted | PASS | Context7 removed from Codex config and docs demoted it. |
| removed surfaces gone from local config | PASS | Redacted config-name check returns no removed-surface hits. |
| Vercel gone from live Claude list | ACCEPTED EXCEPTION | Claude.ai Vercel remains listed as an out-of-scope remote connector outside CLI-managed MCP config. |
| transcript no-read discipline | ACCEPTED EXCEPTION | An over-broad Vercel search under `/Users/spectrasynq/.claude` matched transcript content; the breach is recorded and must not be repeated. |
| `git diff --check` | PASS | Exit 0. |

With the two exceptions accepted by Captain, the correction is eligible for commit.

## Rollback Instructions

For B1-backed config files:

```bash
backup_root=/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/cc-remediation-config-backups/2026-05-05-phase-b1-023133
cp "$backup_root/claude-settings.json" /Users/spectrasynq/.claude/settings.json
cp "$backup_root/codex-config.toml" /Users/spectrasynq/.codex/config.toml
cp "$backup_root/codex-mcp.json" /Users/spectrasynq/.codex/mcp.json
cp "$backup_root/project-mcp.json" .mcp.json
cp "$backup_root/project-claude-mcp-config.json" .claude/mcp-config.json
```

For live Claude registry removals, re-add only the specific MCPs needed per task. The B1 backup root does not contain `/Users/spectrasynq/.claude.json`, so rollback for CLI-managed live registry entries is command-based rather than file-restore based.

Repo docs can be rolled back with Git after deciding whether QMD/Context7 should become protected again.

## Final Git Status

```text
 M .mcp.json
 M BACKLOG.md
 M CLAUDE.md
 M docs/WORKFLOW_ROUTING.md
 M firmware-v3/platformio.ini
 M firmware-v3/test/test_brightness_floor/main.cpp
?? docs/tooling/cc-audit-phase-b1-inventory-report-2026-05-05.md
?? docs/tooling/cc-audit-phase-b2-correction-report-2026-05-05.md
?? instructions/changelog/2026-05-05--repo--phase-b1-config-inventory.md
?? instructions/changelog/2026-05-05--repo--phase-b2-correction.md
```

## No-Touch Statement

No transcript archive, prune, move, or delete was performed. No hooks, skill source folders, plugin cache/source packages, generated `.claude/CLAUDE.md`, or `~/.claude-mem/**` were edited. Google Calendar and Gmail were not removed. The transcript no-read discipline failed once due the over-broad Vercel registry search described above; future local searches must exclude `/Users/spectrasynq/.claude/projects`.
