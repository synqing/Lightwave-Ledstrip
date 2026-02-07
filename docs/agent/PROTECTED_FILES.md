# Protected Files Policy

**This document defines absolute file preservation rules for all agents operating in this repository.**

---

## Core Principle

**You are a guest in this codebase. You do not own these files. You do not decide their value.**

Files that appear "unrelated" to your current task may be:
- Work-in-progress from other sessions
- Research that informed design decisions
- Prototypes that will be referenced later
- Documentation the user hasn't committed yet
- Test artifacts needed for regression testing

**You cannot know. Therefore you must preserve.**

---

## Prohibited Actions

### Direct Deletion
- `rm` / `rm -rf` on any file
- `git rm` on any file
- `git clean -f` or `git clean -fd`
- Any shell command that removes files

### Indirect Deletion (THE SILENT KILLER)
These actions cause file loss without explicit deletion:

| Action | Why It Causes Loss |
|--------|-------------------|
| Selective staging ("only staging relevant files") | Unstaged files get lost on branch switch or reset |
| Filtered stashing ("stashing unrelated changes") | Excluded files remain in limbo, easily overwritten |
| Labeling files as "unrelated" | Creates false justification for exclusion |
| "Cleaning up" before commit | Removes work product without authorisation |
| Cherry-picking "important" changes | Abandons everything else |

### Judgement Calls You Must NOT Make
- "This file isn't part of my task"
- "This looks like temporary/test/debug output"
- "This HTML file seems unrelated to the firmware"
- "I'll exclude these docs to keep the commit clean"
- "These mockups aren't needed for the audio backend"

---

## Protected Directories

The following directories contain work product that must NEVER be excluded from git operations:

| Directory | Contents | Protection Level |
|-----------|----------|------------------|
| `docs/` | All documentation, mockups, research | **ABSOLUTE** |
| `docs/ui-mockups/` | HTML design prototypes | **ABSOLUTE** |
| `lightwave-ios-v2/` | iOS app source | **ABSOLUTE** |
| `.claude/` | Agent configuration and skills | **ABSOLUTE** |
| `tools/` | Development utilities | **ABSOLUTE** |
| `reference/` | Reference implementations | **ABSOLUTE** |
| `firmware/v2/docs/` | Firmware documentation | **ABSOLUTE** |

**ABSOLUTE protection means: Never exclude from stash. Never omit from staging. Never delete.**

---

## Correct Git Operations

### Stashing (CORRECT)
```bash
# Include ALL files - tracked, modified, AND untracked
git stash --include-untracked -m "descriptive message"

# Or use --all to also include ignored files if needed
git stash --all -m "descriptive message"
```

### Stashing (WRONG - CAUSES FILE LOSS)
```bash
# WRONG: Only stashes tracked files, abandons untracked work
git stash -m "wip: unrelated local files"

# WRONG: Selective stashing that excludes "unrelated" files
git stash push -m "wip" -- specific/files/only
```

### Staging (CORRECT)
```bash
# Stage only what you modified as part of your task
git add path/to/files/you/changed

# If user asks to commit all changes
git add -A
```

### Staging (WRONG - CAUSES FILE LOSS)
```bash
# WRONG: Making judgement calls about what to stage
git add firmware/  # "I'll skip docs/ since it's unrelated"

# WRONG: Explicitly excluding files you deem unnecessary
git add . && git reset docs/ui-mockups/  # NO
```

---

## The 2026-02-06 Incident

An agent working on `codex/esv11-audio-backend` decided that documentation files were "unrelated" to its audio backend task. It created stashes with messages like:

- `wip: unrelated local files (ios/docs/research)`
- `wip: unrelated local changes (ui mockups/ios/zones/etc)`

These files were never committed to git and were lost:

| File | Size | Hours of Work |
|------|------|---------------|
| `docs/ui-mockups/LIGHTWAVE_DESIGN_SYSTEM_MOCKUP.html` | 73,798 bytes (1884 lines) | ~4 hours |
| `docs/ui-mockups/components/led-preview.html` | ~900 lines | ~2 hours |

The agent's "helpful" organisation destroyed work product that cannot be recovered from git.

**This is why these rules exist.**

---

## When You Encounter Unfamiliar Files

1. **Do not touch them.** They are not part of your task.
2. **Do not exclude them.** Include everything in stash/staging operations.
3. **Do not label them.** "Unrelated" is not a label you get to assign.
4. **Ask if uncertain.** The user can tell you what's safe to ignore.

---

## Enforcement

Agents that violate these rules will have caused irreversible damage to the user's work. There is no undo for uncommitted file deletion.

**When in doubt: preserve everything.**
