# CC Audit Remediation Proposal

**Label:** DEGRADED-MODE for remediation impact estimates because injected-token volume, billing cost, and productive-token share are not directly measured. GROUNDED for the local inventory facts referenced from `docs/tooling/cc-audit-evidence-matrix-2026-05-05.md`.

**Purpose:** Convert the CC audit evidence matrix into a safe decision plan. This document does not authorise settings changes by itself.

---

## 1. Current Finding

The Lightwave workspace does not have one isolated overhead source. It has stacked surfaces:

- `12,112` words across the audited instruction/document set.
- `7` enabled Claude plugins.
- Claude hooks configured globally: `SessionStart=3`, `PostToolUse=2`, `PreToolUse=4`.
- MCP surfaces across Claude, project MCP config, and Codex.
- `508` skill files across audited Claude/Codex/agent/plugin-cache locations.
- Live post-prune transcript corpus still has `46,988` assistant usage records.

The archive/prune pass already reduced live Claude transcript storage from `918M` to `424M` and removed all live JSONL files older than 60 days.

---

## 2. Non-Negotiable Keep List

These surfaces should not be removed as part of a token-saving sweep.

| Surface | Classification | Reason |
|---|---|---|
| RBDO gate in `CLAUDE.md` | `KEEP mandatory` | Required for every tactical output. |
| READBACK protocol in `CLAUDE.md` | `KEEP mandatory` | Required before work in this repo. |
| Hard constraints in `CLAUDE.md` / `AGENTS.md` | `KEEP mandatory` | Protects firmware, AP/STA, render-path, and workflow invariants. |
| Root allowlist / governance rules | `KEEP mandatory` | Prevents non-compliant root files and uncontrolled structural changes. |
| `claude-mem` / memory routing | `KEEP mandatory` | Required by repo session-start workflow; do not prune or disable casually. |
| clangd-equivalent tooling | `KEEP mandatory for C++ work` | Repo explicitly requires clangd-first symbol navigation. |
| QMD / documentation search equivalent | `KEEP mandatory for documentation work` | Repo routing requires QMD-first documentation lookup. |
| Context7 / external API lookup | `KEEP mandatory when external APIs matter` | Required for non-stable library/API facts. |
| Archive/prune audit trail | `KEEP mandatory` | Proof that transcript deletion was archive-backed and reversible. |

---

## 3. Candidate Workstreams

### A. Protected Instruction Map

**Goal:** Reduce instruction bloat without deleting safety doctrine.

**Input evidence:** `CLAUDE.md` alone is `6,454` words; audited instruction set is `12,112` words.

**Method:**

1. Split `CLAUDE.md`, `AGENTS.md`, `.claude/CLAUDE.md`, and `docs/WORKFLOW_ROUTING.md` into sections.
2. Classify each section:
   - `KEEP inline`
   - `KEEP but move to referenced doc`
   - `DUPLICATE`
   - `STALE / contradiction`
   - `UNKNOWN Captain decision`
3. Produce a proposed patch only after the map exists.

**Known contradictions to handle carefully:**

- Forward handoff guidance conflict between `.claude` handoff language and `AGENTS.md` anti-redundancy rule.
- PipelineCore wording conflict versus current ESV11 canonical build path.

**Expected impact:** high, but not quantified until actual token deltas are measured after edits.

### B. MCP Surface Classification

**Goal:** Stop always-on tool schema sprawl without breaking required repo routing.

**Classification draft:**

| MCP / Tool Family | Proposed State | Rationale |
|---|---|---|
| clangd / C++ symbol tools | `KEEP mandatory when C++ active` | Required by repo. |
| mcp-search / claude-mem | `KEEP mandatory` | Required for memory/session routing. |
| QMD / code-context / context-engineer | `KEEP mandatory when docs/codebase search active` | Required by repo routing. |
| Context7 | `KEEP available; per-task acceptable` | Required for external API facts, but not every task. |
| NotebookLM | `PER-TASK` | Useful for broad architecture only; not current-source truth. |
| filesystem / fetch | `KEEP useful` | General execution support. |
| Playwright / puppeteer | `PER-TASK` | Needed for frontend/browser verification, not firmware/doc-only work. |
| Blender | `PER-TASK` | Needed for K1 Blender work, not Lightwave firmware/tooling audit. |
| GitHub | `PER-TASK` | Needed for PR/CI work, not every local audit. |
| EasyEDA | `PER-TASK` | PCB-specific; not generally needed in Lightwave audit sessions. |
| Vercel / frontend-design / detailspro / stitch / twitter | `DISABLE candidate for Lightwave firmware sessions` | Likely project-irrelevant unless doing web/marketing/social tasks. |

