# Naming Policy v1

File and directory naming conventions for the LightwaveOS repository.

---

## General Rules

1. **Lowercase kebab-case.** All files and directories use lowercase letters, numbers, and hyphens as separators.
   - Correct: `repo-governance-v1.md`, `tab5-encoder/`, `audio-gate-fix.md`
   - Incorrect: `Repo_Governance.md`, `Tab5Encoder/`, `audio gate fix.md`

2. **No spaces.** Spaces are never permitted in filenames or directory names.

3. **No dots in directory names.** Directory names must not contain dots, with the exception of hidden directories (`.git/`, `.github/`, `.claude/`, `.codex/`).

---

## Exceptions

Standard Git governance files use UPPERCASE names. These are the only permitted exceptions:

- `CLAUDE.md`
- `AGENTS.md`
- `README.md`
- `CONTRIBUTING.md`
- `CHANGELOG.md`

These filenames are universally recognised conventions in Git repositories and must retain their uppercase form.

---

## Product Directories

Top-level product directories use kebab-case and follow the pattern `<product-name>/`:

- `firmware-v3/`
- `tab5-encoder/`
- `lightwave-ios-v2/`
- `k1-composer/`

---

## Changelog Fragments

Fragment filenames follow a strict pattern:

```
<YYYY-MM-DD>--<scope>--<slug>.md
```

- Date: ISO 8601 format.
- Scope: One of `firmware-v3`, `tab5-encoder`, `lightwave-ios-v2`, `k1-composer`, `repo`.
- Slug: Kebab-case description of the change.
- Double hyphens (`--`) separate the three components.

Examples:
- `2026-02-27--firmware-v3--fix-rmt-spinlock-crash.md`
- `2026-02-27--repo--add-governance-scaffolding.md`

---

## Versioned Policy Files

Policy and governance documents include a version suffix:

```
<name>-v<N>.md
```

Examples: `repo-governance-v1.md`, `naming-policy-v1.md`, `path-allowlist-v1.txt`.

Old versions are retained for historical reference and are never deleted.
