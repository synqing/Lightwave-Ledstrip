# LightwaveOS firmware-v3 Technical Debt Audit

**Date:** 2026-03-04
**Codebase:** `firmware-v3/src/` -- 737 files, 148,306 lines C++ (ESP32-S3)
**Methodology:** 4 parallel analysis agents + manual investigation
**Confidence:** HIGH (findings backed by line-number evidence, grep-verified)

---

## Executive Summary

The firmware is architecturally sound -- an actor model on FreeRTOS with clean Core 0/Core 1 separation, lock-free cross-core communication, and zero heap allocation in render paths. However, the audit identified **3 confirmed CRITICAL bugs** (1 stale), **9 HIGH-severity issues**, **16 MEDIUM issues**, and **8 LOW issues** across type safety, performance, complexity, and architecture domains. Additionally, the architecture coupling agent found **4 cross-core data races** and **5 module boundary violations**.

**Top 3 immediate risks:**
1. **EffectId truncation to uint8_t** in V1ApiRoutes (5 endpoints) -- silently breaks audio mappings for effects with ID > 255
2. **Dangling pointer** in WsEffectsCommands.cpp -- undefined behaviour in WebSocket responses
3. **Cross-core data races** -- getControlBusMut() gives WebServer unsynchronised mutable access to AudioActor state; getBufferCopy() torn reads of 960 bytes

**Estimated total remediation:** ~95-120 engineering hours (CRITICAL+HIGH: ~45h, MEDIUM: ~50h)

---

## Module Size Distribution

| Module | Lines | Files | Notes |
|--------|-------|-------|-------|
| effects | 49,671 | 354 | 154 effect implementations |
| network | 31,237 | 134 | REST + WebSocket + WiFi |
| audio | 17,731 | 64 | Dual backend (PipelineCore/ESV11) |
| core | 17,114 | 52 | Actor system, persistence, state |
| codec | 10,627 | 60 | JSON/binary serialisation |
| sync | 4,449 | 18 | Cross-core, device sync |
| hal | 3,850 | 16 | LED driver, hardware abstraction |
| plugins | 2,852 | 10 | Effect API surface |
| config | 2,527 | 13 | Feature flags, limits, audio config |
| hardware | 1,848 | 4 | Board-specific definitions |
| palettes | 1,503 | 5 | Colour palette data |
| validation | 1,038 | 4 | Effect validation framework |
| main.cpp | 3,441 | 1 | God object (see C-4) |

---

## CRITICAL Findings

### C-1: StateStore::getCurrentEffect() truncates EffectId to uint8_t

**Files:** `core/state/StateStore.h:91`, `core/state/StateStore.cpp:49`
**Domain:** Type Safety

```cpp
// StateStore.h:91
uint8_t getCurrentEffect() const;

// StateStore.cpp:49 - SystemState::currentEffectId is EffectId (uint16_t)
uint8_t StateStore::getCurrentEffect() const {
    return getState().currentEffectId;  // TRUNCATES high byte
}
```

`SystemState::currentEffectId` is `EffectId` (= `uint16_t`). The stable EffectId system uses namespaced 16-bit IDs where the high byte is the family (e.g., `EID_FIRE = 0x0100`). This function truncates the high byte, returning 0x00 for ANY effect outside family 0. Most callers use `RendererActor::getCurrentEffect()` (correctly typed), but any path through StateStore is silently wrong.

**Fix:** Change return type to `EffectId`. One-line fix in header and implementation.

---

### C-2: Dangling pointer in handleEffectsGetCategories

**File:** `network/webserver/ws/WsEffectsCommands.cpp:417-430`
**Domain:** Type Safety

```cpp
const char* familyNames[10];
for (uint8_t i = 0; i < 10; i++) {
    char familyNameBuf[32];  // Stack-local, destroyed each iteration
    PatternRegistry::getFamilyName(family, familyNameBuf, sizeof(familyNameBuf));
    familyNames[i] = familyNameBuf;  // ALL 10 pointers -> same dead stack slot
}
// Lambda captures familyNames by value (copies pointer array)
// but underlying char arrays are destroyed -> undefined behaviour
```

