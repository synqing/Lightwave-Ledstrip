---
abstract: "Complete enforcement architecture for making AI agents follow the 18 design decision principles. Three enforcement tiers (Readback, Gate Skill, Hooks), distilled 10-commandments prompt insert, placement strategy, and integration plan. Implementable with existing Claude Code infrastructure."
---

# Mandate Enforcement Proposal: Making Decision Principles Stick

## The Problem

We have 18 decision-making principles (12 original + 6 meta-cognitive) distilled from real design exercises. They are documented across 5 files totalling 2,500+ lines. The documentation is correct. The enforcement is zero. An agent that reads "audit for duplicates before placing elements" and then immediately places elements by category is not malfunctioning -- it is following its default optimisation: minimise steps to output. Mandates that do not create structural barriers are suggestions that get optimised away.

---

## Analysis: What Makes Mandates Stick

### Evidence from Existing Enforcement in This Codebase

**1. Agent Readback Protocol (CLAUDE.md) -- WORKS**

The Readback Protocol succeeds because it has three properties:
- **Structural position:** It fires before the first tool call -- the earliest possible intervention point.
- **Observable output:** The readback block is visible to the orchestrator and user. Absence is detectable.
- **Specificity:** It demands named fields (Confidence, Task scope, Subsystem, Hard constraints) -- not "think about it" but "write down the answer to these specific questions."

Where it weakens: the readback can become boilerplate. An agent that copies the template and fills in plausible-sounding answers without genuine engagement passes the gate. The readback checks for *form*, not *substance*.

**2. Pre-Commit Confidence Gate (CLAUDE.md) -- PARTIALLY WORKS**

The Confidence Gate fires at the right moment (before git add) and asks a genuine question ("Am I confident this is correct, or am I hoping?"). But it is self-assessed -- no external verification. An overconfident agent satisfies it trivially. It works as a speed bump, not a wall.

**3. GSD Plan-Phase Gate -- WORKS WELL**

GSD enforces workflow by producing *artefacts*. The plan-phase commits a PLAN.md file. The execute-phase checks that PLAN.md exists before proceeding. The artefact is the gate. This is structural enforcement -- the downstream step physically cannot run without the upstream artefact. It is the strongest pattern in the current system.

**4. Pre-Commit Hooks (Git) -- STRONGEST**

Pre-commit hooks are the gold standard of enforcement. They intercept the action (commit) and block it if criteria are not met. The agent cannot proceed without satisfying the hook. The enforcement is external to the agent's reasoning -- it does not depend on self-assessment.

### Why Mandates Fail (Ranked by Frequency)

| Failure Mode | Mechanism | Frequency |
|---|---|---|
| **Optimisation pressure** | Agent minimises steps to reach output. Reading and applying a 12-step process is 12 more steps than going directly to the answer. | Most common |
| **Context decay** | In long conversations, earlier instructions decay as attention shifts to recent context. A mandate read at line 50 of CLAUDE.md is forgotten by turn 20. | Very common |
| **Boilerplate compliance** | Agent outputs the required format (readback block, checklist) without genuine engagement. Form satisfied, substance absent. | Common |
| **Scope mismatch** | Agent judges the mandate does not apply to this specific task ("this is just a small change, I don't need the full checklist"). | Common |
| **Ambiguity escape** | Mandate uses "consider" or "think about" rather than "output X" or "produce artefact Y." Agent "considers" instantly and proceeds. | Occasional |

### The Key Insight

The only mandates that reliably stick are those where:
1. The enforcement is **external** to the agent (not self-assessed).
2. The required output is **specific and verifiable** (not "think about X" but "produce document Y with fields A, B, C").
3. The **downstream work physically depends** on the upstream artefact.

This means the enforcement architecture must combine *structural gates* (artefacts that downstream steps check for) with *specific output formats* (fields that can be verified for completeness) and *external checks* (orchestrator or hook verification).

---

## The Distilled Principles: Design Decision Commandments

The 18 principles compressed to the minimum enforceable text. Each principle: one sentence, stated as DO or DO NOT, verifiable.

### BEFORE (Pre-Work -- before generating any design, layout, or architecture proposal)

