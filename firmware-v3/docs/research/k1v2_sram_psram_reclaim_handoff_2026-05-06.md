# K1v2 SRAM/PSRAM Reclaim Execution Brief — 2026-05-06

> **RBDO status:** GROUNDED for the current failure, source anchors, commit anchors, and post-fix measurements listed below. DEGRADED-MODE for all future reclaim gains until the next agent measures the ELF/map delta and K1v2 hardware heap after each patch batch.

> **For agentic workers:** this is an execution brief, not a `.claude/handoff` file. Required first reads are `CLAUDE.md` top RBDO gate, `AGENTS.md`, `BACKLOG.md` § Performance, and this file. For source exploration spanning network, audio, effects, and core, follow `CLAUDE.md` context-management rules and dispatch bounded SSAs. Do not let any SSA edit shared firmware files unless `parallel-agent-sandboxing` rules are active and the orchestrator verifies the return receipt.

## Goal

Recover internal DRAM/SRAM headroom on K1v2 by moving or eliminating safe cold-path allocations and static buffers, then return to the Phase 5 visual tuning work with a device that can switch good effects without low-heap shedding or effect-init rejection.

The immediate objective is not to salvage failed Phase 5 effect `0x2100`/RTS. The objective is to make the already-promising light shows easier to test and tune by restoring reliable firmware headroom.

## Authority And Reading Order

1. `CLAUDE.md` top: RBDO gate, readback, delegation, AP-only / WiFi constraints, C++ source-navigation rules.
2. `AGENTS.md`: K1 product constraints, `.claude/handoff*.md` prohibition, hardware-test-before-commit requirement.
3. `BACKLOG.md` § Performance → "K1v2 SRAM/PSRAM reclaim pass".
4. This brief.
5. `firmware-v3/docs/research/c5_phase5_hardware_sweep_2026-05-06.md` for the interrupted Phase 5 hardware context.
6. `firmware-v3/docs/research/c5_phase5_timestamped_observables_2026-05-06.md` for C-5 observable debt.
7. `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md` only if touching VP colour correction, output buffers, silence policy, EdgeMixer defaults, or other visible render behaviour.

Do not use `PASS_3_KILL_ORDER.md` as the live topology plan authority. The canonical synergy-topology plan remains `Topology_Reconciliation.md` §5, as recorded in `BACKLOG.md`.

## Current State

### Branch And Commit

- Current branch at handoff creation: `feature/synergy-topology-resume-2026-05-05`.
- Current relevant head: `63a4b392 fix(firmware-v3): restore k1v2 heap headroom`.
- That commit disabled production diagnostic monitors in the K1v2 production env:
  - `FEATURE_HEAP_MONITORING=0`
  - `FEATURE_MEMORY_LEAK_DETECTION=0`
  - `FEATURE_STACK_PROFILING=0`
  - `FEATURE_VALIDATION_PROFILING=0`
- Source anchor: `firmware-v3/platformio.ini:176-225`.

### Dirty Tree Caution

At handoff creation, the repo had unrelated dirty files outside this task:

```text
M AGENTS.md
M CHANGELOG.md
M CLAUDE.md
M docs/WORKFLOW_ROUTING.md
M docs/tooling/claude-mem-usage-optimisation-2026-05-02.md
?? instructions/changelog/2026-05-05--repo--claude-mem-search-routing-fallbacks.md
```

Do not stage, revert, or "clean up" those files unless Captain explicitly assigns that separate repo-maintenance task.

### Device Evidence

K1v2 target used for the headroom restoration pass:

- Serial port: `/dev/cu.usbmodem2101`
- Verified MAC during upload: `b4:3a:45:a5:87:f8`
- Build env: `esp32dev_audio_esv11_k1v2_32khz`
- Current shipping mode for this task: AP-only. Do not add, test, or assume STA networking during this memory pass.

Post-`63a4b392` hardware probe:

```text
Free heap: 17776 bytes
Min free heap: 15848 bytes
Max alloc heap: 8180 bytes
showSkips=0
LED show avg ~= 6.18 ms
FPS ~= 112/119
effect 0x2103 -> accepted
effect 0x0100 -> accepted
```

This is a real improvement from the observed fault state, but it is not the end target. It remains close enough to the 12 KB effect-init and WebServer shedding floor that another modest regression could break switching again.

## Failure That Triggered This Work

Captain saw K1v2 in a low-heap state during Phase 5 testing:

```text
Low-heap shedding active/enabled
internal ~= 8880-10456 bytes
largest ~= 6132-7668 bytes
Effect 0x2103 REJECTED: internal heap 8880 < 12288 floor
Effect 0x0100 REJECTED: internal heap 8880 < 12288 floor
```

The source-backed gates are:

