# Changelog

All notable changes to the LightwaveOS project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - ESP32-P4 Audio Pipeline & iOS App

### Added
- **tab5:** PSRAM-primary preset storage — NVS demoted to write-behind backup; no code path can silently erase user presets
- **tab5:** Custom partition table — NVS enlarged from 20KB to 64KB (SPIFFS reduced by 44KB)
- **tab5:** NVS health tracking (`isNvsHealthy()`) for storage diagnostics
- **docs:** K1 landing page production pipeline — taglines, strategy, dual-state positioning, build spec, launch video spec, 5 HTML variants
- **firmware:** Vendored FastLED 3.10.0 RMT4 `idf4_rmt_impl.cpp` overlay (non-blocking `showPixels`) with PlatformIO pre-script — `firmware-v3/patches/vendor/FastLED-3.10.0-rmt4/` and `firmware-v3/scripts/apply_fastled_rmt4_patch.py`
- **scripts:** K1 loaded soak harness — serial stress (effect rotation, hotkeys, periodic `s` status) plus optional REST when the host can reach the K1 AP — `firmware-v3/scripts/k1_loaded_soak.py`
- **docs:** Inference task decision brief with evidence tags (latency, memory, execution targets; Orin downstream of open spec) — `firmware-v3/docs/design/INFERENCE_TASK_DECISION_BRIEF.md`
- **docs:** Seeded inference task-placement matrix (DSP / heuristic / ML classes, wire vs contract drift, mapping buckets) — `firmware-v3/docs/design/INFERENCE_TASK_PLACEMENT_MATRIX.md`
- **docs:** Inference placement matrix rev 0.2 — split tempo vs phase rows, vocal inject vs native, mandatory Yes/No/Research-only defaults, explicit failure semantics
- **docs:** Inference placement matrix rev 0.3 — parallel SSA corrections (ESV11 EsBeatClock path, merge m_merged hold, translation LOCK/silence, saliency/style grep vs features.h, wire REST/WS nuance)
- **firmware:** STM (Spectral-Temporal Modulation) dual-edge mode — per-LED spectral modulation for EdgeMixer
- **firmware:** STM 128-band spectral upgrade — mel filterbank + FFT pipeline
- **firmware:** STM snapshot REST endpoint with derived metrics
- **firmware:** STM real-time WebSocket binary stream + heap threshold fix
- **firmware:** STM_SPECTRAL_MAP EdgeMixer mode — per-LED spectral modulation bin mapping
- **tab5:** Zone Mode enable/disable button on ZONES sidebar panel with layout-before-enable sequence
- **tab5:** Zone count selector (1/2/3 zones) on grid slot 5, encoder-only (ENC-B 5)
- **tab5:** LED count display per zone on grid slot 6 (ENC-B 6)
- **tab5:** 3-zone centre-origin layout support (was limited to 2)
- **tab5:** Preset save/restore for EdgeMixer state (mode/spread/strength/spatial/temporal)
- **tab5:** Preset save/restore for colour correction mode (OFF/HSV/RGB/BOTH)
- **tab5:** Preset save/restore for auto-exposure target value
- **tab5:** Preset save/restore for per-zone blend modes
- **tab5:** Zone layout sent before zone.enable on preset recall (prevents K1 no-op)
- **tab5:** Current global effect auto-assigned to all zones on zone mode enable
- **tab5:** I2C recovery rewrite, PSRAM migration, UI and network hardening
- **tab5:** EdgeMixer mode cycling expanded to 9 modes (added STM DUAL, STM SPECTRAL)
- **zone-mixer:** AtomS3+PaHub physical controller — Phases 1-6 (input layer, display, I2C recovery, parameter mapping, LED feedback, echo suppression)
- **harness:** STM feasibility test harness for spectral-temporal modulation
- **scripts:** STM system test suite — zero-dependency offline tooling
- **docs:** STM 128-band spectral upgrade specification
- **docs:** STM endpoint, stream commands, and mode range updates
- **docs:** Tab5 I2C recovery research, memory audit, and implementation guides

