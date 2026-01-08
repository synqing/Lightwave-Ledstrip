---
name: ralph-loop
version: 1.0.0
description: Use for autonomous WORKER mode execution - selects highest priority FAILING item with ralph_loop.enabled=true, reads PRD context, implements with iteration tracking, records attempts, commits on success or reverts on failure. ONE item per invocation with intelligent exit detection.
---

# Ralph Loop: Autonomous Workflow Executor

**Purpose:** Execute ONE backlog item autonomously using domain memory harness and PRD specifications.

**When to use:**
- Feature item has `ralph_loop.enabled = true` in feature_list.json
- Requirements are well-defined (PRD exists)
- Clear acceptance criteria documented
- Repetitive implementation tasks

**When NOT to use:**
- Architectural decisions required
- Requirements unclear (create PRD first)
- Design exploration needed
- Manual WORKER mode preferred

---

## Workflow: 9-Step Autonomous Execution

### 1. Boot Ritual (MANDATORY)

```bash
cd .claude/harness && ./init.sh
```

**Purpose:** Verify project health before starting work.

**Checks:**
- Git repository status
- PlatformIO toolchain availability
- Default build compiles

**If fails:** Fix build issues BEFORE selecting items. STOP immediately.

---

### 2. Select Item

**Script:** `.claude/skills/ralph-loop/scripts/select_next_item.py`

**Selection criteria:**
1. `ralph_loop.enabled = true`
2. `status = FAILING`
3. Dependencies satisfied (PASSING or CANCELLED)
4. Highest priority (lowest number)

**Command:**
```bash
python3 select_next_item.py --harness-dir .claude/harness
```

**Exit conditions:**
- No eligible items → "Backlog complete" (exit code 1)
- `current_iteration >= max_iterations` → Item BLOCKED (exit code 2)
- Item selected → Continue to step 3

---

### 3. Read PRD (If Exists)

**If item has `prd_reference` field:**
```json
{
  "prd_reference": {
    "file": "FEAT-XXX.json",
    "last_updated": "2026-01-07T00:00:00Z",
    "version": "1.0.0"
  }
}
```

**Read:** `.claude/prd/<file>.json`

**Extract:**
- `functional_requirements` - What to build
- `acceptance_criteria` - How to verify
- `constraints` - Hard limits (CENTER_ORIGIN, NO_RAINBOWS, etc.)
- `nonfunctional_requirements` - Performance targets

**If no PRD:** Use acceptance_criteria from feature_list.json item.

---

### 4. Implement

**Constraints (from CLAUDE.md):**
- **CENTER ORIGIN**: All effects MUST originate from LED 79/80
- **NO RAINBOWS**: No rainbow color cycling effects
- **Performance**: Target 120 FPS
- **Memory**: Minimize heap allocations in render loops

**Sub-skills to invoke:**
- `/test-driven-development` - For new features
- `/software-architecture` - For design decisions
- `/subagent-driven-development` - For complex changes

**Implementation approach:**
1. Read relevant source files
2. Make targeted changes
3. Follow project conventions
4. Write tests if needed
5. Build incrementally

---

### 5. Verify

**Run acceptance criteria:**
```bash
# Example from item verification commands
pio run -e esp32dev_wifi
grep NUM_LEDS src/config/hardware_config.h
```

**Convergence criteria:**
- ✅ `build_passes`: Compilation succeeds (exit code 0)
- ✅ `tests_pass`: Tests return 0 (if tests exist)
- ✅ `acceptance_met`: All verification commands succeed

**If all pass:** Proceed to step 6
**If any fail:** Record failure, proceed to step 7 with FAILED result

---

### 6. Record Attempt

**Script:** `.claude/skills/ralph-loop/scripts/update_attempt.py`

**Command:**
```bash
python3 update_attempt.py ITEM_ID RESULT \
    --commands "cmd1,cmd2,cmd3" \
    --summary "Detailed results description" \
    --commit <hash>
```

**RESULT values:**
- `PASSED` - All convergence criteria met
- `FAILED` - One or more criteria failed
- `IN_PROGRESS` - Implementation ongoing (rarely used)

**Evidence structure:**
```json
{
  "run_id": "worker-NNN",
  "timestamp": "2026-01-07T19:30:00Z",
  "result": "PASSED",
  "evidence": {
    "commands_run": ["pio run -e esp32dev_wifi", "grep NUM_LEDS"],
    "results_summary": "Build passes, LED count correct at 320"
  },
  "commit": "abc1234",
  "reverted": false
}
```

---

### 7. Update Status/Iteration

