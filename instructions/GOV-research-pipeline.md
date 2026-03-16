# GOV — Research Pipeline Governance Spec

**Version:** 0.1
**Date:** 2026-03-06
**Status:** Draft — pending Captain ratification
**Scope:** All automated research activity across the K1 Launch Planning, War Room, and sibling project domains.

---

## 1. Purpose

This document defines how the SpectraSynq research pipeline operates: its modes, capacity allocation, input/output protocols, quality gates, escalation triggers, and cross-domain routing. It is the operating constitution — not a process guide. Every research agent, scheduled task, and subagent must comply with these rules.

---

## 2. Pipeline Modes

The pipeline operates in one of four modes at any time. Mode is set by Captain or auto-escalated by trigger conditions.

### 2.1 IDLE (3-5% capacity)

**When:** No active research vectors. No pending Captain decisions. Pipeline is monitoring, not hunting.

**Behaviour:**
- Single scheduled cycle every 60 minutes
- Competitive landscape monitoring only (scan for new releases, announcements, pricing changes)
- No new document creation — append to existing monitors
- No subagent spawning

**Output:** Silent unless a trigger condition fires (see §5).

### 2.2 EXPLORATION (10-15% capacity)

**When:** Active research questions exist but no hard deadline. Captain is synthesising. Pipeline is building context density.

**Behaviour:**
- Scheduled cycle every 30 minutes
- Up to 3 parallel research vectors active simultaneously
- Each vector gets its own subagent with isolated scope
- Cross-domain correlation runs once per cycle (check if findings in Vector A affect Vector B)
- New document creation permitted for substantial findings
- War Room integration: surface relevant KNW notes for active vectors

**Output:** Cycle log updated every run. Captain brief compiled on request or every 6 hours (whichever is sooner).

### 2.3 HUNT (20-30% capacity)

**When:** Captain has defined specific research vectors with clear questions. Time-sensitive decisions pending. Pipeline is targeted.

**Behaviour:**
- Scheduled cycle every 15 minutes
- Up to 5 parallel research vectors active simultaneously
- Each vector uses dedicated subagent with sandbox isolation
- Aggressive web search — multiple search queries per vector per cycle
- Cross-domain correlation runs every cycle
- War Room deep-reads permitted (full KNW note analysis, not just title scans)
- Counter-research required: for every finding that supports a hypothesis, actively search for disconfirming evidence

**Output:** Cycle log updated every run. Findings surfaced to Captain immediately if escalation trigger fires (see §5). Consolidated brief every 3 hours.

### 2.4 VALIDATION (15-20% capacity)

**When:** Captain has made a decision. Pipeline is pressure-testing it before execution commits.

**Behaviour:**
- Scheduled cycle every 20 minutes
- Up to 4 parallel validation threads
- Each thread attacks the decision from a different angle:
  - Thread 1: Competitive counter-evidence (who does this differently and why?)
  - Thread 2: Consumer/user sentiment (what do real people say about this pattern?)
  - Thread 3: Historical precedent (who tried this before and what happened?)
  - Thread 4: Failure mode analysis (what kills this decision?)
- No confirmation bias permitted — validation mode explicitly searches for reasons the decision is WRONG
- If validation surfaces a material risk, escalate immediately (see §5)

**Output:** Validation report per decision. Pass/fail with evidence. Risks ranked by severity.

---

## 3. Research Vector Protocol

A research vector is a defined question with scope, sources, and success criteria.

### 3.1 Vector Definition (required fields)

```
VECTOR: [short name]
QUESTION: [the specific question to answer]
SCOPE: [what's in bounds / what's out of bounds]
SOURCES: [web search | war room | competitive intel | consumer data | all]
SUCCESS: [what does a complete answer look like?]
CONFIDENCE TARGET: [minimum confidence level: LOW / MEDIUM / HIGH]
PRIORITY: [P0 critical | P1 high | P2 normal | P3 background]
EXPIRES: [date after which this vector is stale]
```

