# K1 Agent Prompt Packs

**Status:** ACTIVE - created 2026-05-06.

Reusable first-contact prompts for recurring LightwaveOS agent tasks. These are templates, not forward task lists. Forward work remains in `BACKLOG.md`.

## Packs

| Pack | Use When |
|---|---|
| `effect-repair.md` | Repairing, tuning, or reviewing one or more firmware effects. |
| `vp-audit.md` | Auditing visual-pipeline layers, buffer ownership, colour correction, silence policy, or output order. |
| `hardware-validation.md` | Running K1 hardware validation, serial capture, upload, or visual sign-off preparation. |
| `heap-recovery.md` | Investigating K1v2 SRAM/PSRAM pressure, low-heap shedding, or memory guards. |
| `api-change.md` | Adding or modifying REST, WebSocket, or SerialJSON protocol behaviour. |
| `docs-only-research.md` | Running read-only source-backed research or multi-pass audits. |

## Shared Blocks

| Shared File | Purpose |
|---|---|
| `_shared/startup-readback.md` | Mandatory repo readback shape. |
| `_shared/anti-drift-rules.md` | Drift prevention rules for handoff, source truth, and Captain decisions. |
| `_shared/source-anchor-checklist.md` | Evidence checklist before claims or edits. |
| `_shared/return-contract.md` | Required SSA return format. |

## Rule

Every pack starts from current source truth. Do not paste one of these into an agent and allow it to act from chat memory alone. The receiving agent must read the named files and verify branch, dirty tree, and current runtime constraints before changing anything.
