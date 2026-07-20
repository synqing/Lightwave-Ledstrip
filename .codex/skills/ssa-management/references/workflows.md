# SSA Management — Workflows Reference

## Contents
- Delegation Decision Flow
- Parallel Fan-Out Workflow
- Evidence Promotion Workflow
- Contradiction Resolution Workflow

---

## Delegation Decision Flow

Before spawning any SSA, answer these:

```
1. Is the task >3 file reads OR >1 shell command? → delegate
2. Is the output needed in orchestrator context? → require evidence file path
3. Is the output LOAD-BEARING? → add validator SSA
4. Does scope touch CANONICAL_DECISIONS.md / CLAUDE.md / MEMORY.md? → require orchestrator re-verify + tombstone check
```

If all answers are "no" → do it inline, no SSA needed.

---

## Parallel Fan-Out Workflow

Use when multiple independent scopes need auditing (e.g., each firmware subsystem).

```
Copy this checklist:
- [ ] Define N narrow scopes (≤5 files each)
- [ ] Write consumption contract template
- [ ] Dispatch N SSAs in parallel (one per scope)
- [ ] Each SSA writes to _scratch/ssa-<scope>-findings.txt
- [ ] Orchestrator reads all N files
- [ ] Orchestrator synthesizes — never re-delegates synthesis
- [ ] Classify each finding (LOAD-BEARING vs OPTIONAL)
- [ ] Run validator SSA on all LOAD-BEARING findings
- [ ] Clean up _scratch/ after promotion
```

**SensoryBridge example:** auditing all 25 light_mode files for strobe-law violations:

```python
# new code to add
scopes = [f"src/light_mode/light_mode_{i:02d}.cpp" for i in range(1, 26)]

for scope in scopes:
    agent(
        f"CONSUMPTION CONTRACT: read 1 file. "
        f"Scope: {scope}. "
        f"Task: does any beat-reactive path modify global brightness (not spatial/transport)? "
        f"Write YES/NO + line numbers to _scratch/ssa-strobe-{scope.split('/')[-1]}.txt. "
        f"Return: file path."
    )
# Orchestrator reads each file, aggregates violations
```

---

## Evidence Promotion Workflow

Use when an SSA finding must update a canonical surface.

```
Promotion workflow:
1. Read evidence file (orchestrator, not SSA)
2. Grep repo for contradicting claims
3. Check CANONICAL_DECISIONS.md for existing ruling
4. If contradicted by higher source: discard SSA finding, update stale memory
5. If no contradiction:
   a. Run validator SSA (refutation prompt)
   b. Read validator evidence file
   c. If refuted: discard, log in _scratch/
   d. If not refuted: promote with tombstone on superseded entries
6. Clean _scratch/ artifacts
```

**Tombstone pattern for MEMORY.md:**
```markdown
<!-- SUPERSEDED 2026-06-11 by SSA-validated finding — see CANONICAL_DECISIONS.md §N -->
~~Old claim text~~
```

---

## Contradiction Resolution Workflow

```
When SSA-A and SSA-B disagree:

1. Check source-truth hierarchy:
   a. grep/Read the repo file in question
   b. git log --oneline -- <file> (last 5 commits)
   c. Read CANONICAL_DECISIONS.md
   → Whichever SSA matches the highest source wins. Stop here.

2. If repo is ambiguous (e.g., two valid codepaths):
   a. Dispatch a third SSA with explicit "adjudicate" prompt
   b. Adjudicator must cite file:line for its ruling
   c. Orchestrator reads adjudicator evidence file

3. Escalate to Captain ONLY if:
   - Both SSAs agree AND claim has irreversible blast radius
   - OR genuine source-of-truth gap (no repo evidence exists)
   Format: current state → decision required → options → recommended → blast radius → default if no override
```

**DO NOT:** Ask "SSA-A said X and SSA-B said Y, which is right?" — that is an escalation of an execution-resolvable disagreement. Grep the repo first.

---

## Aggressive Fan-Out Principle (Captain Standing Order 2026-06-04)

Default to fanning out SSAs for heavy reading, running, or exploration. Keep orchestrator context for synthesis and decisions only.

**Consumption contract for heavy exploration:**
```
CONSUMPTION CONTRACT: read ≤10 files, run ≤5 commands, output ≤500 tokens.
Scope: [directory].
Task: [specific question].
Write structured findings to _scratch/ssa-<label>.txt (one finding per line, format: FILE:LINE: FINDING).
Return: file path only.
```

**Orchestrator stays lean:** receives file paths, reads evidence, synthesizes, decides. Never re-delegates synthesis.