- WebServer low-heap shed floor: `LW_INTERNAL_HEAP_SHED_BELOW_BYTES = 12 KB`, resume `28 KB`, largest-block recovery `4 KB`; see `firmware-v3/src/network/WebServer.h:120-160`.
- WebServer latching/recovery logic: `WebServer::updateLowHeapShedState()`; see `firmware-v3/src/network/WebServer.cpp:545-628`.
- Effect-init rejection floor: `RendererActor::handleSetEffect()` refuses effect changes below `12288` internal bytes; see `firmware-v3/src/core/actors/RendererActor.cpp:2247-2256`.

Do not "fix" this by lowering the guard thresholds. Those gates protected the device from failed allocations. The task is to restore real headroom.

## Completion Target

Treat the pass as complete only when all of these are true on K1v2:

- Production build passes: `esp32dev_audio_esv11_k1v2_32khz`.
- K1v2 upload to `/dev/cu.usbmodem2101` succeeds after MAC verification.
- `dbg memory` after boot reports stable internal heap. Preferred target: `>= 22000` free bytes; minimum acceptable target: `>= 20000` free bytes with no client connected.
- Largest alloc/free block remains at least `>= 8192` bytes; prefer `>= 10000` if reclaiming cold static buffers makes it feasible.
- `s` status reports no RMT errors, no panics, no progressive heap collapse, and `showSkips=0`.
- Effect-switch smoke accepts at least `0x2103`, `0x0100`, and `0x2102` without low-heap rejection.
- No render hot-path heap allocations are introduced.
- Any visible render behaviour change has a VP validation run report before commit.

## Candidate Reclaim Batches

The next agent must measure, patch, build, upload, and hardware-check in batches. Do not perform a large blind rewrite.

### Batch A — Low-Risk Cold/Control-Path Candidates

These are the first places to inspect because they are not supposed to be per-frame render surfaces.

1. `firmware-v3/src/serial/CaptureStreamer.h:113-123`
   - Current candidates: `m_frameBufFallback`, `m_dumpFrameFallback`, `m_taskFrameBuf`.
   - Current source already attempts PSRAM for some buffers in `CaptureStreamer.cpp:40-55`, but the fallback arrays still reserve internal storage in the object.
   - Investigate whether production K1v2 can hold those fallback/task buffers in PSRAM or lazy-allocate them only when capture streaming starts.
   - Hard stop: do not break serial capture failure recovery; if PSRAM allocation fails, fail the capture command cleanly rather than consuming permanent DRAM for a rare path.

2. `firmware-v3/src/network/webserver/StaticAssetRoutes.cpp:89`
   - Candidate: `static char buf[3072]`.
   - This is a cold HTTP/static-asset path. Consider PSRAM allocation, smaller stack-safe chunking, or streaming directly without a permanent 3 KB internal static.
   - Must preserve AP dashboard/static route behaviour if that route is still active.

3. `firmware-v3/src/network/webserver/WsCommandRouter.h:80` and `WsCommandRouter.cpp:16`
   - Candidate: `s_handlers[MAX_HANDLERS]`.
   - Control-path handler registry. Inspect registration lifetime and mutation rules.
   - Possible directions: place in PSRAM after registration, reduce entry size, reduce max capacity if evidence shows large unused headroom, or generate a const table if registration is static.
   - Hard stop: do not make command dispatch allocate during WebSocket message handling.

4. `firmware-v3/src/plugins/BuiltinEffectRegistry.cpp:19` and `BuiltinEffectRegistry.h:76`
   - Candidate: `BuiltinEffectRegistry::s_entries[MAX_EFFECTS]`.
   - Registry mutates during registration and reset. Audit whether it can be compacted, moved, or generated without affecting effect lookup latency.
   - Hard stop: do not regress effect switch reliability or create render-path allocation.

### Batch B — Medium-Risk Source-Derived Tables

1. `firmware-v3/src/audio/pipeline/STMExtractor.cpp:35`
   - Candidate: `s_spectralMelBands[kSpectralMelBands]`.
   - It appeared as a multi-KB `.dram0.bss` consumer during map inspection.
   - Audit whether it is genuinely mutable per hop or only initialised/calibrated. If mutable and hot, PSRAM may add DSP cost; if immutable after init, a const/flash or compact representation may be safer.
   - Hardware validation must include audio/reactive behaviour, not just build success.

2. Render scratch/statics surfaced by `nm`
   - Only consider after proving the call path is not per-frame hot or after creating a no-heap hot-path substitute.
   - If a candidate is called from `render()` or `RendererActor::onTick()`, default to "do not move" until timing evidence says otherwise.

### Batch C — High-Risk Architecture Candidates

These are not first-pass reclaim work.

