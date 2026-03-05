# LED Stability Incident Postmortem and Reattempt Plan (2026-03-04)

## Scope

This document captures what was learned from the March 4, 2026 instability cycle across K1/non-K1 ESP32-S3 firmware builds, including:

- dual-strip render stutter/choking;
- RMT channel faults and watchdog aborts;
- `loopTask` stack canary panics and boot loops;
- AP-only runtime pressure and heap shedding interactions;
- the rollback strategy required for a controlled reattempt.

The goal is to stop ad-hoc debugging, preserve hard-won evidence, and define a low-risk path forward.

## Executive Summary

Multiple regressions were introduced in overlapping changesets while build-target context was shifting between K1v2 and non-K1v2 environments. The main fault pattern was not one single defect; it was compounding instability from:

1. large render/network refactors in the Phase-0 commit cluster (`dfc05298` -> `9a59e899`);
2. loop stack pressure in WebServer audio-stream paths (`ControlBusFrame` copies on `loopTask`);
3. runtime memory pressure under AP+WS load (frequent low-heap shedding zone);
4. target drift (K1 vs non-K1 envs/ports/devices) during active triage.

A hard rollback baseline is required before selective reintroduction.

## Verified Facts

### Commit Range Risk Profile

- `f4c579ed` (2026-03-02): pre-Phase-0 baseline (build/test env only).
- `dfc05298` (2026-03-04 00:01): AudioContext/controlbus refactor introduced.
- `f8fac95b` (2026-03-04 00:02): core actor/renderer guard and mode changes.
- `4a8338d6` (2026-03-04 00:03): very large `ieffect` migration and render behaviour changes.
- `9a59e899` (2026-03-04 00:03): additional effect variants.
- `54765b16` (2026-03-04 00:04): build/AP-only config change, but after the risky render block.

Conclusion: `54765b16` is not a clean render baseline. `f4c579ed` is.

### Device/Port Reality

Port assignment changed repeatedly during debugging. The stable guardrail is MAC verification before each flash:

- observed K1 MAC during session: `b4:3a:45:a5:87:f8`;
- observed non-K1 device MAC during session: `b4:3a:45:a5:89:b4`.

### Hardware vs Firmware Isolation

Channel fault moved with logical strip index, not physical wiring, when strip pin mapping was swapped. This exonerates wiring/connectors and points to firmware/driver/runtime path issues.

### Concrete Faults Seen

- `RMT CHANNEL ERR` and renderer watchdog abort during boot in one configuration.
- `Guru Meditation ... Stack canary watchpoint triggered (loopTask)` on non-K1 build.
- persistent low-heap shedding logs in AP+WS operation.
- LittleFS mount corruption (`Corrupted dir pair`) observed independently.

## Root Cause Analysis

## Confirmed Root Causes

1. `loopTask` stack overflow path in WebServer audio streaming:
   - backtrace resolved to `loop()` -> `WebServer::update()` -> `broadcastAudioFrame()`;
   - large `ControlBusFrame` and `MusicalGridSnapshot` fetch/copy patterns were contributing to loop stack pressure.

2. Non-K1 ESV11 profile stack budget mismatch:
   - runtime reported loop stack estimate at 8 KB (`StackMonitor` warnings at ~93-96%);
   - this profile did not have effective loop stack headroom during high WS activity.

3. AP-only memory contention zone:
   - internal heap repeatedly sat near the shedding threshold under WS client activity;
   - this creates instability risk for render + network coexistence and directly affects frame smoothness.

## High-Confidence Contributing Factors

1. Broad render-path churn in `4a8338d6`:
   - this commit touched a large cross-section of effects and behaviour logic;
   - this is the most likely behavioural regression source for visual quality/stability drift.

2. Environment drift during triage:
   - K1v2 and non-K1v2 builds were both in play while ports changed and devices were swapped;
   - this slowed diagnosis and mixed signal from logs/tests.

## Not the Root Cause

