# CC Audit Execution Plan

**Label:** GROUNDED for the plan structure and local preflight facts cited below. DEGRADED-MODE for any future cost, 90-day, or productive-token-share claim until billing/proxy evidence is supplied.

**Purpose:** Run the `CC_Audit.md` workflow in this Lightwave-Ledstrip workspace without violating repository governance, leaking private transcript content, or treating unsupported headline metrics as proved.

**Input document:** `CC_Audit.md`.

**Execution stance:** read-only first. Any plugin, MCP, hook, skill, settings, or instruction-file change is a separate Captain-approved remediation task.

---

## 1. Source Constraints

| Constraint | Source |
|---|---|
| Tactical outputs must be labelled `GROUNDED`, `DEGRADED-MODE`, or `REFUSED`. | `CLAUDE.md:3-21` |
| Claims must be traced to upstream facts when labelled `GROUNDED`. | `CLAUDE.md:7-11` |
| Multi-document / multi-subsystem analysis must use subagents. | `CLAUDE.md:47-64` |
| The repo root is allowlisted; new root files are non-compliant unless approved. | `CLAUDE.md:272-280` |
| Governance docs belong under `instructions/`; technical docs belong under `docs/`; tools under `tools/`. | `CLAUDE.md:282-296` |
| Repository governance forbids silent deletion and requires changelog fragments for agent changes. | `instructions/repo-governance-v1.md:38-64` |
| Filenames should use lowercase kebab-case. | `instructions/naming-policy-v1.md:7-16` |
| `CC_Audit.md` asks for an audit script, but its sample command assumes `~/.claude/logs`. | `CC_Audit.md:356-399` |
| `CC_Audit.md` itself says repo-internal use needs local evidence paths for the 90-day dataset. | `CC_Audit.md:21-27` |

---

## 2. SSA Cross-Check Summary

Four read-only SSAs cross-checked the document before this plan:

| SSA Focus | Result |
|---|---|
| Governance compatibility | The audit is compatible as read-only measurement. Do not blindly shrink `CLAUDE.md`, disable mandatory tools, or create `claude-audit.sh` at repo root. |
| Local configuration inventory | Current local overhead is materially higher than the generic sample: large instruction surface, global hooks, enabled plugins, broad MCP surfaces, and many skill files. |
| Script portability | The sample script is not directly runnable here: `~/.claude/logs` is missing and BSD `grep` does not support the required `-P` usage. Use `jq` over JSONL transcripts instead. |
| Evidence / metrics model | Direct metrics exist for transcript token/cache fields, hooks, MCP counts, instruction sizes, and cache-miss diagnostics. Spend, 90-day usage, and productive-token share are not directly present. |

---

## 3. Current Local Preflight Facts

These facts were gathered read-only on 2026-05-04 from this workspace.

| Area | Current Fact | Evidence Command |
|---|---|---|
| Instruction word count | `~/.claude/CLAUDE.md` 2600 words; repo `CLAUDE.md` 6454 words; `.claude/CLAUDE.md` 82 words; `AGENTS.md` 822 words; combined 9958 words. | `wc -w ~/.claude/CLAUDE.md CLAUDE.md .claude/CLAUDE.md AGENTS.md` |
| Claude global hooks | `SessionStart` 3, `PostToolUse` 2, `PreToolUse` 4. | `jq '.hooks // {} | to_entries | map({key, count:(.value|length)})' ~/.claude/settings.json` |
| Project hooks | `.claude/settings.json` has no hooks. | `jq '.hooks // {}' .claude/settings.json` |
| Enabled Claude plugins | 7 enabled in `~/.claude/settings.json`. | `jq '.enabledPlugins // {}' ~/.claude/settings.json` |
| Claude MCPs | 11 user-level MCP servers in `~/.claude/settings.json`; project MCPs must also be inventoried from `.claude/mcp-config.json` and `.mcp.json`. | `jq '.mcpServers // {} | keys' ~/.claude/settings.json` |
| Codex MCPs | 14 MCP servers in `~/.codex/config.toml`; additional Codex MCP config may exist in `~/.codex/mcp.json`. | `rg -n '^\\[mcp_servers\\.' ~/.codex/config.toml` |
| Claude logs path | `~/.claude/logs` is absent. | `[ -d ~/.claude/logs ]` |
| Claude transcript path | Exact workspace transcripts are under `~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip`. | `find ~/.claude/projects -type f -name '*.jsonl'` |
| Codex transcript path | Codex sessions are under `~/.codex/sessions`. | `find ~/.codex/sessions -type f -name '*.jsonl'` |
| Tool prerequisites | `jq` exists at `/opt/homebrew/bin/jq`; local `/usr/bin/grep` does not support the script's GNU `grep -P` assumption. | `jq --version`; `grep -P` check |

