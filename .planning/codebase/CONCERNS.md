---
abstract: "Inventory of technical debt, security risks, and known bugs across firmware-v3 (ESP32-S3), iOS, and Tab5. Covers critical type-safety issues, audio backend instability, cross-core data races, network security, memory fragility, and architectural coupling. All findings verified against current source code (2026-03-21)."
---

# Codebase Concerns

**Analysis Date:** 2026-03-21

## Critical Issues (MUST FIX BEFORE PRODUCTION)

### C-1: PipelineCore Audio Backend BROKEN — Beat Tracking Non-Functional

**Severity:** CRITICAL
**Files:** `firmware-v3/src/audio/pipeline/PipelineCore.cpp`, `firmware-v3/src/audio/AudioActor.cpp`
**Status:** Confirmed broken, mitigated (use ESV11 instead)

**Issue:** The PipelineCore audio backend (FFT-based) has non-functional beat tracking after migration from Goertzel. This produces unreliable audio data, especially tempo and beat detection.

**Details:**
- Build environment `esp32dev_audio_pipelinecore` is broken and unusable
- Production builds MUST use `esp32dev_audio_esv11_k1v2_32khz` (K1) or `esp32dev_audio_esv11_32khz` (V1)
- PipelineCore produces correct spectral data but beat tracking state diverges from reality
- Goertzel-based ESV11 backend is the only stable audio path

**Impact:**
- Any firmware built with PipelineCore will have unstable audio-reactive effects
- Beat-dependent shows and tap-tempo will not work correctly
- All tempo/beat/onset features are unavailable

**Fix approach:**
- Remove PipelineCore from production build matrix (keep for research only, gate behind `EXPERIMENTAL`)
- Update all build documentation to reflect ESV11 as primary backend
- All future audio enhancements must validate against ESV11 only

**Workaround:** Already in place — use ESV11 build environments exclusively.

---

### C-2: ~~Type Truncation in V1 Audio Mapping API~~ — RESOLVED

**Severity:** ~~CRITICAL~~ RESOLVED
**Files:** `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`
**Status:** Already fixed — endpoints use uint16_t with int→uint16_t validation and HTTP 400 on out-of-range. Verified 2026-03-21.

**Issue:** Five audio mapping endpoints parse effect IDs as `uint8_t` instead of `uint16_t`, silently truncating IDs > 255. EffectId is a 16-bit namespaced type (high byte = family). Effect IDs outside family 0 (0x0100+) are truncated to 0.

**Code pattern:**
```cpp
uint8_t id = request->getParam("id")->value().toInt();  // TRUNCATES to 0x00
handlers::AudioHandlers::handleMappingsGet(request, id, ctx.renderer);  // Expects EffectId (uint16_t)
```

**Impact:**
- Audio mappings cannot be set for any effect with ID >= 256
- Silent data corruption — client sends valid ID, K1 silently ignores it
- Affects: `/rest/audio/mappings/get?id=X`, `/rest/audio/mappings/delete?id=X`, and 3 other endpoints

**Fix approach:**
- Change `uint8_t id` to `EffectId id` (or `uint16_t`) in all 5 handlers
- Add validation: `if (id >= MAX_EFFECT_ID) return error(400)`
- 1-hour fix

---

### C-3: ~~Dangling Pointer in WebSocket Effects Command~~ — RESOLVED

**Severity:** ~~CRITICAL~~ RESOLVED
**Files:** `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp`
**Status:** Already fixed — uses `char familyNameBufs[10][32]` outside loop with per-family persistent buffers. Verified 2026-03-21.

**Issue:** Function `handleEffectsGetCategories()` creates a stack-local `char familyNameBuf[32]`, fills it in a loop, and stores pointers to it in an array. All 10 pointers alias the same dead stack slot after the loop ends. WebSocket response contains garbage or last-written name.