### Fixed
- **firmware:** Core 1 panic "Cache disabled but cached memory region accessed" when the FastLED RMT ISR called `micros()` during flash cache suspend (NVS/WiFi) — vendored `fastled_delay.h` uses `esp_timer_get_time()` for `CMinWait::mark()` on ESP32; apply script copies it with the RMT overlay
- **firmware:** Belt-and-braces hardening of FastLED RMT ISR path — `ESP32RMTController::startNext`, `startOnChannel` and `tx_start` in the vendored `idf4_rmt_impl.cpp` now carry `IRAM_ATTR`. Prevents cache-disabled panics on K1v2 where `gNumControllers` (3: two main strips + StatusStripTouch on GPIO 38) exceeds usable parallel RMT channels (2), causing `doneOnChannel` → `startNext` → `startOnChannel` to execute in ISR context on every `show()`
- **firmware:** Silent lock-up after ~40 rapid effect-change commands ("device keeps running, serial goes dead, LEDs freeze, no crash"). `Actor::run()` drain loop at >50% queue utilisation processed up to 8 messages back-to-back with no watchdog feed, no yield and no `onTick()` call — starving loopTask (and its serial polling) on Core 1 and eventually backing up the HWCDC TX ring until every `Serial.print` caller blocked. Drain is now capped at 4 messages, resets the task watchdog and yields after each dispatch, and forces one `onTick()` per drain cycle so frames keep rendering. Also demoted `IEffect cleanup/init/SUCCESS` per-step logs to `LW_LOGD` (kept the summary line at INFO) to shrink the log bandwidth per change from ~720 B to ~180 B. Also added a 20 ms auto-repeat coalesce to SerialCLI's cycle keys (` `, `n`, `N`) so a held key can't flood the renderer queue in a single tick.
- **firmware:** Progressive cascade lockup while rendering heavy effects (most reliably reproduced with `LGP Gravitational Lensing` 0x0601 — device ran normally, then serial went silent ~90 s into rendering that effect and never recovered). Root cause: `LW_LOG_PRINTF` mapped directly to `Serial.printf()` which blocks indefinitely when the HWCDC TX ring has no headroom; once one task stalled there, every other task calling `Serial.*` queued behind it. `LW_LOG_PRINTF` now guards with `Serial.availableForWrite() >= 256` and drops the log on full rather than blocking the caller. Also coarsened `LGPGravitationalLensingEffect::render()` (outer ray step 2→4, inner step 80→40) to bring its per-frame cost from ~15-25 ms back toward the 2 ms budget.
- **firmware:** Set HWCDC `setTxTimeoutMs(20)` in `initSerial()` so every Serial write — including direct `Serial.printf()` calls in SerialCLI that bypass `LW_LOG_PRINTF` — returns within 20 ms instead of blocking indefinitely on a full TX ring. This is the systemic belt for the same cascade-lockup class of bug; without it a different heavy effect (next observed: `Chimera Crown` 0x1900) would still wedge the device even with the `LW_LOG_PRINTF` guard in place.
- **firmware:** Cumulative-state wedge under sustained effect cycling — final closure. The cascade had multiple compounding sources: (1) `LGPTimeReversalMirrorEffect` base + `_AR` + `_Mod1/2/3` were `memset`-ing the full 45 KB / 321 KB PSRAM history buffer on every `init()` (~9–64 ms blocking Core 1 per change); now only the live 960 B field arrays are zeroed — the history buffer is gated by `m_historyCount=0` so stale data is never read. (2) Nine effects (`LGPReactionDiffusion{,AR,Triangle,TestRig}Effect`, `LGPRDTriangleAREffect`, `LGPCatastropheCausticsAREffect`, `WaveformParityEffect`, `EsOctaveRefEffect`, `LGPLangtonHighwayAREffect`) were freeing their PSRAM in `cleanup()` then re-allocating in the next `init()` — fragmenting PSRAM over ~150 cycles; they now retain their PSRAM across the effect lifetime (matches the pattern used by TRM, SbK1Base and most other effects). (3) `KuramotoTransportEffect::init()` leaked the first one or two PSRAM allocs if the second or third failed; failure paths now roll back. (4) `RendererActor::handleSetEffect` now calls `cleanup()` on the new effect if `init()` fails, so partial allocations don't strand. (5) `WebServer::m_lowHeapShed` could latch permanently if free internal heap oscillated in the 20–26 KB hysteresis band (fed by the WS reconnect-storm caused by `closeAll(1013)`); added `INTERNAL_HEAP_SHED_MAX_LATCH_MS = 10 s` force-clear so the flag cannot stay ON indefinitely. (6) `broadcastAudioFrame()` now gates on `hasSubscribers()` (previously only on `m_lowHeapShed`) — zero work per tick when no WS listener. (7) `RendererActor::handleSetEffect` no longer publishes the dead `EFFECT_CHANGED` message (zero subscribers across the firmware). (8) NVS-deferred `LW_LOGW` warning now rate-limited to 1 Hz (was flooding at 100 Hz when heap < 8 KB). (9) Arduino `enableLoopWDT()` now called in `setup()` so loopTask hangs (serial CLI, NVS, WebServer update) trigger a 5-second task-WDT reset with backtrace — previously only RendererActor was WDT-subscribed, making any loopTask hang invisible. **Verified: 280 rapid effect-change commands sustained over 60 s with zero stall (vs 45-command stall on baseline — 6.2× improvement), 220 effect renders completed, post-burst device still at 119 FPS with healthy CLI.**
- **firmware:** NVS debounced-save deferred when internal heap < 8KB — prevents flash corruption under memory pressure
- **firmware:** WebSocket reconnect storm during heap shedding — `handleWsConnect()` rejects with 1013 while `m_lowHeapShed` is active
- **tab5:** `nvs_flash_erase()` removed from NvsStorage and PresetStorage — was silently destroying user presets on NVS version mismatch
- **firmware:** Removed temporary overlap/JSON serial debug and invalid host-path logging from `LedDriver_S3` and `RendererActor` (ESP32 cannot append to macOS paths).
- **tab5:** Zone effectId truncated from uint16_t to uint8_t — ZoneState.effectId and WsMessageRouter parsing both used uint8_t, losing high byte of K1's hex effect IDs (0x0100+)
- **tab5:** Zone effect encoder sent raw 0,1,2,3 — clamped to valid K1 range 0x0100-0x1F00 with cached name display
- **tab5:** Zone palette encoder had no upper bound — now wraps 0-74 (75 palettes)
- **tab5:** Zone blend encoder had no upper bound — now wraps 0-7 (8 modes)
- **tab5:** Zone encoder zoneId not clamped to zone count — caused "Invalid zoneId" flood when zone count reduced
- **tab5:** Zone param cards 5-6 accepted touch events and bubbled to mode row — LV_OBJ_FLAG_CLICKABLE and LV_OBJ_FLAG_EVENT_BUBBLE cleared
- **tab5:** Preset zone brightness hardcoded to 255 on save — now reads actual ZoneState.brightness
- **tab5:** Preset CC mode used live server state on recall instead of saved value
- **tab5:** Preset AE target hardcoded to 110 on recall instead of saved value
- **tab5:** Preset zone blend mode captured but never restored (missing sendZoneBlend call)
- **tab5:** EdgeMixer mode display showed "???" for modes 7-8 — added STM DUAL and STM SPECTRAL names
- **firmware:** Cross-core race conditions in ZoneComposer and WebSocket broadcast
- **firmware:** Lower heap-shed thresholds and fix probe condition
- **zone-mixer:** Phase 4 bug fixes — per-zone effects, zone layout, layout-before-enable
- **zone-mixer:** Safety audit fixes — WDT, I2C error handling, Arduino compatibility
- **firmware (forensic-audit 2026-04-17, Wave 1):** P0-01 `StateStore` deadlock — inconsistent mutex acquisition ordering between `setActiveIndex()` and `get()` could deadlock under concurrent access from CommandProcessor + RendererActor; ordering now unified, `m_activeIndex` made `std::atomic` for lock-free reads
- **firmware (forensic-audit):** P0-02 OTA session timeout — idle OTA sessions now expire after 120s via periodic `update()` cron; previously a disconnected uploader left `OtaSessionLock` held forever, blocking all future OTA until reboot
- **firmware (forensic-audit):** P0-03 `PluginManagerActor` sizeof bug — `sizeof(manifest_ptr)` returned pointer width (4 B) instead of the pointed-to manifest struct, truncating every plugin manifest load
- **firmware (forensic-audit):** P0-04 `AuthRateLimiter` counter wrap — 32-bit request counter could wrap to 0 after ~4 B requests, opening an unlimited auth-attempt window; now uses wrap-safe subtraction
- **firmware (forensic-audit):** P0-05 `WiFiManager` softAP retry + TWDT — Task-WDT now fed during softAP retry loop; previously long retry sequences tripped WDT and reset the device mid-recovery
- **firmware (forensic-audit):** P0-06 four `portMAX_DELAY` unbounded-block sites — `EncoderManager.cpp`, `microphone.h`, `SerialCLI.cpp`, `WsStreamCommands.cpp` queue sends now time out (10–50 ms) with drop-on-contention telemetry; previously any full queue could wedge the calling task indefinitely
- **firmware (forensic-audit):** P0-07 `AudioActor` Task-WDT + dt correction — watchdog now fed in the I2S hop loop, and per-hop dt uses measured elapsed instead of nominal hop period; previously DSP overruns tripped TWDT and dt drift desynchronised beat tracking
- **firmware (forensic-audit):** P0-08 `BeatPulseSpectralPulseEffect` uint8 underflow — `size - 1` could wrap to 255 when the pulse list was empty, indexing stack garbage; guarded with emptiness check
- **firmware (forensic-audit):** P1-01 AsyncTCP task priority elevated (2 → 3 via `CONFIG_ASYNC_TCP_RUNNING_CORE`/`_PRIORITY`) — prevents TCP starvation when RendererActor runs at priority 2 on the same core
- **firmware (forensic-audit):** P1-03 `UdpStreamer` streaming-active latch cleared when last subscriber leaves — previously the latch stayed on forever after the last UDP client dropped, burning CPU on zero-consumer broadcasts
- **firmware (forensic-audit):** P1-04 `BenchmarkStreamBroadcaster` + `RendererActor` capture latches — same subscription-count-driven latch clearing applied to benchmark stream and LED capture; both now idle when the last subscriber disconnects
- **firmware (forensic-audit):** P1-05 `NVSManager` silent-wipe telemetry — `nvs_flash_erase()` now emits a conspicuous diagnostic line *before* erasing user data; previously a silent corruption-driven wipe left no trace
- **firmware (forensic-audit):** P1-08 transition-during-transition handling — dispatching `SET_EFFECT` while a transition is already in flight now cancels the in-flight transition cleanly (cleanup/init bracketing); previously double-transition corrupted effect state and leaked the pre-empted effect's buffers
- **firmware (forensic-audit):** P1-11 boot-banner phase markers — every `SystemInit::init{Phase1..10}` step now emits `Phase N: ... complete` so a mid-boot crash identifies exactly which subsystem init wedged
- **firmware (forensic-audit):** P1-12 `tempo.h` counter wrap — long-uptime `int` overflow in the tempo BPM smoother fixed with explicit 32→64 bit cast
- **firmware (forensic-audit):** P1-13 (see P0-07 — AudioActor dt-correction is the same fix)
- **firmware (forensic-audit):** P1-14 cross-core atomics — `EffectValidationMetrics`, `ZoneComposer`, `MessageBus` hot-path shared ints converted to `std::atomic` (lock-free verified); `StateStore::m_activeIndex` also atomicised under P0-01
- **firmware (forensic-audit):** P1-15 WebSocket broadcaster cleanup on disconnect — `WebServer::handleWsDisconnect()` now unsubscribes the vanishing client from LogStream / AudioStream / STM / Benchmark so broadcast latches can clear and heap-sensitive broadcasters stop work when the last client drops
- **firmware (forensic-audit):** P1-16 WebSocket frame fragmentation guard — `WsGateway` now refuses to fragment outbound frames above the AsyncWebSocket single-frame limit, preventing the silent corruption previously observed on large status payloads
- **firmware (forensic-audit):** P1-17 `RateLimiter` LRU eviction — full rate-limiter table now evicts the oldest entry instead of rejecting new requests outright, restoring correct behaviour under high client turnover
- **firmware (forensic-audit, Wave 2):** P1-02 `AudioActor` critical-section shrinkage — `m_controlBus.UpdateFromHop()` (DSP smoothing + spike detection, per-hop hot path) was running inside `portENTER_CRITICAL(&m_controlBusApiMux)`, holding IRQs off across hundreds of microseconds of audio work on both the PipelineCore (AudioActor.cpp:1715–1725) and ESV11 (AudioActor.cpp:3462–3472) hop processors. The critical section has been removed from both regions — cross-core reader safety was already guaranteed by the lock-free `SnapshotBuffer::Publish()` at lines 1835 and 3585, which is the actual cross-core publish path. Also deduplicated four `getControlBusFrameSnapshot()` calls (1734+1754 and 3482+3504) into single local `frameRef` values reused across the style-detection and publish consumers — saves ~5 KB memcpy + one mutex round-trip per hop. Verified: pio build SUCCESS, RAM/Flash usage unchanged, zero stack growth, SSA-7 TWDT + dt-correction additions preserved.
- **firmware (root-cause):** RendererActor yield discipline for heavy effects — on the 450 s pressure battery (`/tmp/k1v2_pressure.py`), every run before this fix hit 6–8 task-WDT resets on `loopTask (CPU 1)` within the first sustained burst: a render() or init() on a compute-heavy effect (Chimera Crown, Kuramoto Transport, Talbot Carpet, Lorenz Ribbon, Spirograph Crown AR) held CPU 1 for 5 s+ with no scheduler opening, so the Arduino loopTask could never feed its 5 s TWDT. Three surgical yields + one timeout bump close this class of wedge: (1) `RendererActor::onTick` end-of-frame yield escalates from `vTaskDelay(0)` to `vTaskDelay(1)` whenever the frame overran the 8.33 ms budget (pacingWaitUs == 0), guaranteeing loopTask gets CPU 1 on every overrun frame; (2) `handleSetEffect` brackets `effect->init()` with `vTaskDelay(1)` before and after so heavy init paths (PSRAM alloc, lookup-table build, oscillator field seed) can't starve loopTask either side of the call; (3) `handleStartTransition` concurrent-transition init gets the same bracketing; (4) `main.cpp` raises loopTask TWDT timeout from the 5 s Arduino default to 10 s via `esp_task_wdt_init(10, true)` — genuine hangs still panic, but a slow-but-progressing render/init no longer trips WDT. Verified: `/tmp/k1v2_pressure.py` full 450 s battery PASSES T1/T2/T3 with **0 TWDT resets, 0 reboots, 10/10 heartbeats, 1992 effects cycled, 119 FPS sustained, 27.6 KB heap free post-burst** (previously: 8 resets, 8 reboots).