**Security note:** `~/.claude/settings.json`, `~/.codex/config.toml`, and transcript JSONL can contain tokens, prompts, tool outputs, and environment values. Reports must aggregate and redact; do not paste raw files.

---

## 4. Evidence Model

Use four evidence classes:

| Class | Meaning | Allowed Claims |
|---|---|---|
| Direct | Present in local files or transcript fields. | Token/cache totals, hook counts, MCP counts, instruction word counts, session counts, cache-miss counts. |
| Derived | Computed from direct data plus an explicit rule. | Estimated cost after Captain supplies a price table or billing export; session duration with an idle-gap rule. |
| Classified | Requires human or model judgement over transcript content. | Productive vs overhead token share, wrong-direction generations, irrelevant skill loading, unnecessary extended thinking. |
| Unavailable | Not present locally. | 90-day claim, $1,340 spend claim, HTTP proxy payload methodology, universal 73% overhead claim for this workspace. |

Do not promote a `Derived`, `Classified`, or `Unavailable` item to `Direct`.

---

## 5. Runbook

### Phase 0 - Preflight

Goal: confirm the audit can run without mutation.

Commands:

```bash
pwd
git status --short
command -v jq
jq --version
[ -d "$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip" ] && echo "claude transcripts present"
[ -d "$HOME/.codex/sessions" ] && echo "codex transcripts present"
```

Stop conditions:

- `jq` missing.
- Exact workspace transcript directory missing and Captain expects Claude Code transcript metrics.
- Any command would print raw secrets or transcript content.

### Phase 1 - Instruction Surface Inventory

Goal: measure always-loaded instruction/document surfaces.

Commands:

```bash
wc -l -w "$HOME/.claude/CLAUDE.md" CLAUDE.md .claude/CLAUDE.md AGENTS.md 2>/dev/null
wc -l -w docs/WORKFLOW_ROUTING.md 2>/dev/null
```

Output table:

| File | Lines | Words | Keep / Refactor Candidate | Reason |
|---|---:|---:|---|---|

Classification rules:

- `KEEP mandatory`: RBDO gate, READBACK, hard constraints, source-truth routing, memory routing, root allowlist, C++/QMD/clangd requirements.
- `REFACTOR candidate`: repeated examples, stale contradictions, long narrative, historical detail that can move to a referenced doc.
- `UNKNOWN`: anything that appears to encode a Captain decision but lacks an obvious current source.

### Phase 2 - Hook And Plugin Inventory

Goal: measure hook surfaces without executing or disabling them.

Commands:

```bash
jq '.hooks // {} | to_entries | map({key, count:(.value|length)})' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null
jq '.enabledPlugins // {}' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null
find "$HOME/.claude/plugins/cache" -path '*/hooks/hooks.json' -print 2>/dev/null
```

Output table:

| Surface | Count | Source | Classification | Rationale |
|---|---:|---|---|---|

Classification rules:

- `KEEP mandatory`: hooks that enforce repo safety, memory freshness, audit-chain integrity, or explicit Captain workflow.
- `KEEP useful`: hooks that are high-signal and low-volume.
- `DISABLE candidate`: status-only, duplicate, project-irrelevant, or plugin-specific hooks firing globally.
- `UNKNOWN requires Captain`: hooks with unclear purpose or safety role.

Do not run:

```text
/plugin disable <plugin-name>
```

### Phase 3 - MCP Inventory

Goal: measure MCP schema/tool-surface exposure.

Commands:

```bash
jq '.mcpServers // {} | keys' "$HOME/.claude/settings.json" .claude/mcp-config.json "$HOME/.codex/mcp.json" 2>/dev/null
rg -n '^\[mcp_servers\.' "$HOME/.codex/config.toml" 2>/dev/null
cat .mcp.json 2>/dev/null
```