**Code pattern:**
```cpp
const char* familyNames[10];
for (uint8_t i = 0; i < 10; i++) {
    char familyNameBuf[32];  // NEW stack allocation each iteration, destroyed at end
    PatternRegistry::getFamilyName(family, familyNameBuf, sizeof(familyNameBuf));
    familyNames[i] = familyNameBuf;  // Pointer to stack variable destroyed immediately
}
// familyNames[0..9] all point to same dead address
```

**Impact:**
- WebSocket `/effects/categories` response contains undefined data
- Affects any client parsing effect family names

**Fix approach:**
- Move `char familyNameBufs[10][32]` outside the loop, or
- Use `static` buffer with reset each iteration
- 30 minutes

---

### C-4: ~~Cross-Core Data Race — getControlBusMut()~~ — RESOLVED

**Severity:** ~~CRITICAL~~ RESOLVED
**Files:** `firmware-v3/src/audio/AudioActor.h`
**Status:** Already fixed — getControlBusMut() removed entirely, no callers remain. Audio config mutations route through actor message queue. Snapshot API used for read-only access. Verified 2026-03-21.

**Issue:** WebServer (AsyncTCP task on Core 0) obtains **mutable** reference to AudioActor's ControlBus via `getControlBusMut()`, allowing unsynchronised mutation of DSP state while AudioActor processes audio on Core 0 (different FreeRTOS task, preemptive scheduling).

**Details:**
- Both AsyncTCP and AudioActor run Core 0, different tasks
- Preemptive FreeRTOS scheduler allows interleaving mid-operation
- Audio configuration changes (zone AGC, envelope thresholds) can be partially written while audio loop reads them
- Torn reads of multi-byte fields (float) across 32-bit boundaries possible

**Impact:**
- Audio parameter mutations can corrupt audio processing state
- Potential for audio dropouts, DSP anomalies, or crashes during parameter edits
- Least likely to manifest (slow mutation rate, fast reads), but UB nonetheless

**Fix approach:**
- Remove `getControlBusMut()` entirely
- Route all audio config changes through actor message queue (`SET_ZONE_AGC_CONFIG`, etc.)
- Add `SnapshotBuffer<AudioDiagnostics>` for read-only telemetry access
- 3-4 hour refactor

**Current mitigation:** WebServer audio subscriber has guard to avoid constant polling, reducing contention window.

---

## High-Severity Issues (FIX SOON)

### H-1: LED Stability Regression (March 4 Incident)

**Severity:** HIGH
**Files:** Affects entire firmware-v3, especially render pipeline
**Commit Range:** `dfc05298` -> `9a59e899` (Phase-0 cluster)
**Status:** Mitigated with rollback strategy, reattempt in progress

**Issue:** Multiple overlapping commits introduced render/network refactors that caused:
- Per-strip stutter and corruption
- RMT channel faults
- loopTask stack overflow
- Watchdog aborts

**Root causes identified:**
1. Large render/network refactors in Phase-0 commit cluster
2. `loopTask` stack pressure from large `ControlBusFrame` copies in WebServer audio streaming
3. Runtime heap pressure under AP+WS load (frequent low-heap shedding)

**Details:**
- `f4c579ed` is confirmed clean baseline (pre-Phase-0)
- `54765b16` is NOT a clean render baseline (post-Phase-0, includes risky changes)
- Non-K1 ESV11 profile has stack budget mismatch (loop stack ~8KB, high pressure at 93-96%)
- Commit `4a8338d6` contains largest behavioural risk (broad effect render changes)

**Impact:**
- K1 devices unstable on latest code; require careful rollback/cherry-pick
- Multi-effect cycling causes visible corruption or stuttering
- WebServer audio broadcast paths cause loopTask stack exhaustion

