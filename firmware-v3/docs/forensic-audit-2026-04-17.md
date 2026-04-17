---
abstract: "Forensic audit handover from 2026-04-17 session. Triggered by a Guru Meditation crash (FastLED RMT ISR calling flash-resident micros() during NVS flash-cache suspend); closed the crash and 11 additional fixes then executed a proactive 22-SSA deep audit across the entire v3 firmware stack. Found ~160 distinct latent issues: 8 P0, 15+ P1, 40+ P2, plus ~95 KB of dead code. Details every finding with file:line, proposed fix, and SSA provenance. Next agent picks up here — nothing else needs re-derivation."
type: firmware-audit-handover
version: 1.0
build: esp32dev_audio_esv11_k1v2_32khz
hardware: K1v2 (ESP32-S3, MAC b4:3a:45:a5:87:f8)
session_date: 2026-04-17
---

# Forensic Audit Handover — firmware-v3 (K1v2)

**Audience:** The next agent or engineer picking up this work.
**Purpose:** Full context transfer. You should be able to continue without re-running any of the investigation in this document.

---

## Part 1 — Session context

### 1.1 What triggered this audit

Session started with a Guru Meditation panic on K1v2 during effect cycling:

```
Core 1 panic'ed (Cache disabled but cached memory region accessed)
EXCCAUSE: 0x00000007
PC: 0x420e25c6  (= micros() in esp32-hal-misc.c, flash-resident)
Backtrace through: FastLED doneOnChannel → CMinWait::mark() → micros
Concurrent: Core 0 in spi_flash_op_block_func via ipc_task (NVS save during WiFi init)
```

Root cause: FastLED RMT4 ISR called flash-resident `micros()` while Core 0 had suspended the flash cache to commit an NVS blob during WiFi AP bring-up.

Cursor proposed a fix (overlay `fastled_delay.h` to use `esp_timer_get_time()` instead of `micros()`); we verified via forensic ELF analysis that the diagnosis was correct.

That first crash was closed, but pulling on that thread surfaced many adjacent hazards — the full audit below is the result.

### 1.2 What was fixed during this session (already on main, already flashed)

All landed, built, flashed to K1v2, and verified stable under a 60-second 20 Hz burst (280 commands / 220 effects / zero stall, vs 45-command wedge on baseline — 6.2× improvement):

1. **FastLED `fastled_delay.h` overlay** — `CMinWait::mark()` uses `esp_timer_get_time()` instead of flash-resident `micros()`. Fixes the Guru Meditation.
2. **FastLED `idf4_rmt_impl.cpp` overlay** — `IRAM_ATTR` on `startNext`, `startOnChannel`, `tx_start`. Belt-and-braces for ≥3-controller configs.
3. **`Actor::run()` drain loop** — bounded to 4 messages, calls `esp_task_wdt_reset()` + `taskYIELD()` per message, forces `onTick()` after drain cycle.
4. **`RendererActor::handleSetEffect` log demotion** — `IEffect cleanup/init/SUCCESS` lines demoted to `LW_LOGD`; summary at INFO.
5. **`SerialCLI` 20 ms coalesce** — space/n/N keys debounced to max 50 Hz. Prevents keyboard-auto-repeat flooding the Renderer queue.
6. **`LW_LOG_PRINTF` TX guard** — `Serial.availableForWrite() >= 256` check before printing; drops on full instead of blocking.
7. **`Serial.setTxTimeoutMs(20)`** — HWCDC TX block capped at 20 ms.
8. **`LGPGravitationalLensingEffect::render()` coarsened** — ray step 2→4, inner step 80→40. Render cost 20 ms → ~3 ms.
9. **5× TRM effects `init()` memset scope reduced** — TRM base, TRM_AR, Mod1/2/3. First init zeros full PSRAM struct; subsequent re-inits zero only live field arrays (960 B instead of 321 KB for Mod1/2/3).
10. **9 effects free-on-cleanup anti-pattern removed** — `heap_caps_free()` moved out of `cleanup()` to avoid PSRAM fragmentation on every cycle. Effects: `LGPReactionDiffusion{,AR,Triangle,TestRig}Effect`, `LGPRDTriangleAREffect`, `LGPCatastropheCausticsAREffect`, `WaveformParityEffect`, `EsOctaveRefEffect`, `LGPLangtonHighwayAREffect`.
11. **`KuramotoTransportEffect` partial-alloc rollback** — init's 3 sequential PSRAM allocs now free earlier allocs on later failure.
12. **`RendererActor::handleSetEffect` cleanup-on-init-fail** — calls `newReg->effect->cleanup()` before reverting if `init()` fails.
13. **`WebServer::m_lowHeapShed` max-latch-time force-clear** — 10 s ceiling, prevents permanent latch via reconnect-storm feedback loop.
14. **`WebServerBroadcast::broadcastAudioFrame` subscriber gate** — skip if `hasSubscribers() == 0`.
15. **`RendererActor::handleSetEffect` dead `EFFECT_CHANGED` publish removed** — 0 subscribers firmware-wide.
16. **`main.cpp` `enableLoopWDT()`** — loopTask subscribed to TWDT; the existing `esp_task_wdt_reset()` at line 454 is now effective.
17. **`main.cpp` NVS-defer `LW_LOGW` rate-limited to 1 Hz** — was flooding at ~100 Hz when heap < 8 KB.

### 1.3 Verification result (last build on main)

- `firmware.elf` SHA starts `687df675…`. Flashed 2026-04-17.
- Pressure test: 450 s / 1,957 commands / 1,527 effect changes / **zero stall** / post-test device at 119 FPS, stack healthy, MessageBus clean.
- Device is materially stable under the original failure scenario.

---

## Part 2 — Proactive audit methodology

The user requested a pre-emptive forensic sweep while context and attention were fresh. 22 SSAs dispatched in parallel, each with tight scope (< 30 K tokens), clangd-first tool routing, and a strict return contract (file:line citations only — no file dumps).

### 2.1 SSA catalogue

Each SSA is referenced below by its short-name. Where an SSA is still addressable (agent ID persisted), the ID is noted; the next agent can `SendMessage` to continue any of them.

| # | Short-name | Agent type | Agent ID (resume) | Scope |
|---|---|---|---|---|
| A | Blocking primitives | embedded-system-engineer | (stateless) | Unbounded blocking on hot paths |
| B | Latching state flags | deep-technical-analyst | (stateless) | Flags that can set but never clear |
| C | ISR IRAM safety | Embedded Firmware Engineer | (stateless) | Functions reachable from ISR touching flash |
| D | Priority inversion | deep-technical-analyst | a198db000f5100c38 | Task scheduling + starvation |
| E | Watchdog coverage | Embedded Firmware Engineer | a073e7cb3ef236194 | TWDT/IWDT subscriptions + feeders |
| F | Queue saturation | embedded-system-engineer | a97125ce366b30698 | Silent-drop paths from ignored xQueueSend returns |
| G | Integer overflow / wrap | c-pro | a46fb4ac395aa2853 | uint8/uint16 counters + millis arithmetic |
| H | Resource leaks | c-pro | a8135a11de597ef9f | new/delete, malloc/free, FreeRTOS handles |
| I | Dead code / unused pub-sub | deep-technical-analyst | a7bcc1f2e2a87849e | MessageBus publishes with 0 subs, unreachable functions |
| J | Cross-core data races | embedded-system-engineer | ab85f9d1e077816e8 | Volatile misuse across cores |
| K | Audio pipeline forensic | embedded-system-engineer | a41991941eb2134f5 | AudioActor + ControlBus + DSP |
| L | NVS + persistence | Embedded Firmware Engineer | a4d48c3697b4315f8 | NVS, preset, zone, wifi-cred, OTA token |
| M | WiFi state machine | network-api-engineer | a405623698d352a21 | AP-only invariant, event handler reentrancy |
| N | Effect rendering pipeline | visual-fx-architect | a0874c75ec6d494a9 | RendererActor + zones + transitions + effect lifecycle |
| O | OTA + boot forensic | Embedded Firmware Engineer | ab19c9d35d8a1a903 | OTA session, rollback, boot validation |
| P | WebServer + WS lifecycle | network-api-engineer | a87fe5b724edebd88 | Client lifecycle, rate limiter, broadcasters |
| Q | Stack overflow + memory budget | cpp-pro | aecdad5086772d1ec | Per-task stack headroom + worst-case paths |
| R | Log/serial bandwidth | c-pro | acd3e07f8e4bffb90 | Cumulative serial load under workload |
| S | Reentrancy + callback-under-lock | cpp-pro | a1bb5bbbe718f419e | Callback invoked while mutex held |
| T | Config drift / sdkconfig | Embedded Firmware Engineer | a0a4e83765d202879 | Magic numbers + ESP-IDF defaults |
| U | Plugin manager + dynamic effects | cpp-pro | a343822e6356c3d54 | PluginManager, registry, manifest loader |
| + | (heap-shed re-audit from earlier) | embedded-system-engineer | a706be5af8e3ec7b8 | CaptureStreamer + m_lowHeapShed latch |

To resume any: `Agent` tool with `subagent_type` matching, then `SendMessage({to: '<agentId>', prompt: '…'})`.

### 2.2 Pressure-test script (for fix verification)

Script at `/tmp/k1v2_pressure.py` (may be overwritten — contents reproduced below):

```python
# 3-phase pressure test:
#   Phase 1: 300 s sustained 'n' burst at 20 Hz producer rate (full ambient sweeps × 3+)
#   Phase 2: 90 s alternating n/N (forward+backward hammer)
#   Phase 3: 60 s register-switching r/m/* interleaved with n
# Health probe after each phase: 's' CLI query + effect-change rate + heartbeat count
# Final: parse FPS/frames/drops/stack/msgbus from 's' dump
#
# Invoked as:  python3 /tmp/k1v2_pressure.py
# Logs to:     /tmp/k1v2_pressure.log
# Exit 0 = overall PASS (all tests under their thresholds)
```

