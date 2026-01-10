# Ralph Loop WORKER Mode Guide

**Version:** 1.0.0
**Last Updated:** 2026-01-07
**Audience:** AI agents executing autonomous Ralph Loop workflow

This guide provides detailed reference for agents working in Ralph Loop WORKER mode, covering workflow execution, script usage, troubleshooting, and best practices.

---

## Table of Contents

1. [Quick Reference](#quick-reference)
2. [Workflow Deep Dive](#workflow-deep-dive)
3. [Script Usage](#script-usage)
4. [PRD Integration](#prd-integration)
5. [Convergence Detection](#convergence-detection)
6. [Iteration Management](#iteration-management)
7. [Evidence Requirements](#evidence-requirements)
8. [Troubleshooting](#troubleshooting)
9. [Best Practices](#best-practices)
10. [Escape Hatches](#escape-hatches)

---

## Quick Reference

### One-Page Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ RALPH LOOP - ONE ITEM PER INVOCATION                        │
└─────────────────────────────────────────────────────────────┘

[1] BOOT RITUAL (MANDATORY)
    $ cd .claude/harness && ./init.sh
    → If fails: Fix build BEFORE selecting item

[2] SELECT ITEM
    $ python3 select_next_item.py
    → Highest priority FAILING with ralph_loop.enabled=true
    → Warns if dependencies FAILING

[3] READ PRD (if prd_reference exists)
    $ cat .claude/prd/<file>
    → Load functional requirements
    → Load acceptance criteria
    → Load constraints

[4] IMPLEMENT
    → Follow CENTER ORIGIN for effects
    → NO_RAINBOWS constraint enforced
    → Use sub-skills: /test-driven-development, etc.

[5] VERIFY
    → Run acceptance criteria commands
    → Check convergence: build_passes + tests_pass + acceptance_met

[6] RECORD ATTEMPT
    $ python3 update_attempt.py <ID> <RESULT> \
        --commands "..." --summary "..." --commit <sha>

[7] UPDATE STATUS
    PASSED:
      → Validate evidence first (validate_evidence.py)
      → Change status to PASSING in feature_list.json
      → Git commit with evidence
    FAILED:
      → Keep status FAILING
      → Git revert code changes (NOT harness changes)
      → Increment iteration (increment_iteration.py)

[8] CHECK CONVERGENCE
    → If converged: STOP (backlog item complete)
    → If max_iterations exceeded: BLOCKED → human review
    → Else: continue to next item

[9] STOP
    → ONE item per invocation
    → User or automation re-invokes for next item
```

### Key Commands

```bash
# Boot ritual
.claude/harness/init.sh

# Select next item
python3 .claude/skills/ralph-loop/scripts/select_next_item.py

# Record attempt
python3 .claude/skills/ralph-loop/scripts/update_attempt.py <ID> <RESULT> \
    --commands "cmd1,cmd2" --summary "..." [--commit SHA]

# Increment iteration
python3 .claude/skills/ralph-loop/scripts/increment_iteration.py <ID> \
    [--block-on-max] [--reason "..."]

# Validate evidence
python3 .claude/skills/ralph-loop/scripts/validate_evidence.py [ID] \
    [--strict]
```

---

## Workflow Deep Dive

### Step 1: Boot Ritual

**Purpose:** Verify project health before starting work.

**Command:**
```bash
cd .claude/harness && ./init.sh
```

**What it checks:**
- Git repository health
- PlatformIO toolchain availability
- Default build compiles (esp32dev or esp32dev_audio)

**Exit behavior:**
- Exit code 0: Proceed to Step 2
- Exit code non-zero: **STOP** - fix build issues first

**Why mandatory:**
Ralph Loop should NEVER work on broken infrastructure. If the build is already broken, autonomous work will waste iterations.

**Common failures:**
- Missing toolchain → Run PlatformIO installation
- Git conflicts → Resolve manually
- Missing dependencies → Check platformio.ini

---

### Step 2: Item Selection

**Purpose:** Choose ONE highest-priority eligible item.

**Eligibility criteria:**
1. `ralph_loop.enabled = true`
2. `status = "FAILING"`
3. Dependencies satisfied (not FAILING)

**Command:**
```bash
python3 .claude/skills/ralph-loop/scripts/select_next_item.py
```

**Output:**
```json
{
  "success": true,
  "selected": {
    "id": "FIX-TEST-ASSERTIONS",
    "title": "Fix test assertions for current hardware",
    "priority": 2,
    "ralph_loop": {...},
    "prd_reference": {...},
    "dependencies": [],
    "acceptance_criteria": [...],
    "verification": [...]
  },
  "dependency_check": {
    "satisfied": true,
    "blocking_deps": [],
    "warnings": []
  },
  "stats": {
    "total_items": 9,
    "ralph_enabled": 3,
    "failing": 2,
    "eligible": 2
  }
}
```

**Exit codes:**
- 0: Item selected
- 1: No eligible items (backlog complete or all PASSING)
- 2: Error (file not found, invalid JSON)

**Edge cases:**
- **No eligible items:** Ralph Loop exits gracefully ("backlog complete")
- **Dependencies FAILING:** Selection includes warnings, but doesn't block
- **Multiple eligible:** Selects lowest priority number (highest priority)

---

### Step 3: Read PRD

**Purpose:** Load requirements if PRD reference exists.

**Check for PRD:**
```python
if selected["prd_reference"]:
    prd_file = f".claude/prd/{selected['prd_reference']['file']}"
    # Read PRD JSON
```

**PRD structure:**
```json
{
  "feature_id": "FEAT-XXX",
  "title": "...",
  "overview": "...",
  "functional_requirements": ["FR-1: ...", "FR-2: ..."],
  "nonfunctional_requirements": ["NFR-1: ...", "NFR-2: ..."],
  "acceptance_criteria": ["AC-1: ...", "AC-2: ..."],
  "constraints": ["CENTER_ORIGIN", "NO_RAINBOWS"],
  "version": "1.0.0",
  "last_updated": "2026-01-07T00:00:00Z"
}
```

**What to extract:**
- **Functional requirements:** What the feature must do
- **Acceptance criteria:** How to verify success
- **Constraints:** Hard limits (CENTER_ORIGIN, NO_RAINBOWS, etc.)
- **Non-functional requirements:** Performance, memory, timing constraints

**If PRD doesn't exist:**
Fall back to `acceptance_criteria` and `verification` from feature_list.json item.

---

### Step 4: Implementation

**Purpose:** Make targeted changes to satisfy requirements.

**Guidelines:**

#### Constraints Compliance

**CENTER ORIGIN** (for visual effects):
```cpp
// ✅ CORRECT - Radiate from center
const uint8_t CENTER = NUM_LEDS / 2;
for (uint16_t i = 0; i < CENTER; i++) {
    uint8_t distFromCenter = abs(i - CENTER);
    // Effect logic using distFromCenter
}

// ❌ WRONG - Linear left-to-right
for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // Linear pattern
}
```

**NO_RAINBOWS**:
```cpp
// ❌ FORBIDDEN
EVERY_N_MILLISECONDS(20) { gHue++; }
fill_rainbow(leds, NUM_LEDS, gHue, 7);

// ✅ ALLOWED
CRGBPalette16 palette = OceanColors_p;
for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(palette, index, brightness);
}
```

#### Sub-Skill Invocation

Use project skills when appropriate:

- **test-driven-development** - Write tests first for new features
- **subagent-driven-development** - Complex multi-file changes
- **software-architecture** - Design decisions
- **finishing-a-development-branch** - Final cleanup before merge

**Example:**
```markdown
Since this feature requires adding new API endpoints with tests,
I'll use the /test-driven-development skill to ensure proper test coverage.
```

#### Performance Considerations

From ARCHITECTURE.md:
- Target 120 FPS (8.33ms/frame budget)
- Minimize heap allocations in render loops
- Use TrigLookup for expensive trig operations
- Avoid `new`/`malloc` in hot paths

---

### Step 5: Verification

**Purpose:** Confirm implementation meets acceptance criteria.

**Run verification commands:**
```bash
# From item.verification[]
pio run -e esp32dev_wifi
grep NUM_LEDS src/config/hardware_config.h
# ... etc
```

**Check convergence criteria:**
```python
convergence = selected["ralph_loop"]["convergence_criteria"]

build_passes = convergence.get("build_passes", True)
tests_pass = convergence.get("tests_pass", True)
acceptance_met = convergence.get("acceptance_met", True)

if build_passes and tests_pass and acceptance_met:
    # Item converged - status → PASSING
else:
    # Not converged - increment iteration, try again
```

**Build verification:**
```bash
pio run -e esp32dev_wifi
# Exit code 0 = build_passes
```

**Test verification:**
```bash
pio test -e native
# Exit code 0 = tests_pass
```

**Acceptance verification:**
Run each command in `acceptance_criteria` or PRD acceptance_criteria.

---

### Step 6: Record Attempt

**Purpose:** Append structured attempt to attempts[] array.

**Command:**
```bash
python3 .claude/skills/ralph-loop/scripts/update_attempt.py \
    FIX-TEST-ASSERTIONS \
    PASSED \
    --run-id worker-008 \
    --commands "pio run -e esp32dev_wifi,grep NUM_LEDS" \
    --summary "Build passes, LED count assertion updated 81→320" \
    --commit abc1234
```

**Required fields:**
- `ITEM_ID`: Feature item ID
- `RESULT`: PASSED, FAILED, or IN_PROGRESS
- `--summary`: Results summary (required for PASSED/FAILED)

**Optional fields:**
- `--run-id`: Worker identifier (auto-generated if omitted)
- `--commands`: Comma-separated command list
- `--commit`: Git SHA (auto-detected if omitted)
- `--reverted`: Flag for reverted changes

**Result values:**
- **PASSED**: Implementation successful, convergence criteria met
- **FAILED**: Implementation unsuccessful, bugs or issues remain
- **IN_PROGRESS**: Work started but not complete (rare in Ralph Loop)

---

### Step 7: Status Update

**On SUCCESS (PASSED attempt):**

1. **Validate evidence first:**
   ```bash
   python3 .claude/skills/ralph-loop/scripts/validate_evidence.py FIX-TEST-ASSERTIONS
   ```

2. **Update status in feature_list.json:**
   ```json
   {
     "id": "FIX-TEST-ASSERTIONS",
     "status": "PASSING",  // Changed from FAILING
     "attempts": [...]     // Contains PASSED attempt with evidence
   }
   ```

3. **Git commit:**
   ```bash
   git add -A
   git commit -m "feat(tests): Fix LED count assertion 81→320

   Updated test_main.cpp to reflect dual 160-LED strip configuration.
   Palette count assertion (33) was already correct.

   🤖 Generated with [Claude Code](https://claude.com/claude-code)

   Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
   ```

**On FAILURE (FAILED attempt):**

1. **Keep status FAILING** (no change)

2. **Revert code changes** (preserve harness changes):
   ```bash
   # Revert code files, keep harness files
   git checkout -- src/ test/ data/
   # Do NOT revert .claude/harness/feature_list.json
   ```

3. **Increment iteration:**
   ```bash
   python3 .claude/skills/ralph-loop/scripts/increment_iteration.py FIX-TEST-ASSERTIONS
   ```

4. **Check max_iterations:**
   ```bash
   # If current_iteration >= max_iterations:
   python3 .claude/skills/ralph-loop/scripts/increment_iteration.py \
       FIX-TEST-ASSERTIONS \
       --block-on-max \
       --reason "Max iterations (5) reached without convergence. Requires human architectural review."
   ```

---

### Step 8: Check Convergence

**Convergence conditions:**

```python
ralph_loop = item["ralph_loop"]
current = ralph_loop["current_iteration"]
max_iter = ralph_loop["max_iterations"]

if status == "PASSING":
    # Item converged - STOP
    return "Item complete, ready for next"

elif current >= max_iter:
    # Max iterations exceeded - BLOCKED
    return "Human review required"

else:
    # Not converged, iterations remaining
    return "Continue with next attempt"
```

**Exit types:**

1. **Convergence (success):** Status changed to PASSING, evidence recorded
2. **Max iterations (blocked):** Status changed to BLOCKED, human review needed
3. **Iterations remaining:** Status still FAILING, loop continues

---

### Step 9: STOP

**Purpose:** ONE item per invocation.

**Why stop:**
- Enables controlled iteration
- Allows human oversight between items
- Prevents runaway autonomous execution
- Matches harness "one item per run" principle

**Next invocation:**
- User or automation re-invokes Ralph Loop skill
- Loop starts at Step 1 (boot ritual)
- Selects next highest-priority item

---

## Script Usage

### select_next_item.py

**Purpose:** Select highest priority eligible item.

**Usage:**
```bash
python3 select_next_item.py [--harness-dir PATH]
```

**Selection logic:**
1. Filter: `ralph_loop.enabled = true`
2. Filter: `status = "FAILING"`
3. Check: Dependencies not FAILING
4. Sort: By priority (ascending)
5. Return: First item (highest priority)

**Output fields:**
- `selected`: Item object (null if none eligible)
- `dependency_check`: Dependency satisfaction status
- `stats`: Backlog statistics

**Exit codes:**
- 0: Item selected
- 1: No eligible items
- 2: Error

---

### update_attempt.py

**Purpose:** Append attempt to attempts[] array.

**Usage:**
```bash
python3 update_attempt.py ITEM_ID RESULT [OPTIONS]
```

**Arguments:**
- `ITEM_ID`: Feature item ID (e.g., FIX-TEST-ASSERTIONS)
- `RESULT`: PASSED, FAILED, or IN_PROGRESS

**Options:**
- `--run-id ID`: Worker identifier (default: auto-generated)
- `--commands "cmd1,cmd2"`: Comma-separated commands
- `--summary "..."`: Results summary (required for PASSED/FAILED)
- `--commit SHA`: Git commit (default: auto-detected)
- `--reverted`: Mark as reverted (for FAILED)

**Example:**
```bash
python3 update_attempt.py FIX-TEST-ASSERTIONS PASSED \
    --commands "pio run,grep NUM_LEDS" \
    --summary "Build passes, LED count correct"
```

**Validation:**
- PASSED/FAILED require `--summary`
- Auto-detects git commit if in repo
- Generates run_id if not provided

---

### increment_iteration.py

**Purpose:** Increment current_iteration, check max_iterations.

**Usage:**
```bash
python3 increment_iteration.py ITEM_ID [OPTIONS]
```

**Options:**
- `--block-on-max`: Change status to BLOCKED if max exceeded
- `--reason "..."`: Override reason (required with --block-on-max)

**Behavior:**
1. Read ralph_loop.current_iteration
2. Increment by 1
3. Compare to max_iterations
4. Warn if max exceeded
5. Optionally BLOCK if --block-on-max

**Exit codes:**
- 0: Success
- 1: Max iterations exceeded (warning only)
- 2: Error

**Example:**
```bash
# Increment (warn only)
python3 increment_iteration.py FIX-TEST-ASSERTIONS

# Increment and block on max
python3 increment_iteration.py FIX-TEST-ASSERTIONS \
    --block-on-max \
    --reason "Max iterations (5) reached. Requires human review."
```

---

### validate_evidence.py

**Purpose:** Validate attempts[] evidence structure.

**Usage:**
```bash
python3 validate_evidence.py [ITEM_ID] [OPTIONS]
```

**Options:**
- `ITEM_ID`: Validate specific item (omit for all)
- `--strict`: Treat warnings as failures
- `--status STATUS`: Filter by status

**Validation rules:**
- PASSING items must have at least one PASSED attempt
- PASSED/FAILED attempts require evidence object
- Evidence must have commands_run[] and results_summary
- results_summary must be non-empty
- All attempts must have timestamp and run_id

**Example:**
```bash
# Validate all PASSING items
python3 validate_evidence.py

# Validate specific item
python3 validate_evidence.py FIX-TEST-ASSERTIONS --strict

# Audit all FAILING items
python3 validate_evidence.py --status FAILING
```

**Exit codes:**
- 0: All validations passed
- 1: Validation issues/warnings
- 2: Error

---

## PRD Integration

### When PRDs Exist

**Check prd_reference:**
```python
prd_ref = selected.get("prd_reference")
if prd_ref:
    prd_file = f".claude/prd/{prd_ref['file']}"
    # Load PRD JSON
```

### PRD Precedence

**Priority order:**
1. PRD `acceptance_criteria` → Use for verification
2. feature_list.json `acceptance_criteria` → Fallback if no PRD
3. PRD `functional_requirements` → Implementation guidance
4. PRD `constraints` → Hard limits

### Constraint Enforcement

**From PRD constraints array:**
```json
"constraints": ["CENTER_ORIGIN", "NO_RAINBOWS", "HARDWARE_SPECIFIC"]
```

**Meaning:**
- `CENTER_ORIGIN`: Effects must radiate from center (LED 79/80)
- `NO_RAINBOWS`: No rainbow color cycling allowed
- `HARDWARE_SPECIFIC`: Requires specific hardware (e.g., M5ROTATE8)
- Custom constraints: Defined in CONSTRAINTS.md

---

## Convergence Detection

### Convergence Criteria

**Three-condition check:**
```python
convergence = ralph_loop["convergence_criteria"]

build_passes = run_build() == 0
tests_pass = run_tests() == 0
acceptance_met = all(run(cmd) == 0 for cmd in acceptance_criteria)

converged = build_passes and tests_pass and acceptance_met
```

### Build Passes

**Check:**
```bash
pio run -e esp32dev_wifi
```

**Exit code 0 = build_passes**

**Common build failures:**
- Syntax errors
- Missing includes
- Linker errors
- Configuration issues

### Tests Pass

**Check:**
```bash
pio test -e native
# Or if no native env:
pio run -e esp32dev && echo "Build test passed"
```

**Exit code 0 = tests_pass**

### Acceptance Met

**Run verification commands:**
```bash
# From item.verification[] or PRD acceptance_criteria
for cmd in verification_commands:
    exit_code = run(cmd)
    if exit_code != 0:
        acceptance_met = False
```

---

## Iteration Management

### Iteration Lifecycle

```
Iteration 0 → Attempt 1 → FAILED → Increment → Iteration 1
Iteration 1 → Attempt 2 → FAILED → Increment → Iteration 2
Iteration 2 → Attempt 3 → PASSED → Status: PASSING (converged)
```

### Max Iterations Safety

**Default: 5 iterations**

**When max exceeded:**
```python
if current_iteration >= max_iterations:
    # Human review required
    status = "BLOCKED"
    override_reason = "Max iterations (5) reached without convergence"
```

**Why limit iterations:**
- Prevents infinite loops
- Flags architectural issues
- Requires human intervention for complex problems

### Resetting Iterations

**After human review:**
```json
{
  "ralph_loop": {
    "enabled": true,
    "max_iterations": 5,
    "current_iteration": 0,  // Reset to 0
    "convergence_criteria": {...}
  },
  "override_reason": "Architectural refactor completed. Resetting for new attempt."
}
```

---

## Evidence Requirements

### PASSING Status Requirements

**Rule:** Status = PASSING requires at least one PASSED attempt with evidence.

**Valid evidence:**
```json
{
  "run_id": "worker-008",
  "timestamp": "2025-12-12T08:30:00Z",
  "result": "PASSED",
  "evidence": {
    "commands_run": ["pio run -e esp32dev_wifi", "grep NUM_LEDS"],
    "results_summary": "Build passes, LED count assertion updated 81→320"
  },
  "commit": "abc1234",
  "reverted": false
}
```

### Evidence Structure

**Required fields:**
- `run_id`: Worker/agent identifier
- `timestamp`: ISO8601 format
- `result`: PASSED, FAILED, or IN_PROGRESS
- `evidence`: Object with commands_run[] and results_summary

**Optional fields:**
- `commit`: Git SHA
- `reverted`: Boolean (default false)

### Evidence Validation

**Pre-flight check:**
```bash
python3 validate_evidence.py FIX-TEST-ASSERTIONS --strict
```

**Before changing status to PASSING:**
1. Validate evidence structure
2. Confirm results_summary non-empty
3. Verify commands_run present

---

## Troubleshooting

### Issue: Boot Ritual Fails

**Symptom:**
```bash
$ .claude/harness/init.sh
[FAIL] Build verification failed
```

**Solution:**
1. Check PlatformIO toolchain: `pio --version`
2. Run build manually: `pio run -e esp32dev`
3. Check error messages
4. Fix build issues BEFORE selecting items

**Prevention:**
- Always run init.sh first
- Never skip boot ritual

---

### Issue: No Eligible Items

**Symptom:**
```json
{
  "success": true,
  "selected": null,
  "message": "No eligible items for Ralph Loop execution"
}
```

**Causes:**
1. All items are PASSING (backlog complete)
2. No items have `ralph_loop.enabled=true`
3. All FAILING items have dependencies that are FAILING

**Solution:**
- If backlog complete: Celebrate! Ralph Loop done.
- If no ralph_enabled: Enable ralph_loop on items
- If dependencies blocking: Fix dependencies first

---

### Issue: Max Iterations Exceeded

**Symptom:**
```bash
$ python3 increment_iteration.py FIX-TEST-ASSERTIONS
{
  "warnings": ["Max iterations (5) reached or exceeded (current: 5)"]
}
```

**Causes:**
1. Approach is fundamentally flawed
2. Missing architectural knowledge
3. Insufficient PRD detail
4. Complex problem requiring human insight

**Solution:**
```bash
# Block item for human review
python3 increment_iteration.py FIX-TEST-ASSERTIONS \
    --block-on-max \
    --reason "Max iterations (5) reached. Requires architectural redesign."
```

**Human action:**
- Review attempts[] history
- Identify recurring failure pattern
- Provide architectural guidance
- Reset current_iteration to 0 after fix

---

### Issue: Build Passes But Acceptance Fails

**Symptom:**
- Build: ✅
- Tests: ✅
- Acceptance criteria: ❌

**Debug:**
1. Read acceptance criteria carefully
2. Run verification commands manually
3. Check for edge cases
4. Review PRD non-functional requirements

**Common causes:**
- Misunderstood acceptance criteria
- Edge case not handled
- Performance regression
- Missing constraint enforcement

---

### Issue: Evidence Validation Fails

**Symptom:**
```bash
$ python3 validate_evidence.py FIX-TEST-ASSERTIONS
{
  "success": false,
  "issues": ["Attempt[0]: evidence missing 'results_summary'"]
}
```

**Solution:**
Re-record attempt with proper evidence:
```bash
python3 update_attempt.py FIX-TEST-ASSERTIONS PASSED \
    --commands "pio run" \
    --summary "Build passes with LED count 320"
```

---

## Best Practices

### DO:

✅ **Always run boot ritual first** (.claude/harness/init.sh)

✅ **Read PRD if it exists** (prd_reference.file)

✅ **Follow project constraints** (CENTER_ORIGIN, NO_RAINBOWS)

✅ **Record attempts immediately** after verification

✅ **Validate evidence** before changing status to PASSING

✅ **Use sub-skills** for complex tasks (TDD, subagent-driven-development)

✅ **Commit with evidence** on success

✅ **Revert code** (not harness) on failure

✅ **Stop after one item** (matches Ralph Loop pattern)

✅ **Check convergence criteria** rigorously

✅ **Increment iteration** on failure

### DON'T:

❌ **Skip boot ritual** (builds must pass first)

❌ **Work on multiple items** (one item per invocation)

❌ **Guess requirements** (read PRD or acceptance_criteria)

❌ **Change status without evidence** (validate first)

❌ **Revert harness changes** (only revert code)

❌ **Ignore max_iterations** (prevents infinite loops)

❌ **Violate constraints** (CENTER_ORIGIN, NO_RAINBOWS enforced)

❌ **Record attempts without summary** (required for PASSED/FAILED)

❌ **Continue after max iterations** (block for human review)

❌ **Commit without running verification** (acceptance must pass)

---

## Escape Hatches

### Override Reason

**Purpose:** Manual override for exceptional cases.

**Usage:**
```json
{
  "id": "SPECIAL-CASE",
  "status": "PASSING",
  "override_reason": "Manual verification by human. Build system unavailable.",
  "attempts": []
}
```

**When to use:**
- Human verification instead of automated tests
- Build system unavailable
- Acceptance criteria need revision

**Constraints:**
- Only User/INITIALIZER can add override_reason
- WORKER should not set override_reason (except via --block-on-max)

### Force Mode (Future)

**Planned feature:**
```bash
python3 harness.py validate --force
# Skip certain validation checks
```

**Use cases:**
- Emergency fixes
- Known validation failures
- Temporary workarounds

**Status:** Not yet implemented in harness.py

### Manual Feature List Edits

**Escape hatch:** Directly edit feature_list.json.

**When to use:**
- Correct invalid data
- Reset current_iteration manually
- Change ralph_loop.enabled

**Best practice:**
1. Backup feature_list.json first
2. Validate JSON structure
3. Run `python3 harness.py validate` after editing
4. Document reason in notes field

---

## Appendix: File Locations

```
.claude/
├── harness/
│   ├── init.sh                   # Boot ritual script
│   ├── feature_list.json         # Backlog with attempts[]
│   ├── HARNESS_RULES.md          # Protocol rules
│   ├── ARCHITECTURE.md           # System facts
│   └── CONSTRAINTS.md            # Hard limits
├── prd/
│   ├── schema.json               # PRD JSON schema
│   ├── README.md                 # PRD usage guide
│   └── *.json                    # Individual PRDs
└── skills/
    └── ralph-loop/
        ├── SKILL.md              # Main workflow doc (you are here)
        ├── scripts/
        │   ├── select_next_item.py
        │   ├── update_attempt.py
        │   ├── increment_iteration.py
        │   └── validate_evidence.py
        └── references/
            └── WORKER_MODE_GUIDE.md  # This file
```

---

## Appendix: Schema v2.1 Fields

### ralph_loop Object

```json
"ralph_loop": {
  "enabled": false,              // Enable Ralph Loop execution
  "max_iterations": 5,           // Safety limit
  "current_iteration": 0,        // Progress counter
  "convergence_criteria": {      // Success conditions
    "build_passes": true,        // Compilation succeeds
    "tests_pass": true,          // Tests return 0
    "acceptance_met": true       // Acceptance criteria verified
  }
}
```

### prd_reference Object

```json
"prd_reference": {
  "file": "FEAT-XXX.json",       // PRD filename in .claude/prd/
  "last_updated": "2026-01-07T00:00:00Z",
  "version": "1.0.0"             // PRD version
}
```

### attempts[] Array

```json
"attempts": [
  {
    "run_id": "worker-008",      // Worker identifier
    "timestamp": "ISO8601",      // Attempt time
    "result": "PASSED|FAILED|IN_PROGRESS",
    "evidence": {
      "commands_run": [...],     // Commands executed
      "results_summary": "..."   // Result summary
    },
    "commit": "abc1234",         // Git SHA (optional)
    "reverted": false            // Code reverted? (optional)
  }
]
```

---

**END OF GUIDE**

For questions or issues, refer to:
- `.claude/harness/HARNESS_RULES.md` - Protocol rules
- `.claude/skills/ralph-loop/SKILL.md` - Main workflow doc
- `.claude/prd/README.md` - PRD system guide
