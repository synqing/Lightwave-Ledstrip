# Technical Debt Audit Report (2026-03-04)

## Scope
Primary runtime paths and active products:
- `firmware-v3` (K1 runtime, env `esp32dev_audio_esv11_k1v2_32khz`)
- `tab5-encoder`
- `lightwave-ios-v2`

## Analysis Framework (CDTP)
A repeatable four-pass framework was used.

1. Complexity pass
- Measure LOC and decision density (branch/logic proxy).
- Identify overgrown functions and monolithic files.

2. Churn pass
- Compute recent churn (commit-touch counts since 2025-12-01).
- Build hotspot score: `churn * (decisions + 1)`.

3. Type-safety pass
- Cross-check width/domain consistency for shared fields (`effectId`, zone payloads, request IDs, printf formats).
- Run static analysis (`cppcheck`) and targeted parser checks.

4. Performance pass
- Inspect hot-loop/update paths for avoidable work.
- Check render-path allocation guardrail (`no heap alloc in render`).
- Validate build profiles and runtime threshold configuration.

## Metrics Snapshot
### LOC (tracked first-party files only)
- `firmware-v3`: 1,028 files, 154,396 code lines
- `tab5-encoder`: 130 files, 123,414 code lines
- `lightwave-ios-v2`: 108 files, 16,990 code lines

### Highest-complexity files (decision proxy)
- `firmware-v3/src/main.cpp` (741 decisions, 2,750 LOC)
- `tab5-encoder/src/main.cpp` (515 decisions, 1,703 LOC)
- `firmware-v3/src/audio/AudioActor.cpp` (419 decisions, 2,226 LOC)
- `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` (363 decisions, 1,347 LOC)
- `firmware-v3/src/network/WebServer.cpp` (307 decisions, 1,394 LOC)

### Longest functions (approximate parser)
- `firmware-v3/src/main.cpp::loop` (~2143 lines, starts near line 1299)
- `tab5-encoder/src/main.cpp::setup` (~812 lines, starts near line 1187)
- `tab5-encoder/src/main.cpp::loop` (~793 lines, starts near line 2045)
- `firmware-v3/src/main.cpp::processSerialJsonCommand` (~795 lines, starts near line 501)
- `tab5-encoder/src/ui/DisplayUI.cpp::DisplayUI::begin` (~755 lines, starts near line 109)

### Churn × complexity hotspots
Top risk score (`churn * (decisions + 1)`):
1. `firmware-v3/src/main.cpp` score 3710
2. `firmware-v3/src/audio/AudioActor.cpp` score 2940
3. `firmware-v3/src/core/actors/RendererActor.cpp` score 1728
4. `tab5-encoder/src/main.cpp` score 1548
5. `firmware-v3/src/network/WebServer.cpp` score 1540
6. `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` score 1456
7. `tab5-encoder/src/ui/DisplayUI.cpp` score 783
8. `lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift` score 747
9. `firmware-v3/src/audio/contracts/AudioEffectMapping.cpp` score 711
10. `firmware-v3/src/network/webserver/handlers/AudioHandlers.cpp` score 573

## Build and Static Validation
### Platform builds
- `firmware-v3`: `pio run -e esp32dev_audio_esv11_k1v2_32khz` -> PASS
  - RAM 32.4% (106,132 / 327,680)
  - Flash 32.2% (2,110,417 / 6,553,600)
- `tab5-encoder`: `pio run -e tab5` -> PASS
  - RAM 3.3% (16,884 / 512,000)
  - Flash 68.6% (2,157,545 / 3,145,728)

### Static findings summary
- `tab5-encoder` cppcheck: 102 findings (3 error, 9 warning, 1 performance, 80 style, 9 information)
- `firmware-v3` cppcheck (partial broad scan + focused hotspot scan):
  - Broad scan was interrupted due combinatorial config explosion; 355 findings captured in partial output.
  - Focused hotspot scan (`main`, `WebServer`, `RendererActor`, `AudioActor`) found no compile-blocking defects.

### Render-path heap-allocation check
- Automated scan of `firmware-v3/src/effects` `render(...)` methods found **no `new/malloc/calloc/realloc/String` usage**.

## Issue Register

### Critical
#### C1. Build profile/process drift is causing incorrect firmware deployment risk
- Evidence:
  - `AGENTS.md` still advertises PipelineCore first (`AGENTS.md:10`).
  - Active K1 env is `esp32dev_audio_esv11_k1v2_32khz` (`firmware-v3/platformio.ini:171`).
  - No env alias exists for `esp32_k1v2_esv11_32khz` (name used in ops discussions).
