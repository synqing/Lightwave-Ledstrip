# Repository Governance v1

Governance policy for the LightwaveOS repository structure, enforcement, and change management.

---

## Repository Structure Principles

1. **Flat root, deep products.** The repository root contains only governance files, configuration, and top-level product directories. All implementation lives inside product directories.

2. **Allowlist enforcement.** Only entries listed in `instructions/path-allowlist-v1.txt` may exist at the repository root. Any commit introducing a non-allowlisted root entry is non-compliant and must be corrected before merge.

3. **Product isolation.** Each product directory (`firmware-v3/`, `tab5-encoder/`, `lightwave-ios-v2/`, `k1-composer/`) is self-contained with its own build system, documentation, and tests.

4. **Governance lives in `instructions/`.** All repository policies, allowlists, naming rules, and changelog fragments reside under `instructions/`. This directory is the single source of truth for repository structure decisions.

---

## Allowlist Enforcement

The root allowlist (`instructions/path-allowlist-v1.txt`) is the canonical list of permitted root entries.

### Adding to the Allowlist

1. Open a discussion or issue explaining why a new root entry is necessary.
2. Get explicit approval from a maintainer.
3. Update `instructions/path-allowlist-v1.txt`.
4. Add a changelog fragment documenting the change.

### Removing from the Allowlist

Removal follows the archive policy below. Entries are never silently deleted.

---

## Archive Policy

**No deletion.** Files and directories are never deleted from the repository without explicit maintainer approval.

When content is no longer actively needed:
1. Move it to an external archive or a designated archive branch.
2. Record the move in a changelog fragment.
3. Update any references to the moved content.
4. The original path must not be reused for unrelated content.

This policy exists because past incidents have resulted in permanent loss of work product. See the incident record in `CLAUDE.md` for context.

---

## Agent Compliance Requirements

All AI agents operating in this repository must:

1. **Read governance before structural changes.** Before creating, moving, or renaming files at the repository root or in `instructions/`, agents must consult this document and the allowlist.

2. **Never create non-allowlisted root entries.** If an agent needs to create a file and is unsure where it belongs, it must ask the user.

3. **Never delete files.** Agents must not use `rm`, `git rm`, `git clean`, or any other destructive operation without explicit user instruction.

4. **Create changelog fragments.** Every agent-initiated change requires a fragment in `instructions/changelog/`.

5. **Preserve all working directory state.** When stashing or committing, agents must include all files -- not just the ones they consider "relevant" to their task.

6. **Respect naming policy.** All new files and directories must follow `instructions/naming-policy-v1.md`.

---

## Change Management Process

### Structural Changes (new root entries, directory moves, renames)

1. Propose the change with rationale.
2. Verify against the allowlist and naming policy.
3. Get maintainer approval.
4. Execute the change.
5. Create a changelog fragment.
6. Update governance documents if the allowlist or policies changed.

### Content Changes (code, documentation, configuration)

1. Make changes on a feature branch.
2. Create a changelog fragment.
3. Open a pull request.
4. Merge after review.

### Policy Changes (governance, allowlist, naming)

1. Open a discussion with the proposed change and rationale.
2. Get maintainer approval.
3. Create a new versioned policy file (e.g., `naming-policy-v2.md`).
4. Update references in `CONTRIBUTING.md` and other governance docs.
5. Create a changelog fragment.

Policy files are versioned (`-v1`, `-v2`, etc.) and old versions are retained for historical reference.
