# GOV Amendment 001 — Dual Pipeline Separation

**Amends:** GOV-research-pipeline.md v0.1
**Date:** 2026-03-06
**Status:** Draft — pending Captain ratification
**Reason:** The original GOV spec assumed a single pipeline. Two independent pipelines exist with fundamentally different purposes. Applying unified governance modes to both introduces silent errors.

---

## The Problem

Two scheduled tasks currently run at 30-minute intervals:

| Task | Purpose | Nature | Agent Mail Route |
|------|---------|--------|-----------------|
| `warroom-pipeline-executor` | Harvest sources, extract claims, curate Obsidian vault | **Ingestion** — finds and processes knowledge | MaroonOx → QuietForge |
| `k1-launch-research` | Answer specific research questions, inform Captain decisions | **Directed research** — hunts for answers | MaroonOx → GraySparrow |

These are different operations. Ingestion is constant and rhythmic. Directed research is modal and responsive. Governing both with a single mode system breaks the ingestion rhythm without improving research quality.

---

## Amendment: Split Governance into Two Layers

### Layer 1: War Room Ingestion Pipeline (unchanged rhythm)

**What stays the same:**
- 30-minute fixed cadence — does not change with GOV modes
- 6-stage pipeline (INPUT → FILTER → PROCESS → VALIDATE → INTEGRATE → REPORT)
- MaroonOx → QuietForge reporting
- Obsidian vault naming conventions and quality rules
- Captain approval queue for foundational claims

**What changes:**
- **NEW — K1 decision awareness:** After Stage 2 (FILTER), if a promoted source is relevant to any active decision in the decision register, tag it with `k1_decision: D1/D2/D3/etc` in frontmatter
- **NEW — Cross-domain flag:** After Stage 5 (INTEGRATE), if integrated knowledge affects K1 positioning, pricing, or moat, append a one-line flag to `00-Governance/cross-domain-flags.md`:
  ```
  {date} | {KNW note title} | Affects: {D1/D5/etc} | {one-line summary}
  ```
- **NEW — Escalation hook:** If a source scores >0.8 relevance AND matches an escalation trigger from GOV spec §5 (moat erosion, competitor threat, price signal), send urgent Agent Mail immediately — don't wait for cycle completion

**What does NOT change:**
- Cadence (stays 30 min)
- Stage logic (stays 6-stage)
- Vault write rules (stays the same)
- The War Room pipeline does NOT read GOV modes. It runs regardless.

### Layer 2: K1 Research Pipeline (mode-governed)

This is where GOV spec modes (IDLE/EXPLORATION/HUNT/VALIDATION) apply. Only this pipeline changes behaviour based on mode.

**What changes from current:**
- **Replace flat task list with vector-driven execution.** Tasks 1-4 are dead. The pipeline reads active vectors from `00-Governance/vectors/` each cycle.
- **Mode-aware cadence:**
  - IDLE: 60 min — competitive monitoring only (equivalent to old Task 4)
  - EXPLORATION: 30 min — up to 3 vectors
  - HUNT: 15 min — up to 5 vectors
  - VALIDATION: 20 min — 4 threads per decision under test
- **Cross-domain read:** Each cycle, check `00-Governance/cross-domain-flags.md` for War Room flags relevant to active vectors. If found, incorporate into current research.
- **War Room write-back:** If research produces a finding that meets KNW quality (GOV spec §6.3), write it to the War Room inbox (`01-Inbox/`) for the ingestion pipeline to process. Don't write directly to `03-Knowledge/` — let the War Room pipeline validate it.
- **Escalation logic:** If research hits a GOV spec §5 trigger, send urgent Agent Mail AND write to `00-Governance/escalations.md`
- **Fix stale product facts:** Price is $449-549 (exploring), not $249

### Layer 3: Shared State (new)

A lightweight coordination file both pipelines can reference:

**File:** `00-Governance/pipeline-state.md`