All 10 `familyNames[i]` pointers point to the same stack variable that is destroyed at end of each loop iteration. WebSocket response contains garbage or last-written name for all families.

**Fix:** Use `char familyNameBufs[10][32]` array outside the loop, or `static` buffer.

---

### C-3: V1ApiRoutes truncates EffectId to uint8_t in 5 audio mapping endpoints

**File:** `network/webserver/V1ApiRoutes.cpp:406, 421, 434, 446, 458`
**Domain:** Type Safety

```cpp
uint8_t id = request->getParam("id")->value().toInt();  // TRUNCATES
handlers::AudioHandlers::handleMappingsGet(request, id, ctx.renderer);
// handleMappingsGet expects EffectId (uint16_t)
```

When a client sends effect ID 0x0100 (256 = EID_FIRE), `toInt()` returns 256, but assignment to `uint8_t` truncates to 0. The implicit widening back to `uint16_t` produces 0, not 256. Audio mappings for that effect silently fail. 5 API endpoints affected.

**Fix:** Change `uint8_t id` to `uint16_t id` (or `EffectId`) in all 5 handlers.

---

### ~~C-4: KuramotoTransportEffect missing zone ID upper bounds check~~ STALE

**File:** `effects/ieffect/KuramotoTransportEffect.cpp:117-118`
**Domain:** Complexity / Safety
**Status:** STALE -- already fixed in current code.

Current code at line 117-118:
```cpp
uint8_t zid = (ctx.zoneId == 0xFF) ? 0 : ctx.zoneId;
if (zid >= KuramotoOscillatorField::MAX_ZONES) zid = 0;
```

The upper bounds check is present. This finding was based on a stale read.

---

## HIGH Findings

### H-1: ColorCorrectionEngine 6-pass pipeline (200-800 us/frame)

**File:** `effects/enhancement/ColorCorrectionEngine.cpp:181-217`
**Domain:** Performance

Six separate passes over 320 LEDs: autoExposure, brightnessClamp, saturationBoost, whiteGuardrail, brownGuardrail, gamma. The white guardrail calls `rgb2hsv_approximate()` per pixel (~20 cycles), then reconstructs CRGB from CHSV (~40 cycles).

**Fix:** Fuse all 6 passes into a single LED iteration. Eliminates 5 buffer traversals and cache-line reloads. Biggest single frame-time win: saves 300-500 us/frame.

---

### H-2: Serial.println in TransitionEngine render path (200-500 us)

**File:** `effects/transitions/TransitionEngine.cpp:162, 187`
**Domain:** Performance

```cpp
Serial.printf("[Transition] Started: %s (%dms)\n", getTransitionName(type), durationMs);
Serial.println("[Transition] Complete");
```

Called from `RendererActor::renderFrame()` on Core 1. Serial.print on ESP32 is blocking UART at 115200 baud. These messages take 200-500 us, risking frame drops during transition events.

**Fix:** Replace with `LW_LOGI()` (unified logging, lower overhead) or defer to a flag checked outside the hot loop.

---

### H-3: Serial.printf in effect render() methods

**Files:** `effects/ieffect/LGPSpectrumDetailEffect.cpp:53,72,143,256`, `effects/ieffect/BeatPulseBloomEffect.cpp:138,146,196,216`
**Domain:** Performance

Debug logging left in render hot paths. Same blocking UART issue as H-2 but called every frame (not just during transitions).

**Fix:** Remove or gate behind `#if LW_LOG_LEVEL >= 4`.

---

### H-4: forceOneShotCapture() allocates 1,920 bytes on FreeRTOS stack

**File:** `core/actors/RendererActor.cpp:1045-1058`
**Domain:** Performance / Memory

```cpp
CRGB savedLeds[LedConfig::TOTAL_LEDS];   // 960 bytes on stack
CRGB corrected[LedConfig::TOTAL_LEDS];   // 960 bytes on stack
```

FreeRTOS task stack is finite. 1,920 bytes during capture dumps reduces stack headroom significantly.

**Fix:** Use class member buffer or PSRAM-allocated scratch space.

---

### H-5: main.cpp -- 3,441-line god object with 7+ responsibilities

**File:** `main.cpp`
**Domain:** Complexity
**Cyclomatic complexity:** 710 branching constructs