**Fix approach:**
1. **Stage 0:** Rollback to `f4c579ed`, validate 30-min soak (DONE, tag: `stage0-k1-bootclean-2026-03-04`)
2. **Stage 1:** Selective cherry-pick of safe changes with per-step validation
   - Currently at commit `32703522` (Stage 1 complete, both strips smooth)
   - Apply fixes: data race guards, snapshot API, stack profiling
3. **Stage 2:** Lock discipline for further changes
   - Verify target MAC before every flash
   - One variable change per test cycle
   - Separate render/network/performance changes into different batches

**Reference:** `firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md`, `docs/MULTI_WORKTREE_TESTING_GUIDE.md`

---

### H-2: Persistence Managers Missing Mutex Protection

**Severity:** HIGH
**Files:** `firmware-v3/src/core/persistence/AudioTuningManager.h:71`, `NVSManager.h:71`, `EffectPresetManager.h:139`, `ZonePresetManager.h:144`
**Status:** Confirmed, deferred

**Issue:** 10+ singleton persistence managers use `static T& instance()` pattern. Grep for mutex/semaphore in `core/persistence/` returns zero matches. These are accessed from both Core 0 (audio/network) and Core 1 (renderer) without synchronisation.

**Details:**
- NVS operations are not atomic
- Concurrent reads/writes from different cores can corrupt stored preferences
- AudioTuningManager, EffectPresetManager, ZonePresetManager all unprotected
- Safe at runtime today (low mutation frequency) but architectural violation

**Impact:**
- If user edits presets/tunings while renderer updates them, data corruption possible
- NVS flash wear/tear on corrupted writes
- Lost preset data across reboots

**Fix approach:**
- Add `portMUX_TYPE` spinlock to critical sections in all persistence managers, OR
- Ensure single-core access via actor message routing (preferred, architecturally cleaner)
- 4 hours

---

### H-3: ColorCorrectionEngine 6-Pass Pipeline (200-800 µs)

**Severity:** HIGH
**Files:** `firmware-v3/src/effects/enhancement/ColorCorrectionEngine.cpp:181-217`
**Status:** Confirmed, deferred (low frame budget impact)

**Issue:** Six separate passes over 320 LEDs: autoExposure, brightnessClamp, saturationBoost, whiteGuardrail, brownGuardrail, gamma. Each pass:
- Iterates all 320 LEDs
- Calls `rgb2hsv_approximate()` per pixel (~20 cycles)
- Reconstructs CRGB from CHSV (~40 cycles)

Total overhead: 200-800 µs per frame (0.4-1.6% of 8.33ms budget), but cache-inefficient.

**Impact:**
- Modest frame time penalty
- Cache pressure on other effects
- Cumulative impact on complex effect chains

**Fix approach:**
- Fuse all 6 passes into single LED iteration
- Eliminates 5 buffer traversals + cache reloads
- Saves 300-500 µs per frame
- Effort: 4 hours

---

### H-4: main.cpp God Object (3,441 lines)

**Severity:** HIGH
**Files:** `firmware-v3/src/main.cpp`
**Status:** Confirmed, maintainability concern

**Issue:** Single file conflates 7+ responsibilities:
- Setup (board init, sensor calibration)
- Serial JSON gateway (795 lines)
- Serial CLI (2,101 lines, 42 command handlers)
- WiFi management CLI
- Show playback management
- Effect parameter tuning hotkeys
- System health monitoring

**Details:**
- 42 `#include` directives
- Cyclomatic complexity: 710 branching constructs
- High churn rate in git history
- Makes it difficult to isolate bugs or add features

**Impact:**
- Harder to maintain and debug
- Increased risk of unintended side effects when modifying
- Difficult to test individual features
- New developers struggle with orientation

**Fix approach:**
- Extract `SerialCommandHandler`, `SerialJsonGateway`, `ShowPlaybackController` classes
- 16 hours effort, spread across multiple sessions
- Priority: LOW (works correctly, maintainability only)

---

### H-5: loopTask Stack Pressure During WebServer Audio Streaming

