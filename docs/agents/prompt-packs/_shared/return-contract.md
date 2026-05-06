# Shared SSA Return Contract

Every SSA returns:

```text
GROUNDED / DEGRADED-MODE / REFUSED:

Scope:
- What was investigated or changed.

Files Read:
- path:line anchors for important evidence.

Files Changed:
- path list, or "none".

Findings:
- Source-backed findings only.

Validation:
- Commands run and results, or why no validation applies.

Risks:
- Remaining uncertainty, unresolved assumptions, and revisit triggers.

Next Action:
- One concrete recommended step, or "none".
```

For code-edit SSAs, also list:

- exact write ownership;
- whether unrelated dirty files were present;
- whether any claimed diff was integrated back to the main workspace.
