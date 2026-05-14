# Task Plan: Git Provenance Audit

## Goal
Identify how the named authors and `thedotmack`-related material entered this repository history, using local git evidence first and remote evidence where available.

## Phases
- [x] Capture local repo/remotes/branch context.
- [x] Cluster git authors and commit counts.
- [x] Trace named identities and `thedotmack` references through commits, files, and refs.
- [x] Inspect remote metadata where local evidence points to a hosted repo.
- [x] Summarise with commit hashes, dates, file paths, and uncertainty boundaries.

## Constraints
- Read-only audit of project source/history.
- Do not modify unrelated working-tree changes.
- Do not infer real-world identity beyond git metadata and repository artefacts.

## Decision
The named identities are not in the Lightwave `HEAD`/`origin` history. They are present because this checkout has an extra remote named `synqing` that fetches `https://github.com/synqing/claude-mem.git` into `refs/remotes/synqing/*`.