**Severity:** HIGH
**Files:** `firmware-v3/src/main.cpp:2200+`, `firmware-v3/src/network/WebServer.cpp:200+`
**Status:** Confirmed, mitigated

**Issue:** WebServer `broadcastAudioFrame()` copies large structures on loop stack:
- `ControlBusFrame` (~2.5 KB)
- `MusicalGridSnapshot` (~32 bytes)
- Large temporary buffers in audio handlers

During AP+WS load, loopTask stack usage peaks at 93-96% headroom. Stack overflow kills the device.

**Impact:**
- Non-K1 builds especially vulnerable (smaller stack budget)
- Any WS audio broadcast client triggers stack pressure
- Risk of Guru Meditation (stack canary watchpoint) crashes

**Fix approach (DONE):**
- Move buffers to persistent member storage instead of stack allocation
- Use snapshot copy API for audio frame access (done, `forceOneShotCapture` moved to member buffer)
- Reduce `ControlBusFrame` copies from 3 per broadcast to 1 via caching (done, snapshot API)

---

## Medium-Severity Issues (SCHEDULE FOR NEXT QUARTER)

### M-1: Dual MAX_ZONES Constants (8 vs 4)

**Severity:** MEDIUM
**Files:** `firmware-v3/src/config/limits.h:76` (8), `firmware-v3/src/effects/zones/ZoneDefinition.h:20` (4)
**Status:** Confirmed, consolidated (fixed)

**Issue:** Two different `MAX_ZONES` constants in different namespaces. Safe at runtime (zone count clamped to 4), but masks array-sizing assumptions and increases regression risk.

**Details:**
- `lightwaveos::limits::MAX_ZONES = 8`
- `lightwaveos::zones::MAX_ZONES = 4`
- If code allocates with one constant but iterates with the other, OOB access occurs

**Impact:** Low today (clamping prevents overflow), but architectural debt

**Fix approach (DONE):** Consolidated to single source of truth.

---

### M-2: getBufferCopy() Tearing (High-Frequency Memcpy)

**Severity:** MEDIUM
**Files:** `firmware-v3/src/core/actors/RendererActor.h:214`
**Status:** Confirmed, deferred (cosmetic only)

**Issue:** `getBufferCopy()` copies 960 bytes (`320 * sizeof(CRGB)`) via memcpy while RendererActor on Core 1 writes to `m_leds[]` at 120 FPS. Produces torn frames where first N LEDs are from frame N, remaining from frame N+1.

**Impact:**
- LED dashboard streaming shows occasional tearing
- Physical strip unaffected (native refresh is per-frame atomic)
- Mostly cosmetic for dashboard UI

**Fix approach:**
- Double-buffer `m_leds` and swap atomically at end of `showLeds()`, OR
- Add lightweight spinlock around memcpy
- Effort: 2 hours

---

### M-3: Frame Pacer Uses esp_rom_delay_us Busy-Wait

**Severity:** MEDIUM
**Files:** `firmware-v3/src/core/actors/RendererActor.cpp:878`
**Status:** Confirmed, backlog item P1

**Issue:** Self-clocked frame pacing uses `esp_rom_delay_us()` (CPU spin) for the remainder of the 8.33ms budget. When effects render fast (<2ms), burns 1-2ms of Core 1 CPU with no yield.

**Details:**
- Practically low impact (typically 0-1ms spin since show() takes ~6.3ms)
- IDLE1 gets CPU during semaphore wait inside FastLED.show()
- No watchdog risk (explicit WDT reset every 10 frames)
- Clean fix possible with minimal effort

**Impact:** Modest power/thermal overhead, no correctness issue

**Fix approach:**
- Replace with `esp_timer` one-shot + `ulTaskNotifyTake()` for event-driven pacing
- Zero-overhead wait, no tick rate change needed
- Effort: 3 hours
- Priority: P1 in BACKLOG.md

---