### 3.2 Vector Lifecycle

```
DEFINED → ACTIVE → RESEARCHING → FINDINGS → SYNTHESISED → DELIVERED
                                     ↓
                                  BLOCKED (needs Captain input)
                                     ↓
                                  ESCALATED (trigger condition fired)
```

### 3.3 Vector Limits

- IDLE mode: 1 vector max (competitive monitoring)
- EXPLORATION mode: 3 vectors max
- HUNT mode: 5 vectors max
- VALIDATION mode: 4 threads per decision (not vectors — threads are scoped to one decision)

No single vector should consume more than 40% of available capacity. If a vector is that large, it must be decomposed into sub-vectors.

---

## 4. Input/Output Protocol

### 4.1 Inputs (what feeds the pipeline)

| Source | How It Enters | Processing |
|--------|--------------|------------|
| Captain decisions | Conversation → logged to decision register | Triggers VALIDATION mode for the decision |
| Captain questions | Conversation → converted to research vector | Enters at priority Captain assigns (default P1) |
| Captain gut instinct | Conversation → framed as hypothesis | Enters as EXPLORATION vector with [HYPOTHESIS] label |
| Scheduled monitoring | Timer trigger | Runs at current mode's cadence |
| Cross-domain trigger | Finding in Vector A affects Vector B | Auto-spawns correlation sub-vector |
| War Room KNW note | Referenced or discovered during research | Read, extract relevant data, cite in findings |
| External event | Competitor announcement, market shift | Auto-escalates to Captain if material (see §5) |

### 4.2 Outputs (what the pipeline produces)

| Output | Frequency | Destination |
|--------|-----------|-------------|
| Cycle log entry | Every cycle | `05-Brand-and-Marketing/research/research-cycle-log.md` |
| Research document | When findings are substantial | Domain-appropriate folder in Launch Planning |
| Captain brief | On request, or per mode schedule | Delivered in conversation or as visual brief |
| Escalation alert | Immediate when trigger fires | Agent Mail to Captain + conversation flag |
| Decision validation report | Per decision in VALIDATION mode | `00-Governance/validation/` |
| War Room contribution | When research yields KNW-grade knowledge | Obsidian vault via appropriate template |

### 4.3 Labelling (mandatory on all outputs)

Every claim in every output must be labelled:
- **[FACT]** — verified, sourced, citable
- **[INFERENCE]** — derived logically from facts
- **[HYPOTHESIS]** — speculation, unverified, requires validation

Unlabelled claims are a governance violation.

---

## 5. Escalation Triggers

These conditions cause immediate Captain notification regardless of current mode.

| Trigger | Condition | Action |
|---------|-----------|--------|
| **Competitor threat** | A competitor ships a product that overlaps with K1's positioning or technical moat | Immediate alert with threat assessment |
| **Price signal** | Evidence that K1's price point is materially wrong (too high OR too low) in either direction | Immediate alert with data |
| **Moat erosion** | Any product or open-source project ships PLL-equivalent beat tracking | Immediate alert — P0 |
| **Opportunity window** | Time-sensitive opportunity discovered (partnership, press, event, collaboration) | Immediate alert with action deadline |
| **Decision contradiction** | New evidence materially contradicts a Captain decision already in execution | Immediate alert with evidence and severity |
| **Cross-domain cascade** | A finding in one research vector invalidates or transforms findings in 2+ other vectors | Immediate alert with cascade map |
| **Validation failure** | A decision in VALIDATION mode fails on 2+ threads | Immediate alert — do not proceed with decision |

---

## 6. Quality Gates

### 6.1 Research Quality (per vector)

A vector's findings are not DELIVERED until:
- [ ] Every claim is labelled [FACT] / [INFERENCE] / [HYPOTHESIS]
- [ ] Sources are cited for all [FACT] claims
- [ ] Counter-evidence has been actively sought (not just confirming evidence)
- [ ] Findings answer the vector's QUESTION (not adjacent questions)
- [ ] Confidence level meets or exceeds the vector's CONFIDENCE TARGET
- [ ] Cross-domain implications are noted (even if "none identified")