**If PASSED (Convergence):**
```json
{
  "status": "PASSING",
  "ralph_loop": {
    "current_iteration": N  // Don't reset - shows effort required
  }
}
```

**If FAILED:**

**Script:** `.claude/skills/ralph-loop/scripts/increment_iteration.py`

```bash
python3 increment_iteration.py ITEM_ID
```

**Behavior:**
- Increments `current_iteration`
- Status remains `FAILING`
- If `current_iteration >= max_iterations`: status → `BLOCKED`

**BLOCKED items require human review:**
- Review attempts[] history
- Identify recurring failure pattern
- Provide architectural guidance
- Reset `current_iteration = 0` after intervention

---

### 8. Commit/Revert

**On SUCCESS (PASSED):**
```bash
git add -A
git commit -m "feat(scope): Short summary

Detailed description of changes.

Verification:
- Command 1 output
- Command 2 output

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

**On FAILURE (FAILED):**
```bash
# Revert code changes (NOT harness changes)
git checkout -- src/ test/ data/

# Or use git revert
git revert --no-commit HEAD
git commit -m "revert: ITEM_ID - <failure reason>

Investigation: <what was tried, why it failed, hypothesis for next attempt>

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

**Keep harness updates:**
- Attempts[] array - append-only
- feature_list.json - status and iteration fields

---

### 9. STOP

**Critical:** ONE item per invocation.

**Exit reasons:**
- ✅ Item PASSED → Converged, status PASSING
- ❌ Item FAILED → Iteration incremented, ready for next invocation
- ⚠️ Item BLOCKED → Max iterations reached, human review required
- ℹ️ No eligible items → Backlog complete

**Next invocation:**
User or automation invokes `/ralph-loop` again for next item.

---

## Intelligent Exit Detection

### Convergence Signals

**PASSED → STOP:**
```
build_passes=true + tests_pass=true + acceptance_met=true
→ Status changes to PASSING
→ Commit code changes
→ STOP (success)
```

**BLOCKED → STOP:**
```
current_iteration >= max_iterations
→ Status changes to BLOCKED
→ Add to notes: "Max iterations reached, human review required"
→ STOP (needs intervention)
```

**FAILED → CONTINUE (Next Invocation):**
```
One or more convergence criteria false
→ Status remains FAILING
→ current_iteration incremented
→ Code changes reverted
→ STOP (ready for retry)
```

**No Items → STOP:**
```
No FAILING items with ralph_loop.enabled=true
→ "Backlog complete!"
→ STOP (nothing to do)
```

### Boot Failure → STOP

```
init.sh fails (build broken)
→ "Fix build before running Ralph Loop"
→ STOP (prerequisites not met)
```

---

## Scripts Reference

### select_next_item.py

**Purpose:** Select highest priority eligible item

**Location:** `.claude/skills/ralph-loop/scripts/select_next_item.py`

**Usage:**
```bash
python3 select_next_item.py [--harness-dir DIR]
```

**Output:**
```json
{
  "success": true,
  "selected": {
    "id": "FEAT-XXX",
    "title": "Feature title",
    "priority": 2,
    "ralph_loop": {...},
    "prd_reference": {...}
  },
  "dependency_check": {
    "satisfied": true,
    "blocking_deps": []
  }
}
```

**Exit codes:**
- 0 = Item selected
- 1 = No eligible items
- 2 = Circular dependency detected

---

### update_attempt.py

**Purpose:** Append attempt to attempts[] array with evidence

**Location:** `.claude/skills/ralph-loop/scripts/update_attempt.py`

**Usage:**
```bash
python3 update_attempt.py ITEM_ID RESULT \
    [--commands "cmd1,cmd2"] \
    [--summary "Results"] \
    [--commit HASH] \
    [--failure-reason "Why"] \
    [--investigation "Details"] \
    [--harness-dir DIR]
```

**Required:**
- `ITEM_ID` - Feature item ID
- `RESULT` - PASSED, FAILED, or IN_PROGRESS

**Optional:**
- `--commands` - Comma-separated list of verification commands
- `--summary` - Results summary (required for PASSED)
- `--commit` - Git commit hash
- `--failure-reason` - Why it failed (for FAILED)
- `--investigation` - What to try next (for FAILED)

---

### increment_iteration.py

**Purpose:** Increment iteration counter and optionally block

**Location:** `.claude/skills/ralph-loop/scripts/increment_iteration.py`

**Usage:**
```bash
python3 increment_iteration.py ITEM_ID \
    [--block-on-max] \
    [--override-reason "Reason"] \
    [--harness-dir DIR]
```

**Behavior:**
- Increments `current_iteration`
- Warns if >= `max_iterations`
- With `--block-on-max`: Changes status to BLOCKED
- Requires `--override-reason` when blocking

