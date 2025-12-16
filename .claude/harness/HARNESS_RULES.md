# Domain Memory Harness: Protocol Rules

This document defines the rules for operating on domain memory files. Agents MUST read this before modifying any harness files.

---

## Core Principle: Guidance, Not Gatekeeping

The harness helps agents work correctly. It does NOT block agents from working.

- **CRITICAL** issues block (data loss, corruption risk)
- **WARNING** issues log but don't block
- **INFO** issues suggest but don't enforce

---

## 1. File Ownership & Mutation Rules

### `feature_list.json` - The Backlog

| Operation | Who Can Do It | When | Rules |
|-----------|---------------|------|-------|
| **ADD item** | INITIALIZER, User | New work identified | Must have: id, title, status=FAILING, acceptance_criteria |
| **UPDATE status** | WORKER | After verification | Must link to commit hash, must have evidence |
| **ADD attempt** | WORKER | After any implementation try | Append to attempts[], never replace |
| **UPDATE evidence** | WORKER | After verification run | Timestamp required |
| **MODIFY acceptance_criteria** | User only | Requirements change | Log reason in notes |
| **DELETE item** | Never | - | Mark as CANCELLED with reason instead |

**ID Generation**: `{TYPE}-{NUMBER}` (e.g., `FIX-001`, `FEAT-002`, `VERIFY-003`)

**Immutable Fields** (after creation):
- `id`
- `created_at` (if present)

**Status Values**:
```
FAILING → PASSING   (verification succeeded)
FAILING → BLOCKED   (external dependency, max attempts reached)
PASSING → FAILING   (regression detected)
BLOCKED → FAILING   (blocker resolved, ready to retry)
Any → CANCELLED     (no longer needed)
```

### `agent-progress.md` - The Run Log

| Operation | Who Can Do It | Rules |
|-----------|---------------|-------|
| **APPEND run entry** | Any agent | Every run, no exceptions |
| **MODIFY past entries** | Never | Add correction in new entry instead |
| **ADD to Lessons Learned** | WORKER | When failure reveals reusable knowledge |

**Run Entry Format**:
```markdown
## Run {NNN} - {MODE} MODE

**Timestamp:** {ISO-8601}
**Run ID:** {mode}-{number}
**Mode:** INITIALIZER | WORKER

### Selected Item
{item ID} or N/A

### Actions Taken
- {bullet list}

### Verification Results
- {pass/fail with evidence}

### Commit
`{hash}` - {message} | or "none"

### Next Recommended Action
{one sentence}

---
```

### `ARCHITECTURE.md` - System Facts

| Operation | Who Can Do It | When |
|-----------|---------------|------|
| **UPDATE values** | WORKER | When code structure actually changes |
| **ADD section** | WORKER | New subsystem implemented |

**Never**: Speculate about future architecture. Only document what exists.

### `CONSTRAINTS.md` - Hard Limits

| Operation | Who Can Do It | When | Rules |
|-----------|---------------|------|-------|
| **UPDATE value** | WORKER | Verified wrong with evidence | Must cite source |
| **ADD constraint** | WORKER | Discovered through failure | Must link to run ID |

### `CLAUDE.md` - Agent Instructions

| Operation | Who Can Do It | When |
|-----------|---------------|------|
| **UPDATE** | After harness changes | Keep in sync with harness files |
| **ADD section** | New capability added | |

---

## 2. State Machine: Feature Status

```
                    ┌─────────────┐
                    │   FAILING   │◄──────────────────┐
                    └──────┬──────┘                   │
                           │                          │
            ┌──────────────┼──────────────┐           │
            │              │              │           │
            ▼              ▼              ▼           │
    ┌───────────┐  ┌───────────┐  ┌───────────┐      │
    │  PASSING  │  │  BLOCKED  │  │ CANCELLED │      │
    └─────┬─────┘  └─────┬─────┘  └───────────┘      │
          │              │                            │
          │              │ (blocker resolved)         │
          │              └────────────────────────────┤
          │                                           │
          │ (regression)                              │
          └───────────────────────────────────────────┘
```

**Valid Transitions**:
- `FAILING → PASSING`: Verification succeeded (requires evidence)
- `FAILING → BLOCKED`: External blocker OR max attempts reached (requires reason)
- `FAILING → CANCELLED`: No longer needed (requires reason)
- `PASSING → FAILING`: Regression detected (requires evidence)
- `BLOCKED → FAILING`: Blocker resolved, ready to retry
- `BLOCKED → CANCELLED`: Decided not to pursue

**Invalid Transitions** (CRITICAL - block):
- `PASSING → BLOCKED`: Doesn't make sense
- `CANCELLED → *`: Cancelled items stay cancelled
- Any transition without required evidence/reason

---

## 3. Revert Protocol (On Failure)

When a WORKER run fails verification:

### Step 1: Record the Attempt
```json
{
  "run_id": "worker-XXX",
  "timestamp": "ISO-8601",
  "result": "FAILED",
  "failure_reason": "specific reason",
  "investigation": "what was tried, why it failed, what to try next",
  "files_changed": ["list", "of", "files"],
  "commit": "abc123 (will be reverted)",
  "reverted": true
}
```

