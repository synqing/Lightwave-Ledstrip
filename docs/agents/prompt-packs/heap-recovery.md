# Prompt Pack: Heap Recovery

Use this for K1v2 SRAM/PSRAM pressure, low-heap shedding, allocation guards, or memory headroom recovery.

## First Contact Prompt

```text
GROUNDED:

You are investigating K1v2 memory pressure. Start by reading:
- CLAUDE.md top RBDO gate.
- BACKLOG.md Performance section for the current SRAM reclaim state.
- firmware-v3/docs/research/k1v2_sram_psram_reclaim_handoff_2026-05-06.md if new reclaim work is being considered.
- firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md for the Batch A precedent.
- firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md if trace instrumentation is relevant.

Task:
1. Verify current HEAD and whether SRAM reclaim is already shipped.
2. Capture baseline static RAM and runtime heap before proposing changes.
3. Classify candidates as hot render/audio path, warm control path, or cold/control path.
4. Prefer cold/control-path PSRAM or flash moves first.
5. Do not move ControlBusFrame out of internal DRAM.
6. Do not lower heap guard thresholds to make tests pass.
7. Do not add heap allocation to render.
8. Build and hardware-smoke any firmware memory change before commit.

Return:
- baseline measurements;
- candidate table with risk;
- changed files;
- before/after static and runtime evidence;
- serial effect-switch smoke result.
```