- physical strip wiring/connector failure (failed to match observed index-swapping behaviour).

## Session Mitigations Already Applied

The following emergency mitigations were applied in working state during triage:

- renderer/network audio snapshot API changed to copy via persistent buffers instead of by-value hot-path pulls;
- WebServer audio/beat/FFT call sites moved to those snapshot copy APIs;
- Audio handler paths updated to use snapshot-copy API;
- K1/non-K1 uploads rebuilt and flashed for validation runs;
- RMT channel-cap and status-strip ordering work was tested in the branch timeline.

These mitigations reduced immediate crash frequency, but did not establish a clean behavioural baseline because the codebase remained post-Phase-0 and under high memory pressure.

## Recovery Strategy (Recommended)

## Stage 0: Stabilise Baseline

1. Create a clean recovery branch from `f4c579ed` (or `6a040b36` if doc commit is preferred).
2. Build and flash only:
   - `esp32dev_audio_esv11_32khz` for non-K1;
   - `esp32dev_audio_esv11_k1v2_32khz` for K1.
3. Validate 30-minute soak on AP-only with one active WS client.

Exit criteria:

- no watchdog/reset/panic;
- no `RMT CHANNEL ERR`;
- no visible per-strip corruption;
- stable input/control path.

## Stage 1: Reintroduce Safe Changes First

Cherry-pick in this order, validating each step:

1. `a5e7d247` (tests only, low runtime risk).
2. `54765b16` (build/AP-only config).
3. `dfc05298` (AudioContext layer, moderate risk).
4. `f8fac95b` (core actor changes, higher risk).
5. `4a8338d6` (largest render/effect behavioural risk).
6. `9a59e899` (new radial variants).

Each pick requires full smoke + visual pass before the next one.

## Stage 2: Lock Discipline

Use these operational controls for the reattempt:

1. Verify target MAC before every upload.
2. One variable change per test cycle (no multi-axis edits).
3. Keep AP-only mode constant during render diagnostics.
4. Record logs and frame observations per commit checkpoint.
5. No merging of “performance”, “render”, and “network” changes in the same verification batch.

## AP-Only Runtime Guidance

For stability while diagnosing render quality:

1. keep AP-only as a fixed topology (Tab5/iOS as clients);
2. preserve conservative low-heap shedding and log internal free + largest block;
3. avoid additional WS broadcast load while measuring visual fidelity;
4. treat low-30 KB internal heap as a danger zone requiring strict traffic discipline.

## Test Matrix for Reattempt

Minimum required at each checkpoint:

1. Build:
   - `pio run -e esp32dev_audio_esv11_32khz`
   - `pio run -e esp32dev_audio_esv11_k1v2_32khz`
2. Boot logs:
   - no panic, no RMT errors, no renderer watchdog abort.
3. Visual:
   - both strips smooth across effect cycling (including previously affected effects).
4. Load:
   - one WS control client + parameter changes + effect stepping for 30 minutes.
5. Memory/stack:
   - no progressive stack-canary risk trend;
   - no repeated heap-shed flapping outside expected guard conditions.

## Lessons Learned

1. Large cross-domain refactors require hard checkpoints and immediate soak tests.
2. Port/device ambiguity is expensive; MAC-first flashing must be mandatory.
3. “No crash in short window” is not equivalent to render stability.
4. Build-target drift can invalidate conclusions even when logs look plausible.
5. Rollback-first is faster than patch-stacking once signal quality is compromised.

## Addendum (2026-03-05): Dual-Channel Corruption + False Low-FPS Signal

### What happened

After Stage 1 cherry-picks, both LED channels showed visible corruption and apparent ~5 FPS behaviour, despite no immediate panics.

### What was actually wrong

1. **Telemetry blind spot**:
   - `RenderStats.currentFPS` was an 8-bit field;
   - high real FPS could wrap/truncate and report misleading low values.