### M-4: Serial.printf in Effect Render Paths

**Severity:** MEDIUM
**Files:** `firmware-v3/src/effects/ieffect/LGPSpectrumDetailEffect.cpp:53,72,143,256`, `firmware-v3/src/effects/ieffect/BeatPulseBloomEffect.cpp:138,146,196,216`
**Status:** Confirmed (BeatPulseBloom gated, SpectrumDetail not)

**Issue:** Debug logging left in render hot paths. UART blocking at 115200 baud. `Serial.printf()` blocks for 200-500 µs per message.

**Details:**
- BeatPulseBloomEffect: gated behind `g_bloomDebugEnabled` toggle (safe)
- LGPSpectrumDetailEffect: marked as `#region agent log` (debug leftover), rate-limited to 10s intervals
- Still adds unpredictable frame time jitter

**Impact:**
- Frame drops during debug logging
- Unreliable performance measurement
- Confuses FPS telemetry

**Fix approach:**
- Remove all debug logging from render methods, OR
- Replace with `LW_LOGI()` (unified logging, lower overhead), OR
- Gate behind `#if LW_LOG_LEVEL >= 4`
- Effort: 2 hours

---

### M-5: Spectrum Effects Using Goertzel `bins64[]` Instead of `bands[]`

**Severity:** MEDIUM
**Files:** `firmware-v3/src/effects/ieffect/LGPSpectrumDetailEffect.cpp:134`, `firmware-v3/src/effects/ieffect/LGPSpectrumDetailEnhancedEffect.cpp:142`
**Status:** Confirmed, deferred (shim works, TODO comments in place)

**Issue:** Two spectrum visualisers still directly access Goertzel-specific `bins64[]` instead of backend-agnostic `bands[]` + FrequencyMap. Only works with ESV11; would break if PipelineCore ever became functional again.

**Details:**
- `bands[0..7]` (octave bands) is the stable API across backends
- `bins64[]` is Goertzel-specific internal state
- Effects should use `FrequencyMap` helper for accurate frequency-to-LED mapping

**Impact:**
- Architectural coupling to audio backend
- Code cannot be easily ported to other backends
- Regression risk if audio architecture changes

**Fix approach:**
- Migrate to `bands[]` + FrequencyMap
- Use helper: `(bands[5] + bands[6] + bands[7]) * (1.0f / 3.0f)` for treble
- Effort: 4 hours
- Priority: M-15 in debt audit

---

### M-6: AudioActor Triple-Backend in Single File

**Severity:** MEDIUM
**Files:** `firmware-v3/src/audio/AudioActor.cpp` (3,039 lines)
**Status:** Confirmed, deferred

**Issue:** Three complete backend implementations via `#if` preprocessor:
- PipelineCore (dead stubs: `getLastHop()` returns nullptr, `hasNewHop()` returns false)
- ESV11 (active)
- Unused third path

**Details:**
- Dead code complicates maintenance
- Hard to reason about which code path is active
- Increases cyclomatic complexity

**Impact:**
- Maintainability debt
- Confusion during debugging
- Risk of accidentally activating dead PipelineCore path

**Fix approach:**
- Extract strategy pattern: `IAudioBackend` interface with `PipelineCore` and `ESV11` implementations
- Runtime backend selection via factory or config
- Effort: 8 hours
- Priority: Can defer until PipelineCore is removed

---

## Security Concerns

### S-1: Open WiFi AP, No Authentication

**Severity:** HIGH
**Files:** `firmware-v3/src/network/WiFiManager.h/cpp`
**Status:** By design, architectural constraint

**Issue:** K1 operates as open WiFi Access Point (no password). Any device within range can connect.

**Details:**
- K1 forces `WIFI_MODE_AP` (never `WIFI_MODE_APSTA`)
- AP uses open network (no WPA2, eliminates WPA handshake as failure point)
- Network traffic is unencrypted
- REST API has no authentication