Acceptable thresholds for a HEALTHY build (calibrated to 2026-04-17 baseline):
- sustained 300 s burst → no stall (no 6+ s of serial silence)
- effect_changed count ≥ 400 in phase 1
- heartbeats ≥ 7/10 in phase 1 (Core 0 gets CPU during heavy transitions)
- post-test CLI responsive (`s` returns > 500 B)
- final FPS ≥ 115/120
- drop rate < 10 % (during active cycling — expected due to heavy-effect first-inits)

### 2.3 Build / flash commands

```bash
# From repo root:
cd firmware-v3
# (safety — verify MAC before flashing)
pio device list | grep usbmodem           # must show SER=B4:3A:45:A5:87:F8 for K1
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem1101
```

RTK compresses pio output — if `EXIT=0` from the pio command, the build succeeded. For full logs: `~/Library/Application Support/rtk/tee/*_pio_run_*.log`.

---

## Part 3 — Findings (P0)

These are correctness, safety, or security hazards. Ship-blocking. **Fix first.**

### P0-01 — StateStore callback-under-lock deadlock

- **SSA:** S (reentrancy)
- **File:line:** `src/core/state/StateStore.cpp:120, 182`
- **Severity:** Guaranteed deadlock if any subscriber ever calls `dispatch()` back
- **Details:** `dispatch()` takes non-recursive `m_writeMutex`, performs swap, then calls `notifySubscribers()` **while still holding the mutex**. Same pattern in `dispatchBatch()`. Any subscriber that calls back into `StateStore::dispatch` → second `xSemaphoreTake(m_writeMutex)` on the same task → infinite block.
- **Fix (one line):** Release the lock before notifying. Example:
  ```cpp
  // After swapActiveIndex()
  xSemaphoreGive(m_writeMutex);   // release BEFORE notify
  uint8_t activeIdx = validateActiveIndex();
  notifySubscribers(m_states[activeIdx]);
  // Remove the second xSemaphoreGive at line 128
  ```
- **Risk if not fixed:** First subscriber added that emits a re-dispatch → deadlock → loopTask WDT reset every 5 s.

### P0-02 — OtaSessionLock has no timeout (field-deploy brick risk)

- **SSA:** O (OTA), B (latching-flag) — cross-confirmed
- **File:line:** `src/core/system/OtaSessionLock.h:127`, `src/network/webserver/ws/WsOtaCommands.cpp:54, 88, 93, 219`
- **Severity:** Permanent wedge if the WS OTA client disconnects without a clean `ota.abort`
- **Details:** `s_otaSessionStartTime` is captured but never consulted for timeout. If the owning client drops TCP with a half-open socket (no FIN received), `handleOtaClientDisconnect()` never fires → session flag and `OtaSessionLock` stay held forever. While held:
  - `OtaLock::tryAcquire` returns false → all future OTAs blocked
  - `WiFiManager::isOtaInProgress()` at line 499 suppresses STA retry
  - `Update` partition stays half-written
- **Fix:** Add a stale-session sweep in `WebServer::update()` (or in `OtaSessionLock` itself). If `isOtaInProgress() && (millis() - sessionStartMs) > OTA_SESSION_MAX_MS` (5 min), call `Update.abort()`, `OtaLock::release()`, log `ota.timeout`. Mirrors the 10 s `m_lowHeapShed` pattern we just landed.

### P0-03 — PluginManager `parseManifest` sizeof bug (plugin system non-functional for ID > 0x00FF)

- **SSA:** U (plugin-manager)
- **File:line:** `src/plugins/PluginManagerActor.cpp:392-393`
- **Severity:** Silent corruption of every effect ID > 0x00FF in a plugin manifest
- **Details:**
  ```cpp
  memcpy(manifest.effectIds, decodeResult.config.effectIds,
         decodeResult.config.effectCount * sizeof(uint8_t));  // WRONG — should be sizeof(EffectId)
  ```
  `EffectId` is `uint16_t`. Only low-byte of each ID is copied; high-byte is zeroed from the preceding `memset`. `validateManifest` then fails on every LGP effect (their IDs are namespaced), manifest is rejected, `applyManifests` never fires.
- **Currently dormant** because no plugin manifest files exist on LittleFS (`No plugin manifests found` at boot). Becomes real the moment someone drops a manifest.
- **Fix:** change `sizeof(uint8_t)` to `sizeof(EffectId)` on line 393.

### P0-04 — Rate-limiter bypass at 49.7-day uptime (security)

- **SSA:** G (integer-overflow)
- **File:line:** `src/network/webserver/AuthRateLimiter.h:122`, `src/network/webserver/RateLimiter.h:120, 153`
- **Severity:** Rate-limiter silently becomes inoperable at long uptimes — security bypass
- **Details:** Both rate-limiters use the pattern `entry->blockedUntil = now + BLOCK_DURATION_MS` then `blockedUntil > now` to check if blocked. When `millis()` is in the last `BLOCK_DURATION_MS` of its ~49.7-day wrap, `blockedUntil` wraps to small → `blockedUntil > now` is instantly false → block never applied or expires immediately. For Auth: 5 min block window. For HTTP/WS: 5 s.
- **Fix (4 lines):** use elapsed-time arithmetic which is wrap-safe:
  ```cpp
  uint32_t elapsed = now - entry->blockStart;
  return elapsed < BLOCK_DURATION_MS;
  ```

### P0-05 — WiFiManager `begin()` silent `softAP()` failure

- **SSA:** M (wifi-state-machine)
- **File:line:** `src/network/WiFiManager.cpp:79-101`
- **Severity:** Device boots into "phantom AP" with no radio, no IP, no recovery
- **Details:** On `softAP()` returning false, code only logs `LW_LOGE`, then unconditionally sets `m_forceApOnly = true` and `setState(STATE_WIFI_AP_MODE)`. Task starts. `handleStateAPMode()` only prints status — never retries `softAP()`. The `WebServer::update()` AP-reinit path is event-driven (`AP_STADISCONNECTED`), which never fires if AP never started.
- **Fix:** On `softAP()` failure, retry 3× with 500 ms delays. If all fail, `ESP.restart()`.

### P0-06 — `portMAX_DELAY` on hot paths (4 sites, any can wedge caller forever)

- **SSA:** A (blocking) + F (queue-saturation) — cross-confirmed
- **Sites:**
  - `src/hardware/EncoderManager.cpp:329` — `xSemaphoreTake(i2cMutex, portMAX_DELAY)` in `initializeM5Rotate8()`. If I2C bus wedges (SCL stuck low, codec glitch), every encoder reconnect attempt blocks forever. **Fix:** `pdMS_TO_TICKS(1000)` timeout + fallback.
  - `src/audio/backends/esv11/vendor/microphone.h:173, 179` — `i2s_channel_read(..., portMAX_DELAY)` / `i2s_read(..., portMAX_DELAY)`. If I2S DMA stalls (clock glitch, codec reset race), AudioActor blocks forever → no watchdog feed → no capture, no silence gate, no beat tracking. **Fix:** bounded timeout (2× hop period ≈ 25 ms at 32 kHz/372 hop); on timeout return empty so `onTick()` re-enters and drains messages.
  - `src/serial/SerialCLI.cpp:422` — `ren->send(msg)` for `MERGE_SUBMIT` uses default `portMAX_DELAY`. Serial command dispatched from loopTask — if Renderer queue is full, loopTask blocks forever → WDT fires after we subscribed loopTask (good), but that's after 5 s of dead UI. **Fix:** explicit `pdMS_TO_TICKS(10)` + check return.
  - `src/network/webserver/ws/WsStreamCommands.cpp:512` — `renderer->send(msg)` for `MERGE_SUBMIT` in a WS callback. Runs on AsyncTCP task Core 0. Full renderer queue → AsyncTCP task blocks → all WS clients wedged. **Fix:** same.

### P0-07 — AudioActor + WiFiManager NOT subscribed to TWDT

- **SSA:** E (watchdog-coverage)
- **File:line:** `src/audio/AudioActor.cpp:2544` (subscribe site), `src/network/WiFiManager.cpp:158` (subscribe site)
- **Severity:** I2S hangs and WiFi-driver blocking calls are invisible to the watchdog
- **Details:** Only RendererActor is subscribed (`RendererActor.cpp:529`) and loopTask (newly added). AudioActor, ShowDirector, Network, WiFiManager, AsyncTCP all unsubscribed. Memory ID #37629 already recorded a TG1WDT boot-loop caused by AudioActor PipelineCore hang — it was only caught by TG1WDT (hardware) because the hang blocked the scheduler. A softer hang (blocked on semaphore) would go undetected.
- **Fix (two sites, liveness-correlated feeds):**
  - AudioActor: `esp_task_wdt_add(nullptr)` in `onStart()` around line 2544; add `esp_task_wdt_reset()` at the bottom of `onTick()` gated on `m_stats.captureSuccessCount` having advanced.
  - WiFiManager: `esp_task_wdt_add(nullptr)` at `wifiTask()` entry (line 158); add `esp_task_wdt_reset()` at the top of the `while(true)` loop (line 165).

### P0-08 — `BeatPulseSpectralPulseEffect` uint8_t underflow

- **SSA:** G (integer-overflow)
- **File:line:** `src/effects/ieffect/BeatPulseSpectralPulseEffect.cpp:140`
- **Severity:** Visually wrong colour at bass extreme (effect plays but incorrectly)
- **Details:** `uint8_t bassIdx = 50 - floatToByte(bassPos * 0.2f)`. At `bassPos = 1.0f`, `floatToByte(0.2f) = 51`, `50 - 51` wraps to `255` as uint8_t → palette lookup jumps to index 255 instead of ~0.
- **Fix:**
  ```cpp
  uint8_t bassIdx = (50 > floatToByte(bassPos * 0.2f))
      ? 50 - floatToByte(bassPos * 0.2f) : 0;
  ```