### Changed
- **firmware:** `ILedDriver::isShowInProgress()` documentation clarified for async RMT (wire time may continue after the flag clears).
- **tab5:** Zone purge 4→3 — 1-indexed (Zone 1/2/3), max 3 zones, no Zone 0
- **tab5:** I2CRecovery stripped to error counter only (-835 lines)
- **tab5:** Purged 22 unused font assets — 79K LOC removed
- **tab5:** Encoder bring-up hardened, external I2C init restored
- **tab5:** Dashboard initialised before host connection
- **tab5:** Pinned validated display and encoder dependencies
- **firmware:** Zone purge — 1-indexed (Zone 1/2/3), max 3, no Zone 0

### Removed
- **firmware:** Quarantined four heavy-compute effects from the registry and cycle order pending a render-budget rewrite — each was reproducibly triggering task-WDT resets on `loopTask (CPU 1)` under sustained 20 Hz effect cycling before the root-cause yield discipline landed. Source files are retained so the effects can be re-registered once their render/init paths fit the 2.0 ms budget:
  - `LGPChimeraCrownEffect` (EID 0x1900) — Kuramoto-Sakaguchi nonlocal coupling render busts the budget; 8 × TWDT resets in a 300 s burst
  - `KuramotoTransportEffect` (EID 0x1501) — 80-oscillator RK2 integration + nonlocal coupling per-frame; 7 × TWDT resets in one round
  - `LGPLorenzRibbonEffect` (EID 0x1903) — Lorenz ODE trail + radial projection
  - `LGPTalbotCarpetEffect` (EID 0x1800) — Fresnel harmonic sum render overruns; 8 × TWDT resets in one round