**Impact:**
- Local network only (802.11 range ~30-50m), mitigates some risk
- Devices on LAN can control LED strip, change effects, access device state
- No audit trail of who sent commands
- Vulnerability to replay attacks if recording network traffic

**Mitigation:**
- Network isolation (private LAN, no internet bridge)
- Only trusted devices have WiFi access
- App-level API key support exists but not enforced

**Recommendation:**
- Add optional WPA2-PSK with configurable passphrase (user-set at provisioning)
- Implement basic API key/token validation in REST handlers
- Store credentials in NVS (secure boot optional)

---

### S-2: No API Authentication or Rate Limiting

**Severity:** MEDIUM
**Files:** `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`, `firmware-v3/src/network/webserver/handlers/`
**Status:** By design

**Issue:** All REST endpoints accept requests from any connected client. No per-client rate limiting, no API key validation.

**Details:**
- Any WebSocket or REST client can send unlimited requests
- Endpoints change effects, presets, audio config, system state
- No differentiation between privileged and unprivileged actions
- Flood attack possible (e.g., rapid effect changes)

**Impact:**
- DoS risk: flood device with commands, cause watchdog reset
- Unauthorized control (if network is compromised)
- No accountability for changes

**Mitigation:**
- Per-client request bucketing (existing WebSocket rate limiter is partial)
- Optional API key validation
- Command signature/HMAC validation

**Recommendation:**
- Implement per-IP rate limiter in WebServer::update()
- Add optional `apiKey` header validation
- Log all commands to NVS (optional, for audit)

---

### S-3: Unencrypted Network Traffic

**Severity:** MEDIUM
**Files:** All network handlers
**Status:** By design

**Issue:** WiFi AP runs on open network with no encryption. All traffic (JSON, audio frames, commands) visible to any local device.

**Impact:**
- Audio data reveals song/tempo information
- Commands reveal user preferences
- Replay attacks possible if recording network

**Mitigation:**
- Network isolation
- Avoid transmitting sensitive data over the network

**Note:** This is typical for IoT devices on private networks. Not a blocker.

---

## Fragile Areas (High Risk of Regression)

### F-1: Effect Zone ID Bounds Checking

**Severity:** MEDIUM (correctness fragility)
**Files:** 154 effect implementations in `firmware-v3/src/effects/ieffect/`
**Status:** Confirmed fix applied (RMT spinlock crash fix)

**Issue:** Every effect that accesses `ctx.zoneId` must bounds-check before array access. Global render sets `ctx.zoneId = 0xFF`. Missing bounds check causes array OOB write.

**Example (FIXED):**
```cpp
// WRONG (was in EsAnalogRefEffect.cpp, EsOctaveRefEffect.cpp)
float& var = m_vuSmooth[ctx.zoneId];  // If zoneId=0xFF, writes 1020 bytes past array

// CORRECT (now everywhere)
const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
float& var = m_vuSmooth[z];
```

**Status:** All checked and fixed as of 2026-03-05 after RMT spinlock crash investigation.

**Ongoing risk:** New effects must follow this pattern without exception. Code review gate required.

---

### F-2: Heap Allocation in Render Path

**Severity:** CRITICAL (must prevent)
**Files:** All effect implementations
**Status:** Good adherence, monitored

**Hard constraint:** No `new`, `malloc`, `String()`, or any heap allocation in `render()` or functions called from it.

**Why:**
- FreeRTOS heap is shared across cores
- Allocation during render can block audio path on Core 0
- Fragmentation risk
- Watchdog timeouts

**Current status:**
- All effects correctly allocate in `init()`, not `render()`
- PSRAM buffers for large data structures
- String concatenation avoided in hot paths

**Risk:** New effects from contributors might not follow this pattern. Need CI check.

---

### F-3: WiFi Mode Must Remain AP-Only