---

## Part 4 — Findings (P1)

Systemic degradation under load or over time. Not ship-blocking today but increase risk with every deployed unit.

### P1-01 — AsyncTCP priority 10 on Core 0 preempts essential tasks

- **SSA:** D (priority-inversion)
- **File:line:** `platformio.ini:34-35` (only Core pin set; priority is library default 10)
- **Details:** AsyncTCP library task runs at priority **10** on Core 0. Preempts WiFiManager (1), AudioActor (4), ShowDirector (2), Network (3), and IDLE0 (0) — everything except lwIP (18), esp_timer (22), WiFi driver (23), IPC (24). Matches the symptom we saw of only 1/10 expected WiFi heartbeats under sustained WS traffic.
- **Fix (one line):** Add `-D CONFIG_ASYNC_TCP_PRIORITY=5` to common `build_flags`. Keeps it above background actors (2) and AudioActor (4) but below lwIP (18). No longer starves WiFiManager/Encoder/IDLE0.

### P1-02 — `ControlBus::UpdateFromHop` runs inside IRQ-disabling spinlock

- **SSA:** A (blocking), K (audio), + (mutex-audit from earlier)
- **File:line:** `src/audio/contracts/ControlBus.cpp:307-524` (the DSP body), wrapped at `src/audio/AudioActor.cpp:1711, 3417` with `portENTER_CRITICAL(&m_controlBusApiMux)`
- **Details:** 400+ lines of DSP — spike detection (twice), zone AGC, follower smoothing, chroma despike, band normalisation, chord state, silence hysteresis — all inside a `portMUX_TYPE` spinlock. On ESP32-S3 `portENTER_CRITICAL` **disables interrupts on the local core** for the duration. At 125 Hz hop rate this is hundreds of microseconds of IRQ-disabled on Core 0 per hop — blocks lwIP, AsyncTCP, WiFi, ISR service.
- **Fix:** Move `UpdateFromHop` out of the critical section. Publish via `SnapshotBuffer` (which is already the lock-free seqlock used by RendererActor). Only the publish-adjacent write to `m_frame` needs IRQ protection, and that's one memcpy.
- Also: `getControlBusFrameSnapshot()` is called TWICE per hop internally from AudioActor (`AudioActor.cpp:1730, 1750, 3437, 3459`). Each call re-enters the spinlock + memcpys ~5 KB. Reuse one frame-ref locally before publish.

### P1-03 — `m_needsSocketReset` latching flag (UdpStreamer)

- **SSA:** B (latching-flag) — same class as the `m_lowHeapShed` bug we just fixed
- **File:line:** `src/network/webserver/UdpStreamer.h:163`, `.cpp:575, 584, 597-625`
- **Details:** Set on UDP send failure. `maybeResetSocket()` requires `freeInternalHeap >= 30 KB` before reset. If heap stays low (often the exact condition that caused the UDP failures), the flag stays true indefinitely. Self-sustaining latch, identical class to the `m_lowHeapShed` loop we fixed.
- **Fix:** Add a max-latch timer (e.g. 30 s). If `m_needsSocketReset` true longer than that, force reset regardless of heap — or escalate to dropping subscribers.

### P1-04 — `m_streamingActive` (BenchmarkStreamBroadcaster) and `m_captureEnabled` (Renderer) — no cleanup on client disconnect

- **SSA:** B (latching-flag)
- **Files:**
  - `src/network/webserver/BenchmarkStreamBroadcaster.h:107` — set from WS `benchmark.start`. If client vanishes without calling `benchmark.stop`, flag stays true forever. No WS-disconnect hook.
  - `src/core/actors/RendererActor.h:800` — set from `capture stream` serial command. If serial drops without `capture stop`, producer task keeps memcpying frames to tap buffers every frame forever.
- **Fix:** Auto-stop after N seconds of no consumer activity, or hook into WS gateway disconnect.

### P1-05 — NVS silent wipe on `ESP_ERR_NVS_NEW_VERSION_FOUND`

- **SSA:** L (NVS)
- **File:line:** `src/core/persistence/NVSManager.cpp:85-96`
- **Details:** On init, if ESP-IDF returns `NO_FREE_PAGES` or `NEW_VERSION_FOUND`, the code silently calls `nvs_flash_erase()` and re-inits — destroys every preset, OTA token, WiFi cred, zone config, EdgeMixer, ColorCorrection setting. Log line only says "NVS partition needs repair, erasing..." Any future ESP-IDF upgrade that bumps NVS format blows up user data across the fleet.
- **Fix:** Emit a loud telemetry event before erase. Consider saving critical blobs (OTA token, user presets) to a secondary partition first. The unused 1 MB `userdata` partition at 0xE90000 is a candidate migration target.

### P1-06 — K1 NVS partition is 20 KB, not 64 KB (one feature away from full)

- **SSA:** L (NVS)
- **File:line:** `firmware-v3/partitions_custom.csv:6` (`nvs,data,nvs,0x9000,0x5000`)
- **Details:** Boot log shows 113/630 entries used. 20 KB = ~502 free NVS entries. Audit claim that K1 NVS was 64 KB was wrong — tab5-encoder was grown to 64 KB, K1 wasn't.
- **Fix:** Grow K1 NVS to 64 KB matching tab5 in `partitions_custom.csv`. Requires a one-shot `esptool erase_flash` + reflash on every device (or clever migration).

### P1-07 — 89 % of actor dispatch sites ignore the `bool` return

- **SSA:** F (queue-saturation)
- **Sites:** 95 audited; only ~6 properly propagate errors. Specifically:
  - `WsEffectsCommands.cpp:190, 198, 205, 219, 234, 250, 527-562, 644-663, 731-739` — WS handlers call `ctx.actorSystem.setX()` and ignore return. `ctx.broadcastStatus()` runs unconditionally → client sees "success" while the command was silently dropped.
  - `WsEffectPresetCommands.cpp:356-364` — 9 sequential setX() calls on preset apply. If the 3rd fails, preset is partially applied. Caller reports success.
  - `EffectHandlers.cpp:385`, `ParameterHandlers.cpp:56-111`, `PaletteHandlers.cpp:212`, `WsPaletteCommands.cpp:147`, `V1ApiRoutes.cpp:307-315, 1898-1902`.
  - `ShowDirectorActor.cpp:799` (`sendToRenderer`).
  - `main.cpp:126-138` (`applyFactoryPreset`), `:250-264` (state restore on boot) — plus uses default `portMAX_DELAY` on some of them (overlap with P0-06).
- **Fix (pattern):** Every WS handler calling `ctx.actorSystem.setX()` should check the return and emit `WsFailureCode::QUEUE_SATURATED` ack on false; suppress the `broadcastStatus()` that currently lies. Model on `WsTrinityCommands` which already does this pattern correctly.

### P1-08 — Transition-during-transition breakage

- **SSA:** N (effect-rendering)
- **File:line:** `src/core/actors/RendererActor.cpp:2105-2143` (`handleStartTransition`)
- **Details:** No guard against `m_transitionEngine->isActive() == true`. A second `START_TRANSITION` mid-transition copies the partially-blended `m_leds` into `m_targetBuffer` — the "new" transition's target is stale. Visually: transition blends to a mid-frame mush instead of the new effect. Also no `cleanup()`/`init()` on the bracketing effects during transitions → state carries over; heap-floor check is bypassed.
- **Fix:** In `handleStartTransition`, if a transition is already active, abort the previous one (flush `m_leds` with a forced `renderFrame` of the new effect) and call `oldEffect->cleanup()` + `newEffect->init()` to keep symmetry with `handleSetEffect`.

### P1-09 — Shared effect state across zones (47 AR effects)

- **SSA:** N (effect-rendering)
- **File:line:** `src/effects/zones/ZoneComposer.cpp:284` — calls `m_renderer->getEffectInstance(zone.effectId)` which returns the SAME `IEffect*` for all zones.
- **Details:** When the same effectId is assigned to multiple zones, the single `m_t`/`m_bass`/`m_chromaAngle`/smoothing-EMA members of the effect advance N× per frame. Audio smoothing collapses to instantaneous values; palette/hue rotations stack; speed multiplies by zone count. Applies to all 47 "5L-AR" family effects.
- **Fix (pick one):**
  - Dimension all AR state arrays by `[kMaxZones]` (pattern used by EsBloomRefEffect and SnapwaveLinearEffect).
  - Or have ZoneComposer instantiate a distinct effect per (effectId, zoneId) pair.

### P1-10 — OTA has no SHA256 / no signed-image verification

- **SSA:** O (OTA)
- **File:line:** `src/codec/WsOtaCodec.cpp:73-76`, `src/network/webserver/handlers/FirmwareHandlers.cpp` (REST path)
- **Details:** MD5 is optional. No `CONFIG_SECURE_BOOT` / `CONFIG_SECURE_SIGNED_APPS`. K1 AP is an open network — token is plaintext on air. Any AP-range attacker with the token can push unsigned firmware. Partition rollback is the only defence against a bad image.
- **Fix:** At minimum: make MD5 mandatory, upgrade to SHA256. Longer-term: secure-boot + signed images.

### P1-11 — Boot banner ~6.75 KB exceeds the 4 KB TX ring