1. **CLASSIFY the problem as visual, structural, or functional.** State the classification and your evidence. DO NOT propose solutions before classifying.
2. **COLLECT all hard constraints** (physical, interaction, technical, brand) into a numbered list. DO NOT generate options before listing constraints.
3. **BUILD a complete parameter/element inventory** listing every item and its current location. DO NOT place any element without checking the inventory for duplicates.
4. **AUDIT the current design** with explicit KEEP / EVALUATE / FIX labels. DO NOT discard KEEP elements in any proposal.
5. **RESEARCH the diagnosed problem domain**, not the surface complaint. If the problem is structural, DO NOT research visual treatments.

### DURING (While Working -- during option generation and evaluation)

6. **GENERATE minimum 3 options** for structural decisions, **minimum 5** for visual/aesthetic decisions. DO NOT present a single option and ask for approval.
7. **ELIMINATE options that violate any collected constraint** before presenting them. DO NOT present options you know violate constraints.
8. **DEFINE 3+ distinct visual hierarchy levels** (frame / content / active) with different border, surface, and type treatments. DO NOT use the same treatment across levels.
9. **ASSIGN semantic meaning to every colour.** State what each colour communicates. DO NOT use colour for decoration.
10. **APPLY theoretical frameworks exhaustively** when they exist (colour wheel harmonies, typographic scales, grid systems). DO NOT guess when a framework covers the decision.
11. **EXPLORE sub-decisions** within each placement choice. DO NOT stop at the top-level placement without testing at least 2 sub-variants.
12. **MATCH control type to useful resolution.** Count the parameter's meaningful distinct values. Tap-to-cycle for <8, encoder for 50+. DO NOT assign encoders to low-resolution parameters.

### AFTER (Validation -- before finalising any design)

13. **VERIFY zero duplicates.** Sort the parameter inventory alphabetically and confirm each name appears exactly once. DO NOT finalise with any parameter appearing in two locations.
14. **TRACE second-order effects** one level downstream for each decision. State what the decision changes in adjacent components.
15. **CLASSIFY each decision as reversible or irreversible.** Flag irreversible decisions for extra scrutiny.
16. **DOCUMENT the decision, alternatives considered, and reason each alternative was eliminated.**

### META (Process Quality)

17. **GENERATE at least two alternative problem framings** before committing to one diagnosis.
18. **ASK whether this decision closes doors** that should remain open for the next 6 months.

---

## Enforcement Architecture: Three Tiers

### Tier 1: Decision Readback (Light -- ~50 tokens overhead)

**What it is:** A structured output block, identical in function to the existing Agent Readback Protocol, but scoped to design decisions. Required before any UI/architecture/product design work.

**Where it lives:** In CLAUDE.md, directly after the existing Agent Readback Protocol section.

**The prompt insert:**

```
### Design Decision Readback (MANDATORY for design/UI/architecture work)

Before generating any mockup, layout, parameter allocation, navigation design,
visual treatment, colour selection, or architecture proposal, output a
DESIGN READBACK block. No design work without a readback. If missing, the
orchestrator or user will halt the session.

DESIGN READBACK:
- Problem classification: [visual | structural | functional] + evidence
- Constraints collected: [count] — list them
- Parameter inventory built: [yes/no] — if yes, count and location of each
- Current design audit: [KEEP: count | EVALUATE: count | FIX: count]
- Research domain: [what domain, not "visual design" unless classification is visual]
- Duplicate check: [done/not done] — duplicates found: [count]
- Options to generate: [count] — structural (3+) or visual (5+)?
- Irreversible decisions in scope: [list]
- Theoretical frameworks applicable: [list]

If any field is blank or "N/A", you have not completed pre-work. STOP and
fill the gap before proceeding.
```

**Enforcement mechanism:** Social pressure + orchestrator detection. The orchestrator (or user) scans for the block. Absence = halt. This is identical to how the existing Readback Protocol works.

**Strength:** Low overhead. Does not slow down trivial tasks. Catches the "jumped straight to generating mockups" failure mode.

**Weakness:** Self-assessed. An agent that fills in plausible answers without doing the work passes the gate. Mitigated by Tier 2.

---

### Tier 2: Decision Gate Skill (Medium -- ~200-500 tokens overhead)

**What it is:** A `/decision-gate` skill that walks the agent through the 18 principles as a structured interview, producing a committed artefact. The artefact is a `.planning/decisions/DECISION-{topic}.md` file that downstream agents can verify exists.

**Where it lives:** `.claude/skills/decision-gate/SKILL.md`

**The skill definition:**