2. **Visual corruption driver**:
   - on ESP32-S3 FastLED/RMT path, `FastLED.show()` could return before full WS2812 wire drain;
   - render code then wrote next-frame data into strip buffers too early;
   - this produced dual-channel tearing/corruption.

3. **Cadence instability**:
   - renderer timing tied to actor tick semantics caused pacing drift/quantisation;
   - fixing to explicit self-clocked 120 FPS pacing removed this ambiguity.

4. **Build-target drift amplified triage time**:
   - K1v2 pin mapping and status-strip assumptions were changed during debugging;
   - this mixed hardware/profile signals and delayed isolation.

### Fixes that restored stability

1. `LedDriver_S3` now enforces **minimum WS2812 frame hold time** after `FastLED.show()` based on longest strip length.
2. `RenderStats.currentFPS` widened to 16-bit; message packing clamps explicitly where needed.
3. Renderer switched to **self-clocked pacing** (`~8333 us` frame budget) instead of periodic actor tick pacing.
4. K1v2 profile pinned to:
   - `FEATURE_STATUS_STRIP_TOUCH=0`;
   - stable strip pin mapping for current hardware (`K1_LED_STRIP1_DATA=6`, `K1_LED_STRIP2_DATA=7`).

### Operational guardrails (mandatory for future agents)

1. If dual 160-strip `LED show avg` is around **1 ms**, treat as failure signal immediately.
   - expected steady-state is roughly **4.8-5.5 ms**.
2. Do not accept FPS from one field alone.
   - always cross-check `framesRendered / uptime` and `avgFrameTimeUs`.
3. For memory work near visual/network boundary:
   - move large snapshots/scratch buffers to persistent allocations (PSRAM-preferred);
   - avoid transient large stack objects in `loop()`/WebServer update paths.
4. Enforce MAC-first flashing and profile verification before every upload.
5. After each render-path change, run serial `s` checks and confirm:
   - no panic/RMT errors;
   - `showSkips=0`;
   - frame cadence near target;
   - no worsening stack/heap trend.

## Addendum (2026-03-05B): Water Caustics Canary Confirmation

### New confirmed checkpoint

`LGP Water Caustics` (`0x1608`) was re-tested on K1 at commit `39437150` (`AudioContext accessor layer only`) and is now confirmed smooth by direct visual validation.

### Why this matters

1. The effect itself is not inherently broken.
2. Regression blame should be shifted to post-`39437150` runtime changes (renderer/pipeline/load interactions), not Water Caustics base maths.
3. Water Caustics can now be used as an explicit **canary gate** for render smoothness.

### Mandatory policy from this point

1. Every runtime-affecting pick must pass a Water Caustics visual gate before progressing.
2. Any reintroduced stutter on Water Caustics is an immediate stop condition for further picks.
3. Comparisons must keep topology constant (same build target, same device MAC, AP + one WS client).

### Canonical guardrail reference

Use:

- `docs/performance/LGP_WATER_CAUSTICS_SMOOTHNESS_GUARDRAILS_2026-03-05.md`

This is now the source-of-truth checklist for:
- what not to do (anti-patterns), and
- what must be done (validation and rollback discipline).

## Addendum (2026-03-05C): Final Stable Baseline Evidence

### Frozen firmware state

- **Commit**: `800ac874` in `/tmp/stage0-baseline`
- **Tag**: `stage1-d14partial-stable-2026-03-05`
- **Branch**: `stage0/k1-prephase0-baseline`
- **Build env**: `esp32dev_audio_esv11_k1v2_32khz`
- **Device**: K1 (MAC `b4:3a:45:a5:87:f8`, port `/dev/cu.usbmodem212401`)

### Applied changesets (cumulative)

1. Stage 0 baseline (`f4c579ed` + GPIO fix `e39b1152`)
2. Stage 1 complete (6 cherry-picks, tag `0dec43eb`)
3. d14 Step 1: StackMonitor hysteresis (`ed28ea6e`)
4. d14 Step 2: WiFiManager AP auth fix (`86abcb12`)
5. d14 Sub-step 3a: `broadcastLEDFrame()` subscriber early-exit (`800ac874`)