**Required gate:** Captain approval before disabling any MCP/plugin globally.

### C. Hook Classification

**Goal:** Preserve safety hooks while removing duplicate or project-irrelevant hook injection.

**Direct evidence:** configured global hooks plus transcript hook strings: `PostToolUse=23,925`, `PreToolUse=2,209`, `SessionStart=1,252`, `UserPromptSubmit=200`.

**Next audit command class:** inspect hook names and command paths only, redacting arguments/env. Do not print raw settings files.

**Classification target:**

| Hook Type | Default Position |
|---|---|
| Memory freshness / queue safety | `KEEP mandatory` |
| Tool-output compression / safety guard | `KEEP useful`, measure output impact |
| Status-only loaded messages | `DISABLE candidate` |
| Plugin hooks for inactive domains | `PER-TASK` or `DISABLE candidate` |

### D. Skill Surface Classification

**Goal:** Distinguish installed skills from actually useful daily skills.

**Direct evidence:** `508` skill files across audited locations.

**Plan:**

1. Count installed skills by location.
2. Search current live transcripts for `SKILL.md`, `skillCount`, and explicit skill invocation markers.
3. Mark domain-specific skills as `PER-TASK`.
4. Keep firmware, memory, review, debugging, and project-governance skills available.

**Caveat:** installed skill count is direct; relevance/waste requires classification.

### E. Session And Reasoning Defaults

**Direct evidence:** Claude settings currently show `effortLevel=xhigh`, `autoCompactEnabled=false`, `awaySummaryEnabled=false`; Codex reasoning is `medium`.

**Candidate changes:**

| Setting | Candidate | Risk |
|---|---|---|
| Claude effort level | Move away from global `xhigh`; use high reasoning per task. | Complex firmware audits may genuinely need high reasoning. |
| Auto compact | Enable or create explicit compaction policy. | Bad compaction can discard needed audit context. |
| Away summary | Consider enabling if it reduces resume-cache churn. | Generated summaries may become stale or misleading. |

**Required gate:** Captain approval and rollback path before settings changes.

### F. Transcript Retention Policy

**Current proven policy:** keep last 60 days live; archive `>60 day` JSONL to `/Users/spectrasynq/00.Project.archieves`, verify archive, then prune only manifest-listed originals.

**Recommendation:** make this a monthly maintenance task:

1. Create manifest for `>60 day` transcript JSONL.
2. Archive to `00.Project.archieves`.
3. SHA256 checksum.
4. Verify tar listing equals manifest.
5. Prune only after verification.
6. Write post-prune count/size report.

Do not apply this policy to `claude-mem`.

---

## 4. Proposed Execution Order

1. **Instruction map first.** It is the largest direct surface and the highest risk if edited casually.
2. **MCP classification second.** It can produce large context savings but must preserve Lightwave routing.
3. **Hook classification third.** Measure and classify before disabling.
4. **Skill classification fourth.** Installed count is large, but actual loading is not yet quantified.
5. **Settings defaults fifth.** Change reasoning/compaction defaults only with rollback.
6. **Monthly retention task last.** Archive/prune is now proven and can be operationalised.

---

## 5. Decision Gates For Captain

| Gate | Decision Needed |
|---|---|
| Gate 1 | Approve a protected-instruction map pass. No edits yet. |
| Gate 2 | Approve a redacted MCP/hook inventory that lists names and roles only. |
| Gate 3 | Approve a remediation patch touching settings/plugins/MCP defaults, if the inventories justify it. |
| Gate 4 | Approve durable archive/prune script under `tools/` for future transcript maintenance. |

---

## 6. Recommended Next Step

Run Gate 1: produce a protected-instruction map for `CLAUDE.md`, `AGENTS.md`, `.claude/CLAUDE.md`, and `docs/WORKFLOW_ROUTING.md`.

Output path:

`docs/tooling/cc-audit-protected-instruction-map-2026-05-05.md`

No settings should be changed until that map and the MCP/hook inventory exist.
