---
abstract: "Docs-backed claude-mem capability map and local integration decisions for Lightwave agents: memory search, Smart Explore, File Read Gate, folder context, configuration, and health gates."
---

# Claude-Mem Usage Optimisation - 2026-05-02

## Scope

Captain asked for a granular study of these official claude-mem resources and for integration into global and local agent instructions:

1. <https://docs.claude-mem.ai/usage/getting-started>
2. <https://docs.claude-mem.ai/architecture/search-architecture>
3. <https://docs.claude-mem.ai/context-engineering>
4. <https://docs.claude-mem.ai/usage/search-tools>
5. <https://docs.claude-mem.ai/usage/folder-context>
6. <https://docs.claude-mem.ai/progressive-disclosure>
7. <https://docs.claude-mem.ai/smart-explore-benchmark>
8. <https://docs.claude-mem.ai/file-read-gate>
9. <https://docs.claude-mem.ai/configuration>

## Capability Map

| Area | Documented Functions | Integration Decision |
|------|----------------------|----------------------|
| Automatic capture | Session hooks capture tool activity such as `Read`, `Write`, `Edit`, `Bash`, `Glob`, and `Grep`; the worker extracts title, subtitle, narrative, facts, concepts, type, and files. | Preserve automatic capture. Treat SessionStart output as an index, not complete truth. |
| Session summaries | Stop hook generates request, investigated, learned, completed, and next-step summaries. | Use summaries for orientation only. Verify current source/runtime for live behaviour. |
| Memory MCP | `__IMPORTANT`, `search`, `timeline`, `get_observations`. | Standard route is `search -> timeline -> get_observations`; no stale `mem-search` namespace. |
| Worker API | `GET /api/search`, `GET /api/timeline`, `POST /api/observations/batch`, `GET /api/health`, `GET /api/version`. | Health/version checks are part of the trust gate when memory freshness warnings appear. |
| Queue admin API | `GET /api/pending-queue`, `POST /api/pending-queue/process`, `DELETE /api/pending-queue/failed`, `DELETE /api/pending-queue/all`. | Queue admin routes are method-specific. `GET /api/pending-queue/failed` and `GET /api/pending-queue/all` returning `404` does not prove the routes are missing. Do not validate destructive routes by firing `DELETE` unless repair mode is explicit and backup/export protection is acceptable. |
| Search backend | SQLite FTS5 plus Chroma semantic search/hybrid retrieval. | Use FTS5 syntax for precise incidents and file names; use semantic search for vague topics. |
| Search filters | `query`, `limit`, `offset`, `type`, `obs_type`, `project`, `dateStart`, `dateEnd`, `orderBy`. | Start with `limit=3-5`, add `project` and date/type filters before fetching details. |
| Query syntax | Boolean operators, exact phrases, and column searches such as `title:`, `content:`, and `concepts:`. | Prefer exact phrases for known errors and symptoms, for example `"Timed out waiting for agent pool slot"`. |
| Token discipline | Index results are cheap; full observations are expensive; batching selected IDs saves requests/tokens. | Never fetch all search hits by default. Batch selected IDs in one `get_observations` call. |
| Context engineering | Just-in-time context, hybrid retrieval, compaction, structured notes, and sub-agent architectures. | Use memory indexes and references first; deploy sub-agents for independent research domains; keep main context for synthesis and edits. |
| Progressive disclosure | L1 index, L2 timeline/context, L3 full detail, L4 source files. | The instruction layer now names the four layers and requires selective fetching. |
| Legend/types | `session-request`, `gotcha`, `problem-solution`, `how-it-works`, `what-changed`, `discovery`, `why-it-exists`, `decision`, `trade-off`. | Critical types such as gotchas, decisions, and trade-offs are worth fetching earlier than generic changes. |
| Smart Explore | `smart_search`, `smart_outline`, `smart_unfold`. | Use for exact non-C++ code navigation and large Markdown/code files when available. In Lightwave C++ symbol work, clangd still wins because the repo hard gate says clangd first. |
| Smart Explore benchmark | Smart Explore is much cheaper for targeted code reads; Explore agents are better for cross-file synthesis. | Use Smart Explore for "where is this?" and "show this symbol"; use SSAs for "explain this subsystem". |
| File Read Gate | PreToolUse `Read` hook surfaces a by-file timeline before full read; small files under 1,500 bytes bypass the gate. | Treat gate output as intended guidance. Escalate from timeline to observations to Smart Explore before a full large-file read. |
| File timeline API | `GET /api/observations/by-file`. | Use as semantic priming for large files; current file reads remain necessary when the source may have changed. |
| Folder context | Generated folder `CLAUDE.md` files preserve manual content outside `<claude-mem-context>`, support worktrees, and can be regenerated or cleaned. | Keep disabled for Lightwave unless Captain explicitly approves generated file churn. Root governance must remain hand-authored. |
| Folder context commands | `bun scripts/regenerate-claude-md.ts --dry-run`, `--clean --dry-run`, `--clean`, `--project=<project>`. | Use only after explicit opt-in. If enabled temporarily, run dry-run first and clean before PRs. |
| Configuration | `~/.claude-mem/settings.json`, env overrides, worker port/host/model/provider/context/folder settings. | Make Lightwave's settings explicit for port/host and folder-context disabled state. |
| Troubleshooting | Restart worker, verify env, inspect logs, handle invalid model fallback and port conflicts. | Freshness hook now resolves the worker port from env/settings/fallback rather than hardcoding `37777`. |

