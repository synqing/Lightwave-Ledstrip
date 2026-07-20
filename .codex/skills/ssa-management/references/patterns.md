# SSA Management — Patterns Reference

## Contents
- Consumption Contracts
- Evidence Classification
- Contradiction Resolution
- Promotion Gate
- Anti-Patterns

---

## Consumption Contracts

Every `agent()` call must open with a consumption contract. Without it, the SSA has no scope boundary and will overrun context or return untrusted summaries.

```
CONSUMPTION CONTRACT: read ≤N files, run ≤M commands, output ≤T tokens.
Scope: [explicit file/directory/symbol boundary].
Task: [one-sentence action].
Write findings to [_scratch/ssa-<label>-<topic>.txt] before returning.
Return only: the file path.
```

**Why:** An SSA's closing text is self-reported. The on-disk file is auditable. The orchestrator reads the file—never trusts the summary string.

**DO:**
```python
# Explicit scope + evidence path
agent("CONSUMPTION CONTRACT: read ≤3 files. Scope: src/dsp/. Task: list all sb_* function signatures. Write to _scratch/ssa-dsp-sigs.txt. Return: file path.")
result_path = "_scratch/ssa-dsp-sigs.txt"
# Orchestrator reads:
Read(result_path)
```

**DON'T:**
```python
# No contract, no evidence path — trusting summary string
result = agent("List all DSP functions.")
# result is what the SSA claimed it found — not verified
```

---

## Evidence Classification

Before acting on an SSA finding, classify it:

| Class | Criteria | Orchestrator action |
|-------|----------|---------------------|
| LOAD-BEARING | Drives a commit, deploy, CLAUDE.md update, or Captain decision | Re-run the claim independently; validator SSA refutes |
| OPTIONAL | Informational, can be wrong without blast radius | Accept at face value |

```python
# Classify before promoting
finding = "AGC clips on dark-start palettes (SSA report)"

# LOAD-BEARING — re-verify before CLAUDE.md update
validator = agent(
    "CONSUMPTION CONTRACT: read ≤2 files. "
    "Task: try to REFUTE this claim: 'AGC clips on dark-start palettes'. "
    "Write verdict to _scratch/ssa-agc-validator.txt. Return: file path."
)
Read("_scratch/ssa-agc-validator.txt")  # orchestrator judges
```

---

## Contradiction Resolution

When two SSAs disagree, apply the source-truth hierarchy—do not ask the user to adjudicate.

**Hierarchy (highest wins):**
1. Repo source files + git log (`git blame`, `git log --oneline`)
2. `CANONICAL_DECISIONS.md` (Captain-ratified)
3. Current `CLAUDE.md` / `AGENTS.md`
4. SSA with on-disk evidence
5. SSA with summary-only output

```python
# SSA-A says function exists; SSA-B says it was removed
# Resolution: grep the repo — don't ask Captain
Grep("sb_goertzel_tick", type="cpp")
# Repo answer is authoritative; whichever SSA matches wins
```

**Never escalate SSA disagreements to Captain** unless both SSAs agree and the claim has irreversible blast radius (deploy, legal, financial).

---

## Promotion Gate

Before promoting SSA output to any canonical surface:

```
Promotion checklist:
- [ ] On-disk evidence file exists and was Read by orchestrator
- [ ] Claim classified (LOAD-BEARING vs OPTIONAL)
- [ ] If LOAD-BEARING: validator SSA ran and did not refute
- [ ] Source-truth hierarchy checked (repo/git beats SSA)
- [ ] No stale memory contradicts the claim (grep MEMORY.md)
- [ ] If updating CLAUDE.md or MEMORY.md: tombstone superseded entries
```

---

## Anti-Patterns

### WARNING: Trusting SSA Closing Summary

**The Problem:**
```python
result = agent("Audit all test failures.")
# Acting on result text directly
update_claude_md(result)  # BAD
```

**Why This Breaks:** The SSA describes what it intended. File writes, grep results, and test runs may have failed silently. The summary is the SSA's belief, not verified state.

**The Fix:** Always require an on-disk evidence path and `Read()` it.

---

### WARNING: Unbounded SSA Scope

**The Problem:**
```python
agent("Analyze the entire firmware and find all bugs.")
```

**Why This Breaks:** No scope boundary → SSA reads dozens of files → floods orchestrator context on return → loses the original task thread.

**The Fix:** Scope to ≤5 files or one directory. Fan out multiple narrow SSAs in parallel instead of one wide one.

---

### WARNING: Promoting Without Validator on LOAD-BEARING Claims

**The Problem:** Single SSA finds "tempo confidence floor is 0.60 not 0.41" → orchestrator updates `CANONICAL_DECISIONS.md` directly.

**Why This Breaks:** One SSA can misread a constant, pick the wrong file version, or confuse a test fixture with production. In SensoryBridge this caused a 2-month false "PipelineCore BROKEN" claim propagating across 3 project memory dirs.

**The Fix:** Validator SSA with explicit refutation prompt before any canonical write.