```yaml
---
name: decision-gate
description: MANDATORY before any design, UI, layout, architecture, or product decision. Walks through the 18 decision principles and produces a committed decision artefact. Invoke with /decision-gate before design work begins.
---
```

**The skill workflow:**

1. **Trigger:** Agent invokes `/decision-gate` (or orchestrator includes it in the delegation prompt). The skill can also be auto-triggered by including this instruction in CLAUDE.md: "If your task involves UI, layout, parameter allocation, navigation design, visual treatment, or architecture proposal, invoke `/decision-gate` before starting design work."

2. **Phase 1: Problem Framing** (Principles 1, 2, 17)
   - Classify the problem (visual / structural / functional).
   - Generate at least 2 alternative framings.
   - Identify the correct research domain.
   - Output: problem statement with classification and alternative framings.

3. **Phase 2: Constraint Collection** (Principles 2, 11)
   - Fill the Constraint Collection Template from `AGENT_DESIGN_INSTRUCTIONS.md`.
   - For each constraint, state what class of solutions it eliminates.
   - Output: numbered constraint list with elimination statements.

4. **Phase 3: Inventory and Audit** (Principles 3, 4, 10, 13)
   - Build the complete parameter/element inventory.
   - Perform the KEEP / EVALUATE / FIX audit.
   - Run the Duplicate Audit Protocol.
   - Output: inventory table, audit table, duplicate report (zero duplicates required).

5. **Phase 4: Decision Record** (Principles 15, 16, 18)
   - For each decision in scope: classify reversibility, list alternatives, state elimination rationale.
   - Flag any doors being closed.
   - Output: decision record table.

6. **Phase 5: Commit Artefact**
   - Write all outputs to `.planning/decisions/DECISION-{topic}.md`.
   - The file includes a machine-parseable header:

   ```
   ---
   topic: [short description]
   classification: [visual | structural | functional]
   constraints: [count]
   inventory_items: [count]
   duplicates_found: 0
   options_minimum: [3 or 5]
   irreversible_decisions: [count]
   status: approved
   date: YYYY-MM-DD
   ---
   ```

   - The artefact is NOT committed to git (it lives in `.planning/` which is gitignored). This avoids polluting the repository with process artefacts. But it persists on disk for downstream verification.

**Enforcement mechanism:** Artefact existence check. Downstream design steps (mockup generation, code writing) can be gated on: "Does `.planning/decisions/DECISION-{topic}.md` exist with `status: approved` and `duplicates_found: 0`?" This is the GSD pattern -- the artefact IS the gate.

**Strength:** Structural enforcement. The downstream step cannot proceed without the artefact. The artefact contains verifiable fields (duplicate count, constraint count, option minimum). An orchestrator or hook can verify these fields programmatically.

**Weakness:** Overhead. For a small design tweak (change a border colour), the full 5-phase gate is overkill. This is addressed by the scope filter (see "Scope Discrimination" below).

---

### Tier 3: Principle Enforcement Hooks (Heavy -- near-zero runtime overhead, high setup cost)

**What it is:** Claude Code hooks that intercept specific actions and verify that the Decision Gate artefact exists before allowing the action to proceed. This is the pre-commit hook pattern applied to design work.

**Where it lives:** `.claude/settings.json` (hooks configuration) + a verification script.

**Implementation:**

Claude Code supports hooks via the settings configuration. The hook intercepts Write/Edit tool calls targeting specific paths and checks for the decision artefact.

**Hook logic (conceptual -- implementation depends on Claude Code hook API):**

```
ON Write/Edit to paths matching:
  - tab5-encoder/src/ui/**
  - tab5-encoder/src/parameters/**
  - firmware-v3/src/effects/**
  - lightwave-ios-v2/*/Views/**
  - *.html (mockup files)

CHECK:
  1. Does .planning/decisions/DECISION-*.md exist with a modified date within the last 24 hours?
  2. Does the artefact contain duplicates_found: 0?
  3. Does the artefact contain constraints: [>= 3]?

IF ANY CHECK FAILS:
  Block the write with:
  "[DESIGN GATE] No approved decision artefact found. Run /decision-gate before modifying UI/design files.
   Missing: [list which checks failed]"
```

**Practical implementation path:**

Since Claude Code hooks are currently limited in programmability, the most practical implementation is a pre-commit hook script that checks for the artefact:

```bash
#!/bin/bash
# .git/hooks/pre-commit (or .pre-commit-config.yaml)

# Check if any staged files are in design-sensitive paths
DESIGN_PATHS="tab5-encoder/src/ui/ tab5-encoder/src/parameters/ firmware-v3/src/effects/"
DESIGN_FILES_STAGED=false

for path in $DESIGN_PATHS; do
  if git diff --cached --name-only | grep -q "^$path"; then
    DESIGN_FILES_STAGED=true
    break
  fi
done

if [ "$DESIGN_FILES_STAGED" = true ]; then
  # Check for recent decision artefact
  RECENT_DECISION=$(find .planning/decisions/ -name "DECISION-*.md" -mmin -1440 2>/dev/null | head -1)

  if [ -z "$RECENT_DECISION" ]; then
    echo "[DESIGN GATE] Staged files modify UI/design paths but no decision artefact found."
    echo "Run /decision-gate before committing design changes."
    echo "Artefact expected at: .planning/decisions/DECISION-{topic}.md"
    exit 1
  fi

  # Check for zero duplicates
  if ! grep -q "duplicates_found: 0" "$RECENT_DECISION"; then
    echo "[DESIGN GATE] Decision artefact has unresolved duplicates."
    echo "Fix: $RECENT_DECISION"
    exit 1
  fi
fi
```

**Strength:** External enforcement. The agent cannot bypass it. The check is mechanical, not self-assessed. It catches violations at the last possible moment before they enter the codebase.

**Weakness:** Late enforcement. The agent has already done the design work before the commit is blocked. This wastes the agent's tokens on work that will be rejected. Ideally, Tier 1 or Tier 2 catches the violation early; Tier 3 is the safety net.

**Weakness 2:** False positives. A one-line CSS fix to a border colour does not warrant a full Decision Gate. The path-based trigger will fire on any change to UI files. This is addressed by the scope filter.

---

## Scope Discrimination: When Each Tier Fires

Not every design change needs the full 18-principle treatment. The enforcement must scale with decision weight.

| Change Type | Example | Tier Required |
|---|---|---|
| **Cosmetic fix** (< 5 LOC, single property change) | Change border colour from #333 to #444 | None. No gate. |
| **Minor design adjustment** (5-20 LOC, single component) | Adjust spacing in param card, change font size | Tier 1 only (Decision Readback). 3-field abbreviated version. |
| **Component redesign** (20-100 LOC, affects visual hierarchy or layout) | Redesign sidebar tab selector, add a new param card layout | Tier 1 + Tier 2 (Decision Readback + Decision Gate Skill). |
| **System-level design** (100+ LOC, affects navigation, parameter allocation, or information architecture) | Redesign entire tab system, reallocate parameters across screens | Tier 1 + Tier 2 + Tier 3 (Full gate + pre-commit hook). |

**Scope determination rule (for CLAUDE.md):**

```
Before starting design work, classify the scope:

- COSMETIC (< 5 LOC, single visual property): proceed directly, no gate.
- MINOR (5-20 LOC, single component): output abbreviated Design Readback (3 fields:
  classification, constraints, duplicate check).
- COMPONENT (20-100 LOC, affects hierarchy/layout): full Design Readback + /decision-gate.
- SYSTEM (100+ LOC, affects navigation/param allocation/IA): full Design Readback +
  /decision-gate + decision artefact required before commit.

If unsure, classify one level higher. Under-classifying is the common error.
```

---

## Where Each Piece Lives

| Artefact | Location | Loaded When |
|---|---|---|
| **Design Decision Commandments (16 lines)** | CLAUDE.md, under Hard Constraints section | Every conversation (auto-loaded) |
| **Design Readback block** | CLAUDE.md, after Agent Readback Protocol | Every conversation (auto-loaded) |
| **Scope classification rule** | CLAUDE.md, before Design Readback | Every conversation (auto-loaded) |
| **`/decision-gate` skill** | `.claude/skills/decision-gate/SKILL.md` | On demand (when invoked or triggered) |
| **Full principle docs** (existing 5 files) | `tab5-encoder/docs/` | Referenced by `/decision-gate` skill, NOT loaded into every conversation |
| **Pre-commit hook** | `.pre-commit-config.yaml` or `.git/hooks/pre-commit` | On every commit (automatic) |
| **Decision artefacts** | `.planning/decisions/DECISION-{topic}.md` | Created by skill, checked by hook |
| **Subagent delegation insert** | Included in orchestrator's subagent prompt template | When delegating design work to subagents |

### Why This Placement