## Local Integration Decisions

1. **Memory routing is layered.** `$RECALL_CLI` remains first for exact raw transcript recall because local history says it is more reliable for exact wording. claude-mem is first for synthesised decisions, bugfixes, discoveries, and session continuity. Direct source, DB, process, and log checks outrank memory for current truth.

2. **The canonical claude-mem tool namespace is `mcp-search`.** Active instruction files must use:

```text
mcp__plugin_claude-mem_mcp-search__search
mcp__plugin_claude-mem_mcp-search__timeline
mcp__plugin_claude-mem_mcp-search__get_observations
```

3. **Progressive disclosure is mandatory.** Agents should start with search index results, use timeline only when order matters, and fetch details only for filtered IDs.

4. **Folder context remains off.** The docs make folder context useful, but this repo already has strict root `CLAUDE.md` and `AGENTS.md` governance. Generated folder `CLAUDE.md` files would create avoidable drift unless Captain opts in.

5. **File Read Gate is not a failure.** It should prompt timeline/observation/Smart Explore use before expensive large-file reads.

6. **Smart Explore is adopted with Lightwave constraints.** It is suitable for non-C++ structural exploration and large Markdown navigation. For firmware C++ symbols, root `CLAUDE.md` still requires clangd first.

7. **Health gates are part of memory trust.** A reachable worker is not enough if queue backlog, version drift, or parser storms are present. Current-state claims must be verified live when warnings appear.

8. **Queue route validation must be method-aware.** The live 12.4.9 queue admin contract is:

```text
GET    /api/pending-queue
POST   /api/pending-queue/process
DELETE /api/pending-queue/failed
DELETE /api/pending-queue/all
```

Do not classify `/failed` or `/all` as missing from a `GET`/`POST` `404`; those routes are destructive `DELETE` endpoints. Freshness and audit checks should prove destructive route registration by inspecting the live worker bundle or by using a dedicated non-destructive route-introspection mechanism, not by calling `DELETE` against a live queue.

## Patched Surfaces

| Surface | Purpose |
|---------|---------|
| `~/.claude/CLAUDE.md` | Global memory protocol now distinguishes Crispy raw transcripts, claude-mem observations, direct truth, episodic fallback, and file memory. |
| `~/.claude/commands/recall.md` | `/recall` now follows layered routing and `search -> timeline -> get_observations`. |
| `CLAUDE.md` | Repo Session Start now uses `mcp-search`, batching, Smart Explore/File Read Gate guidance, and health-warning caveats. |
| `AGENTS.md` | Codex-visible repo instructions now point agents at the current memory routing. |
| `docs/WORKFLOW_ROUTING.md` | Phase 0 and memory table now encode progressive disclosure and remove the stale help/tool ambiguity. |
| `.claude/commands/recall.md` | Local command copy now matches the global command with Lightwave examples. |
| `.claude/mcp-config.json` | Local stale cache path updated to the current marketplace plugin script path. |
| `~/.claude/hooks/claude-mem-freshness-check.sh` | Freshness check resolves the worker port dynamically and validates queue routes with method-aware, non-destructive checks. |
| `~/.claude-mem/settings.json` | Folder-context disabled state is explicit. |

## Guardrails For Future Agents

- Do not treat claude-mem as source truth for current code. It is prior-session context.
- Do not call stale `mcp__plugin_claude-mem_mem-search__*` tools.
- Do not enable `CLAUDE_MEM_FOLDER_CLAUDEMD_ENABLED` in this repo without an explicit Captain decision.
- Do not edit inside generated `<claude-mem-context>` blocks.
- Do not ignore claude-mem health warnings; verify worker health, version, logs, and queue state before trusting fresh memory.
- Do not test `/api/pending-queue/failed` or `/api/pending-queue/all` with `GET` and report the resulting `404` as route drift. They are `DELETE` routes, and firing them is destructive.
- Do not read large code files blindly if File Read Gate or Smart Explore can answer the narrower question.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-02 | agent:codex | Created docs-backed claude-mem capability map and local integration decision record. |
| 2026-05-02 | agent:codex | Added method-aware queue admin route contract after live 12.4.9 validation. |
