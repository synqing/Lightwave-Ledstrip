---
abstract: "Lightweight architecture decision ledger for durable K1 decisions that are too important to leave in chat but too small for a full ADR."
---

# Lightweight Architecture Decision Ledger

**Status:** ACTIVE - created 2026-05-06.

**Purpose:** keep durable architecture decisions discoverable without turning `BACKLOG.md` into a history book. Forward tasks stay in `BACKLOG.md`; decisions that should prevent future agent drift live here.

**Pattern source:** `docs/adr/zone-composer-architecture-decisions.md:1-27` establishes the local ADR habit of recording status, context, decision, alternatives, reversibility, gates, implementation impact, and test impact.

## Ledger Rules

1. Add a row when a decision changes architecture behaviour, validates/parks a subsystem, or prevents recurring agent drift.
2. Every row needs source anchors.
3. Every row needs a revisit trigger.
4. If implementation changed, cite the changelog plus source/research evidence.
5. If product behaviour is dormant or parked, say that directly.
6. Keep forward tasks in `BACKLOG.md`; keep durable decision state here.
7. For visible firmware behaviour, require hardware evidence or explicit Captain sign-off before changing defaults.

## Status Values

| Status | Meaning |
|---|---|
| ACTIVE | Current rule or doctrine. |
| SHIPPED | Implemented and verified enough to serve as current state. |
| PARKED | Intentionally not worked unless Captain reopens. |
| DORMANT | Code/hooks may exist but are not active product behaviour. |
| GATED | Decision accepted but blocked on a specific validation gate. |
| REVERSED | Superseded by a later decision. |

## Decision Rows

| ID | Status | Decision | Current Rule | Evidence Anchors | Revisit Trigger | Owner/Scope | Last Reviewed |
|---|---|---|---|---|---|---|---|
| ADL-001 | ACTIVE/GATED | AP/STA doctrine | Shipping K1 remains AP-only unless explicit dual-mode work is in scope. Goal-state is AP OR STA, never concurrent AP+STA. | `CLAUDE.md:121`, `BACKLOG.md:92-103`, `docs/protocol/k1-rest-contract.yaml:1-9` | Pure-STA validation session or Captain-approved WiFi-mode work. | Firmware network | 2026-05-06 |
| ADL-002 | ACTIVE | RMT wire-time fence | Keep the post-show WS2812 wire-time protection unless replaced by true TX-complete synchronisation. | `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md:13-17`, `:133-135`, `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md:24-32` | Hardware evidence proving equivalent or stronger TX-complete protection. | VP output / HAL | 2026-05-06 |
| ADL-003 | SHIPPED/GATED | Gamma LUT lifecycle | Gamma config writes must keep LUT state truthful; do not judge visual defaults from stale gamma math. | `instructions/changelog/2026-05-05--firmware-v3--gamma-lut-lifecycle-status.md`, `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md:148-150` | Any new gamma setter, NVS gamma load change, or colour A/B campaign. | VP colour | 2026-05-06 |
| ADL-004 | SHIPPED | ControlBusFrame DRAM placement | `SnapshotBuffer<ControlBusFrame>` payload belongs in internal DRAM unless a measured hot/cold split supersedes it. | `BACKLOG.md:280-289`, `instructions/changelog/2026-05-06--firmware-v3--controlbusframe-internal-dram-relocation.md` | New trace proves bus copy is no longer hot, or SRAM pressure requires a measured hot/cold split. | Audio/render handoff | 2026-05-06 |
| ADL-005 | PARKED | RTS `0x2100` | RTS remains parked for Phase 5 visual-quality work unless Captain explicitly reopens it. | `BACKLOG.md:49-52`, `BACKLOG.md:130`, `firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md` | Captain reopens RTS, or sign-off matrix is redesigned to remove/replace failed RTS row. | Phase 5 effects | 2026-05-06 |
| ADL-006 | DORMANT | Trinity status | Trinity is legacy inactive compatibility/dormant hooks, not active product behaviour. | `firmware-v3/docs/research/trinity_inactive_status_note_2026-05-06.md:7-38`, `docs/protocol/k1-ws-contract.yaml:1447-1497` | Captain explicitly revives Trinity or code paths become active in production. | Effects / protocol | 2026-05-06 |
| ADL-007 | SHIPPED | K1v2 SRAM reclaim boundary | Cold/control-path SRAM reclaim is allowed; do not lower heap guards, alter render hot paths, change WiFi mode, or move ControlBusFrame out of DRAM. | `firmware-v3/docs/research/k1v2_sram_psram_reclaim_handoff_2026-05-06.md:97-109`, `firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md:32-63`, `instructions/changelog/2026-05-06--firmware-v3--k1v2-batch-a-sram-reclaim.md` | Heap pressure returns under normal AP/effect-switch load. | K1v2 memory | 2026-05-06 |
| ADL-008 | ACTIVE | VP stack truth model | There is one frame lifecycle with a buffer-ownership fork, not two unrelated render pipelines. | `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md:13-23`, `firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md` | Renderer branch or buffer ownership implementation changes. | VP architecture | 2026-05-06 |

## Entry Template

```text
| ADL-XXX | ACTIVE | Decision title | Current rule | Source anchors | Revisit trigger | Owner/scope | YYYY-MM-DD |
```

Optional detail sections may be added below the table when a row needs nuance. Do not add forward task lists here.