- Root cause: docs/tooling naming drift and multi-profile ambiguity.
- Impact: flashing wrong profile leads to mismatched stack sizing, heap shedding thresholds, and runtime behaviour.
- Affected: release flow, on-device stability diagnostics.

#### C2. Monolithic control functions in highest-churn files
- Evidence:
  - `firmware-v3/src/main.cpp::loop` ~2143 lines (starts near `main.cpp:1299`).
  - `tab5-encoder/src/main.cpp::loop` ~793 lines (`tab5-encoder/src/main.cpp:2045`).
- Root cause: feature accretion into central loops without decomposition.
- Impact: regression blast radius, difficult scheduling reasoning, latent performance regressions.
- Affected: K1 runtime, Tab5 runtime.

### High
#### H1. Tab5 hot-loop duplicate I2C polling and UI churn
- Evidence:
  - Duplicate transport polling in loop: `tab5-encoder/src/main.cpp:2107`, `:2632`, `:2677`.
  - Cached getters already exist: `tab5-encoder/src/input/DualEncoderService.h:553`.
  - Per-loop UI updates: `tab5-encoder/src/main.cpp:2724`, `:2749`, `:2756`.
  - Unconditional LVGL writes: `tab5-encoder/src/ui/DisplayUI.cpp:1062`, `:1102`, `:1313`.
- Root cause: polling and UI update logic not fully event-gated.
- Impact: avoidable I2C contention, render-thread contention, UI redraw overhead.

#### H2. K1 WebServer does avoidable frame-copy work even with no subscribers
- Evidence:
  - Unconditional renderer copy for UDP: `firmware-v3/src/network/WebServer.cpp:652`.
  - Unconditional renderer copy for WS LED stream: `firmware-v3/src/network/WebServer.cpp:1664`.
  - Broadcaster already early-outs on no subscribers: `firmware-v3/src/network/webserver/LedStreamBroadcaster.cpp:63`.
- Root cause: subscriber checks happen after expensive copy/setup.
- Impact: steady CPU and stack pressure during idle stream conditions.

#### H3. Callback-context WebSocket sends in Tab5 router
- Evidence:
  - Direct sends inside message handling: `tab5-encoder/src/network/WsMessageRouter.h:255`, `:415`.
  - Defer pattern already used elsewhere (`zones.changed`): `tab5-encoder/src/network/WsMessageRouter.h:476`.
- Root cause: mixed send semantics in callback context.
- Impact: re-entrancy/blocking risk inside `_ws.loop()`, bursty latency.

#### H4. Type-width inconsistency for zone `effectId` between client and server
- Evidence:
  - Server expects uint16-range effect IDs: `firmware-v3/src/codec/WsZonesCodec.cpp:111-121`; type alias `EffectId = uint16_t` (`firmware-v3/src/config/effect_ids.h:28`).
  - Tab5 sends zone effect as `uint8_t`: `tab5-encoder/src/network/WebSocketClient.cpp:682`.
  - Tab5 preset zone storage keeps `uint8_t effectId`: `tab5-encoder/src/storage/PresetData.h:38`.
- Root cause: legacy 8-bit zone model persisted while firmware contract moved to 16-bit namespaced IDs.
- Impact: truncation risk and future incompatibility for zone effects beyond 255.

#### H5. Multi-layer silence gating is fragmented across pipeline stages
- Evidence:
  - PipelineAdapter Schmitt gate: `PipelineAdapter.h:101-103`, `PipelineAdapter.cpp:82-90`.
  - ControlBus silence hysteresis/fade: `ControlBus.cpp:551-587`.
  - AudioActor AGC/noise-floor activity gating: `AudioActor.cpp:2060-2074`.
- Root cause: gating evolved independently in multiple layers.
- Impact: hard-to-predict gating interactions, aggressive/early silence behaviour for some effects.

### Medium
#### M1. Inconsistent numeric parsing and bounds checks in effect ID handling
- Evidence:
  - Safe bounds-check path exists (`WsMessageRouter.h:233-237`).
  - Unsafe casts without bounds also exist (`WsMessageRouter.h:403-410`).
- Root cause: incremental patching without normalised parsing helpers.
- Impact: wraparound risk if malformed/negative payload arrives.