- Post-root-cause-fix (RendererActor yield discipline + 10 s loopTask TWDT), these effects no longer crash the system. They can be re-enabled individually by uncommenting the `renderer->registerEffect` call in `CoreEffects.cpp` and restoring their entries in `display_order.h` + `PatternRegistry.cpp` once their render paths are profiled and brought under 2.0 ms/frame.

### Fixed (previous)
- **firmware:** REST EdgeMixer mode validation rejected Triadic (5) and Tetradic (6) — `V1ApiRoutes.cpp` validated `mode > 4` instead of `mode > 6`
- **firmware:** WS speed validation capped at 50 instead of 100 — `WsEffectsCodec.cpp` `decodeSetSpeed` and `parameters.set` used stale range (1-50) while REST and RendererActor use extended range (1-100)
- **firmware:** `ZoneConfigManager.h` `MAX_SPEED` was 50, misaligned with `RendererActor.h` and `ZonePresetManager.h` (both 100)
- **firmware:** SerialCLI `zs` command validated speed 1-50 instead of 1-100
- **docs:** `api-v1.md` EdgeMixer section documented 5 modes (0-4); now documents all 7 modes (0-6) including Triadic and Tetradic
- **docs:** `api-v1.md` effects pagination documented `start`/`count` params; actual implementation uses `page`/`limit`/`offset`
- **docs:** `api-v1.md` transition `toEffect` documented as uint8 (0-46); corrected to uint16
- **docs:** `api-v1.md` parameters section missing `hue`, `mood`, `fadeAmount` fields; added with correct ranges
- **docs:** `api-v1.md` device status example showed STA mode (`apMode: false`, `192.168.1.100`); corrected to AP-only reality
- **docs:** `api-v1.md` presets section falsely claimed handlers are stubs returning NOT_IMPLEMENTED; disclaimer removed
- **docs:** `api-v1.md` speed range documented as 1-50; corrected to 1-100
- **docs:** `k1-ws-contract.yaml` speed ranges updated from 1-50 to 1-100 for `setSpeed` and `parameters.set`
- **docs:** `k1-rest-contract.yaml` EdgeMixer section expanded with parameter ranges and all 7 mode descriptions