**CLAUDE.md gets the minimum effective dose.** The 16-line commandments, the readback block, and the scope rule total approximately 60 lines. This is the same order of magnitude as the existing Hard Constraints section (~10 lines) and the Agent Readback Protocol (~15 lines). It does not significantly increase CLAUDE.md's size. It occupies the same cognitive tier as "K1 is AP-ONLY" and "No heap alloc in render()" -- constraints that agents actually follow because they are short, specific, and always present.

**The skill gets the detailed workflow.** The 5-phase Decision Gate process is too long for CLAUDE.md (it would be ~100 lines). But as a skill, it loads on demand -- only when invoked. The skill references the full principle docs in `tab5-encoder/docs/` by path, so those docs are read only when the skill is active, not in every conversation.

**The pre-commit hook is the safety net.** It does not add any tokens to the agent's context. It fires silently and only blocks bad commits. It catches violations that slipped past Tiers 1 and 2.

**Subagent delegation inserts live in the orchestrator's vocabulary.** When the orchestrator delegates design work, it includes the commandments and the readback requirement in the subagent prompt. This solves the "subagents inherit zero context" problem specifically for design decisions.

---

## The Prompt Insert for CLAUDE.md

This is the complete text to add to CLAUDE.md. It sits after the Hard Constraints section and before the Workspace Rules section.

```markdown
## Design Decision Enforcement

### Scope Classification (MANDATORY before any design work)

Before starting any UI, layout, parameter allocation, visual treatment, colour
selection, navigation design, or architecture proposal work, classify the scope:

- **COSMETIC** (< 5 LOC, single visual property): proceed directly. No gate.
- **MINOR** (5-20 LOC, single component): output abbreviated Design Readback
  (classification + constraints + duplicate check only).
- **COMPONENT** (20-100 LOC, affects hierarchy or layout): full Design Readback
  + invoke `/decision-gate`.
- **SYSTEM** (100+ LOC, affects navigation, parameter allocation, or information
  architecture): full Design Readback + `/decision-gate` + decision artefact
  required before commit.

If unsure, classify one level higher.

### Design Decision Commandments

These apply to all MINOR/COMPONENT/SYSTEM scope design work:

**BEFORE:**
1. CLASSIFY the problem as visual, structural, or functional. State evidence.
2. COLLECT all hard constraints into a numbered list before generating options.
3. BUILD a complete parameter inventory with current locations. Check for duplicates.
4. AUDIT the current design: KEEP / EVALUATE / FIX. Preserve KEEP items.
5. RESEARCH the diagnosed domain, not the surface complaint.

**DURING:**
6. GENERATE 3+ options (structural) or 5+ options (visual). Never present one.
7. ELIMINATE constraint-violating options before presenting.
8. DEFINE 3+ distinct visual hierarchy levels. Never uniform treatment.
9. ASSIGN semantic meaning to every colour. No decorative colour.
10. APPLY theoretical frameworks exhaustively when they exist.
11. EXPLORE sub-decisions within each placement choice.
12. MATCH control type to useful resolution (tap for <8, encoder for 50+).

**AFTER:**
13. VERIFY zero duplicates in the final parameter inventory.
14. TRACE second-order effects one level downstream.
15. CLASSIFY each decision as reversible or irreversible.
16. DOCUMENT the decision, alternatives, and elimination rationale.

### Design Readback (MANDATORY for MINOR+ scope)

Before generating any design output, output a DESIGN READBACK block:

    DESIGN READBACK:
    - Scope: [COSMETIC | MINOR | COMPONENT | SYSTEM]
    - Problem classification: [visual | structural | functional] + evidence
    - Constraints: [count] — list them
    - Inventory: [built: yes/no, items: count, duplicates: count]
    - Current design audit: [KEEP: count | EVALUATE: count | FIX: count]
    - Options planned: [count]
    - Irreversible decisions: [list or none]

No design work without a readback. Missing = session halt.

For COMPONENT+ scope, invoke `/decision-gate` after the readback.
```

Total: ~50 lines. Fits within the existing CLAUDE.md structure without expanding it beyond its current density.

---

## The Skill File