Conflates: setup, serial JSON gateway (795 lines), serial CLI (2,101 lines), WiFi management CLI, show playback management, effect parameter tuning hotkeys, system health monitoring. 42 `#include` directives.

**Fix:** Extract `SerialCommandHandler`, `SerialJsonGateway`, `ShowPlaybackController` classes. ~16h effort.

---

### H-6: Duplicate capture dump handler in main.cpp

**File:** `main.cpp:1471-1564 AND 2166-2259`
**Domain:** Complexity

Binary frame dump protocol (magic byte 0xFD, 16-byte header, RGB payload) copy-pasted verbatim in two separate `if` branches. Protocol divergence risk.

**Fix:** Extract common `writeCaptureFrame()` function. ~1h effort.

---

### H-7: extern global coupling bypassing actor model

**Files:** `main.cpp:89,106,110`, `WebServer.cpp:116,127`, `V1ApiRoutes.cpp:54`
**Domain:** Architecture

Three globals leak across module boundaries via `extern`:
- `webServerInstance` (WebServer*) -- referenced in 5+ locations
- `zoneConfigMgr` (ZoneConfigManager*) -- passed to 6 handler functions
- `presetMgr` (PresetManager*) -- **always nullptr, class never implemented**

These bypass the actor system's message-passing thread safety guarantees.

**Fix:** Inject dependencies via WebServerContext, remove extern declarations. ~6h effort.

---

### H-8: Persistence singletons have NO mutex protection

**Files:** `core/persistence/AudioTuningManager.h:71`, `core/persistence/ZonePresetManager.h:144`, `core/persistence/NVSManager.h:71`, `core/persistence/EffectPresetManager.h:139`
**Domain:** Architecture

10+ singleton managers use `static T& instance()` pattern. Grep for `SemaphoreHandle_t|mutex|std::mutex|portMUX_TYPE` in `core/persistence/` returns **zero matches**. These singletons are accessed from both Core 0 (audio/network) and Core 1 (renderer) without synchronisation.

NVS operations are not atomic -- concurrent reads/writes from different cores can corrupt stored preferences.

**Fix:** Add `portMUX_TYPE` spinlock to critical sections in persistence managers, or ensure single-core access via actor message routing.

---

### H-9: Dual MAX_ZONES constants (8 vs 4) in different namespaces

**Files:** `config/limits.h:76` (`MAX_ZONES = 8`), `effects/zones/ZoneDefinition.h:20` (`MAX_ZONES = 4`)
**Domain:** Type Safety

`lightwaveos::limits::MAX_ZONES = 8` vs `lightwaveos::zones::MAX_ZONES = 4`. If code allocates with one constant but iterates with the other, OOB access occurs. Currently safe at runtime (zone count clamped to 4) but the inconsistency masks array-sizing assumptions.

**Fix:** Consolidate to a single source of truth. Remove the duplicate.

---

## MEDIUM Findings

### M-1: Triple validateEffectId() calls per frame (~9 us wasted)
**File:** `core/actors/RendererActor.cpp:774, 1448, 1645`
Linear scan of 256 entries x 3 per frame. Cache once at frame start.

### M-2: Heavy trig math per-pixel in transitions (50-200 us during transitions)
**File:** `effects/transitions/TransitionEngine.cpp:477-1048`
`sinf()`, `cosf()`, `atan2f()` per pixel in 6 transition types. Use `sin8()/cos8()` or pre-computed LUTs.

### M-3: Duplicate silence gate strip iteration (40-60 us)
**File:** `core/actors/RendererActor.cpp:1632-1688`
Two separate `nscale8()` passes. Combine into single scale factor.

### M-4: AudioActor.cpp triple-backend in single file (3,039 lines)
**File:** `audio/AudioActor.cpp`
Three complete backend implementations via `#if`. ESV11 has dead stubs (`getLastHop()` returns nullptr, `hasNewHop()` returns false). Extract strategy pattern. ~8h effort.

### M-5: vTaskDelay(1) adds 10ms audio latency per hop
**File:** `audio/AudioActor.cpp:242, 251, 693, 814`
Required for IDLE task watchdog feeding, but caps effective audio frame rate at ~96 Hz (below 125 Hz target). Documented as Sprint 2 fix (event-driven I2S).