### 6.2 Brief Quality (per Captain brief)

A brief is not delivered until:
- [ ] All active vectors are summarised with current status
- [ ] New findings since last brief are highlighted
- [ ] Pending decisions are listed with any new evidence affecting them
- [ ] Cross-domain leverage opportunities are identified
- [ ] Reading time estimate is provided
- [ ] Format matches Captain's current cognitive state (text brief vs visual brief — ask if unclear)

### 6.3 War Room Contribution Quality

Research findings promoted to War Room KNW notes must:
- [ ] Follow the War Room note template (frontmatter, summary, body, cross-references)
- [ ] Include replication/confidence status for empirical claims
- [ ] Link to source material
- [ ] Identify cross-domain connections

---

## 7. Cross-Domain Routing

The K1 launch spans multiple domains. Research in one domain frequently affects others. The pipeline must actively route findings across domains.

### 7.1 Domain Map

```
┌─────────────────────────────────────────────┐
│                 WAR ROOM                     │
│         (strategic intelligence)             │
│                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ GTM      │  │ Firmware │  │ Design   │  │
│  │ Research  │  │ Research │  │ Research │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  │
│       │              │              │        │
└───────┼──────────────┼──────────────┼────────┘
        │              │              │
        ▼              ▼              ▼
┌─────────────────────────────────────────────┐
│           LAUNCH PLANNING                    │
│        (execution artifacts)                 │
│                                              │
│  00-Product    02-Market    05-Brand         │
│  Definition    Intelligence  & Marketing     │
│                                              │
│  01-Hardware   03-Mfg       06-Community     │
│                                              │
│  04-Certs      07-Operations 08-Timeline     │
│                                              │
└─────────────────────────────────────────────┘
        │              │              │
        ▼              ▼              ▼
┌──────────┐  ┌──────────────┐  ┌──────────┐
│ Firmware │  │ Landing Page │  │ Blender  │
│ Codebase │  │   Codebase   │  │ Assets   │
└──────────┘  └──────────────┘  └──────────┘
```

### 7.2 Routing Rules

| Finding Domain | Routes To | Example |
|---------------|-----------|---------|
| Competitive intel (pricing) | Product Definition, Brand & Marketing | "TE prices EP-133 at $199" → affects K1 pricing decision |
| Competitive intel (features) | Firmware, Product Definition | "WLED adds beat detection" → moat erosion check |
| Brand research | Landing Page, Content, Community | "Edge Line as recognition device" → hero image angle, favicon |
| Consumer sentiment | Product Definition, Brand, Community | "WLED users want plug-and-play" → messaging, Discord strategy |
| Technical research | Firmware, Product Definition | "New I2S codec available" → BoM options |
| Content research | Brand, Community, Landing Page | "Study concept validated" → production priority |

### 7.3 Cascade Protocol

When a finding affects 2+ domains:
1. Log the finding with all affected domains tagged
2. Create a cross-domain note in the cycle log
3. If 3+ domains affected, escalate to Captain (see §5, cross-domain cascade)
4. Each affected domain's next research cycle must acknowledge the finding

---

## 8. Capacity Allocation on Claude Code Max x20

### 8.1 Available Resources

[FACT] Claude Code Max x20 provides substantial parallel processing capacity. The current single-task-every-30-minutes configuration uses approximately 3% of available throughput.

### 8.2 Allocation by Mode

| Mode | Parallel Agents | Cycle Frequency | Web Searches/Cycle | Estimated Capacity |
|------|----------------|-----------------|--------------------|--------------------|
| IDLE | 1 | 60 min | 2-3 | ~3-5% |
| EXPLORATION | 3 | 30 min | 5-8 per vector | ~10-15% |
| HUNT | 5 | 15 min | 8-12 per vector | ~20-30% |
| VALIDATION | 4 threads | 20 min | 5-8 per thread | ~15-20% |