### Added
- **firmware:** Shared gradient rendering kernel — GradientRamp, GradientCoord, GradientTypes under `effects/gradient/`. Centre-origin, dual-edge, 8-stop ramps with clamp/repeat/mirror modes, linear/eased/hard-stop interpolation, stack-allocated (~35 bytes)
- **firmware:** Retrofitted LGPPerceptualBlendEffect with 3-stop eased gradient ramp — proves multi-stop palette composition
- **firmware:** LGPGradientField effect (0x1F00) — operator-surfaced gradient proof with 6 parameters: basis, repeatMode, interpolation, spread, phaseOffset, edgeAsymmetry. Dirty-flag ramp rebuild.
- **firmware:** Colour correction skip for LGP_GRADIENT_FIELD (gradient ramps require uncorrected output)
- **firmware:** K1 coordinate helpers — uCenter(), uSigned(), uLocal(), edgeId(), halfIndex(), writeCentrePairDual() with correct 79.5 midpoint
- **firmware:** Colour correction skip for gradient-sensitive effects (EID_LGP_PERCEPTUAL_BLEND)
- **docs:** Gradient system design doc — coordinate model, API reference, validation scenes, forbidden patterns
- **ios:** EdgeMixer card on Play tab — 7 colour harmony modes, spread/strength sliders, spatial mask and temporal modulation pickers
- **ios:** EdgeMixer WebSocket integration — `edge_mixer.get/set/save` commands with 150ms debounce and echo prevention
- **ios:** EdgeMixer status broadcast sync — passive multi-client state updates from periodic device status
- **firmware:** FFT onset detector — 1024-point spectral flux (Bello/Dixon/Boeck), activity-gated, ~400us/hop, zero heap [quarantined]
- **firmware:** Band-energy ratio detector — variance-adaptive percussion triggers on grouped bands (Patin/WLED) [WIP]
- **firmware:** First-class onset API surface — OnsetContext with 6 semantic channels (beat, downbeat, transient, kick, snare, hihat) [WIP]
- **firmware:** OnsetSemantics module — extracted onset channel tracking from RendererActor into testable free functions
- **firmware:** AudioReactivePolicy TriggerMode — 7 modes with metronome fallback for effect trigger routing
- **firmware:** Onset capture telemetry — CaptureStreamer v2 bytes [26:32] carry onsetEnv, onsetEvent, percussion triggers
- **firmware:** EsV11Backend sample history accessor — getSampleHistory()/getSampleHistoryLength() for raw PCM access
- **tools:** Onset capture tooling — led_capture.py onset parsing, analyze_beats.py onset statistics, 2 capture suite scripts
- **docs:** Onset detector spec, quarantine matrix, ADR, WLED/aubio/essentia research references
- **tools:** Integrated RTK v0.34.2 token compression proxy — compresses Bash command output (git, builds, file listings) before reaching LLM context, reducing token consumption by 50-80% on CLI operations