Output table:

| MCP | Scope | Mandatory For Lightwave? | Default State Recommendation | Rationale |
|---|---|---|---|---|

Rules:

- Do not disable QMD/clangd-equivalent paths needed by Lightwave routing.
- Do not disable claude-mem or memory tooling without a replacement for the repo's session-start requirements.
- Treat browser/design/GitHub/Blender/EasyEDA tools as candidates for per-task enablement unless current workflow proves always-on value.

Do not run:

```text
/mcp disable <server>
```

### Phase 4 - Skill Inventory

Goal: distinguish installed skill surface from actually invoked skill surface.

Commands:

```bash
find "$HOME/.claude/skills" .claude/skills "$HOME/.codex/skills" "$HOME/.agents/skills" -name SKILL.md 2>/dev/null | sort | wc -l
find "$HOME/.claude/skills" .claude/skills "$HOME/.codex/skills" "$HOME/.agents/skills" -name SKILL.md 2>/dev/null | sort
find "$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip" -type f -name '*.jsonl' -mtime -30 -print0 2>/dev/null \
  | xargs -0 rg -n '"skillCount"|SKILL.md|skill_invoked' 2>/dev/null
```

Output table:

| Skill | Scope | Observed Evidence | Classification | Rationale |
|---|---|---|---|---|

Limitations:

- Installed skill count is direct.
- Invocation frequency may be weak or unavailable because this workspace does not use `~/.claude/logs/*.log`.

### Phase 5 - Token And Cache Aggregates

Goal: compute direct token/cache totals from Claude transcript JSONL without reading raw prompt content.

Command:

```bash
project_logs="$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip"

find "$project_logs" -type f -name '*.jsonl' -print0 |
while IFS= read -r -d '' f; do
  jq -r '
    select(.type == "assistant" and .message.usage?)
    | .message.usage
    | [
        (.input_tokens // 0),
        (.cache_creation_input_tokens // 0),
        (.cache_read_input_tokens // 0),
        (.cache_creation_input_tokens_5m // 0),
        (.cache_creation_input_tokens_1h // 0),
        (.output_tokens // 0)
      ]
    | @tsv
  ' "$f"
done |
awk '
  {
    input += $1
    cache_create += $2
    cache_read += $3
    cache_5m += $4
    cache_1h += $5
    output += $6
    count++
  }
  END {
    if (!count) { print "No usage records found"; exit 1 }
    printf "usage_records\t%d\n", count
    printf "input_tokens\t%d\n", input
    printf "cache_creation_input_tokens\t%d\n", cache_create
    printf "cache_read_input_tokens\t%d\n", cache_read
    printf "cache_creation_input_tokens_5m\t%d\n", cache_5m
    printf "cache_creation_input_tokens_1h\t%d\n", cache_1h
    printf "output_tokens\t%d\n", output
    printf "avg_input_tokens\t%.0f\n", input / count
  }'
```

Output table:

| Metric | Value | Evidence Class | Caveat |
|---|---:|---|---|

### Phase 6 - Cache-Miss And Hook Event Aggregates

Goal: count cache misses and hook events without exporting transcript bodies.

Commands:

```bash
project_logs="$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip"

find "$project_logs" -type f -name '*.jsonl' -print0 |
xargs -0 jq -r '
  select(.cache_miss? or .cacheMiss? or .cacheMissReason?)
  | [(.cache_miss // .cacheMiss // true), (.reason // .cacheMissReason // "unknown")] | @tsv
' 2>/dev/null | sort | uniq -c

find "$project_logs" -type f -name '*.jsonl' -print0 |
xargs -0 jq -r '
  select(.hook_event_name? or .hookEventName? or .event?)
  | (.hook_event_name // .hookEventName // .event)
' 2>/dev/null | sort | uniq -c | sort -rn
```

If transcript schemas differ, fall back to:

```bash
find "$project_logs" -type f -name '*.jsonl' -print0 |
xargs -0 rg -o '"(SessionStart|UserPromptSubmit|PreToolUse|PostToolUse|Stop)"' 2>/dev/null |
sed 's/.*"//; s/"//' | sort | uniq -c | sort -rn
```

### Phase 7 - Evidence Matrix