```markdown
# Pipeline State

## Current Mode
MODE: EXPLORATION
SET_BY: Captain
SET_DATE: 2026-03-06
REASON: Active research questions, no hard deadline

## Active Vectors
See: vectors/ directory

## Escalation Flags
See: escalations.md

## Cross-Domain Flags
See: cross-domain-flags.md
```

**Write rules:**
- Captain can set mode at any time (in conversation, applied next cycle)
- K1 research pipeline reads mode each cycle to determine cadence and capacity
- War Room pipeline reads cross-domain flags but does NOT read or change mode
- Auto-escalation from EXPLORATION → HUNT only if Captain approves

---

## Capacity Model (corrected)

The original GOV spec percentages assumed a single pipeline. Corrected model:

| Component | Capacity | Cadence | Notes |
|-----------|----------|---------|-------|
| War Room ingestion | ~3% (constant) | 30 min fixed | Always running, does not scale with mode |
| K1 research — IDLE | ~2% | 60 min | Monitoring only |
| K1 research — EXPLORATION | ~8-12% | 30 min | 3 vectors |
| K1 research — HUNT | ~17-27% | 15 min | 5 vectors |
| K1 research — VALIDATION | ~12-17% | 20 min | 4 threads |
| **Total at IDLE** | **~5%** | | War Room + IDLE research |
| **Total at EXPLORATION** | **~11-15%** | | War Room + EXPLORATION |
| **Total at HUNT** | **~20-30%** | | War Room + HUNT |
| **Total at VALIDATION** | **~15-20%** | | War Room + VALIDATION |

These totals now match the original GOV spec targets but correctly account for the War Room baseline.

---

## Agent Mail Routing (corrected)

| Pipeline | Sender | Normal Report To | Escalation To |
|----------|--------|-----------------|---------------|
| War Room ingestion | MaroonOx | QuietForge | QuietForge (urgent) |
| K1 research | MaroonOx | GraySparrow | GraySparrow (urgent) |
| Cross-domain flag | Either | Both QuietForge + GraySparrow | Both (urgent) |

**Open question:** There is no "Captain" agent in Agent Mail. Escalations go to QuietForge/GraySparrow but there's no guarantee Captain sees them in real-time. Options:
1. Register a Captain agent that Captain can check manually
2. Use importance: "urgent" and rely on Captain checking inbox
3. Accept that escalation = next conversation, not real-time

**Recommendation:** Option 2 for now. Revisit if escalation latency becomes a problem.

---

## What This Amendment Does NOT Change

These sections of the original GOV spec remain exactly as written:
- §3: Research Vector Protocol (definition, lifecycle, limits)
- §5: Escalation Triggers (conditions and actions)
- §6: Quality Gates (all three tiers)
- §9: Decision Register (format and current register)
- §10: Pipeline Evolution Protocol (amendment process, metrics, anti-patterns)

---

## Implementation Checklist

If Captain ratifies this amendment:

- [ ] Create `00-Governance/pipeline-state.md` with initial mode = EXPLORATION
- [ ] Create `00-Governance/cross-domain-flags.md` (empty, header only)
- [ ] Create `00-Governance/escalations.md` (empty, header only)
- [ ] Update `warroom-pipeline-executor` SKILL.md — add K1 decision awareness, cross-domain flag, and escalation hook to existing stages
- [ ] Rewrite `k1-launch-research` SKILL.md — replace flat task list with vector-driven execution, add mode awareness, add War Room write-back
- [ ] Fix price from $249 to "$449-549 (exploring)" in K1 SKILL.md
- [ ] Update GOV spec §2 to reference "K1 Research Pipeline" specifically, not "the pipeline"
- [ ] Update GOV spec §8 capacity table with corrected dual-pipeline numbers
- [ ] Update GOV spec §11 file locations with new shared state files

**Estimated implementation time:** ~30 minutes of task file editing. No code changes. No architectural risk — both tasks keep running during the transition.

---

*This amendment preserves the working War Room ingestion pipeline while enabling mode-governed K1 research. The key principle: ingestion is constant, research is modal. Don't conflate them.*
