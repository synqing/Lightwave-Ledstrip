# Work Blocking Protocol v1

Policy for containing critical discovered work without derailing the current mission.

---

## Definition

A **Work Block** is a critical task, fix, investigation, or accountability gap discovered while executing the current mission that:

1. is material to correctness, safety, product quality, governance, or future agent reliability;
2. is outside the approved scope of the current mission;
3. needs concrete ownership, scope, and success/failure conditions; and
4. would otherwise pull the session into a rabbit hole.

A Work Block is not a casual TODO, optional improvement, or speculative idea. It is a containment mechanism for important work that must be preserved without silently changing the current mission.

---

## Mandatory Agent Behaviour

When an agent discovers a candidate Work Block, it MUST:

1. **Surface it plainly to Captain.** State the discovered issue, why it matters, and why it is outside the current mission.
2. **Push for concrete scope.** Establish the task boundary, success conditions, failure conditions, and what evidence would close the block.
3. **Record it in `BACKLOG.md`.** Forward work belongs in the backlog, not hidden handover files or loose chat prose.
4. **Preserve the current mission.** Once the Work Block is logged, return to the original mission unless Captain explicitly re-scopes the session.
5. **Label tactical output correctly.** If the current mission depends on the unresolved Work Block, use the RBDO Gate (`GROUNDED` / `DEGRADED-MODE` / `REFUSED`) honestly.

The default outcome is: log the Work Block, hand it to another agent/team, then continue the original work.

---

## Required Backlog Fields

Every Work Block entry in `BACKLOG.md` must include:

- **Problem statement**
- **Trigger / evidence**
- **Scope**
- **Non-goals**
- **Success conditions**
- **Failure conditions**
- **Owner / pickup mode**
- **Resume rule for the original mission**

Unknown is allowed at first capture, but unknown fields must not be promoted into product truth or design decisions.

---

## Hard Stops

Do not continue the original mission if the discovered Work Block means continuing would violate:

- a K1 hard constraint;
- RBDO hard stops in `CLAUDE.md`;
- hardware-test-before-commit discipline;
- AP-only constraints;
- render-path heap/timing constraints;
- audit-chain integrity; or
- an explicit Captain stop/re-scope decision.

In those cases, classify the output as `REFUSED` or `DEGRADED-MODE` per the RBDO Gate and ask Captain for the next decision.

---

## Anti-Patterns

Do not:

- bury a Work Block in prose without a backlog entry;
- keep investigating the block after Captain has not approved a scope change;
- relabel speculative ideas as Work Blocks;
- write forward task lists in `.claude/` handover files;
- claim the original mission is complete when it is only blocked, degraded, or diverted; or
- treat trace/log evidence as visual or product sign-off.
