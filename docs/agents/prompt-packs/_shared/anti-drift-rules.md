# Shared Anti-Drift Rules

1. Read `CLAUDE.md` top RBDO gate before tactical output.
2. Do not write `.claude/handoff*.md` forward task lists.
3. Forward work belongs in `BACKLOG.md`; durable decisions belong in `docs/adr/`.
4. Verify branch, HEAD, and dirty tree before edits.
5. Grep/read the target before applying a patch; abort if the content already exists.
6. Do not rely on stale handovers when HEAD has moved.
7. Do not change firmware behaviour without the validation gate named by the task.
8. For C++ symbol work, use clangd first where available.
9. For REST/WS changes, update protocol YAML first.
10. For visible K1 behaviour, separate metric/trace evidence from Captain visual judgement.
11. Do not enable STA, pure-STA validation, or WiFi mode rewrites unless Captain explicitly opened that task.
12. British English in docs, comments, logs, and UI strings.