### M-6: FastLedDriver mutex 10ms timeout
**File:** `hal/led/FastLedDriver.cpp:244`
If contended, renderer blocks 10ms (exceeds 8.33ms frame budget). Architecture prevents contention, but reduce to try-lock for safety.

### M-7: volatile globals defined in headers (ODR violation risk)
**Files:** `audio/backends/esv11/vendor/vu.h` (4 volatile floats), `audio/backends/esv11/vendor/goertzel.h` (volatile bool)
Globals defined (not just declared) in headers. If included from multiple translation units, One Definition Rule violation. Works today because ESV11 vendor code is only compiled into one TU.

### M-8: const_cast abuse in WiFiManager (5 sites)
**Files:** `network/WiFiManager.h:369,377,384`, `network/WiFiManager.cpp:577`, `network/WiFiCredentialsStorage.cpp:429`
`const_cast<WiFiCredentialsStorage&>(m_credentialsStorage)` -- indicates member should not be const, or calling methods should not be const.

### M-9: void* m_ps pattern in 15+ effect classes
**Files:** 15+ effects in `effects/ieffect/`
Type-erased PSRAM data under `#else` (non-PSRAM) fallback. Correct by construction but loses all type safety.

### M-10: 8 Enhanced effect variants duplicate base rendering logic (3,781 lines)
**Files:** RippleEnhanced, BPMEnhanced, LGPSpectrumDetailEnhanced, LGPStarBurstEnhanced, LGPWaveCollisionEnhanced, LGPPhotonicCrystalEnhanced, LGPInterferenceScannerEnhanced, ChevronWavesEnhanced
Share rendering boilerplate but rewrite rather than composing/inheriting. ~12h to introduce AudioReactiveEffectBase mixin.

### M-11: AudioHandlers.cpp dual-backend JSON serialisation (1,822 lines)
**File:** `network/webserver/handlers/AudioHandlers.cpp`
Separate ESV11/PipelineCore JSON paths for every audio endpoint. ~4h to extract common serialisation.

### M-12: DynamicShowStore reinterpret_cast on raw memory (alignment risk)
**File:** `core/shows/DynamicShowStore.h:182-184`
Manual memory layout without alignment guarantees. ESP32-S3 (Xtensa) can throw LoadStoreAlignment exceptions on unaligned multi-byte access.

### M-13: Dead PresetManager forward declaration (always nullptr)
**File:** `main.cpp:109-110`, `V1ApiRoutes.cpp:1129`
Forward-declared class never implemented. Pointer always null, passed to handlers that may dereference.

### M-14: V1ApiRoutes preset ID parsing uses uint8_t (12 sites)
**File:** `network/webserver/V1ApiRoutes.cpp:1171-1407`
`uint8_t id = path.substring(...).toInt()` truncates preset IDs > 255. Unvalidated.

### M-15: 2 spectrum effects still using bins64[] directly
**Files:** `LGPSpectrumDetailEffect.cpp:134`, `LGPSpectrumDetailEnhancedEffect.cpp:142`
Uses Goertzel-specific `bins64[]` instead of backend-agnostic `bands[]` + FrequencyMap. TODO comments present.

### M-16: EffectParamUpdate name field oversized (64 bytes, typical names 8-20 chars)
**File:** `core/actors/RendererActor.h:664-670`
16 entries x 70 bytes = 1,120 bytes DRAM. A 32-byte name field saves 512 bytes.

---

## LOW Findings

### L-1: C-style casts in 30+ effect files
Pervasive `(uint8_t)(value)` instead of `static_cast`. No crash risk (constrained inputs) but hinders compiler diagnostics.

### L-2: 5 inline treble calculations should use ctx.audio.treble()
Pattern `(getBand(5) + getBand(6) + getBand(7)) * (1.0f/3.0f)` duplicated instead of using accessor.

### L-3: setCurrentLegacyEffectId() dead stub
**File:** `core/actors/RendererActor.cpp:104-106` -- no-op, never called.

### L-4: Hardcoded WebServer heap threshold (38000)
**File:** `main.cpp:323` -- not configurable via build flags.