**Severity:** CRITICAL (architectural constraint)
**Files:** `firmware-v3/src/network/WiFiManager.h/cpp`
**Status:** Hard constraint

**Issue:** K1 is architecturally locked to AP mode. STA (client) connection attempts fail at ESP-IDF 802.11 driver level (AUTH_EXPIRE reason 2, AUTH_FAIL reason 202).

**Why K1 can't be STA:**
- WiFi stack shares encryption key pools between AP and STA modes
- AP operation corrupts STA auth state
- 6+ mitigation attempts (compatibility profiles, BSSID scrubs, PMF options) all failed
- Root cause: firmware architecture constraint, not fixable at driver level

**Current status:**
- `m_forceApOnly` flag prevents state machine from retrying STA
- STA only available via serial `wifi connect` escape hatch (unreliable)
- Production builds locked to AP-only

**Risk:** Future agents might attempt to "fix" STA connectivity. This will fail. **Do not attempt.**

Reference: CLAUDE.md "WiFi Architecture" section.

---

## Build & Configuration Issues

### B-1: Build Profile Documentation Drift

**Severity:** MEDIUM
**Files:** Multiple docs (README.md, AGENTS.md, firmware-v3/docs/)
**Status:** Confirmed, partially addressed

**Issue:** Documentation still references PipelineCore as primary backend, but K1 production target is ESV11.