1. `SnapshotBuffer<ControlBusFrame>`
   - Current `BACKLOG.md` records the Captain-approved relocation of the ControlBus snapshot payload into internal DRAM and the follow-up trace result.
   - Do not move it back to PSRAM. The next plausible lever is a hot/cold split, not a blind allocation-region reversal.
   - Source/research anchor: `BACKLOG.md` § "ControlBusFrame hot/cold split".

2. `ActorSystem`, `RendererActor`, `ZoneComposer`, and large renderer members
   - These may contain large fields, but they are core lifecycle objects.
   - Only decompose them under an explicit architecture plan, not as a quick SRAM pass.

## Non-Candidates / Hard Refusals

Do not do any of the following in this task:

- Do not create `.claude/handoff*.md` forward-task files.
- Do not enable STA mode, pure-STA validation, or concurrent AP+STA.
- Do not lower the 12 KB effect-init guard or WebServer shed floor as a substitute for reclaiming memory.
- Do not move `ControlBusFrame` snapshot payload out of internal DRAM without a measured hot/cold split plan and Captain approval.
- Do not add heap allocation to `render()` or any function called from `render()`.
- Do not revive the failed two-unit manual A/B workflow.
- Do not claim visual sign-off for C-5 rows from trace-only evidence.
- Do not tune or salvage RTS/`0x2100` as part of this pass unless Captain explicitly reopens that effect.
- Do not commit private clip/media paths or any local playback corpus names.
- Do not stage unrelated dirty files.

## Required Execution Sequence

1. Read the authority files listed above and perform the repo readback required by `CLAUDE.md`.
2. Record `git status --short` before editing.
3. Build the current baseline:

   ```bash
   cd firmware-v3
   pio run -e esp32dev_audio_esv11_k1v2_32khz
   ```

4. Produce a current symbol-size list from the baseline ELF:

   ```bash
   /Users/spectrasynq/.platformio/packages/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5/bin/xtensa-esp32s3-elf-nm -S --size-sort .pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf | tail -n 120
   ```

5. Classify candidates into Batch A/B/C. If classification spans network + audio + core/effects, use parallel SSAs for read-only candidate mapping.
6. Patch only Batch A first.
7. Rebuild and compare:
   - PlatformIO RAM summary.
   - `.dram0.bss` / `.dram0.data` symbol deltas.
   - Whether any candidate moved to PSRAM or was eliminated.
8. Upload only after build success and MAC verification:

   ```bash
   cd firmware-v3
   pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101
   ```

9. Serial hardware checks:

   ```text
   dbg memory
   s
   effect 0x2103
   effect 0x0100
   effect 0x2102
   dbg memory
   s
   ```

10. If Batch A meets the completion target, commit and stop. If not, open Batch B with timing evidence and repeat build/upload/hardware checks.
11. Write a run report under `firmware-v3/docs/research/` with before/after SRAM, largest block, effect-switch results, and any visual-risk notes.
12. Commit with a body reference to `BACKLOG.md` § "K1v2 SRAM/PSRAM reclaim pass" and the run report.

## Testing Matrix

Minimum verification for documentation-only handoff changes:

```bash
git diff --check
```

Minimum verification for SRAM/PSRAM code changes:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Then hardware is mandatory before committing:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101
```

If the patch touches native-covered modules, run the scoped host harness for that module. Do not revive stale broad `native_test` references; use the current scoped harness route.

## Return-To-Work Rule

Once the headroom pass is complete, resume the previous task at the Phase 5 visual-quality path:

- Keep RTS/`0x2100` parked unless Captain explicitly reopens it.
- Focus on the good-light-show path, especially `0x2101` PVF and `0x2102` BPS, plus any other effects Captain identifies as promising.
- Use `VP_VALIDATION_PROTOCOL_2026-05-06.md` before any colour-correction default, silence-scaling policy, buffer-ownership, or EdgeMixer change.
- If the next step requires Captain observation, batch all needed operator checks at the end rather than interrupting for one row at a time.

## One-Screen TL;DR

K1v2 hit real internal-heap pressure: effect changes were rejected below the 12 KB floor and WebServer low-heap shedding latched. Commit `63a4b392` restored about 6.9 KB static RAM by disabling production diagnostic monitors, and K1v2 now switches `0x2103`/`0x0100` again with about 17.7 KB free internal heap. That is improved but still thin. The next agent should reclaim more DRAM from cold/static candidates first (`CaptureStreamer`, `StaticAssetRoutes`, WS/effect registries), measure every batch, upload to `/dev/cu.usbmodem2101`, run serial memory/status/effect-switch checks, and only then return to Phase 5 visual tuning. Do not touch STA, lower heap guards, move `ControlBusFrame` back to PSRAM, revive two-unit A/B, or commit private media paths.