### Reverted
- **firmware:** LGPRadialRippleEffect restored to original — gradient retrofit replaced sin16() brightness with triangular ramp and collapsed hue range from 64 to 4 steps
- **firmware:** LGPChromaticShearEffect restored to original — gradient retrofit reduced centre dimming from 50% to 30% with different curve shape

### Changed
- **firmware:** Silence gate uses raw PCM RMS instead of post-AGC band average — fixes false gate reopening during silence
- **firmware:** Silence hysteresis reduced from 8000ms to 150ms — responsive musical drops with ~550ms silence-to-black
- **firmware:** AGC noise floor lowered from 0.01 to 0.001 — only disables during electrical silence, not quiet passages
- **firmware:** Waveform effects (SbK1Waveform, SbK1WaveformHybrid) — RMS brightness scaling, silentScale gating, accelerated trail decay
- **firmware:** LittleFS.begin(true) — auto-format on blank/corrupted partition instead of silent failure
- **firmware:** BeatPulseBloom, BeatPulseBreathe, RippleEsTuned migrated to onset API trigger routing [WIP]

### Added
- **firmware:** EdgeMixer two new colour harmony modes — Triadic (120° hue shift) and Tetradic (90° hue shift); spread controls saturation blend (0=full sat, 60=70% sat)
- **tab5:** EdgeMixer mode button cycles through all 7 modes including Triadic and Tetradic
- **ESP32-P4 Platform Support**: Full audio capture and LED control on Waveshare ESP32-P4-WIFI6
  - ES8311 audio codec integration via I2S standard driver
  - Dual WS2812 LED strips (320 LEDs total) via RMT peripheral
  - ESP-IDF v5.5.2 toolchain with RISC-V GCC
  - Build scripts: `build_with_idf55.sh`, `flash_and_monitor.sh`

