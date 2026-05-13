---
scope: firmware-v3
change_type: docs
summary: Add VP validation protocol before visible output-path changes
---

- Added a non-subjective VP validation protocol covering buffer-ownership correction, silence-policy metadata, and colour-correction default changes.
- Anchored the workflow to serial capture taps, fixed capture suites, K1_Testbed's limited pre-hardware role, and explicit pass/fail gates.
- Updated `BACKLOG.md` so the VP follow-up now points at the protocol instead of an open drafting task.

## Validation
- `git diff --check -- BACKLOG.md firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md instructions/changelog/2026-05-06--firmware-v3--vp-validation-protocol.md`
- `rg -n "VP validation protocol|buffer-ownership correction|K1_Testbed|serial capture" BACKLOG.md firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md`