---

### validate_evidence.py

**Purpose:** Verify attempts[] array has proper evidence structure

**Location:** `.claude/skills/ralph-loop/scripts/validate_evidence.py`

**Usage:**
```bash
python3 validate_evidence.py [ITEM_ID] \
    [--strict] \
    [--harness-dir DIR]
```

**Validation rules:**
- PASSING items must have ≥1 PASSED attempt
- PASSED attempts must have evidence.commands_run
- PASSED attempts must have evidence.results_summary
- All attempts must have timestamp (ISO8601)
- All attempts must have run_id

---

## PRD System Integration

### PRD Structure

**Location:** `.claude/prd/<feature_id>.json`

**Schema:** `.claude/prd/schema.json`

**Required fields:**
```json
{
  "$schema": "https://lightwave-os.local/schemas/prd-v1.json",
  "feature_id": "FEAT-XXX",
  "title": "Feature Title",
  "overview": "2-4 sentence summary",
  "functional_requirements": [
    "FR-1: System shall...",
    "FR-2: System shall..."
  ],
  "acceptance_criteria": [
    "AC-1: Build compiles without errors",
    "AC-2: Tests pass with exit code 0"
  ]
}
```

**Optional fields:**
- `nonfunctional_requirements` - Performance, memory, timing
- `constraints` - Hard limits (CENTER_ORIGIN, NO_RAINBOWS)
- `version` - Semver version
- `last_updated` - ISO8601 timestamp

### Linking PRD to Item

**In feature_list.json:**
```json
{
  "id": "FEAT-XXX",
  "prd_reference": {
    "file": "FEAT-XXX.json",
    "last_updated": "2026-01-07T00:00:00Z",
    "version": "1.0.0"
  },
  "ralph_loop": {
    "enabled": true,
    "max_iterations": 5,
    "current_iteration": 0,
    "convergence_criteria": {
      "build_passes": true,
      "tests_pass": true,
      "acceptance_met": true
    }
  }
}
```

---

## Safety Mechanisms

### Max Iterations Limit

**Default:** 5 iterations

**Prevents:**
- Infinite loops
- Repeated failed approaches
- Wasted compute resources

**Recovery:**
After BLOCKED status, human:
1. Reviews attempts[] history
2. Identifies failure pattern
3. Provides architectural guidance
4. Resets `current_iteration = 0`
5. Updates PRD if needed

### Advisory Lock

**File:** `.harness.lock`

**Timeout:** 30 minutes

**Non-blocking:** Warns if detected, doesn't prevent work

**Git merge conflicts** are ultimate arbiter of concurrent access.

### Evidence Requirements

**PASSING status requires:**
- At least one PASSED attempt
- Evidence with commands_run[] and results_summary
- Valid commit hash
- ISO8601 timestamp

**Validation:** Run `validate_evidence.py` before marking PASSING

---

## Example Invocation

```bash
# User or automation invokes:
/ralph-loop

# Ralph Loop executes:
1. cd .claude/harness && ./init.sh
2. python3 scripts/select_next_item.py
3. # Read .claude/prd/FEAT-XXX.json (if exists)
4. # Implement feature
5. # Run acceptance criteria
6. python3 scripts/update_attempt.py FEAT-XXX PASSED --commands "pio run" --summary "Build passes"
7. # Update status to PASSING
8. git commit -m "feat(scope): Implement FEAT-XXX"
9. # STOP (one item complete)
```

**Next invocation:** Repeat for next FAILING item with `ralph_loop.enabled=true`

---

## References

- **Complete workflow guide:** `.claude/skills/ralph-loop/references/WORKER_MODE_GUIDE.md`
- **Harness protocol:** `.claude/harness/HARNESS_RULES.md`
- **PRD system guide:** `.claude/prd/README.md`
- **Project constraints:** `CLAUDE.md`

---

## Summary: The Golden Rules

1. **ONE item per invocation** - Never process multiple items
2. **Boot ritual first** - init.sh MUST pass before work
3. **Follow constraints** - CENTER_ORIGIN, NO_RAINBOWS mandatory
4. **Record all attempts** - Even failed ones (append-only)
5. **Evidence required** - PASSING needs proof
6. **Revert code, not harness** - Keep attempt history
7. **STOP after one item** - Let automation/user invoke again
8. **Max iterations safety** - Prevent infinite loops
9. **Commit with evidence** - Include verification results
10. **Convergence detection** - Know when to stop

---

**Version:** 1.0.0
**Last Updated:** 2026-01-07
**Status:** Production Ready ✅