### 8.3 Burst Capacity

On Captain request, any mode can temporarily burst to 40% for a single cycle. This is for time-critical research (e.g., competitor announcement just dropped, need immediate analysis). Burst returns to normal allocation after one cycle.

### 8.4 Guardrails

- No mode exceeds 30% sustained capacity (reserve for Captain's own sessions)
- No single research vector exceeds 40% of its mode's allocation
- If pipeline detects it's impacting Captain's interactive session responsiveness, auto-throttle to IDLE
- All agents must sandbox their file operations (no cross-contamination between parallel vectors)

---

## 9. Decision Register

All Captain decisions are logged and tracked. Research vectors auto-generate from pending decisions.

### 9.1 Register Format

```
DECISION: [short name]
STATUS: PENDING | MADE | VALIDATING | EXECUTED | REVISED
DATE: [when made or when pending since]
OPTIONS: [what was considered]
CHOICE: [what was chosen, or "pending"]
EVIDENCE: [what informed the decision]
VALIDATION: [PASS | FAIL | IN PROGRESS | NOT YET VALIDATED]
AFFECTED DOMAINS: [which domains this decision touches]
DOWNSTREAM: [what this decision unblocks or changes]
```

### 9.2 Current Register

| # | Decision | Status | Choice | Validation |
|---|----------|--------|--------|------------|
| D1 | Colour palette | PENDING | — | Not yet |
| D2 | Recognition device | PENDING | Rec: Edge Line + Centre-Out | Not yet |
| D3 | Manifesto enemy | PENDING | — | Not yet |
| D4 | Manifesto audience | **MADE** | Dual-purpose (Nike model) | Not yet |
| D5 | Pricing | PENDING | Exploring $449-549 range | Not yet |
| D6 | First-run choreography | PENDING | Concept defined, arch decisions needed | Not yet |

---

## 10. Pipeline Evolution Protocol

This governance spec is a living document. It evolves through use.

### 10.1 Amendment Process

- Captain can amend any section at any time
- Research executor can PROPOSE amendments with rationale — Captain must ratify
- After every 10 research cycles, the executor must review this spec and propose refinements based on observed pipeline behaviour
- Amendments are versioned and logged

### 10.2 Metrics to Track (pipeline health)

| Metric | What It Measures | Target |
|--------|-----------------|--------|
| Cycle completion rate | % of scheduled cycles that run and produce output | >90% |
| Escalation accuracy | % of escalations that Captain agrees were warranted | >80% |
| Decision velocity | Average time from PENDING to MADE | Decreasing trend |
| Cross-domain hit rate | % of findings that route to 2+ domains | >30% |
| Validation rigour | % of decisions that survive all 4 validation threads | Track only (no target) |
| Captain override rate | % of recommendations Captain rejects | Track only (learn from overrides) |

### 10.3 Anti-Patterns (pipeline must avoid)

- **Research for research's sake** — every vector must have a clear QUESTION and SUCCESS criteria
- **Confirmation bias** — counter-evidence search is mandatory, not optional
- **Context flooding** — Captain briefs must be digestible, not exhaustive
- **Stale vectors** — vectors past their EXPIRES date are auto-closed
- **Domain silos** — cross-domain routing is mandatory, not optional
- **Capacity waste** — if a mode's allocation isn't being used, it doesn't justify upgrading the mode

---

## 11. File Locations

| Artifact | Path |
|----------|------|
| This spec | `00-Governance/GOV-research-pipeline.md` |
| Cycle log | `05-Brand-and-Marketing/research/research-cycle-log.md` |
| Decision register | `00-Governance/decision-register.md` |
| Validation reports | `00-Governance/validation/` |
| Vector definitions | `00-Governance/vectors/` |
| Pipeline health metrics | `00-Governance/metrics/` |

---

*This document governs all automated research activity for the K1 launch. Compliance is mandatory. Amendments require Captain ratification.*