### L-5: Health check interval magic number (10000ms)
**File:** `main.cpp:3406` -- should be a named constant.

### L-6: FastLedDriver void* m_controllers array
**File:** `hal/led/FastLedDriver.h:177` -- type-erased to avoid FastLED headers.

### L-7: ZoneHandlers void* for ZoneConfigManager parameter
**File:** `network/webserver/handlers/ZoneHandlers.h:35-37` -- forward declaration would suffice.

### L-8: 960 bytes always allocated for m_transitionSourceBuffer
**File:** `core/actors/RendererActor.h:705` -- transitions happen rarely. Acceptable trade-off.

---

## Churn Hotspots (Git History)

Files with highest change frequency indicate areas of ongoing development or instability:

| File | Commits | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| main.cpp | HIGH | 3,441 | 710 | God object, serial CLI growth |
| AudioActor.cpp | HIGH | 3,039 | 538 | Backend migration work |
| RendererActor.cpp | HIGH | 2,025 | 356 | Effect pipeline evolution |
| WebServer.cpp | MEDIUM | 1,937 | 370 | API endpoint additions |
| V1ApiRoutes.cpp | MEDIUM | 1,722 | -- | Route registration |
| WiFiManager.cpp | MEDIUM | 1,183 | -- | WiFi debugging (now stable) |

---

## Architecture Positives (Preserve These)

These patterns are excellently implemented and should NOT be refactored:

- **Lock-free SnapshotBuffer** for cross-core audio data transfer (zero mutex contention)
- **vTaskDelay(0) in render loop** (yields without 10ms penalty)
- **PSRAM allocation for large buffers** (all effects correctly allocate in init(), never in render())
- **Tone map LUT in .rodata** (256 bytes flash, zero DRAM)
- **Cached renderer state for WebServer** (avoids cross-core reads from HTTP handlers)
- **shouldSkipColorCorrection() gate** (bypasses 6-pass pipeline for effects that don't need it)
- **Zero String() allocations in effects** (no heap alloc in hot paths)
- **100% #pragma once coverage** (415/415 headers)
- **Only 8 TODO markers** for 148K lines (remarkably clean)
- **Low-heap shedding** in WebServer (hysteresis-based protection)

---

## Prioritised Remediation Plan

### Phase 1: CRITICAL Fixes (Week 1, ~8h)

| # | Finding | Effort | Impact |
|---|---------|--------|--------|
| 1 | C-2: familyNameBuf dangling pointer fix | 1h | Fix WebSocket category UB |
| 2 | C-3: V1ApiRoutes uint8_t -> uint16_t (5 sites) | 1h | Fix audio mapping for IDs > 255 |
| 3 | R3: Eliminate getControlBusMut() -- route via actor messages | 4h | Fix most dangerous cross-core race |
| 4 | H-3: Remove leftover agent-log Serial.printf from SpectrumDetail | 0.5h | Remove debug leftover from render path |
| 5 | ~~C-4~~: STALE -- already fixed | -- | -- |
| 6 | C-1: StateStore::getCurrentEffect() return type | 1h | Latent (zero production callers), but correct the API |

### Phase 2: HIGH Performance + Safety (Week 2-3, ~18h)

| # | Finding | Effort | Impact |
|---|---------|--------|--------|
| 7 | H-1: Fuse ColorCorrectionEngine into single pass | 4h | Save 300-500 us/frame (estimated) |
| 8 | R1: Fix getBufferCopy() tearing (double-buffer) | 3h | Prevent torn LED dashboard frames |
| 9 | R2: getCachedAudioFrame/getLastMusicalGrid return-by-value | 2h | Prevent cross-core struct tearing |
| 10 | H-2: Replace Serial.print in TransitionEngine with LW_LOGI | 1h | Remove blocking UART at transition events |
| 11 | H-4: Move forceOneShotCapture() buffers off stack | 2h | Prevent stack overflow |
| 12 | H-6: Deduplicate capture dump handler | 1h | Eliminate protocol divergence risk |
| 13 | H-7: Remove extern globals + fix webServerInstance dual def | 6h | Restore actor model safety |

### Phase 3: HIGH Architecture (Week 3-4, ~12h)

| # | Finding | Effort | Impact |
|---|---------|--------|--------|
| 14 | H-8: Add mutex to persistence singletons | 4h | Prevent NVS corruption |
| 15 | H-9: Consolidate dual MAX_ZONES | 2h | Prevent future OOB bugs |
| 16 | H-5: Begin main.cpp decomposition | 6h | First extraction: SerialCommandHandler |

### Phase 4: MEDIUM Cleanup (Quarter, ~50h)

| # | Finding | Effort | Priority |
|---|---------|--------|----------|
| 14 | M-1: Cache validateEffectId per frame | 1h | Quick win |
| 15 | M-3: Merge silence gate passes | 1h | Quick win |
| 16 | M-4: Extract AudioActor backend strategy | 8h | Maintainability |
| 17 | M-10: Enhanced effect deduplication | 12h | 3,781 lines saved |
| 18 | M-7: Move volatile globals to .cpp files | 2h | ODR compliance |
| 19 | M-8: Fix const_cast patterns in WiFiManager | 2h | Const-correctness |
| 20 | M-11: Extract common AudioHandlers serialisation | 4h | Reduce duplication |
| 21 | M-13: Remove dead PresetManager | 1h | Dead code removal |
| 22 | M-14: Fix preset ID truncation (12 sites) | 2h | Type safety |
| 23 | M-15: Migrate spectrum effects to bands[] | 4h | Backend portability |
| 24 | M-2: Transition trig optimisation | 4h | Conditional perf win |
| 25 | M-5: Event-driven I2S (Sprint 2) | 8h | 125 Hz audio target |

---

## Cross-Core Data Races (Architecture Agent)

These findings come from the architecture coupling agent (a3df9cb) and were verified by source-level inspection.

### R1: getBufferCopy() tearing (HIGH)

**File:** `core/actors/RendererActor.h:214`

`getBufferCopy()` copies 960 bytes (`320 * sizeof(CRGB)`) via `memcpy` while RendererActor on Core 1 writes to `m_leds[]` at 120 FPS. Produces torn frames where first N LEDs are from frame N, remaining from frame N+1. Affects LED dashboard streaming, not physical strip.

**Fix:** Double-buffer `m_leds` and swap atomically at end of `showLeds()`, or add lightweight spinlock.

### R2: getCachedAudioFrame() / getLastMusicalGrid() tearing (MODERATE)

**File:** `core/actors/RendererActor.h`

Return `const &` to `ControlBusFrame` (~2.5 KB) and `MusicalGridSnapshot` (~32 bytes) updated on Core 1 at 120 FPS. WebServer reads from Core 0 can see partially-written data.

**Fix:** Return by value (copy), or use `SnapshotBuffer<T>` pattern.

### R3: getControlBusMut() cross-task mutation (CRITICAL)

**Files:** `audio/AudioActor.h`, `network/webserver/ws/WsAudioCommands.cpp:506`, `network/webserver/handlers/AudioHandlers.cpp:1086,1610`

WebServer obtains unsynchronised **mutable** reference to AudioActor's ControlBus via `getControlBusMut()`. Both WebServer (AsyncTCP task) and AudioActor DSP pipeline run on Core 0 but in different FreeRTOS tasks -- preemptive scheduling makes interleaving possible mid-operation.

**Fix:** Remove `getControlBusMut()`. Route configuration changes through Actor message queue (`SET_ZONE_AGC_CONFIG` etc). Add `SnapshotBuffer<AudioDiagnostics>` for read-only telemetry.

### R4: TempoTracker cross-core shared state (MODERATE)

RendererActor calls `advancePhase()` on Core 1 while AudioActor calls `updateNovelty()` / `updateTempo()` on Core 0. No internal synchronisation. Internal floats could be torn during cross-core reads.

---

## Module Boundary Violations (Architecture Agent)

### V1: network/ directly accesses audio/ internals (CRITICAL)

`AudioHandlers.cpp` and `WsAudioCommands.cpp` call `audio->getControlBusRef()`, `getControlBusMut()`, `getPipelineTuning()`, `getDspState()` -- bypassing the SnapshotBuffer lock-free contract entirely.

### V2: effects/ accesses core/actors/ for registration (MINOR)

`CoreEffects.cpp` and 7 `LGP*Effects.h` files take `RendererActor*` parameter. `PatternRegistry.cpp` calls `ActorSystem::instance().getRenderer()` directly. Should use `IEffectRegistry*` interface.

### V3: audio/ depends upward on plugins/api/ (MODERATE)

`AudioBehaviorSelector.h` includes `../../plugins/api/BehaviorSelection.h` and `EffectContext.h`. Audio domain layer should not depend on plugin API layer.

### V4: codec/ depends upward on core/actors/ and effects/ (MODERATE)

`codec/` includes `RendererActor.h`, `TransitionTypes.h`, `ZoneComposer.h`. Serialisation layer coupled to application types.

### V5: webServerInstance dual definition (MINOR, ODR risk)

`WebServer* webServerInstance = nullptr;` defined in BOTH `main.cpp:89` AND `WebServer.cpp:127`. Technically an ODR violation -- works because toolchain merges common symbols, but fragile.

---

## Missing Default Clauses (Architecture Agent)

38 switch statements without `default:` clause found across the codebase. Notable locations:
- `main.cpp` -- 6 instances in serial handler
- `RendererActor.cpp:660` -- message type dispatch (new message types silently dropped)
- `WiFiManager.cpp:166` -- WiFi state machine
- `AudioActor.cpp` -- 2 instances
- `BehaviorSelection.h` -- 2 instances

---

## Verification Truth Table (Critical + High)

Post-audit source-level verification pass. Each finding re-checked against current code.

| ID | Finding | Verdict | Notes |
|----|---------|---------|-------|
| C-1 | StateStore::getCurrentEffect() uint8_t | **Confirmed, Overstated** | Bug exists (`:91`), but grep shows ZERO production callers -- only doc/README references. Downgrade to MEDIUM. |
| C-2 | Dangling pointer WsEffectsCommands | **Confirmed CRITICAL** | `familyNameBuf[32]` stack-local at `:423`, all 10 pointers alias same dead slot. Real UB. |
| C-3 | V1ApiRoutes uint8_t truncation (5 sites) | **Confirmed CRITICAL** | Lines 406, 421, 434, 446, 458 all use `uint8_t id`. |
| C-4 | KuramotoTransportEffect zone bounds | **STALE** | Line 118 already has `if (zid >= MAX_ZONES) zid = 0`. |
| R3 | getControlBusMut() cross-task mutation | **Confirmed CRITICAL** | 3 sites give WebServer mutable ref to AudioActor state. |
| H-1 | ColorCorrectionEngine 6-pass | **Confirmed** | Lines 181-217: 6 separate iterations over 320 LEDs. Timing (200-800 us) is estimated. |
| H-2 | TransitionEngine Serial.printf | **Confirmed, Overstated** | Lines 162/187 are real, but fire on transition events only (not per-frame). ~5-10/min max. |
| H-3 | Serial.printf in effect render() | **Partially Confirmed** | BeatPulseBloom: gated behind `g_bloomDebugEnabled` toggle -- NOT always-on. SpectrumDetail: NOT gated (just `#ifndef NATIVE_BUILD`), but rate-limited to 10s intervals + labeled `#region agent log` (debug leftover). |
| H-4 | forceOneShotCapture() 1920B stack | **Confirmed** | Lines 1045+1058: two `CRGB[320]` arrays on FreeRTOS stack. |
| H-5 | main.cpp god object (3,441 lines) | **Confirmed** | 42 includes, ~2,500 lines serial handler, cyclomatic complexity 710. |
| H-6 | Duplicate capture dump handler | **Confirmed** | Lines 1502-1553 AND 2191-2246: near-identical binary protocol (magic 0xFD, 16-byte header, RGB payload). First version has `forceOneShotCapture` fallback; second doesn't. |
| H-7 | extern globals bypassing actor model | **Confirmed** | `webServerInstance` (dual definition in main.cpp:89 + WebServer.cpp:127), `zoneConfigMgr` (extern in WebServer.cpp:116 + V1ApiRoutes.cpp:54), `presetMgr` (extern in V1ApiRoutes.cpp:55, always nullptr). |
| H-8 | Persistence singletons no mutex | **Confirmed** | Grep for mutex/semaphore in `core/persistence/` returns ZERO matches. |
| H-9 | Dual MAX_ZONES (8 vs 4) | **Confirmed** | `limits.h:76` = 8, `ZoneDefinition.h:20` = 4. Safe at runtime (zone count clamped) but masks sizing assumptions. |
| R1 | getBufferCopy() tearing | **Confirmed** | 960-byte memcpy without synchronisation, Core 1 writes concurrently. |
| R2 | getCachedAudioFrame() const& tearing | **Confirmed** | Returns reference to Core 1 data read from Core 0. |

**Summary:** 2 CRITICAL confirmed, 1 CRITICAL stale, 1 CRITICAL downgraded to MEDIUM, 1 new CRITICAL added (R3). 9 HIGH confirmed (2 with severity notes). Overall accuracy: **12/14 findings correct** (86%), with 2 severity adjustments needed.

---

## Cross-Reference: Session Audit (findings.md) Additions

The following issues were found by the separate session audit (2026-03-02 through 2026-03-04) and are NOT covered above. They extend this report's coverage.

### Already Implemented (no action needed)

- ControlBusFrame copy pressure: `GetFrameRef()` added, hot-path by-value copies removed
- PipelineCore processHop(): DC/RMS/peak/window loops fused, inner-loop `lroundf` removed
- FFT twiddle caching in `FFT.h` for standard 512-point operation
- SB parity side-car gating (`FEATURE_SB_PARITY_SIDECAR`, `AUDIO_SB_SIDECAR_DECIMATION`)
- Centre-origin violations in 3 effects: LGPBirefringentShear, LGPNeuralNetwork, LGPSolitonWaves
- BloomParity hue-wheel and SpectrumDetail rainbow palette remediations
- BehaviorContext wiring into EffectContext from RendererActor
- Effect contract checker: `tools/check_effect_contracts.py` + CI workflow
- K1 heap/stack warning threshold tuning and delta-based logging
- Tab5 16-bit effect ID support in WebSocketClient and PresetData

### Open Issues (action still needed)

| Finding | Severity | Source |
|---------|----------|--------|
| Tab5 duplicate I2C polling after DualEncoderService::update() | HIGH | Hot-path audit |
| Tab5 per-loop unconditional LVGL UI writes | HIGH | Hot-path audit |
| Tab5 callback-context WS sends (re-entrancy risk in _ws.loop()) | HIGH | Hot-path audit |
| Tab5 effect encoder command bursting (up to 5 cmds/50ms callback) | MEDIUM | Hot-path audit |
| Tab5 connection LED I2C storm during connect phase | MEDIUM | Hot-path audit |
| K1 WebServer unconditional frame copy before subscriber check | HIGH | Hot-path audit |
| iOS stale zones/preset + zones/count endpoints | HIGH | Protocol audit |
| iOS TransitionViewModel sends toEffect=-1 | HIGH | Protocol audit |
| iOS zones.enabledChanged event-name gap | MEDIUM | Protocol audit |
| Build profile documentation drift (PipelineCore primary in docs, K1 target is ESV11) | MEDIUM | CDTP audit |

---

## Methodology Notes

**Analysis team:** 4 parallel subagents + manual investigation + post-audit verification pass
- **Type Safety Agent** (aec7687): 45 files read, 30+ grep patterns, found C-1 through C-3, H-9, M-9, M-14
- **Performance Agent** (a0266ec): 14 bottlenecks identified, timing estimates based on ESP32-S3 cycle counts
- **Complexity Agent** (ad8d9e9): All 737 files searched, cyclomatic complexity measured, dead code inventory
- **Architecture Agent** (a3df9cb): Singleton inventory, mutex audit, module boundary analysis, dependency direction, cross-core data races
- **Verification Pass:** Every Critical/High finding re-verified against current source code. 12/14 confirmed correct, 1 stale (C-4), 1 severity adjustment (C-1).
- **Cross-Reference:** Compared against separate session audit (findings.md/progress.md/task_plan.md) to identify overlapping and unique findings.

**Verification:** All CRITICAL and HIGH findings re-verified by reading actual source code with line numbers. Truth table above documents each verdict. MEDIUM findings verified by pattern-based grep detection.