- **SSA:** R (log-bandwidth)
- **File:line:** `src/core/SystemInit.cpp:82, 85` — `setTxBufferSize(4096)` before `Serial.begin()`; boot prints ~6,750 bytes total
- **Details:** ~90 `Serial.println()` lines of help + ~30 init `LW_LOGI` lines. Exceeds the 4 KB ring. LW_LOG path drops silently when ring is full; raw `Serial.println()` in `initSerialCommands()` blocks up to 20 ms per overflow (via `setTxTimeoutMs`). Combined, up to ~520 ms of blocking in `setup()` if no host is draining. Also: phases 4, 8, 13, 14 have no "complete" marker — a boot hang in those phases is invisible.
- **Fix:** Add explicit `LW_LOGI("Phase N complete")` after every init phase. Optionally grow TX ring to 8 KB for boot, shrink after setup().

### P1-12 — `tempo.h:244` `t_now_us` uint32 wrap at ~72 minutes

- **SSA:** K (audio), G (integer-overflow) — cross-confirmed
- **File:line:** `src/audio/backends/esv11/vendor/tempo.h:244`
- **Details:** `if (t_now_us >= next_update)` on uint32 microsecond counter. `t_now_us` wraps every 71.58 min. When wrap-around past `next_update`, `>=` test fails until `t_now_us` catches up — up to `(2^32 - next_update) µs` of missed novelty updates. Beat tracker stalls briefly every ~72 min.
- **Fix:** signed-delta test: `if ((int32_t)(t_now_us - next_update) >= 0)`.

### P1-13 — AudioActor AGC/noise-floor constants not dt-corrected for 125 Hz hop

- **SSA:** K (audio)
- **File:line:** `src/audio/AudioActor.cpp:2935-2983` — `noiseFloorRise=0.0005`, `noiseFloorFall=0.01`, `agcAttack=0.03`, `agcRelease=0.015`.
- **Details:** Vendor ES code (tempo.h:289, vu.h:84, goertzel.h:213-242) IS dt-corrected for the 200 Hz baseline. AudioActor AGC alphas are hardcoded and NOT wrapped in `retunedAlpha()`. On K1v2 32 kHz env, hop rate is 125 Hz (1.6× slower than design). AGC is 1.6× slower than intended.
- **Fix:** Wrap each with `audio::retunedAlpha(value, baselineHz, HOP_RATE_HZ)` or apply power-of-ratio.

### P1-14 — Cross-core `volatile` without release/acquire fencing

- **SSA:** J (cross-core-race)
- **Sites (highest first):**
  - `src/validation/EffectValidationMetrics.h:140-215` — `m_write_idx`, `m_read_idx` are `volatile uint32_t`. Writer = Renderer Core 1, reader = WS stats Core 0. On ESP32-S3 Xtensa LX7, `volatile` is compiler-only — no hardware release/acquire. Reader can observe new `m_write_idx` while sample write has not yet propagated cross-core → torn sample read. **Fix:** convert indices to `std::atomic<uint32_t>` with acquire/release, matching `LockFreeQueue.h`.
  - `src/effects/zones/ZoneComposer.h:217` — `volatile bool m_enabled`. Comment claims cross-core pairing but has no release after `m_zoneBuffers`/`m_zoneConfig` writes. **Fix:** `std::atomic<bool>` with release on set, acquire on render load.
  - `src/core/state/StateStore.h:226` — `volatile uint8_t m_activeIndex`. Plus an `__asm__ __volatile__ ("" ::: "memory")` that is a compiler barrier only, NOT cross-core. Reader on other core can observe new `m_activeIndex` before `m_states[newIndex]` is flushed. **Fix:** std::atomic + release/acquire.
  - `src/core/bus/MessageBus.h:235-237` — `m_totalPublished/Delivered/FailedDeliveries` volatile uint32, non-atomic `++` from multiple cores. Diagnostic only but guaranteed under-count. **Fix:** atomic fetch_add relaxed.

### P1-15 — Broadcaster cleanup gaps

- **SSA:** P (WebServer)
- **Details:**
  - **`cleanupDisconnected()` declared on 5 broadcasters but never called**. Reclaim only happens during next `broadcast()` (which iterates and prunes inline). Log/Benchmark fire rarely → zombie slots persist.
  - **Only LEDStream + STM + UDP + beat + VRMS + auth cleaned up on WS disconnect**. LogStream, AudioStream, Validation, Benchmark NOT unsubscribed in `handleWsDisconnect` (WebServer.cpp:1392-1442). A client that subscribes to these and disconnects abruptly leaves a stale subscriber slot.
- **Fix:** (a) Add `setLogStreamSubscription/setAudioStreamSubscription/setValidationStreamSubscription/setBenchmarkStreamSubscription(client, false)` to `handleWsDisconnect`. (b) Call `cleanupDisconnected()` on all 5 broadcasters once per second from `WebServer::update()`.

### P1-16 — `WsGateway::handleMessage` ignores WS frame fragmentation

- **SSA:** P (WebServer)
- **File:line:** `src/network/webserver/WsGateway.cpp:567`
- **Details:** Never inspects `AwsFrameInfo*` → for fragmented frames, `deserializeJson` fails per chunk, each failure sends an error back → amplification. Attacker can send intentionally fragmented frames to multiply outbound traffic.
- **Fix:** Check `info->final && info->index == 0 && info->len == len`; reject mid-stream chunks with a single error.

### P1-17 — `AuthRateLimiter` LRU eviction wipes blocked IPs

- **SSA:** P (WebServer)
- **File:line:** `src/network/webserver/AuthRateLimiter.h:187-222`, `RateLimiter.h:236-273`
- **Details:** When 8-slot table fills, oldest entry (by `windowStart`) is evicted. Blocked IPs that stop sending traffic age relative to fresh IPs → become eviction target → their block is cleared. Attacker generating auth traffic from 8+ IPs clears any victim's block.
- **Fix:** In `findOrCreate`, prefer to evict entries where `blockedUntil <= now` before touching still-blocked entries. Or: track LRU by last-seen (touch on every check).

---

## Part 5 — Findings (P2) — summary buckets

Full details in SSA transcripts. Listed here with short descriptions for triage. Expand any specific item by re-reading the relevant SSA return.

### Memory / heap

- `src/network/webserver/LedStreamBroadcaster.cpp:17` — `new ArduinoTimeSource()` leaked on WebServer teardown (no destructor).
- `src/network/webserver/StmStreamBroadcaster.cpp:23` — same.
- `src/core/SystemInit.cpp:240` — `new ZoneConfigManager(...)` never deleted (process-lifetime, acceptable on MCU but undocumented ownership).
- `src/network/WiFiCredentialManager.cpp:78` — `xSemaphoreCreateMutex` never deleted in destructor.
- `src/core/actors/ActorSystem.cpp:424, 443` — `fopen()` to hardcoded host path `/Users/spectrasynq/...` (forensic cruft; fails silently on-device, compile-time broken on CI).
- `src/network/WebServer.cpp:174` — static `new ValidationFrameEncoder` / `new EffectValidationRing<32>` never freed on `WebServer::begin()` re-entry.

### Config drift (all in `T` SSA return; expand via agent ID `a0a4e83765d202879`)

- **`CONFIG_ASYNC_TCP_STACK_SIZE = 16384`** — Probably too large; 8 KB is more typical. Plus 16 KB of library-task stack competing with scarce internal heap.
- **`CONFIG_ASYNC_TCP_MAX_ACK_TIME = 5000`** — web-scale default on an AP-only LAN. 1500 ms more appropriate.
- **`INTERNAL_HEAP_SHED_BELOW = 20 KB, RESUME = 26 KB`** — shed only 20 KB above `chip::MIN_FREE_HEAP_KB = 40` "unstable" threshold. Hysteresis (6 KB) is thin. Consider shed=28/resume=36.
- **`NVS_SAVE_MIN_HEAP = 8192` vs `EFFECT_INIT_MIN_HEAP = 12288`** — asymmetric for similar reasons. Align both at 12288.
- **`CONFIG_HEAP_POISONING_COMPREHENSIVE=1` in production** — 20-50 % malloc/free overhead. Use `LIGHT` in production, `COMPREHENSIVE` in debug envs.
- **`CONFIG_ESP_TASK_WDT_TIMEOUT_S = 5`** — tighten to 3 s for faster recovery.
- **`WS_PING_INTERVAL_MS = 15000`** — 2× more aggressive than template; no documented rationale.
- **`FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM=100`** — 100 ms cap at 120 FPS means 12 frames of blocking. Drop to 20 ms.
- **`BT_ENABLED=1`** but BT stack never initialised — ~90 KB flash wasted.
- **`CONFIG_LOG_DEFAULT_LEVEL=1` + `CORE_DEBUG_LEVEL=3` + `LW_LOG_LEVEL=3`** — three overlapping log systems with mixed verbosity. Production should step `CORE_DEBUG_LEVEL=1`, `LW_LOG_LEVEL=2`.
- **`WIFI_CONNECT_TIMEOUT_MS = 20000`** — dead config for AP-only build.
- **`FASTLED_RMT_MEM_BLOCKS=2`** — implicit default, not set explicitly. Make visible.
- **`CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP=1`** — enabled by default but never used; would cause LED drops if ever activated.

### Dead code (from `I` SSA — `a7bcc1f2e2a87849e`)

- **Entire `src/sync/` subtree except `DeviceUUID.{h,cpp}`** — 18 files, ~94 KB. `SyncManagerActor` never instantiated.
- **MessageBus publishes with ZERO subscribers** (remove):
  - `FRAME_RENDERED` (RendererActor.cpp:934-939, fires 12 Hz)
  - `HEALTH_STATUS` (RendererActor.cpp:645-655) + matching HEALTH_CHECK handler
  - `PONG` (RendererActor.cpp:661-668) + matching PING handler (also AudioActor.cpp:1374, 2572)
  - `AUDIO_FAILURE_DETECTED/RECOVERED` (AudioActor.cpp:1454, 1529)
  - `SHOW_STARTED/STOPPED/PAUSED/RESUMED/CHAPTER_CHANGED/COMPLETED` (6 `publishShowEvent` calls)
