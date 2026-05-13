# CC Audit Evidence Matrix

**Label:** GROUNDED for directly measured local facts. DEGRADED-MODE for cost, productive-share, and "wasted-token" claims that require billing data or a content classifier.

**Workspace:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip`

**Snapshot time:** 2026-05-05 00:13:38 +0800

**Plan source:** `docs/tooling/cc-audit-execution-plan-2026-05-04.md`

**Aggregate output directory:** `/tmp/lightwave-cc-audit-2026-05-05/`

**Archive/prune audit trail:** `/Users/spectrasynq/00.Project.archieves/Lightwave-Ledstrip/claude-project-transcripts/2026-05-04/`

---

## 1. Scope And Evidence Classes

| Class | Meaning | Used For |
|---|---|---|
| Direct | Present in local files, settings keys, archive audit logs, or transcript usage fields. | Counts, token/cache totals, hook-event strings, archive/prune verification. |
| Derived | Computed from direct data plus an explicit assumption. | Cost estimates after a price table or billing export exists. |
| Classified | Requires judgement over transcript content. | Productive vs overhead share, wrong-direction generation, unnecessary thinking, irrelevant skill loading. |
| Unavailable | Not present in local evidence. | Original 90-day / 430-hour / $1,340 / 73% claims for this workspace. |

Rule: do not promote `Derived`, `Classified`, or `Unavailable` to `Direct`.

---

## 2. Corpus State

| Metric | Value | Evidence Class | Source |
|---|---:|---|---|
| Live Lightwave Claude project size after prune | `426M` | Direct | `/tmp/lightwave-cc-audit-2026-05-05/preflight-post-prune.txt` |
| Live Lightwave Claude JSONL files after prune | `1,101` | Direct | `/tmp/lightwave-cc-audit-2026-05-05/preflight-post-prune.txt` |
| Live JSONL older than 60 days after prune | `0` | Direct | `/tmp/lightwave-cc-audit-2026-05-05/preflight-post-prune.txt` |
| Recent Codex JSONL files, 30-day window | `96` | Direct | `/tmp/lightwave-cc-audit-2026-05-05/preflight-post-prune.txt` |
| Archived/pruned cold Claude JSONL files | `5,118` | Direct | archive `*.metadata.txt`, `*.verify.txt`, `*.prune-log.txt`, `*.post-prune-verify.txt` |
| Cold archive size | `52M` | Direct | archive `*.verify.txt`, `*.post-prune-verify.txt` |
| Cold archive checksum | `OK` | Direct | archive `*.verify.txt`, `*.prune-preflight.txt` |

Archive result: the cold `>60 day` set was compressed to `52M`, verified, then its live originals were pruned. The archive manifest and tar listing both contain `5,118` files.

---

## 3. Local Overhead Inventory

| Area | Direct Measurement | Evidence Class | Source |
|---|---:|---|---|
| Combined instruction/document word count for audited set | `12,112` words | Direct | `/tmp/lightwave-cc-audit-2026-05-05/preflight-post-prune.txt` |
| `~/.claude/CLAUDE.md` | `2,600` words | Direct | same |
| repo `CLAUDE.md` | `6,454` words | Direct | same |
| `.claude/CLAUDE.md` | `82` words | Direct | same |
| `AGENTS.md` | `822` words | Direct | same |
| `docs/WORKFLOW_ROUTING.md` | `2,154` words | Direct | same |
| Claude global configured hooks | `SessionStart=3`, `PostToolUse=2`, `PreToolUse=4` | Direct | same |
| Project configured hooks | none in `.claude/settings.json` | Direct | same |
| Enabled Claude plugins | `7` | Direct | same |
| Claude user MCPs | `11` | Direct | same |
| Project Claude MCPs | `3` | Direct | same |
| Codex `~/.codex/mcp.json` MCPs surfaced in preflight | `4` | Direct | same |
| Codex `~/.codex/config.toml` MCP server sections | `14` | Direct | same |
| Skill files across audited Claude/Codex/agent/plugin-cache locations | `508` | Direct | same |

Interpretation: the workspace is far above the generic audit target for always-loaded/available surfaces. That does not by itself prove waste; it identifies the surfaces to classify as mandatory, useful, disable-candidate, or unknown.

---

## 4. Live Transcript Usage Aggregate

Post-prune live aggregate from `~/.claude/projects/-Users-spectrasynq-Workspace-Management-Software-Lightwave-Ledstrip`.

| Metric | Value | Evidence Class | Source |
|---|---:|---|---|
| Assistant usage records | `46,988` | Direct | `/tmp/lightwave-cc-audit-2026-05-05/token-cache-live-post-prune.tsv` |
| Input tokens | `2,393,526` | Direct | same |
| Cache creation input tokens | `529,187,688` | Direct | same |
| Cache read input tokens | `6,513,100,558` | Direct | same |
| Output tokens | `52,861,332` | Direct | same |
| Average input tokens per usage record | `51` | Direct | same |
| 5-minute cache creation field | `0` | Direct | same |
| 1-hour cache creation field | `0` | Direct | same |

Notes:

- The cache fields are direct transcript usage counters.
- These counters are not a billing report.
- They do not classify whether a token was productive or wasted.

---

## 5. Hook Event Evidence

| Event String | Count | Evidence Class | Source |
|---|---:|---|---|
| `PostToolUse` | `23,925` | Direct string-count | `/tmp/lightwave-cc-audit-2026-05-05/hook-cache-live-post-prune.txt` |
| `Stop` | `2,714` | Direct string-count | same |
| `PreToolUse` | `2,209` | Direct string-count | same |
| `SessionStart` | `1,252` | Direct string-count | same |
| `UserPromptSubmit` | `200` | Direct string-count | same |

Limitation: this is a string-count fallback over JSONL, not a structured hook-execution ledger. It is enough to prove hook surfaces are active in transcript history, but not enough to calculate injected-token volume.

---

## 6. Cache Miss Evidence

| Metric | Value | Evidence Class | Source |
|---|---:|---|---|
| Structured cache-miss reason records in live post-prune corpus | `0` | Direct for current corpus/query | `/tmp/lightwave-cc-audit-2026-05-05/hook-cache-live-post-prune.txt` |
| Structured cache-missed input token records in live post-prune corpus | `0` | Direct for current corpus/query | same |

The pre-prune corpus had cache-related field names in archived/cold transcripts, but those files have now been archived and pruned from live storage. If historical cache-miss analysis matters, run it against the archive by decompressing to a temporary location, not by rehydrating live Claude state.

---

## 7. Claim Matrix

| Audit Claim / Pattern | Local Result | Evidence Class | Status | Next Action |
|---|---|---|---|---|
| 90-day audit window | Local live corpus no longer contains the full cold set; archive metadata covers pruned `>60 day` transcripts. | Unavailable/Direct archive support | Not proved as a 90-day live audit. | Use archive + live corpus if a historical audit is required. |
| 430 active hours | No active-hour field found. | Unavailable | Not proved. | Requires session-duration model or external time ledger. |
| $1,340 spend | No billing/cost field found. | Unavailable | Not proved. | Requires Anthropic/Codex billing export or price table. |
| 73% overhead / 27% productive share | Productive-vs-overhead is not a native field. | Classified | Not proved. | Requires classifier/rubric over transcript content. |
| `CLAUDE.md` / instruction bloat | Audited instruction set is `12,112` words; repo `CLAUDE.md` alone is `6,454`. | Direct | Confirmed as a large surface. | Create protected-instruction map before proposing cuts. |
| Conversation history re-reads | Transcript corpus and usage counters exist, but per-turn history reread attribution was not computed. | Derived/Classified | Partially measurable. | Requires turn-level token/time model. |
| Hook injection waste | Configured hooks and hook-event strings exist. Injected-token volume not measured. | Direct + Classified | Surface confirmed; waste not quantified. | Classify each hook and measure hook tool-result sizes if needed. |
| Cache misses on resume | Current live corpus query found no structured cache-miss records after prune. | Direct for current corpus | Not confirmed in live post-prune set. | Historical analysis should run against archived cold corpus if needed. |
| Irrelevant skill loading | `508` skill files exist across audited locations. Invocation relevance was not classified. | Direct + Classified | Surface confirmed; waste not quantified. | Build skill invocation/relevance classifier or inspect session samples. |
| MCP tool schema overhead | Multiple MCP surfaces are configured: Claude user `11`, project `3`, Codex config `14`, Codex mcp JSON `4`. | Direct | Confirmed large surface. | Classify mandatory vs per-task MCPs. |
| Extended thinking on simple tasks | Claude effort-level risk was previously observed in settings; current matrix did not re-read that key. | Direct if re-read; Classified for "unnecessary" | Not quantified. | Add settings key to next collector; classify tasks by complexity. |
| Wrong-direction generation | No native field. | Classified | Not measured. | Requires transcript sampling/redaction workflow. |
| Plugin auto-update redundancy | `7` Claude plugins enabled; plugin hook configs not re-expanded in post-prune snapshot. | Direct surface | Surface confirmed; redundancy not proved. | Audit plugin hook configs with redacted output. |

---

## 8. Immediate Remediation Candidates

These are not authorised changes; they are candidates for the next Captain decision.

| Candidate | Basis | Risk | Required Gate |
|---|---|---|---|
| Create protected-instruction map for `CLAUDE.md` / `AGENTS.md` / `WORKFLOW_ROUTING.md`. | `12,112` audited words; repo `CLAUDE.md` `6,454` words. | Deleting mandatory safety gates would violate RBDO/workflow discipline. | Read and classify before editing. |
| Classify MCPs into `KEEP mandatory`, `KEEP useful`, `PER-TASK`, `DISABLE candidate`. | Large direct MCP surface across Claude and Codex. | Disabling QMD/clangd/memory tooling can break required workflow. | Captain approval before mutation. |
| Classify global hooks and plugin hooks. | Configured hooks + high hook-event counts. | Some hooks enforce memory freshness or safety. | Redacted hook inventory first. |
| Build durable collector script under `tools/`. | Manual commands are now proven. | Script must not emit secrets or raw transcript bodies. | Captain approval plus changelog fragment. |
| Archive policy for future Claude transcripts. | Cold archive/prune worked and reduced live project size from `918M` to `424M`. | Over-pruning could break recent recall. | Keep last 60 days live; archive first, prune after verification. |

---

## 9. Current Bottom Line

The CC audit is now grounded for this workspace at the inventory level:

- Live transcript storage has been reduced and cold transcripts are archived with checksum and manifest.
- The largest direct local issue is not one single setting; it is stacked surface area: large instruction files, many enabled tools/plugins/skills, and active hook history.
- The headline waste percentages and cost claims remain unproved here until a classifier and billing model are supplied.
- The next safe step is a protected-instruction and MCP/hook classification pass, not immediate disabling.
