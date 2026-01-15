# Contributing to LightwaveOS

Welcome! This guide explains how to contribute to LightwaveOS using the domain memory harness and Ralph Loop autonomous workflow system.

**Table of Contents:**
- [Getting Started](#getting-started)
- [Workflow Modes](#workflow-modes)
- [Manual WORKER Mode](#manual-worker-mode)
- [Ralph Loop Mode](#ralph-loop-mode)
- [PRD System](#prd-system)
- [Verification](#verification)
- [Git Workflow](#git-workflow)
- [Troubleshooting](#troubleshooting)

---

## Getting Started

### Prerequisites

**Required:**
- PlatformIO (CLI or VS Code extension)
- Python 3.7+ (for harness scripts)
- Git

**Optional:**
- `jsonschema` Python package (for PRD validation): `pip install jsonschema`

### Initial Setup

1. **Clone the repository:**
   ```bash
   git clone <repo-url>
   cd Lightwave-Ledstrip
   ```

2. **Run boot ritual:**
   ```bash
   cd .claude/harness && ./init.sh
   ```

   This verifies:
   - Git repository health
   - PlatformIO toolchain
   - Default build compiles

3. **Review harness files:**
   ```bash
   cat .claude/harness/HARNESS_RULES.md      # Protocol rules
   cat .claude/harness/ARCHITECTURE.md       # System facts
   cat .claude/harness/CONSTRAINTS.md        # Hard limits
   cat .claude/harness/feature_list.json     # Current backlog
   ```

---

## Workflow Modes

LightwaveOS supports two contribution workflows:

### Manual WORKER Mode
**Best for:** Human contributors, learning the system, complex design decisions

**Characteristics:**
- Manual item selection from backlog
- Human decision-making at each step
- Full control over implementation approach
- Iterative with human review

### Ralph Loop Mode
**Best for:** AI agents, repetitive tasks, well-defined requirements

**Characteristics:**
- Autonomous item selection (highest priority FAILING)
- Automatic iteration tracking
- Convergence detection (build_passes + tests_pass + acceptance_met)
- ONE item per invocation (controlled iteration)

Choose the mode that fits your working style and the complexity of the task.

---

## Manual WORKER Mode

### Step-by-Step Workflow

#### 1. Boot Ritual (MANDATORY)

```bash
cd .claude/harness && ./init.sh
```

**If fails:** Fix build issues BEFORE selecting items.

#### 2. Select Item

**Read backlog:**
```bash
cat .claude/harness/feature_list.json | jq '.items[] | select(.status=="FAILING")'
```

**Choose ONE item** based on:
- Priority (lower number = higher priority)
- Dependencies satisfied
- Your expertise area

#### 3. Read Requirements

**From feature_list.json:**
- `acceptance_criteria`: How to verify success
- `verification`: Commands to run
- `dependencies`: Items that must be PASSING first

**From PRD (if `prd_reference` exists):**
```bash
cat .claude/prd/<file>.json
```

Extract:
- Functional requirements
- Acceptance criteria
- Constraints (CENTER_ORIGIN, NO_RAINBOWS, etc.)

#### 4. Implement

**Follow project constraints:**
- **CENTER ORIGIN**: All effects originate from LED 79/80
- **NO_RAINBOWS**: No rainbow color cycling
- **Performance**: Target 120 FPS
- **Memory**: Minimize heap allocations in render loops

**Use project skills:**
- `/test-driven-development` - For new features
- `/software-architecture` - For design decisions
- `/subagent-driven-development` - For complex changes

#### 5. Verify

**Run acceptance criteria:**
```bash
# Example from verification commands
pio run -e esp32dev_audio
grep NUM_LEDS src/config/hardware_config.h
```

**Check convergence:**
- ✅ Build passes (exit code 0)
- ✅ Tests pass (exit code 0)
- ✅ Acceptance criteria met (all commands succeed)

#### 6. Record Attempt

**Manual approach (edit feature_list.json):**

Add to `attempts[]` array:
```json
{
  "run_id": "manual-001",
  "timestamp": "2026-01-07T12:00:00Z",
  "result": "PASSED",
  "evidence": {
    "commands_run": ["pio run -e esp32dev_audio", "grep NUM_LEDS"],
    "results_summary": "Build passes, LED count correct"
  },
  "commit": "abc1234",
  "reverted": false
}
```

**Script approach (recommended):**
```bash
python3 .claude/skills/ralph-loop/scripts/update_attempt.py \
    FIX-TEST-ASSERTIONS \
    PASSED \
    --commands "pio run,grep NUM_LEDS" \
    --summary "Build passes, LED count assertion updated 81→320"
```

#### 7. Update Status

**On SUCCESS:**
1. Change `status` to `PASSING` in feature_list.json
2. Commit code + harness changes

**On FAILURE:**
1. Keep `status` as `FAILING`
2. Revert code changes (keep harness)
3. Document investigation for next attempt

#### 8. Commit

```bash
git add -A
git commit -m "feat(tests): Fix LED count assertion 81→320

Updated test_main.cpp to reflect dual 160-LED strip configuration.
Palette count assertion (33) was already correct.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
```

#### 9. Push (After Verification)

```bash
# Run pre-push quality gate
scripts/verify.sh

# If passes:
git push origin <branch>
```

---

## Ralph Loop Mode

### Overview

Ralph Loop is an autonomous workflow executor that processes backlog items one at a time with automatic iteration tracking and convergence detection.

**When to use:**
- Well-defined requirements (PRD exists)
- Clear acceptance criteria
- Repetitive implementation tasks
- Multiple similar items in backlog

**When NOT to use:**
- Architectural decisions required
- Requirements unclear
- Design exploration needed
- First-time feature (create PRD first)

### Invoking Ralph Loop

**Via Skill:**
```bash
# In Claude Code CLI
/ralph-loop
```

**Via Skill Tool:**
Use the `Skill` tool with skill="ralph-loop"

### What Ralph Loop Does

```
1. Boot ritual          → .claude/harness/init.sh
2. Select item          → Highest priority FAILING with ralph_loop.enabled=true
3. Read PRD             → Load requirements from .claude/prd/<file>.json
4. Implement            → Follow constraints, use sub-skills
5. Verify               → Run acceptance criteria, check convergence
6. Record attempt       → Append to attempts[] with evidence
7. Update status/iter   → PASSING (converged) or increment iteration
8. Commit/Revert        → Commit on success, revert on failure
9. STOP                 → ONE item per invocation
```

### Enabling Ralph Loop

**In feature_list.json:**
```json
{
  "id": "FEAT-XXX",
  "status": "FAILING",
  "ralph_loop": {
    "enabled": true,              // Enable autonomous execution
    "max_iterations": 5,          // Safety limit
    "current_iteration": 0,       // Progress counter
    "convergence_criteria": {
      "build_passes": true,
      "tests_pass": true,
      "acceptance_met": true
    }
  },
  "prd_reference": {
    "file": "FEAT-XXX.json",      // Link to PRD
    "last_updated": "2026-01-07T00:00:00Z",
    "version": "1.0.0"
  }
}
```

### Convergence Detection

Ralph Loop stops when:
1. **Converged:** Status changes to PASSING (success)
2. **Max iterations:** current_iteration >= max_iterations (blocked)
3. **No eligible items:** All items PASSING or disabled

### Iteration Safety

**Lifecycle:**
```
Iteration 0 → Attempt 1 → FAILED → Increment → Iteration 1
Iteration 1 → Attempt 2 → FAILED → Increment → Iteration 2
Iteration 2 → Attempt 3 → PASSED → Status: PASSING (converged)
```

**Max iterations safety:**
- Default: 5 iterations
- Prevents infinite loops
- Flags items needing human review
- After BLOCKED: Human reviews, resets current_iteration to 0

### Ralph Loop Scripts

Located in `.claude/skills/ralph-loop/scripts/`:

#### select_next_item.py
```bash
# Select highest priority eligible item
python3 select_next_item.py

# Output: JSON with selected item or null
```

#### update_attempt.py
```bash
# Record attempt with evidence
python3 update_attempt.py ITEM_ID RESULT \
    --commands "cmd1,cmd2" \
    --summary "Results summary"
```

#### increment_iteration.py
```bash
# Increment iteration counter
python3 increment_iteration.py ITEM_ID

# Block on max iterations
python3 increment_iteration.py ITEM_ID \
    --block-on-max \
    --reason "Max iterations reached"
```

#### validate_evidence.py
```bash
# Validate evidence structure
python3 validate_evidence.py          # All PASSING items
python3 validate_evidence.py ITEM_ID  # Specific item
python3 validate_evidence.py --strict # Warnings as errors
```

---

## PRD System

### What is a PRD?

Product Requirements Document - authoritative specification for a feature.

**Purpose:**
- Provide clear requirements for implementation
- Enable autonomous execution (Ralph Loop)
- Document acceptance criteria
- Specify constraints

### Creating a PRD

1. **Create JSON file:**
   ```bash
   cat > .claude/prd/FEAT-XXX.json << 'EOF'
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
       "AC-1: Build compiles...",
       "AC-2: Tests pass..."
     ],
     "constraints": ["CENTER_ORIGIN", "NO_RAINBOWS"],
     "version": "1.0.0",
     "last_updated": "2026-01-07T00:00:00Z"
   }
   EOF
   ```

2. **Validate PRD:**
   ```bash
   python3 -c "import jsonschema, json
   schema = json.load(open('.claude/prd/schema.json'))
   prd = json.load(open('.claude/prd/FEAT-XXX.json'))
   jsonschema.validate(prd, schema)
   print('PRD valid!')"
   ```

3. **Link to feature item:**
   ```json
   {
     "id": "FEAT-XXX",
     "prd_reference": {
       "file": "FEAT-XXX.json",
       "last_updated": "2026-01-07T00:00:00Z",
       "version": "1.0.0"
     }
   }
   ```

### PRD Structure

**Required fields:**
- `feature_id`: Must match feature_list.json item ID
- `title`: Human-readable feature name
- `overview`: 2-4 sentence summary
- `functional_requirements`: What the feature must do
- `acceptance_criteria`: How to verify success

**Optional fields:**
- `nonfunctional_requirements`: Performance, memory, timing
- `constraints`: Hard limits (CENTER_ORIGIN, NO_RAINBOWS, etc.)
- `version`: Semver version
- `last_updated`: ISO8601 timestamp

### Updating PRDs

**When to update:**
- Requirements change
- Acceptance criteria refined
- Constraints added/removed

**How to update:**
1. Edit PRD JSON file
2. Bump `version` (semver)
3. Update `last_updated` timestamp
4. Update `prd_reference` in feature_list.json

---

## Verification

### Boot Ritual (`init.sh`)
**Purpose:** Quick health check (~10-60s)

```bash
cd .claude/harness && ./init.sh
```

**Use before:**
- Starting work
- After pulling changes
- After modifying dependencies

### Pre-Push Quality Gate (`verify.sh`)
**Purpose:** Comprehensive validation (~2-5 mins)

```bash
scripts/verify.sh                # Full (includes builds)
scripts/verify.sh --skip-build   # Quick (skip builds)
```

**6 Check Categories:**
1. Harness schema validation
2. PRD JSON schema validation
3. Cross-reference integrity
4. Pattern compliance (NO_RAINBOWS, CENTER_ORIGIN)
5. Build verification
6. Git status

**Use before:**
- `git push`
- Creating pull requests
- Marking items PASSING

### Git Hook (Optional)

```bash
# Install pre-push hook
cat > .git/hooks/pre-push << 'EOF'
#!/bin/bash
scripts/verify.sh
EOF
chmod +x .git/hooks/pre-push
```

---

## Git Workflow

### Branch Strategy

**main branch:**
- Production-ready code
- All tests pass
- verify.sh passes

**feature branches:**
- One branch per feature item
- Named: `feature/<item-id>-short-description`
- Example: `feature/FEAT-001-dual-encoder-support`

### Commit Messages

**Format:**
```
<type>(<scope>): <short summary>

<detailed description>

<evidence/verification>

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `refactor`: Code restructuring
- `docs`: Documentation only
- `test`: Adding/modifying tests
- `perf`: Performance improvements

**Examples:**
```
feat(audio): Add Goertzel-based beat detection

Implemented TempoTracker with onset detection and tempo estimation.
Replaces K1 beat tracker with lighter-weight solution.

Verification:
- pio run -e esp32dev_audio (SUCCESS)
- Audio effects respond to beat
- RAM usage: 18.2% (down from 19.5%)

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

### Reverting Changes

**On failed attempt:**

```bash
# Revert code changes (keep harness)
git checkout -- src/ test/ data/
# Do NOT revert .claude/harness/feature_list.json

# Or use git revert
git revert --no-commit HEAD
git commit -m "revert: FEAT-XXX - <reason>"
```

---

## Troubleshooting

### Issue: Boot Ritual Fails

**Symptom:**
```
[FAIL] Build verification failed
```

**Solution:**
1. Check PlatformIO: `pio --version`
2. Manual build: `pio run -e esp32dev`
3. Check error messages
4. Fix build BEFORE selecting items

### Issue: No Eligible Ralph Loop Items

**Symptom:**
```json
{
  "success": true,
  "selected": null,
  "message": "No eligible items for Ralph Loop execution"
}
```

**Causes:**
- All items PASSING (backlog complete!)
- No items have `ralph_loop.enabled=true`
- Dependencies blocking

**Solution:**
- Check backlog status: `cat feature_list.json | jq '.items[] | {id, status, enabled: .ralph_loop.enabled}'`
- Enable Ralph Loop on items if needed
- Fix dependencies if blocking

### Issue: Max Iterations Exceeded

**Symptom:**
```
WARNING: Max iterations (5) reached or exceeded (current: 5)
```

**Causes:**
- Approach fundamentally flawed
- Missing architectural knowledge
- Insufficient PRD detail
- Complex problem needing human insight

**Solution:**
1. Review attempts[] history
2. Identify recurring failure pattern
3. Human provides architectural guidance
4. Reset current_iteration to 0
5. Update PRD if needed

### Issue: Evidence Validation Fails

**Symptom:**
```json
{
  "success": false,
  "issues": ["Attempt[0]: evidence missing 'results_summary'"]
}
```

**Solution:**
Re-record attempt with proper evidence:
```bash
python3 .claude/skills/ralph-loop/scripts/update_attempt.py \
    ITEM_ID PASSED \
    --commands "cmd1,cmd2" \
    --summary "Complete description of results"
```

### Issue: PRD Validation Fails

**Symptom:**
```
jsonschema.exceptions.ValidationError: 'feature_id' is a required property
```

**Solution:**
1. Check PRD against schema: `.claude/prd/schema.json`
2. Ensure all required fields present
3. Fix JSON syntax
4. Re-validate

### Issue: Circular Dependencies

**Symptom:**
```
CRITICAL: Circular dependency detected
A → B → A
```

**Solution:**
1. Review dependency graph
2. Break circular dependency
3. Refactor items if needed
4. Human intervention required

---

## Additional Resources

**Key Files:**
- `.claude/harness/HARNESS_RULES.md` - Protocol rules
- `.claude/harness/ARCHITECTURE.md` - System facts
- `.claude/harness/CONSTRAINTS.md` - Hard limits
- `.claude/skills/ralph-loop/SKILL.md` - Ralph Loop workflow
- `.claude/skills/ralph-loop/references/WORKER_MODE_GUIDE.md` - Detailed reference
- `.claude/prd/README.md` - PRD system guide
- `CLAUDE.md` - Agent instructions

**Scripts:**
- `.claude/harness/init.sh` - Boot ritual
- `scripts/verify.sh` - Pre-push quality gate
- `.claude/skills/ralph-loop/scripts/*.py` - Ralph Loop utilities

**Getting Help:**
- Review `CLAUDE.md` for project constraints
- Check `HARNESS_RULES.md` for protocol
- Read skill documentation in `.claude/skills/`
- Consult agent inventory in `.claude/agents/README.md`

---

## Summary: The Golden Rules

1. **Always run boot ritual first** - Ensure build health
2. **ONE item at a time** - Focus on single feature
3. **Follow project constraints** - CENTER_ORIGIN, NO_RAINBOWS
4. **Record all attempts** - Even failed ones (append-only)
5. **Validate before PASSING** - Evidence required
6. **Verify before pushing** - Run scripts/verify.sh
7. **Commit with evidence** - Include verification results
8. **Revert code, not harness** - Keep attempt history
9. **Document overrides** - If bypassing rules
10. **Leave escape hatches** - Edge cases exist

---

**Welcome to LightwaveOS development! When in doubt, refer to this guide and the harness documentation. Happy contributing! 🚀**