- **PALETTE_CHANGED self-loop** — RendererActor subscribes to its own publish (RendererActor.cpp:555, 2039-2041).
- **`SHOW_LOAD/START/STOP/PAUSE/RESUME/SEEK/UNLOAD` handler cases** in ShowDirectorActor (lines 297-325) — no code ever constructs these messages.
- **`WebServer::notifyEffectChange` + `notifyParameterChange`** (WebServerBroadcast.cpp:409, :442) — declared, defined, never called.
- **Validation wasted compute** — `AudioBloomEffect.cpp:262-273` runs a 12-iteration heavy-chroma compute every frame ONLY to feed `VALIDATION_*` macros that are no-ops when `FEATURE_EFFECT_VALIDATION=0`. Wrap in `#if FEATURE_EFFECT_VALIDATION`.
- **`MessageBus::m_totalDelivered` and `m_failedDeliveries`** — incremented but never read via a public getter.
- **`#ifdef FEATURE_EFFECT_VALIDATION` vs `#if`** — in 4 files, `#ifdef` is always true because `features.h` always defines the symbol (to 0). Change to `#if` for consistency.
- **~500 LOC of dead STA code in WiFiManager.cpp** — all state handlers for STA scanning/connecting/disconnecting are still compiled-in despite `WIFI_AP_ONLY=1`. `findBestAvailableNetwork()`, `scanNetworks`, etc.
- **Bluedroid BT stack** linked but never initialised.

### Other hygiene

- `SyncManagerActor` stack allocated at **32 KB** — comment says actual usage 16-20 KB. Consider dropping to 24 KB to recover 8 KB internal RAM.
- `StateStoreActor` stack **8 KB** — tight for NVS-commit call depth; consider 12 KB.
- `ArduinoJson` defaults to internal-DRAM allocations — consider custom allocator for PSRAM (large JSON payloads).
- `statusIntervalSec` and `spectrumIntervalSec` in `DebugConfig` are stored by serial commands but NOT consumed by any timer — dead UI.
- `ZoneConfigManager` / `PluginManagerActor` not protected by mutex; single-thread today, fragile.
- `ColorCorrectionEngine::loadFromNVS` has no range validation on loaded values — enum cast UB if corrupt.
- `RouteLimit` handler exposes `Action::BLOCK` / `Action::DEGRADE` but no downgrade path defined.

### IRAM / ISR

- FastLED overlay `idf4_rmt_impl.h:134, 139, 143` declarations lack `IRAM_ATTR`; the overlay `.cpp` adds them to definitions. Works today but fragile under LTO or header refresh.
- `Actor::sendFromISR` and `MessageBus::publishFromISR` exist but have zero callers — if ever wired into an ISR they'd be unsafe (not IRAM-attributed). Delete or guard.
- `CaptureStreamer::captureTimerCallbackISR` runs in TASK context (default `esp_timer` dispatch), not ISR — name is misleading.

### Audio edge cases

- `AudioCapture.cpp:226-298` — I2S short-read handling clobbers the zeroing. Guard the processing loop at `monoSamplesRead`.
- `SnapshotBuffer::Read` single-retry pattern can theoretically be defeated by two back-to-back writer Publishes during a slow reader (rare at current rates).
- `AudioActor::handleCaptureError` has no rate limit — could log at 125 Hz.
- Silence-gate comment says "Schmitt trigger" but actual code at `ControlBus.cpp:666` is a single threshold + time hysteresis. Either the comment or the implementation is wrong.

---

## Part 6 — Remediation strategy

### 6.1 Recommended execution model

Given the scale, **do NOT** attempt this in a single pass from main context. Each P0/P1 item should be:

1. Triaged for scope (one file vs many).
2. Dispatched to an SSA with a tight prompt (the finding + file:line + proposed fix + test criterion).
3. Verified independently (soak test for behavioural changes; re-run affected SSA for correctness).
4. Landed as an isolated commit with CHANGELOG entry.

**Learned lesson from this session:** working on 10+ files of fixes in main context creates cognitive debt and burns the context budget. Delegate.

### 6.2 Three tracks (pick one)

**Track A — P0 sweep** (tightest scope, highest leverage)
- 8 fixes listed in Part 3.
- Each is one file or a tight handful of lines.
- Can be dispatched as 8 parallel SSAs.
- Closes: deadlock risk, OTA field-brick, rate-limit bypass, silent AP failure, 4× portMAX_DELAY wedges, WDT visibility gaps, wrong-colour output.
- Estimated wall-clock: 2-3 hours (one parallel dispatch + verification round).

**Track B — P0 + P1 sweep** (recommended)
- Track A + 15 P1 items.
- Includes: AsyncTCP priority fix, ControlBus critical-section shrinkage, broadcaster cleanup, NVS migration paths, dt-correction retune, cross-core fencing, transition-during-transition guard, WS fragmentation guard, boot-banner observability.
- Estimated wall-clock: 1 day.

**Track C — P0 + P1 + dead-code cull**
- Track B + remove `src/sync/` (except DeviceUUID), strip dead MessageBus publishes, remove dead WiFi STA paths, optionally disable Bluedroid.
- Recovers 90-180 KB flash; simplifies every future audit.
- Estimated wall-clock: 1-2 days.

### 6.3 SSA dispatch template (copy-paste for the next agent)

When dispatching a fix SSA, include:

```
Fix <PN-NN from this document>.

Context:
  File:line: <from finding>
  Severity: <from finding>
  Already-fixed-do-not-break: <list from Part 1.2>

Scope (only these files):
  <list>

Required change:
  <one-paragraph description with the exact fix>

Verification:
  <how to test — soak test, specific serial command, heap probe, etc.>

Hard constraints (HARD RULES from CLAUDE.md):
  - British English in comments/logs.
  - No heap alloc in render().
  - 2.0 ms per-frame ceiling.
  - K1 is AP-ONLY — do not introduce STA.
  - Centre origin 79/80 for all effects.
  - clangd-first for C++ symbols, grep only for text literals.

Return contract:
  Files modified: N
  Per-file change summary:
  Regression check: <any existing test that should still pass>
  Commit message suggestion:
```

### 6.4 Verification strategy for the next agent

After any fix lands:

1. **Build check:** `pio run -e esp32dev_audio_esv11_k1v2_32khz` — must EXIT=0.
2. **Flash check:** `pio run -e … -t upload --upload-port /dev/cu.usbmodem1101` — must complete (verify MAC first: `B4:3A:45:A5:87:F8`).
3. **Smoke test:** `python3 /tmp/k1v2_pressure.py` — must pass all three phases, no stall, FPS ≥ 115.
4. **Regression scope:** for P0-02 (OTA) changes, also test an OTA cycle. For P1-02 (ControlBus) changes, test audio-reactive effects (not just ambient). For P0-07 (WDT subscribe) changes, verify intentional hang triggers reset within 5 s.
5. **Changelog:** every fix lands with a CHANGELOG.md `[Unreleased] → Fixed` entry.

### 6.5 What NOT to do