```markdown
# /decision-gate Skill

## `.claude/skills/decision-gate/SKILL.md`

---
name: decision-gate
description: MANDATORY before COMPONENT or SYSTEM scope design work. Walks through
  the 18 decision principles and produces a decision artefact that gates downstream
  work. Invoke with /decision-gate.
---

## Trigger

Invoked explicitly via `/decision-gate` or auto-triggered when CLAUDE.md's Design
Decision Enforcement section identifies COMPONENT or SYSTEM scope.

## Prerequisites

Read these files before proceeding (total ~400 lines):
- `tab5-encoder/docs/AGENT_DESIGN_INSTRUCTIONS.md` — the 12 failure modes + templates
- `tab5-encoder/docs/DESIGN_DECISION_PROCESS.md` — the 12 original principles

## Phase 1: Problem Framing

1. Classify the problem: visual, structural, or functional.
2. Generate 2+ alternative framings. State each as a one-sentence hypothesis.
3. Identify the research domain that matches the chosen classification.
4. Output the classification with evidence.

## Phase 2: Constraint Collection

1. Fill the Constraint Collection Template from AGENT_DESIGN_INSTRUCTIONS.md:
   - Physical constraints (hardware, screen, inputs)
   - Interaction constraints (no hidden modes, max depth, spatial mapping)
   - Technical constraints (render budget, protocol limits)
   - Visual/brand constraints (anchor colour, existing KEEP elements)
2. For each constraint, state what it eliminates.
3. Minimum 3 constraints required to proceed.

## Phase 3: Inventory and Audit

1. Build the complete parameter/element inventory:
   - Every controllable parameter
   - Current location of each (which screen, tab, control)
   - Control type (encoder, tap, slider, display-only)
2. Perform the Duplicate Audit Protocol:
   - Sort alphabetically
   - Search all screens/tabs/modes for each parameter
   - Resolve any duplicates (one home per parameter)
3. Perform KEEP / EVALUATE / FIX audit of current design:
   - Minimum 3 KEEP items required (if nothing is worth keeping, you have not
     audited thoroughly enough)

## Phase 4: Option Generation

1. Generate options within constraint boundaries:
   - Structural decisions: minimum 3 options
   - Visual/aesthetic decisions: minimum 5 options
2. For each option, state which constraints it satisfies.
3. Eliminate any option that violates a constraint. State the violation.
4. Present surviving options with one-line differentiators.

## Phase 5: Decision Record and Artefact

1. For each decision made:
   - State the decision
   - List alternatives considered
   - For each eliminated alternative, state why
   - Classify as reversible or irreversible
   - Trace one level of second-order effects
2. Write the artefact to `.planning/decisions/DECISION-{topic}.md`

The artefact MUST include this machine-parseable frontmatter:

    ---
    topic: [short description]
    classification: [visual | structural | functional]
    constraints: [count]
    inventory_items: [count]
    duplicates_found: 0
    options_generated: [count]
    options_surviving: [count]
    irreversible_decisions: [count]
    status: approved
    date: YYYY-MM-DD
    ---

3. Verify: `duplicates_found` MUST be 0. If not, return to Phase 3.
4. Verify: `options_surviving` MUST be >= 2 (if only 1 survives, either a
   constraint is too aggressive or options were not generated broadly enough).

## After the Skill

Return to the main task. Design work may now proceed. The artefact at
`.planning/decisions/DECISION-{topic}.md` is the evidence that the gate was passed.

Downstream agents can verify the gate by checking:
- File exists
- `status: approved`
- `duplicates_found: 0`
- `constraints: >= 3`
```

---

## Subagent Delegation Insert

When the orchestrator delegates design work to a subagent, include this block in the subagent prompt:

```
## Design Decision Enforcement (Mandatory)

This task involves design/UI/architecture decisions. You MUST follow these rules:

1. Before any design output, output a DESIGN READBACK block:
   - Problem classification: [visual | structural | functional] + evidence
   - Constraints: [count] — list them
   - Inventory: [built: yes/no, items: count, duplicates: count]
   - Current design audit: [KEEP: count | EVALUATE: count | FIX: count]
   - Options planned: [count]

2. The 12 Design Commandments (abbreviated):
   - CLASSIFY before solving. COLLECT constraints before options.
   - BUILD inventory, CHECK duplicates. AUDIT current design (KEEP/EVALUATE/FIX).
   - GENERATE 3+ structural or 5+ visual options. ELIMINATE constraint violations.
   - 3+ hierarchy levels. Semantic colour only. Use frameworks, not gut feel.
   - MATCH control to resolution. VERIFY zero duplicates before finalising.

3. If you place ANY element without checking the inventory for duplicates,
   your output will be rejected.

4. Reference: tab5-encoder/docs/AGENT_DESIGN_INSTRUCTIONS.md (full instructions)
```

