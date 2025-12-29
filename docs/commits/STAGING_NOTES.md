# Staging Notes and Special Considerations

## main.cpp Multi-Commit Handling

The file `v2/src/main.cpp` contains changes that belong to multiple commits:
- **Commit 4**: `adbg` CLI commands (lines ~508-545)
- **Commit 6**: `k1` CLI commands (lines ~917-944)  
- **Commit 10**: Logging migration (throughout file)

### Option 1: Manual Staging (Recommended)

Use `git add -p` (interactive patch mode) to stage specific hunks:

```bash
# Commit 4: Stage only adbg-related changes
git add -p v2/src/main.cpp
# Select: y for adbg includes, adbg CLI help text, adbg command handler
# Select: n for everything else (logging migration, k1 commands)

# Commit 6: Stage only k1-related changes  
git add -p v2/src/main.cpp
# Select: y for k1 includes, k1 CLI help text, k1 command handler
# Select: n for everything else (logging migration)

# Commit 10: Stage remaining logging migration
git add v2/src/main.cpp
```

### Option 2: Accept Mixed Commits

If manual staging is too complex, accept that:
- Commit 4 will include adbg CLI + some logging migration in main.cpp
- Commit 6 will include k1 CLI + remaining logging migration in main.cpp
- Commit 10 becomes a cleanup commit for any missed logging changes

This is less ideal but still functional.

### Option 3: Restructure Commits

Combine CLI commits:
- Commit 4: Audio debug verbosity (config + AudioActor/Capture changes only, no main.cpp)
- Commit 6: K1 debug infrastructure + CLI (includes main.cpp k1 commands)
- Commit 10: All logging migration (includes main.cpp logging + CLI help text updates)

This groups related changes but breaks the "one feature per commit" principle.

## Recommended Approach

Use **Option 1** (manual staging with `git add -p`) for the cleanest atomic commits. The staging script can be modified to pause for manual intervention at commits 4, 6, and 10.

## File Dependencies Validation

All file dependencies are correct:
- ✅ Commit 1: Foundation (no dependencies)
- ✅ Commits 2-3: Depend on Commit 1
- ✅ Commit 4: Depends on Commits 1-3
- ✅ Commit 5: Depends on Commit 1
- ✅ Commit 6: Depends on Commits 1, 5
- ✅ Commits 7-10: Depend on Commit 1

## Atomicity Check

All commits are atomic except for the main.cpp handling:
- Each commit represents a single logical change
- Dependencies are properly ordered
- No commit breaks the build (each builds on previous)

## Conventional Commit Compliance

All commit messages follow conventional commit format:
- ✅ Type: feat, refactor (correct)
- ✅ Scope: logging, audio, k1, renderer, hardware, network, main (appropriate)
- ✅ Subject: 50-char summary (compliant)
- ✅ Body: Detailed explanation (comprehensive)
- ✅ Footer: Dependencies, testing, refs (complete)