#### M2. Static analysis portability and formatting debt
- Evidence:
  - `printf` format/type mismatches (examples):
    - `tab5-encoder/src/ui/DisplayUI.cpp:1375` onwards
    - `firmware-v3/src/core/system/HeapMonitor.cpp:43` onwards
  - `memset` on float-containing structs (`memsetClassFloat`) in multiple effect/codec files.
- Root cause: mixed embedded C idioms and unchecked format contracts.
- Impact: undefined/portable-behaviour risk and harder diagnostics.

#### M3. Dead or misleading logic paths in high-use modules
- Evidence:
  - Redundant second decode check in `handleZoneSetEffect`: `WsZonesCommands.cpp:111` and `:123`.
  - Known-true/known-false conditions flagged in hot modules.
- Root cause: layered defensive code + feature toggles without cleanup.
- Impact: cognitive load, masked intent, regression risk.

#### M4. Constructor initialisation hygiene issues in key types
- Evidence:
  - Uninitialised member warnings in TempoTracker and several effect classes (cppcheck findings).
- Root cause: reliance on follow-up initialisation instead of constructor/member initialisers.
- Impact: latent UB risk under edge paths and harder reasoning.

#### M5. Build log reliability debt in Tab5 pipeline
- Evidence:
  - `pio run -e tab5` output includes `Installing Arduino Python dependencies` followed by `*** Error 1`, while overall build reports success.
- Root cause: noisy/ignored dependency script failure in build chain.
- Impact: CI/operator confusion and reduced signal trust.

## Prioritised Remediation Plan

### P0 (Immediate: 1-2 days)
1. Lock deployment profile naming and tooling.
- Add canonical env alias for K1 32kHz (`esp32_k1v2_esv11_32khz`) in `platformio.ini`.
- Update docs/tasks to make K1 env default for K1 device workflows.
- Add boot-time build fingerprint log (`env`, key compile flags).

2. Remove avoidable hot-path work.
- Use cached encoder switch/button reads only in Tab5 loop.
- Gate `setWiFiInfo`, `updateWebSocketStatus`, `updateRetryButton` by change detection.
- Gate K1 LED frame copy by subscriber presence before `getBufferCopy()`.

3. Defer WS sends out of callback context.
- Mirror pending-refresh model for effect parameter requests.

### P1 (Short term: 1 week)
1. Normalise type parsing and width contracts.
- Introduce shared parse helpers for `effectId` (`int -> uint16_t` with bounds).
- Convert Tab5 zone effect path and storage to `uint16_t` (including presets and queue payloads).

2. Silence-gate unification.
- Define one authority layer for “global silence blackout”.
- Keep per-band/per-stage gates as local features but expose a single policy surface.
- Add telemetry fields for each gate stage state to debug false closures.

3. Fix portability and formatting debt in runtime logs.
- Replace `%u/%lu` mismatches with explicit casts or `PRIu32/PRIu64`-style macros.
- Replace `memset` on float-containing PODs with value-initialisation.

### P2 (Medium term: 2-3 weeks)
1. Break up monolithic loop/control files.
- Extract scheduler slices from `firmware-v3/src/main.cpp` and `tab5-encoder/src/main.cpp`.
- Move serial/debug command handling out of main loop function bodies.

2. Refactor high-churn/high-complexity modules behind interfaces.
- `AudioActor`, `RendererActor`, `WebServer`, `V1ApiRoutes`, `DisplayUI`.
- Add per-module ownership boundaries and smaller command handlers.

3. Harden build and static checks in CI.
- Add targeted `cppcheck` jobs for hotspot files with stable config.
- Add config to suppress known false positives and fail on new high-severity findings.

### P3 (Continuous)
1. Establish a debt budget dashboard (weekly).
- Track hotspot score, function length ceilings, and static warning deltas.

2. Add performance benchmark harness.
- Benchmarks for frame-time budget (<2 ms effect render), WebServer update cost, and WS command latency under load.
- Include AP+WS single-client and multi-client scenarios.

## Suggested Owners
- Runtime/performance: firmware core team (`AudioActor`, `RendererActor`, `WebServer`).
- Type contracts/protocol: codec + Tab5 network owners.
- Build/release process: platform/build tooling owner.
- UI loop hygiene: Tab5 UI owner.

## Residual Risk if Unaddressed
- Recurring stutter regressions despite “no functional change” patches.
- Continued on-device mismatch incidents from wrong build profile flashing.
- Future protocol expansion blocked by 8-bit legacy assumptions in Tab5 zone paths.
- Ongoing debugging friction from inconsistent/low-signal diagnostics.