This is 25 lines -- within the subagent prompt budget.

---

## Graceful Degradation

The system must not become a bottleneck for trivial tasks. Here is how it degrades gracefully:

| Scenario | What happens |
|---|---|
| **Cosmetic fix (< 5 LOC)** | No gate fires. Agent proceeds directly. The scope classification rule filters it out. |
| **Minor adjustment (5-20 LOC)** | Agent outputs abbreviated 3-field readback. ~15 seconds overhead. No skill invocation. |
| **Agent does not invoke /decision-gate** | Tier 1 (readback) still catches most failure modes. The pre-commit hook (Tier 3) catches any that slip through at commit time. |
| **No .planning/ directory exists** | The `/decision-gate` skill creates it. The pre-commit hook checks for it and only fires if design-sensitive paths are staged -- no `.planning/` means no block (assumes non-design change). |
| **Skill produces incorrect artefact (duplicates > 0)** | The skill itself includes a verification step that loops back to Phase 3. The pre-commit hook double-checks. |
| **Agent argues the gate is unnecessary** | CLAUDE.md states "if unsure, classify one level higher." The readback block is mandatory for MINOR+. There is no "I think this does not apply" escape hatch for COMPONENT+ scope changes. |
| **Orchestrator forgets to include subagent insert** | The pre-commit hook still fires on the subagent's output when the orchestrator attempts to commit. Late, but caught. |
| **Pre-commit hook produces false positive on trivial UI fix** | The commit message can include `[design-gate-skip: reason]` which the hook recognises as an explicit bypass. This is the "break glass" mechanism. Overuse is visible in git log and can be audited. |

---

## Implementation Plan

### Phase 1: Immediate (This Session)

1. Create `.claude/skills/decision-gate/SKILL.md` with the skill definition above.
2. Create `.planning/decisions/` directory with a `.gitkeep` and `.gitignore` entry.
3. Draft the CLAUDE.md insert (Design Decision Enforcement section) for review.

### Phase 2: Integration (Next Session)

4. Add the Design Decision Enforcement section to CLAUDE.md after Hard Constraints.
5. Add the subagent delegation insert to the Subagent Delegation Protocol section.
6. Create the pre-commit hook script and add it to `.pre-commit-config.yaml`.

### Phase 3: Validation (Following Session)

7. Run a design task through all three tiers. Verify:
   - Tier 1: Readback block appears before design work.
   - Tier 2: Decision artefact is produced by `/decision-gate`.
   - Tier 3: Pre-commit hook fires when staging UI files without artefact.
8. Run a cosmetic fix to verify it bypasses all gates cleanly.
9. Run a minor adjustment to verify only Tier 1 fires.

### Phase 4: Iteration (Ongoing)

10. After 5 design tasks, review:
    - Did agents produce genuine readbacks or boilerplate?
    - Did the `/decision-gate` skill catch any real issues (duplicates, missing constraints)?
    - Did the pre-commit hook fire any legitimate blocks?
    - Was the overhead proportionate to the decision weight?
11. Adjust scope thresholds if needed (COSMETIC/MINOR boundary).
12. Add principle-specific checks to the pre-commit hook if specific failures recur.

---

## Summary

| Tier | Mechanism | Where | When | Catches | Overhead |
|---|---|---|---|---|---|
| 1 | Design Readback | CLAUDE.md | Before design output | Skipped pre-work, wrong classification, no constraints | ~50 tokens |
| 2 | `/decision-gate` skill | `.claude/skills/` | COMPONENT+ scope | Duplicates, insufficient options, missing audit | ~500 tokens |
| 3 | Pre-commit hook | `.pre-commit-config.yaml` | At commit time | Everything that slipped past 1 and 2 | ~0 tokens (runs externally) |

The three tiers are defence in depth. Tier 1 is cheap and catches the most common failure (jumping straight to output). Tier 2 is thorough and catches the most dangerous failure (duplicates and constraint violations). Tier 3 is external and catches everything else. Together they create a system where following the principles is the path of least resistance, and skipping them creates friction that increases with the severity of the skip.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:prompt-engineer | Created. Three-tier enforcement architecture for 18 design decision principles. Includes distilled commandments, skill definition, pre-commit hook, CLAUDE.md insert, subagent delegation template, scope discrimination rules, and implementation plan. |