### Step 2: Revert Code Changes
```bash
git revert --no-commit HEAD  # Revert the code changes
git commit -m "Revert: {feature-id} - {reason}"
```

### Step 3: Keep Harness Updates
- **Keep**: The attempt entry in `feature_list.json`
- **Keep**: The run entry in `agent-progress.md`
- **Revert**: Status change (status stays FAILING)

### Step 4: Log for Next Run
In the attempt's `investigation` field, document:
1. What was tried
2. Why it failed
3. Hypothesis for next attempt

---

## 4. Concurrency Protocol

### Lock Acquisition

Before modifying `feature_list.json`:

1. Check for `.harness.lock` file
2. If exists and < 30 minutes old: **WARNING** - another agent may be working
3. If exists and > 30 minutes old: Stale lock, safe to break
4. Create lock: `{"agent_id": "...", "timestamp": "...", "feature_id": "..."}`

### Lock Release

After committing changes:
1. Delete `.harness.lock`

### Conflict Detection

If two agents modify same item:
1. Git merge conflict will occur
2. **WARNING**: Manual resolution required
3. Preserve both attempts in `attempts[]` array
4. Human decides which status is correct

### Escape Hatch

```bash
# Force break a lock (use sparingly)
rm .harness.lock
```

Or in harness.py:
```python
harness.acquire_lock(force=True)
```

---

## 5. Dependency Rules

### Soft Warning (Not Blocking)

When working on item with FAILING dependencies:

```
⚠️ WARNING: Dependencies not met
   - DEP-001: FAILING
   - DEP-002: FAILING

   Proceeding anyway. Results may be incomplete.
```

Agent may proceed but should:
1. Note the warning in attempt record
2. Consider if work is useful without dependencies
3. May mark item BLOCKED if truly blocked

### Circular Dependency (CRITICAL - Block)

If A depends on B and B depends on A:
```
🚫 CRITICAL: Circular dependency detected
   A → B → A

   Cannot proceed. Human intervention required.
```

---

## 6. Validation Rules

### On Read (feature_list.json)

| Check | Severity | Action |
|-------|----------|--------|
| JSON parse fails | CRITICAL | Abort, report corruption |
| Missing required fields | WARNING | Log, use defaults |
| Unknown fields | INFO | Ignore |
| Invalid status value | WARNING | Treat as FAILING |
| Orphan dependency reference | WARNING | Log, ignore dependency |

### On Write (feature_list.json)

| Check | Severity | Action |
|-------|----------|--------|
| Status → PASSING without evidence | CRITICAL | Block write |
| Status → BLOCKED without reason | CRITICAL | Block write |
| Duplicate item ID | CRITICAL | Block write |
| Invalid JSON | CRITICAL | Block write |
| Missing optional fields | INFO | Add defaults |

---

## 7. Escape Hatches

### Override Validation

In `feature_list.json` item:
```json
{
  "id": "FEAT-001",
  "override_reason": "Manually verified by human, CI not available",
  "status": "PASSING"
}
```

### Force Commands

```bash
# Skip all validation (use sparingly)
python harness.py --force validate

# Break stale lock
python harness.py --force unlock

# Work on blocked item anyway
python harness.py --ignore-dependencies next
```

### Manual Recovery

If harness files are corrupted:
1. `git log --oneline feature_list.json` - find last good state
2. `git checkout {hash} -- feature_list.json` - restore
3. Add recovery note to `agent-progress.md`

---

## 8. Run ID Format

```
{mode}-{sequential-number}

Examples:
  init-001      First initializer run
  worker-002    Second worker run
  worker-003    Third worker run
```

Sequential across all runs, not per-mode.

To find next ID:
```bash
grep -E "^## Run [0-9]+" agent-progress.md | tail -1
# Extract number, increment
```

---

## 9. Bootstrapping (First Run Detection)

Agent checks for harness:

```python
if not exists("feature_list.json"):
    mode = "INITIALIZER"
elif feature_list_is_empty():
    mode = "INITIALIZER"
else:
    mode = "WORKER"
```

INITIALIZER creates:
- `feature_list.json` with initial items
- `agent-progress.md` with first entry
- `init.sh` boot script

---

## 10. Quick Reference: Severity Levels

| Level | Symbol | Meaning | Agent Action |
|-------|--------|---------|--------------|
| CRITICAL | 🚫 | Data loss or corruption risk | MUST stop, cannot proceed |
| WARNING | ⚠️ | Potential issue | Log and continue, note in attempt |
| INFO | ℹ️ | Suggestion | Consider, may ignore |

---

## Summary: The Golden Rules

1. **Never delete** - Mark as CANCELLED instead
2. **Never modify history** - Append corrections
3. **Always record attempts** - Even failed ones
4. **Always cite evidence** - Status changes need proof
5. **Warn, don't block** - Unless data loss risk
6. **Leave escape hatches** - Edge cases exist
7. **Document overrides** - If you bypass a rule, say why