### Changed
- **Audio Pipeline Cadence Alignment** (ESP32-P4): Fixed timing mismatch between hop rate and scheduler
  - Changed HOP_SIZE from 128 to 160 samples (8ms → 10ms) to match FreeRTOS 100Hz tick
  - Hop rate now perfectly aligned: 160 samples @ 16kHz = 10ms = 100 Hz = 1 tick
  - Eliminates DMA timeouts, watchdog triggers, and erratic capture rates
  - See `docs/AUDIO_PIPELINE_CADENCE_FIX.md` for full technical analysis

- **DC Blocker Coefficient**: Corrected from 0.9922 to 0.992176 using proper formula
  - Formula: R = exp(-2π × fc / fs) where fc=20Hz, fs=16000Hz
  - Ensures accurate DC removal without affecting audio content

- **ES8311 Microphone Gain**: Added explicit 24dB gain setting
  - Signal levels were ~0.2% of full scale (-54dB), now properly amplified
  - Available gains: 0dB to 42dB in 6dB steps

- **Spike Detection Improvements**:
  - Added noise floor check (0.005 threshold) to skip detection on quiet signals
  - Raised warning threshold from 5.0 to 10.0 spikes/frame (20 bins checked per frame)
  - Prevents false warnings from noise-floor fluctuations

- **I2C Probe for ES8311**: Changed to low-level ACK check using `i2c_cmd_link` API
  - Fixes intermittent probe failures with `i2c_master_write_read_device()` 0-length read

### Fixed
- **AudioActor FreeRTOS Tick Conversion**: Added `LW_MS_TO_TICKS_CEIL_MIN1()` macro
  - `pdMS_TO_TICKS(8)` returns 0 at 100Hz ticks (8ms < 10ms tick period)
  - New macro uses ceiling division with minimum of 1 tick

- **Self-clocked Mode Stability** (retained but unused): Actor.cpp now supports tickInterval=0
  - Caused watchdog triggers due to IDLE0 starvation - not recommended for audio
  - Kept for potential future use cases with proper yield points

---

## [Previous Unreleased] - Light Guide Plate Feature

### Added
- **Light Guide Plate Mode**: Revolutionary optical waveguide display system
  - Dual-edge LED injection into 329mm acrylic plate
  - Advanced interference pattern effects using wave physics
  - Depth illusion system with volumetric display capabilities
  - Physics simulations: plasma fields, magnetic field lines, particle collisions
  - Interactive features: proximity sensing, gesture recognition, touch-reactive surfaces
  - Advanced optical effects: holographic patterns, energy transfer visualization