### Serial `s` baseline capture (2026-03-05, post-boot)

```
Uptime: 44681 ms
Active actors: 3
Heap: 8087559 / min 8071959 bytes
Effect: 4096 (LGP Holo Auto-Cycle)
FPS: 112 (target: 120)
Frame time: avg=8910, min=6431, max=55711 us
Stack watermark: 11080 words (renderer)
loopTask: 500 bytes free / 8192 (93% usage)
WS diag: active=0, ok=1, all reject/error counters=0
Low-heap shedding active (internal=35884)
```

### Visual confirmation

- Both strips (top 0-159, bottom 160-319) rendering smoothly.
- No stutter, corruption, or single-channel degradation.
- LGP Holo Auto-Cycle cycling palettes without visual artefacts.
- Water Caustics canary gate: PASS (confirmed smooth in earlier bisect step).

### Do-not-merge list

| Hunk | Description | Risk | Status |
|------|-------------|------|--------|
| d14 #2 | `getLargestInternalHeapBlock()` helper | **HIGH** | DROPPED — heap mutex contention with Core 1 renderer |
| d14 #3 | Enhanced heap-shed logging using #2 | **HIGH** | DROPPED — depends on #2 |
| d14 #4 | WS diagnostics 60s/no-client suppression | Low | Deferred |
| d14 #5 | `softAP()` simplified call | Low | Deferred |
| d14 #7 | Configurable threshold constants (WebServer.h) | Minimal | Deferred (constants only) |

### Hard rule established

**No expensive heap introspection (`heap_caps_get_largest_free_block`, heap walking) in high-frequency `WebServer::update()` paths.** This caused bottom-channel stutter and corruption when applied as part of d14 Step 3. The function traverses the heap free list under mutex, blocking cross-core memory operations including Core 1 renderer heap access.

If #2/#3 are revisited in future:
- Sample at most every 5000 ms, not every update cycle.
- Prefer sampling only on low-heap shed state transitions.
- Guard behind a default-off build flag.
- Any visual regression is an immediate revert condition.

## Immediate Next Action

Recovery plan complete. All stages validated. Firmware frozen at known-good state `800ac874`. No further changes this session.

## Final Stable Ladder (2026-03-05D)

Final validated runtime-stable state:

- **Tag:** `stage1-d14-r3-stable-2026-03-05`
- **Commit:** `32703522`
- **Built at (UTC):** 2026-03-05T02:33:05Z
- **Build environment:** `esp32dev_audio_esv11_k1v2_32khz`
- **Firmware binary:** `firmware-v3/.pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.bin`
- **Firmware size (bytes):** 2143920
- **Firmware SHA-256:** `3160694119cb67d01509bdac96093e567fffde6dfc2dcf6bbfd4e7a920b0b61f`
- **Validation hardware MAC:** `b4:3a:45:a5:87:f8` (K1)
- **Visual gate:** PASS (both channels smooth, no corruption/stutter)
- **Serial gate:** PASS (no panic/RMT faults; stable stack/heap trend)

Applied commit ladder:

1. **Stage 0 clean baseline:** `stage0-k1-bootclean-2026-03-04` -> `e39b1152`
2. **Stage 1 complete (6 picks):** `stage1-complete-2026-03-05` -> `0dec43eb`
3. **d14 partial stable:** `stage1-d14partial-stable-2026-03-05` -> `800ac874`
4. **Batch A (d14 #4/#5/#7):** `bccb48fe`
5. **Batch B (R3 data-race fixes):** `449283d5` + `32703522`
6. **Final stable tag:** `stage1-d14-r3-stable-2026-03-05` -> `32703522`

Hard rule retained:
- No unthrottled `heap_caps_get_largest_free_block()` in high-frequency `WebServer::update()` paths.