Goal: convert the audit into a workspace-specific truth table.

Required columns:

| CC Audit Claim | Source Line | Local Evidence | Evidence Class | Result | Caveat / Next Step |
|---|---|---|---|---|---|

Minimum claims to map:

- Headline 90-day / 430-hour / $1,340 / 73% claims.
- `CLAUDE.md` bloat.
- Conversation history re-reads.
- Hook injection.
- Cache misses.
- Skill loading.
- MCP tool definitions.
- Extended thinking.
- Wrong-direction generation.
- Plugin auto-update redundancy.

### Phase 8 - Remediation Plan

Goal: produce a separate Captain-decision artefact after read-only results exist.

Required sections:

1. `KEEP mandatory` list.
2. `KEEP useful` list.
3. `DISABLE candidate` list.
4. `REFACTOR candidate` instruction sections.
5. Governance contradictions requiring explicit correction.
6. Expected token/cost impact, labelled Direct/Derived/Classified.
7. Rollback plan.

No changes may be executed in this phase unless Captain explicitly approves them.

---

## 6. Output Artefacts

Recommended read-only outputs:

| Artefact | Path | Notes |
|---|---|---|
| Execution plan | `docs/tooling/cc-audit-execution-plan-2026-05-04.md` | This file. |
| Raw aggregate command output | `/tmp/lightwave-cc-audit-2026-05-04/` | Keep outside repo unless Captain approves check-in. |
| Evidence matrix | `docs/tooling/cc-audit-evidence-matrix-2026-05-04.md` | Create after running the audit. |
| Remediation proposal | `docs/tooling/cc-audit-remediation-proposal-2026-05-04.md` | Create only after evidence matrix. |
| Durable collector script | `tools/cc-audit-collector.sh` | Create only after Captain approval. |

Do not create `claude-audit.sh` at repository root.

---

## 7. Unsupported Claims Until More Evidence Exists

These must remain `DEGRADED-MODE` or `Unavailable` for this workspace unless Captain supplies billing/proxy data:

- 90-day coverage.
- 430 active hours.
- $1,340 API spend.
- 73% overhead share.
- 27% productive token share.
- ~65% productive token share after fixes.
- Exact cost impact of each proposed fix.

The workspace can still produce a useful direct audit of instruction size, hook/MCP/skill surface, transcript token/cache fields, cache misses, and session footprint.

---

## 8. Immediate Next Command Set

Run this first, read-only:

```bash
mkdir -p /tmp/lightwave-cc-audit-2026-05-04

{
  echo "=== preflight ==="
  pwd
  git status --short
  command -v jq
  jq --version

  echo
  echo "=== instruction counts ==="
  wc -l -w "$HOME/.claude/CLAUDE.md" CLAUDE.md .claude/CLAUDE.md AGENTS.md docs/WORKFLOW_ROUTING.md 2>/dev/null

  echo
  echo "=== claude hooks ==="
  jq '.hooks // {} | to_entries | map({key, count:(.value|length)})' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null

  echo
  echo "=== claude plugins ==="
  jq '.enabledPlugins // {}' "$HOME/.claude/settings.json" .claude/settings.json 2>/dev/null

  echo
  echo "=== claude mcps ==="
  jq '.mcpServers // {} | keys' "$HOME/.claude/settings.json" .claude/mcp-config.json "$HOME/.codex/mcp.json" 2>/dev/null

  echo
  echo "=== codex mcps ==="
  rg -n '^\[mcp_servers\.' "$HOME/.codex/config.toml" 2>/dev/null || true

  echo
  echo "=== skill files ==="
  find "$HOME/.claude/skills" .claude/skills "$HOME/.codex/skills" "$HOME/.agents/skills" -name SKILL.md 2>/dev/null | sort | wc -l

  echo
  echo "=== transcript inventory ==="
  project_logs="$HOME/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip"
  find "$project_logs" -type f -name '*.jsonl' 2>/dev/null | wc -l
  find "$HOME/.codex/sessions" -type f -name '*.jsonl' -mtime -30 2>/dev/null | wc -l
} | tee /tmp/lightwave-cc-audit-2026-05-04/preflight.txt
```

Then run Phase 5 and Phase 6 into separate aggregate files. Do not print raw transcript rows in terminal or commit `/tmp` output.