### Technical Documentation Added
- `docs/LIGHT_GUIDE_PLATE.md`: Comprehensive 240+ line technical specification
  - Physical configuration and hardware specifications
  - Optical theory and wave interference mathematics
  - 5 major effect categories with detailed implementations
  - Performance optimization strategies and memory management
  - 6-phase implementation roadmap with weekly milestones
  - Testing and validation frameworks
  - Future enhancement possibilities

### Architecture Enhancements
- Light guide effect base classes and coordinate mapping systems
- Interference calculation framework with real-time optimization
- Edge-to-center coordinate transformation algorithms
- Specialized synchronization modes for optical effects
- Performance-optimized interference pattern storage

### Configuration Extensions
- New feature flags for light guide mode detection
- Hardware configuration for dual-edge LED control
- M5Stack encoder integration for light guide parameters
- Compile-time optimization for optical calculations

### Effect Categories Planned
1. **Interference Pattern Effects**
   - Standing wave patterns with mathematical precision
   - Moiré interference from overlapping frequencies
   - Constructive/destructive zone visualization

2. **Depth Illusion Effects**
   - Volumetric display simulation with apparent 3D objects
   - Parallax-like effects using edge intensity control
   - Z-depth mapping with atmospheric perspective

3. **Physics Simulation Effects**
   - Plasma field visualization with realistic physics
   - Magnetic field line rendering using dipole equations
   - Particle collision chamber with momentum conservation
   - Wave tank simulation with proper propagation physics

4. **Interactive Applications**
   - Proximity detection using light occlusion analysis
   - Gesture recognition from shadow pattern changes
   - Touch-reactive surfaces with ripple effects
   - Real-time data visualization framework

5. **Advanced Optical Effects**
   - Edge coupling resonance with feedback loops
   - Energy transfer visualization with conservation laws
   - Holographic interference pattern generation
   - Interactive light-based games (optical pong, light tennis)

### Performance Specifications
- Maintained 120 FPS target with complex interference calculations
- Optimized memory usage with efficient pattern storage
- Real-time interference calculation using ESP32-S3 dual cores
- Temporal coherence optimization for smooth animations

### Future Integration Points
- Machine learning for advanced gesture recognition
- Multi-unit synchronization for large installations
- Camera integration for computer vision applications
- Advanced physics: quantum mechanics and relativistic effects

### Changed
- Web control plane refactor (robustness-first):
  - Replaced ESPAsyncWebServer/AsyncTCP web stack with ESP-IDF `esp_http_server` backend (REST + WebSocket) in WiFi environments.
  - Default build (`esp32dev`) no longer compiles any web stack dependencies.
  - JSON handling remains cJSON-only.
  - Feature flags: `FEATURE_WEB_SERVER`, `FEATURE_WEBSOCKET`, `FEATURE_OTA_UPDATE` now default to OFF unless enabled via PlatformIO env build flags.

### Fixed
- Eliminated cross-platform dependency leakage in ESP32 builds (no ESP8266/RP2040 async TCP libraries pulled into ESP32-S3 builds).
- **Audio Gate Responsiveness**: Fixed activity gate closing on valid audio signals
  - Lowered default `gateStartFactor` from 1.5 to 1.0 (more permissive threshold)
  - Fixed hardcoded noise floor rise rate (now uses tunable `noiseFloorRise` parameter)
  - Increased SNR threshold from 2.0 to 3.0 to prevent floor drift during active audio
  - Added automatic recovery mechanism: forces noise floor down when gate stuck with signal present
  - Audio-reactive effects now respond correctly to normal audio levels
  - See `docs/audio-visual/audio-gate-fix-2025-01.md` for comprehensive documentation

---

## Previous Releases

### [Phase 1] - LED Strips Mode Implementation
- Added dual 160-LED strips infrastructure
- M5Stack 8encoder I2C integration
- 12 strip-specific advanced effects
- Propagation and synchronization modes

### [Base System] - Core Architecture
- Modular effect system with base classes
- PlatformIO project structure
- Performance monitoring and optimization
- 16 core visual effects
- 33 color palettes
- Smooth transition system