**Examples:**
- README.md emphasizes beat tracking (PipelineCore feature)
- AGENTS.md lists PipelineCore as primary
- Some docs suggest PipelineCore is usable (it's not)

**Impact:**
- New developers confused about which backend to use
- Wasted time trying to debug PipelineCore
- Incorrect build advice

**Fix approach:**
- Update README to clarify: "ESV11 is production (stable), PipelineCore is research-only (broken beat tracking)"
- Update build docs to remove PipelineCore from quick-start
- Add "DO NOT USE PipelineCore" warning prominently
- Effort: 3 hours

---

### B-2: K1v2 GPIO Override Guards Required for Rollback Builds

**Severity:** HIGH (for older commits)
**Files:** `firmware-v3/src/config/chip_esp32s3.h`, `firmware-v3/src/audio/backends/esv11/vendor/microphone.h`
**Status:** Fixed on main, but pre-phase-0 commits need patching

**Issue:** Before commit `f4c579ed`, GPIO pin definitions were hardcoded (not respecting `-D K1_*` build flags). K1v2 hardware uses non-default pins.

**Details:**
- LED pins: default 4/5, K1v2 uses 6/7
- I2S pins: default 12/13/14, K1v2 uses 11/13/14
- Build flags exist but had no effect without `#ifdef` guards in source

**Impact:** When building from pre-Phase-0 commits for K1v2, LEDs are dark and audio is silent.

**Fix approach (for rollback):**
- Add `#ifdef K1_LED_STRIP1_DATA` guards in `chip_esp32s3.h`
- Add `#ifndef I2S_LRCLK_PIN` / `#ifdef K1_I2S_LRCL` guards in `microphone.h`
- Patches available in commit `e39b1152`

---

## Test Coverage Gaps

### T-1: WebSocket Command Routing Incomplete Tests

**Severity:** LOW
**Files:** `firmware-v3/test/test_native/test_ws_gateway.cpp`
**Status:** Confirmed, skipped tests

**Issue:** Test file has TODO markers for critical functionality:
- Rate limiter callback not tested
- Auth callback not tested
- Invalid JSON rejection not tested
- Message routing not tested

**Impact:**
- WebSocket request validation untested
- Regressions possible in API stability

**Fix approach:**
- Implement 4 skipped test cases
- 3 hours

---

### T-2: HTTP Route Registration Not Fully Tested

**Severity:** LOW
**Files:** `firmware-v3/test/test_native/test_webserver_routes.cpp:85,101,114`
**Status:** Confirmed, stubbed

**Issue:** Route registration tests are stubbed with TODO comments. No coverage for:
- Static asset routes
- Legacy API routes
- V1 API routes

**Impact:**
- Route registration bugs not caught
- Endpoint availability not verified

**Fix approach:**
- Call actual route registration functions in tests
- 4 hours

---

### T-3: Integration Contract Tests Incomplete

**Severity:** LOW
**Files:** `firmware-v3/test/test_native/test_webserver_integration_contract.cpp`
**Status:** Confirmed, stubbed

**Issue:** Contract tests are all TODOs. No validation of:
- HTTP endpoint response formats
- WebSocket response formats
- Error response consistency

**Impact:**
- Client compatibility issues not detected
- Contract drift possible

**Fix approach:**
- Implement contract assertions
- 5 hours

---

## Performance Concerns

### P-1: validateEffectId() Called 3x Per Frame

**Severity:** LOW
**Files:** `firmware-v3/src/core/actors/RendererActor.cpp:774, 1448, 1645`
**Status:** Confirmed, fixed (DONE)

**Issue:** Linear scan of 256 entries × 3 per frame (~9 µs wasted). Should cache on effect change only.

**Fix approach (DONE):** Cached validation result on effect change, eliminated 3 render-path calls.

---

### P-2: TransitionEngine Heavy Trig Math (50-200 µs during transitions)

**Severity:** LOW
**Files:** `firmware-v3/src/effects/transitions/TransitionEngine.cpp:477-1048`
**Status:** Confirmed, deferred

**Issue:** Six transition types use `sinf()`, `cosf()`, `atan2f()` per pixel. Only affects ~5% of frames (transition events rare).

**Fix approach:**
- Use `sin8()/cos8()` or pre-computed LUTs
- Effort: 4 hours

---

## Documentation & Process Issues

### D-1: Reference Files Must Be Kept Current

**Severity:** MEDIUM
**Files:** `firmware-v3/docs/reference/codebase-map.md`, `firmware-v3/docs/reference/fsm-reference.md`
**Status:** Maintained, but requires discipline

**Issue:** Reference files are generated/maintained manually. If source structure changes, docs drift.

**Fix approach:**
- Add CI check to detect unmaintained reference docs
- Update reference docs with any major refactor

---

## Summary of Severity Distribution

| Severity | Count | Examples |
|----------|-------|----------|
| CRITICAL | 4 | PipelineCore broken, Type truncation, Dangling pointer, Data race |
| HIGH | 5 | LED stability, Persistence mutex, ColorCorrection, main.cpp, loopTask |
| MEDIUM | 8 | MAX_ZONES, Tearing, Frame pacer, Serial.printf, bins64[], AudioActor strategy |
| LOW | 4 | validateEffectId caching, TransitionEngine trig, WebSocket tests |

**Total estimated remediation:** ~120 hours
- CRITICAL+HIGH: ~50 hours (most already addressed or mitigated)
- MEDIUM: ~50 hours (architectural cleanup, deferred)
- LOW: ~20 hours (performance optimizations, tests)

---

## Recommended Immediate Actions (Next Sprint)

1. **Merge LED stability reattempt** — All Stage 1 cherry-picks complete, K1 running stably (`32703522`). Gate main branch on this baseline.
2. **Remove PipelineCore from build matrix** — Mark as `EXPERIMENTAL`-only, update all docs.
3. **Fix three CRITICAL type-safety issues** — C-2 (V1 API), C-3 (dangling ptr), C-1 (StateStore return type). 2-3 hour effort total.
4. **Eliminate getControlBusMut()** — Route audio config changes through actor messages. 4 hours.
5. **Validate M-5 spectrum effects** — Migrate to `bands[]` if audio backend changes become likely.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-21 | agent:concerns | Created comprehensive concerns document. Verified all CRITICAL and HIGH findings against current source. Documented LED stability incident, PipelineCore status, security posture, and prioritized remediation. |
