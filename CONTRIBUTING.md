# Contributing to LightwaveOS

Guidelines for contributing to the LightwaveOS repository. All contributors -- human and agent -- must follow these rules.

---

## Repository Root Placement Rules

Only allowlisted entries may exist at the repository root. No exceptions without explicit approval.

### Root Allowlist

```
.git/
.github/
.claude/
.codex/
.gitignore
.pre-commit-config.yaml
AGENTS.md
CLAUDE.md
README.md
CONTRIBUTING.md
CHANGELOG.md
instructions/
firmware-v3/
tab5-encoder/
lightwave-ios-v2/
k1-composer/
```

Anything not on this list must be placed inside a subdirectory. If you believe a new root entry is needed, open a discussion before creating it.

The canonical allowlist is maintained at `instructions/path-allowlist-v1.txt`.

---

## Filename Policy

- All files and directories use **lowercase kebab-case** (e.g., `repo-governance-v1.md`, `tab5-encoder/`).
- **No spaces** in filenames or directory names.
- **No dots** in directory names (except hidden directories like `.github/`).
- **Exceptions**: Standard Git governance files use UPPERCASE: `CLAUDE.md`, `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`.

Full policy: `instructions/naming-policy-v1.md`.

---

## Changelog Fragments

Every change made by an agent requires a changelog fragment. Human contributors are strongly encouraged to do the same.

### Fragment Location

```
instructions/changelog/
```

### Fragment Naming

```
<YYYY-MM-DD>--<scope>--<slug>.md
```

Examples:
- `2026-02-27--firmware-v3--fix-rmt-spinlock-crash.md`
- `2026-02-27--repo--add-governance-scaffolding.md`
- `2026-02-27--k1-composer--update-effect-list.md`

### Fragment Content

Use the template at `instructions/changelog/_fragment-template.md`. Each fragment must include:
- Date and scope
- Change type (feature, bugfix, refactor, docs, chore)
- One-line summary
- Files changed
- Validation method

### Release Process

At release time, fragments in `instructions/changelog/` are merged into `CHANGELOG.md` and moved to `instructions/changelog/releases/`. Fragments are never deleted -- they are archived.

---

## PR Process

1. Create a feature branch from `main`.
2. Make your changes, following all placement and naming rules.
3. Add a changelog fragment for each logical change.
4. Open a pull request. The PR description should reference the changelog fragment(s).
5. At release, maintainers merge fragments into `CHANGELOG.md`.

---

## Agent Compliance

AI agents operating in this repository must:
1. Read and follow `instructions/repo-governance-v1.md` before making structural changes.
2. Never create root-level entries not on the allowlist.
3. Never delete or move files without explicit user instruction.
4. Always create a changelog fragment for their changes.
5. Respect the file preservation rules in `CLAUDE.md`.