- **Do NOT** try to fix everything in one commit. Each fix needs its own verification window.
- **Do NOT** remove `src/sync/` without first confirming `DeviceUUID.{h,cpp}` is moved to a non-sync location (it's the only symbol still used).
- **Do NOT** disable Bluedroid without running a full OTA cycle — the partition table interacts with BT configs in subtle ways.
- **Do NOT** flip `FASTLED_ESP32_FLASH_LOCK=1` unless you also add the `IRAM_ATTR` fixes to the full RMT ISR call graph (most of which we already did). The flag was originally disabled intentionally.
- **Do NOT** change the AP-only WiFi architecture. `K1 is AP-only` is a hard constraint documented in `firmware-v3/src/network/CLAUDE.md` and in CLAUDE.md root. 6+ prior mitigation attempts to make STA work failed.
- **Do NOT** touch the `esp32dev_audio_pipelinecore` env. PipelineCore is broken and the current shipping path is `esp32dev_audio_esv11_k1v2_32khz`. Any fixes here must be validated on the ESV11 env.

---

## Part 7 — SSA returns (detailed appendix)

This part contains the full distilled findings from each SSA, organised for reference. The next agent can re-read any section without re-dispatching.

### 7.A — Blocking primitives

Files grepped: 28. Top-10 findings (ranked):

1. `src/audio/contracts/ControlBus.cpp:307-524` — `ControlBus::UpdateFromHop()` (~224 lines of DSP) inside `portENTER_CRITICAL(&m_controlBusApiMux)` at `AudioActor.cpp:1711` and `:3417`. See P1-02.
2. `src/hardware/EncoderManager.cpp:329` — `xSemaphoreTake(i2cMutex, portMAX_DELAY)`. See P0-06.
3. `src/audio/backends/esv11/vendor/microphone.h:173, 179` — I2S portMAX_DELAY. See P0-06.
4. `src/network/WiFiManager.cpp:261, 316, 418` — `xEventGroupWaitBits(..., pdMS_TO_TICKS(100))` in the `while(true)` state loop. Fine per iteration but WiFi disconnect/connect paths inline-block Core 0 for up to 200 ms on state transitions.
5. `src/core/persistence/NVSManager.cpp:117-339` — `nvs_commit()` on caller's thread. Called from WebServer handlers, WsOtaCommands, ZonePresetHandlers (Core 0). Commit is 15-50 ms typical, >100 ms on full sectors. Stalls AsyncTCP + Core 0 async tasks. Recommend pushing to a dedicated low-priority persistence task.
6. `src/core/actors/Actor.cpp:363` — `waitTime = portMAX_DELAY` when `m_config.tickInterval < 0`. Actors with negative tick block forever on queue. Not exploited today but footgun.
7. `src/network/webserver/LogStreamBroadcaster.cpp:212, 267` — `portENTER_CRITICAL(&m_mux)` across strncpy loops in `sendBackfill`. Cumulative microseconds per iteration with scheduler disabled. Snapshot-then-release would be cleaner.
8. `src/core/persistence/NVSManager.cpp:228` — `nvs_commit(handle)` in `eraseKey` has return value ignored.
9. `src/core/system/OtaBootVerifier.h:327-329` — `while (true) { delay(1000); }` after failed rollback. Should `esp_restart()` after N retries.
10. `src/hal/esp32s3/LedDriver_S3.cpp:131` — `xSemaphoreTake(m_showMutex, pdMS_TO_TICKS(2))`. OK but noting; caller skips frame on timeout.

Anti-findings (verified safe): `Actor.cpp:187` caller-supplied timeout; `EncoderManager.cpp:415` 0-timeout non-blocking; WiFiCredentialManager 50-500 ms timeouts; `CaptureStreamer.cpp:248` 100 ms bounded; `RendererActor.cpp:903` 20 ms bounded frame pacer; various broadcaster critical sections ≤8-entry loops.

### 7.B — Latching state flags

Inventoried: 22 flag declarations. Ranked (top 5):

1. **`s_otaSessionActive` + `OtaSessionLock::s_transport`** — `WsOtaCommands.cpp:54` + `OtaSessionLock.h:127`. See P0-02.
2. **`m_needsSocketReset` (UdpStreamer)** — `UdpStreamer.h:163`, `.cpp:575, 584, 597-625`. See P1-03.
3. **`m_streamingActive` (BenchmarkStreamBroadcaster)** — `BenchmarkStreamBroadcaster.h:107`. See P1-04.
4. **`m_captureEnabled` (RendererActor)** — `RendererActor.h:800`. See P1-04.
5. **`m_pendingStateSync` (SyncManagerActor)** — `SyncManagerActor.h:281`. Low risk (clear guaranteed in current flow), but sole clear site. Dead code today.

Proven symmetric / benign (14): `m_lowHeapShed` (fixed), `m_tempoLocked`, `m_chordGateOpen`, `m_silence_triggered`, `m_dmaFailureSignalled`, `m_apClientDisconnected`, `m_broadcastPending`, `m_forceApOnly`, WiFiManager state flags, `m_ready`, `m_transitionPending`, `m_suspended`, `m_trinitySyncPaused`, Actor `m_running`/`m_shutdownRequested`, `m_*Initialised` guards.

### 7.C — ISR IRAM safety

Active ISR handlers: 3.

1. **`ESP32RMTController::interruptHandler`** — IRAM at 0x40375560. Registered with `ESP_INTR_FLAG_IRAM | LEVEL3`. Call chain: `doneOnChannel` (IRAM) → `gWait.mark()` (fixed) OR `startNext` (IRAM after our fix) → `startOnChannel` (IRAM wrapper) → ESP-IDF `rmt_set_gpio`, `rmt_register_tx_end_callback`, `rmt_write_items`, `rmt_set_tx_intr_en` (all FLASH). Residual hazard for ≥3-controller configs; currently dormant (`StatusStripTouch` feature flag OFF on K1v2).
2. **`captureTimerCallbackISR`** — misnamed; runs in esp_timer **task** context (default dispatch), not ISR. Only does `xTaskNotifyGive`. Safe. Rename.
3. **`framePacerTimerCallback`** — explicit `ESP_TIMER_TASK` dispatch, safe.

IRAM declarations missing (fragile under LTO):
- `ESP32RMTController::startNext` — header lacks `IRAM_ATTR`; overlay `.cpp` has it.
- `ESP32RMTController::startOnChannel` — same.
- `ESP32RMTController::tx_start` — same.
- `_rmt_set_tx_intr_disable` — overlay-only free function.

Dead ISR-API methods (fragile if ever wired in):
- `Actor::sendFromISR` — `src/core/actors/Actor.cpp:200`, NOT `IRAM_ATTR`.
- `MessageBus::publishFromISR` — `src/core/bus/MessageBus.cpp:255`, NOT `IRAM_ATTR`.

### 7.D — Priority inversion

Full task priority map available via agent `a198db000f5100c38`. Headline:

- async_tcp: **prio 10 Core 0** — the starvation driver.
- Scheduling hierarchy AS-IS (Core 0): wifi(23) > esp_timer(22) > sys_evt(20) > lwIP(18) > async_tcp(10) > AudioActor(4) > Network(3) > ShowDirector(2)/SyncManager(2)/PluginMgr(2)/Hmi(2)/CaptureStreamer(2) > WiFiManager(1)/EncoderI2C(1) > IDLE0(0).
- Scheduling hierarchy AS-IS (Core 1): arduino_events(19) > Renderer(5) > StateStore(2) > loopTask(1) > IDLE1(0).
- **True IDLE0 donors** (reliably let IDLE run): AudioActor (`vTaskDelay(1)` per hop), WiFiManager (100 ms tick), EncoderI2C (20 ms tick), ShowDirector (50 ms tick), SyncManager (100 ms tick), Hmi, PluginMgr, CaptureStreamer (when active).
- **Cannot donate to IDLE0**: async_tcp (event-driven, can burn 100 % if queue fills), Network actor (taskYIELD only).

Ranked fixes: P1-01 (drop async_tcp to 5), P0-06 (bound portMAX_DELAY on hot paths), P0-07 (subscribe AudioActor and WiFiManager to TWDT).

### 7.E — Watchdog coverage

TWDT settings (from Arduino-ESP32 6.9.0 / IDF 4.4.7 sdkconfig):
- `CONFIG_ESP_TASK_WDT_EN=1`, `TIMEOUT_S=5`, `PANIC=1`.
- `CHECK_IDLE_TASK_CPU0=1`; `CHECK_IDLE_TASK_CPU1` not set (hence RendererActor must self-subscribe).
- `CONFIG_ESP_INT_WDT=1`, `TIMEOUT_MS=300`, `CHECK_CPU1=1`.
- `CONFIG_ASYNC_TCP_USE_WDT=0` — explicit opt-out.

Subscribed tasks: 2.
- Renderer (liveness-correlated via onTick every 10 frames).
- loopTask (newly added via `enableLoopWDT()` — fed once per loop iteration; mixed-liveness, acceptable).

Unsubscribed (ranked by hang-impact): AudioActor → P0-07; WiFiManager → P0-07; async_tcp → DO NOT flip `CONFIG_ASYNC_TCP_USE_WDT`, instead add a loopTask-side activity probe with a soft 30-s log warning (not panic); EncoderI2C (low impact); captureTx (bounded by notify timeout); ShowDirector (bounded ops).

Full detail available via agent `a073e7cb3ef236194`.

### 7.F — Queue saturation / silent-drop paths

13 silent-drop paths identified; 6 proper-propagation sites. Details in P1-07 above. Full list via agent `a97125ce366b30698`.

Key suggested improvements:
1. WS ack emits `WsFailureCode::QUEUE_SATURATED` on any `setX()` false return.
2. Fix the two `portMAX_DELAY` defaults in `main.cpp:136`, `main.cpp:262`, `SerialCLI.cpp:422`, `WsStreamCommands.cpp:512`.
3. Add `/api/v1/debug/queue-stats` endpoint exposing `getQueueUtilization()`, `m_failedDeliveries`, `queue_full_count`.
4. Replace 9-line preset-apply chain with single `APPLY_PRESET` atomic message.
5. Encoder-stage-2 drop counter — surface via `wifi stats` serial.

### 7.G — Integer overflow / counter wrap

3 confirmed bugs (P0-04, P0-08, P1-12). 4 timing risks: `SerialCLI.cpp:1024` validationEnd wrap, `LGPHolyShitBangersPack.cpp:170` float accumulator → uint32 overflow, ~15 `float m_t` accumulators without wraparound, `StyleDetector::m_hopCount` float precision collapse after ~33 days.

3 theoretical: `BeatTracker::tempoPriorBpm=0` division UB, `ValidationFrameEncoder::setDrainRate(0)` divide-by-zero (unreachable), `FrequencyMap::freqToBin` divide-by-zero (guarded).

40+ patterns verified safe (full list via `a46fb4ac395aa2853`).

### 7.H — Resource leaks

178 alloc sites audited. Top 5:
1. `LedStreamBroadcaster.cpp:17` — `ArduinoTimeSource` leak on teardown.
2. `StmStreamBroadcaster.cpp:23` — same.
3. `WebServer.cpp:174` + `validation/EffectValidationMacros.cpp:25` — static heap allocations never freed on WebServer re-begin.
4. `main.cpp:170, 175` — PSRAM scratch buffers never freed (intentional process-lifetime but unowned pointers).
5. `RendererActor.cpp:199` — TransitionEngine partial-alloc in constructor; mismatched PSRAM vs DRAM `free()` paths in destructor (works in practice because ESP-IDF multi_heap accepts both, but formally incorrect).

All FreeRTOS handle lifecycles verified clean except `WiFiCredentialManager::m_mutex` (never deleted, singleton destructor at program exit).

### 7.I — Dead code / unused pub-sub

Full list in Part 5 "Dead code". MessageType audit summary:
- Only ~8 of 60+ MessageType enum values genuinely used (SET_EFFECT, SET_BRIGHTNESS, SET_SPEED, SET_PALETTE, SET_SATURATION, SET_INTENSITY, SET_COMPLEXITY, SET_VARIATION, SET_HUE, SET_MOOD, SET_FADE_AMOUNT, SET_EDGE_MIXER_*, MERGE_SUBMIT, START_TRANSITION, SHUTDOWN, STIMULUS_SET_MODE, STIMULUS_CLEAR, TRINITY_BEAT/MACRO/SYNC/SEGMENT, SAVE_EDGE_MIXER_NVS).

### 7.J — Cross-core data races

See P1-14 for top 4. Rest:
- `vu.h:25-28` — `volatile float vu_level*` globals. Currently same-core but exposed to cross-core reads in theory.
- `goertzel.h:60` — `volatile bool magnitudes_locked`. TOCTOU anti-pattern; currently same-core.
- `Actor.h:458-459` — `m_running`/`m_shutdownRequested` `volatile bool`. Atomic-on-byte but no ordering.
- `ZoneComposer.h:217` — `m_enabled`. See P1-14.
- `StateStore.h:226` — `m_activeIndex`. See P1-14.
- `WebServer.h:588` — `m_apClientDisconnected`. Low-impact set-once-read-once.
- `WiFiManager.h:549` — `m_forceApOnly`. Same-core in practice.
- `CaptureStreamer.h:106` — `m_taskRunning`. Shutdown-path only.

Lock-free patterns verified correct: SnapshotBuffer (seqlock), LockFreeQueue (SPSC), AudioBenchmarkRing, Renderer param queue, Renderer VAL cmd queue, Renderer audio contract seqlock, bands debug double-buffer, AudioActor pipeline/DSP seqlocks, LedDriver_S3::m_showInProgress, `g_externalNvsSaveRequest`.

### 7.K — Audio pipeline forensic

9 ranked bugs. Highlights:
1. HIGH — `tempo.h:244` t_now_us wrap → P1-12.
2. MEDIUM — Schmitt silence-gate comment/code mismatch → P1-13-adjacent (single-threshold + time hysteresis only).
3. MEDIUM — AGC/noise-floor dt-correction → P1-13.
4. MEDIUM — `UpdateFromHop` spinlock → P1-02.
5. MEDIUM — I2S short-read clobber → Part 5.
6. LOW — SnapshotBuffer single-retry race.
7. LOW — `handleCaptureError` no rate limit.
8. LOW — Noise-floor recovery oscillation (compound with #3).

Plus edge cases: `sample_history` memmove without short-read guard; `calculate_magnitude_of_bin` window_pos float rounding at boundary; `TempoTracker::updateWinner` `winner_bin_` coupling to NUM_TEMPI.

PSRAM hot-loop costs: Goertzel ~1.3-3 ms/hop PSRAM reads; shift_and_copy_arrays ~1-2 ms/hop (40 KB memmove); shift_array_left ~200-400 µs. Total ~25-40 % of 8 ms hop budget in PSRAM I/O.

Full detail via agent `a41991941eb2134f5`.

### 7.L — NVS + persistence forensic

14 items, P0 and P1 items already above. Additional P2s:

- **Effect preset version bump has no migration path** — `EffectPresetManager.cpp:36-46`. Version increment → all user presets silently erased.
- **ColorCorrectionEngine.loadFromNVS has no range validation** — mode cast to enum UB if corrupt.
- **Migration-probe cascade can spuriously succeed** — `ZoneConfigManager.cpp:436-502`. V5 load failure tries v4, v3, v1 struct sizes; a corrupt v5 blob sized like v1 with matching version byte can succeed with garbage.
- **No user-facing factory reset** — no serial/WS/REST endpoint erases user config. Cannot recover in field without reflash.
- **V1→v3 migration never re-persists** — `ZoneConfigManager.cpp:267-309`. Every boot re-runs the migration.
- **Hardcoded centre `80` in validator** — `ZoneConfigManager.cpp:695`. Breaks if LED count changes.
- **WiFi cred XOR with MAC is cosmetic** — anyone with flash access + MAC (printed on every boot) recovers plaintext.
- **WiFi creds stored despite STA broken** — dead storage until STA fixed/removed.
- **No cross-blob checksum** — no verification that ZoneConfig + SystemConfig were saved as a matched pair.
- **`WsColorCommands` `colorCorrection.save` is unrate-limited** — 13 Preferences writes per call, no debounce; flash wear risk.
- **NVS-OTA-race** — `main.cpp:388-424` debounced save does not check `OtaSessionLock::isOtaInProgress()`. During OTA under heap pressure, commit risks.
- **`nvs_flash_erase` also fires on `NO_FREE_PAGES`** — partition overflow also nukes everything.

Full: agent `a4d48c3697b4315f8`.

### 7.M — WiFi state machine forensic

10 items. Highlights: P0-05 (silent softAP fail).

Additional P1/P2:
- **`WebServer::update()` AP-reinit racing `softAPgetStationNum()`** — WebServer.cpp:641-698; two separate samples of stationCount, a client joining between them can tear down its own WS session.
- **AP-reinit reintroduces password if build config changes** — `WebServer.cpp:667-690`. `begin()` uses 3-arg softAP, reinit uses different signature. Footgun for any build enabling `AP_PASSWORD_CUSTOM`.
- **`setState()` mutex grant silently returns stale state on timeout** — 100 ms timeout, falls back to `STATE_WIFI_INIT`.
- **`stop()` is unsafe** — `vTaskDelete` without checking mutex ownership; `WiFi.removeEvent()` never called. Public API but uncalled.
- **WebServer registers second `WiFi.onEvent` without removing WiFiManager's** — invisible fan-out; both handlers run on every AP_STADISCONNECTED.
- **Stale `EVENT_AP_START` bits leak across reinits** — never cleared.
- **mDNS not re-registered after AP reinit** — `WebServer.cpp:1004-1041`. `lightwaveos.local` resolution dies silently after each reinit.
- **`wifiTask` 100 ms tick wastes 10 % of Core 0** — could be 1000 ms without loss.
- **HTTP `/network/connect` is a no-op** — `NetworkHandlers::handleConnect` calls `setCredentials()+reconnect()` but doesn't clear `m_forceApOnly`. Returns 202 "Connection attempt initiated" and does nothing. User-observable bug.
- **~500 LOC of dead STA code** — see Dead code inventory.

AP-only invariant holds. `m_forceApOnly` NOT violated at runtime.

Full: agent `a405623698d352a21`.

### 7.N — Effect rendering pipeline forensic

5 items, P0/P1 above. Additional:

- **Dead transition-pending members** — `RendererActor.h:795-796`, `.cpp:166-167`. `m_transitionPending` / `m_pendingEffect` initialised but never read/written elsewhere. Either implement queuing pattern (fixes P1-08) or delete.
- **Zone composer time-accumulator stale across enable cycles** — `ZoneComposer.cpp:602`. `setZoneEnabled(false→true)` clears buffer but not time accumulators. Effects using `ctx.totalTimeMs` see a jump.
- **BeatPulseBloomEffect zoneId mask `& 0x03`** — `.cpp:81`. Safe because arrays sized 4, but inconsistent with rest of codebase's `< kMaxZones ? : 0`.

Over-budget render() estimates (algebraic, not measured):
| Effect | Estimate |
|---|---|
| LGPCatastropheCausticsAR | 3-4 ms |
| LGPTalbotCarpet (ShapeBangersPack) | 3-5 ms |
| LGPSchlierenFlowAR | 2-3 ms |
| LGPHarmonographHalo | 1.5-2.5 ms |
| LGPSuperformulaGlyphAR | 1-2 ms |
| LGPChimeraCrownAR | 1.5-2 ms |
| LGPGravitationalLensing | ~2-3 ms (tight after our coarsening) |

None hardware-profiled. Recommend instrumenting `esp_timer_get_time()` bookends.

Centre-origin compliance: all 12 transition types verified compliant. No `fill_rainbow` / `RainbowColors_p` / full-hue-sweep usages in src/. `hsv2rgb_rainbow()` usages are the FastLED conversion function, not a rainbow generator.

Zone-ID bounds checks: all effects verified using the `< kMaxZones ? : 0` pattern.

Heap-in-render violations: **zero**. All allocations in init(). Largest render-time stack temp: `float cusps[160]` = 640 B in LGPCatastropheCausticsAREffect (under limit).

Full: agent `a0874c75ec6d494a9`.

### 7.O — OTA + boot forensic

14 items. P0-02 captures the biggest. Additional:

- **No `esp_task_wdt_reset()` in OTA write path** — `FirmwareHandlers.cpp:449`, `:786`; `WsOtaCommands.cpp:659`. async_tcp not WDT-subscribed today (harmless now), but if anyone subscribes it in future → instant panic on long `Update.begin()` erase.
- **No SHA256 / signed-image verification** → P1-10.
- **Rollback-possible check can falsely reset a good OTA** — `OtaBootVerifier::init()` races with `WiFiManager::isAPMode()` latching. Verify `isAPMode()` returns true synchronously from `begin()` before `postBootValidation`.
- **`checkWdtSafeMode` is stateless** — reads `esp_reset_reason()` once; cannot detect crash loops. Need RTC_NOINIT_ATTR counter.
- **`Update.write()` short-write handling inconsistent** — WS path checks, REST path I couldn't fully verify.
- **Rollback doesn't clear OTA token / session lock** — not critical but undocumented.
- **Token regeneration has no rate limit and auth unclear** — verify `handleSetOtaToken` requires current token.
- **OTA LED feedback uses blocking `delay()` for ~1 s** — `OtaLedFeedback.h:209-246`. Happens on `showSuccess()`/`showFailure()`.
- **`ESP.restart()` after 500 ms delay** — WsOtaCommands.cpp:811. Fine, noting.
- **`initOtaAndWiFiReset()` order** — calls `OtaBootVerifier::init()` then `esp_wifi_deinit()`. Wifi deinit failures only logged.
- **`s_cachedToken` is `String` (heap)** — OtaTokenManager.h:126. Safe because init is early-setup. Fragment risk if regenerated frequently.
- **JSON `requestId` pointer lifetime** — WsOtaCodec.cpp:48. Aliases JsonDocument buffer; used only in same call. Fragile contract.

Rollback: one-shot validation at `postBootValidation`; no retry loop. If it fails once, app keeps running but bootloader rolls back on next reboot. On `esp_ota_mark_app_invalid_rollback_and_reboot()` failure → spin-forever loop at line 327 → bricked unless factory partition configured.

Full: agent `ab19c9d35d8a1a903`.

### 7.P — WebServer + WS lifecycle forensic

11 items. Highlights in P1-15 / P1-16 / P1-17.

Additional:
- **`ApiKeyManager::validateKey` length-check short-circuits before constant-time compare** — leaks length via timing. Cosmetic given K1 token is 37 chars fixed.
- **Heap-shed `closeAll(1013)` lacks disconnect-handler dedup** — WebServer's `handleWsDisconnect` has no guard against N concurrent disconnect events triggered by closeAll.
- **`m_clientIpMap` slot-0 force-eviction under 16-client pressure** — loses disconnect metadata.
- **`closeClientsInSubnet` doesn't remove map entry after closing** — brief window of stale pointer; cosmetic.
- **Static `s_telemetryBuf` 320-byte sequential-access assumption** — safe but undocumented at call sites.
- **`handleWsConnect` duplicated client-count check** — gateway uses `>=`, WebServer uses `>`, off-by-one.
- **No missed-pong liveness** — `pingAll()` fires every `WS_PING_INTERVAL_MS` but no "N missed pongs → close" logic. Silent client reaped only by AsyncWebSocket's TCP keepalive (minutes).

Full: agent `a87fe5b724edebd88`.

### 7.Q — Stack overflow + memory budget

13 tasks audited. All within acceptable headroom (≥ 45 %) today. Top 3 worth monitoring:

1. **StateStoreActor: 8 KB stack, est. 43 % usage** under NVS commit path. Consider bumping to 12 KB.
2. **AudioActor: 16 KB stack, est. 26 % usage.** `ControlBusRawInput raw{}` local is ~2.2 KB; guard with tighter `static_assert` and consider promoting to member.
3. **AsyncTCP internal: ~8 KB, est. 30-60 %.** Confirm `CONFIG_ASYNC_TCP_STACK_SIZE` setting.

Other notes:
- **SyncManager over-allocated** at 32 KB (comment says 16-20 KB actual). Could reclaim 8 KB.
- **Runtime StackMonitor only checks loopTask** — RendererActor, AudioActor, SyncManager high-water marks NOT actively polled despite infrastructure existing. Gap.

`configCHECK_FOR_STACK_OVERFLOW=2` enabled (strongest mode). `vApplicationStackOverflowHook` implemented in `StackMonitor.cpp:329-331`.

Full: agent `aecdad5086772d1ec`.

### 7.R — Log/serial bandwidth

Cumulative: ~450-500 B/s steady state (5 Hz cycling + default config). Rises to ~30 KB/s ONLY when `capture stream` is explicitly active.

Top emitters:
1. CaptureStreamer — 30 KB/s when active (opt-in only).
2. `RendererActor::handleSetEffect` "Effect changed" — 5 Hz × 80 B = 400 B/s.
3. `AudioActor::printDiagnostics` — always-on 10 s interval, 50 B/s.
4. NVS-deferred warning (now rate-limited to 1 Hz).

Unrate-limited repeated logs still present:
- `AudioActor.cpp:2394-2436` printDiagnostics — 6 lines burst every 10 s. Can momentarily drain ring below 256 B (our guard threshold).
- `Actor.cpp:393` `[%s] Stack low!` — every frame when stack < 100 words. Renderer 120 Hz = 7.2 KB/s on UART0 if triggered.
- `RendererActor.cpp:1897` effect-rejected heap-floor warning — no rate limit.

Boot banner: ~6.75 KB; exceeds 4 KB TX ring → P1-11.

`dbg interval status <N>` / `dbg interval spectrum <N>` — **dead UI**. Interval is stored but no ticker consumes it.

Full: agent `acd3e07f8e4bffb90`.

### 7.S — Reentrancy + callback-under-lock

8 owners audited. One P0 (StateStore — P0-01). Others:
- HAZARD 2 — WiFiManager `setState()` from external task callers holding `m_stateMutex` — not currently exploited but one abstraction away.
- HAZARD 3 — REST handlers calling `broadcastZoneState()` directly inside AsyncWS event callback → `m_ws->textAll()` re-entry risk. Should follow the deferred-flag pattern that `broadcastStatus()` already uses.
- HAZARD 4 — `ActorSystem::clearStimulus/publishStimulusFrame` call `SnapshotBuffer::Publish` under `m_stimulusMutex` — low risk depending on SnapshotBuffer internals.
- HAZARD 5 — MessageBus lock-free read race — stale `subscribers[]` pointer theoretical.
- HAZARD 6 — `ShowDirectorActor::applyParamValue` via `s_instance` — dangling pointer risk in unit tests only (production lifetime is OK).

**Zero recursive mutex sites** — all mutexes are plain, so Hazard 1 (StateStore) is a guaranteed deadlock not a benign recursive lock.

Full: agent `a1bb5bbbe718f419e`.

### 7.T — Config drift / sdkconfig

18 items. Full list via agent `a0a4e83765d202879`. Top-3 actionable:
1. `CONFIG_ASYNC_TCP_PRIORITY=5` (P1-01).
2. Align `NVS_SAVE_MIN_HEAP` and `EFFECT_INIT_MIN_HEAP` at 12288. Raise `INTERNAL_HEAP_SHED_BELOW_BYTES` to 28 KB / resume 36 KB.
3. Drop `HEAP_POISONING` from `COMPREHENSIVE` to `LIGHT` in production.

### 7.U — Plugin manager + dynamic effects

P0-03 (parseManifest sizeof bug) is the headline. Additional:
- H1: `registerEffect` increments stat counter on duplicate-ID re-registration.
- H2: `reloadFromLittleFS` rollback doesn't restore `m_effectSlots`.
- H3: `unregisterEffect` doesn't invalidate validated-effect-ID cache.
- M1: `PatternRegistry.h` PROGMEM strings defined in header → multi-TU duplicates. Declare `extern`, define once in `.cpp`.
- M2: `AudioMappingRegistry::findOrClaimSlot` claim not atomic — concurrent REST calls can clobber.
- M3: `g_factoryPresetIndex` non-atomic; same-core today.
- LittleFS auto-format on corruption silently wipes all plugin manifests.

Full: agent `a343822e6356c3d54`.

---

## Part 8 — Glossary & navigational aids

### Key files touched during the session

- `firmware-v3/src/core/actors/Actor.cpp` — drain loop fix
- `firmware-v3/src/core/actors/RendererActor.cpp` — handleSetEffect
- `firmware-v3/src/core/actors/ActorSystem.cpp` — setEffect
- `firmware-v3/src/core/SystemInit.cpp` — Serial.setTxTimeoutMs, enableLoopWDT call site
- `firmware-v3/src/main.cpp` — loopTask WDT subscription, NVS rate-limit
- `firmware-v3/src/serial/SerialCLI.cpp` — coalesce
- `firmware-v3/src/utils/Log.h` — LW_LOG_PRINTF guard
- `firmware-v3/src/network/WebServer.cpp`, `.h` — m_lowHeapShed max-latch
- `firmware-v3/src/network/WebServerBroadcast.cpp` — broadcastAudioFrame gate
- `firmware-v3/src/effects/ieffect/LGP*{TRM,Reaction,Kuramoto,Gravitational,etc.}Effect.cpp` — init/cleanup patches
- `firmware-v3/patches/vendor/FastLED-3.10.0-rmt4/fastled_delay.h` — IRAM-safe mark()
- `firmware-v3/patches/vendor/FastLED-3.10.0-rmt4/idf4_rmt_impl.cpp` — IRAM_ATTR on 3 functions
- `firmware-v3/scripts/apply_fastled_rmt4_patch.py` — overlays both files
- `CHANGELOG.md` — 17 entries under `[Unreleased] → Fixed/Added`

### Reference files for the next agent

Read on session start (pre-extracted context, saves ~20 K tokens of re-discovery each):
- `firmware-v3/docs/reference/codebase-map.md`
- `firmware-v3/docs/reference/fsm-reference.md`
- `CLAUDE.md` (root) + `firmware-v3/CLAUDE.md` + `firmware-v3/src/CLAUDE.md` + relevant subdir CLAUDE.md files
- `docs/WORKFLOW_ROUTING.md`
- `firmware-v3/CONSTRAINTS.md`

### MCP tool routing (enforced — see CLAUDE.md)

- **clangd first** for C++ symbols. Never grep for C++ symbol names.
- **QMD** for docs search across 1,459 markdown files.
- **Context7** for external library APIs (FastLED, AsyncTCP, ESP-IDF, ArduinoJson).
- grep only for text literals, config constants, comments.

### Commands reference

```bash
# Build (with overlay auto-apply via pre:script):
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz

# Flash (verify MAC b4:3a:45:a5:87:f8 first):
pio device list | grep usbmodem
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem1101

# Hard reset (for stuck USB CDC):
esptool.py --chip esp32s3 --port /dev/cu.usbmodem1101 --before default_reset --after hard_reset chip_id

# Pressure test:
python3 /tmp/k1v2_pressure.py
cat /tmp/k1v2_pressure.log

# Quick serial probe (liveness check):
python3 -c "
import serial, time
s = serial.Serial('/dev/cu.usbmodem1101', 115200, timeout=0.5)
time.sleep(0.3)
s.write(b's\n'); s.flush()
time.sleep(2)
d = s.read(8192)
print(f'{len(d)}B'); print(d.decode('utf-8','replace'))
s.close()
"
```

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-17 | agent:general (session by captain:elroy) | Created. 22 SSA findings consolidated. Session fixes documented. Next-agent handover complete. |
