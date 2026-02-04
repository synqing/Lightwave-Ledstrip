# P4 Control Flow Mapping (ESP32-P4 LightwaveOS v2)

## 1. Scope and Objectives

This document maps the execution and control flow for the ESP32-P4 build of LightwaveOS v2, tracing from the P4 entry point through the Actor system, Renderer pipeline, and the primary integration points (audio snapshot, zone composition, LED output).

It is code-backed: key statements include file paths and line numbers.

**In-scope**
- Entry point and initialisation sequences (S3 vs P4 comparison, because the P4 port reuses shared subsystems).
- Actor base class scheduling model (tick semantics, queue backpressure, task pinning).
- ActorSystem lifecycle and cross-actor wiring.
- Renderer hot path (renderFrame → colour correction → LED driver show).
- Memory allocation patterns relevant to determinism (especially “no heap alloc in render paths”).
- Error handling and propagation patterns.
- P4 build context: ESP-IDF v5.5.2 + Arduino-compat shim (per `LightwaveOS-P4/EMBEDDER.md`).

**Out-of-scope**
- Web server / network stack behaviour on S3 beyond what is necessary for comparison (P4 excludes `/network/`).
- Board-level boot ROM / bootloader internals (handled by ESP-IDF/Arduino core).

---

## 2. Entry Point and System Initialisation

### 2.1 ESP32-S3 (Arduino) Entry Path

**Entry**: Arduino `setup()` / `loop()` in `firmware/v2/src/main.cpp`.

**Initialisation sequence (high level)**
1. Serial initialisation, boot heartbeat.
2. OTA boot verification.
3. Monitoring subsystems initialisation (stack/heap/leak/validation profilers).
4. `ActorSystem::init()` (creates actors) and `registerAllEffects(renderer)`.
5. NVS init.
6. ZoneComposer init + attach to RendererActor + NVS restore.
7. `ActorSystem::start()` (starts actor tasks).
8. Additional subsystems (plugins, WiFi, web server, etc.)

**Code anchors**
- Actor system init + effects + NVS + ZoneComposer + actor start: `firmware/v2/src/main.cpp:165-223`.
- ZoneConfigManager and PluginManager heap allocations during init: `firmware/v2/src/main.cpp:204-206`, `firmware/v2/src/main.cpp:231-239`.

**Failure behaviour**
- Fail-fast (hard halt) on `actors.init()` or `actors.start()` failure: `firmware/v2/src/main.cpp:166-169`, `firmware/v2/src/main.cpp:219-222`.

### 2.2 ESP32-P4 (ESP-IDF) Entry Path

**Entry**: ESP-IDF `app_main()` in `firmware/v2/esp-idf-p4/main/main.cpp`.

Note: older docs sometimes reference `LightwaveOS-P4/esp-idf-p4/...`; in this repo the ESP-IDF P4 app lives under `firmware/v2/esp-idf-p4/` (with `LightwaveOS-P4/` providing wrapper scripts and embedder docs).

**Initialisation sequence (high level)**
1. Serial initialisation (Arduino compat layer), short delay.
2. Monitoring subsystems initialisation (stack/heap/leak/validation profilers).
3. `ActorSystem::init()` and `registerAllEffects(renderer)`.
4. NVS init.
5. ZoneComposer init + attach + NVS restore.
6. `ActorSystem::start()`.
7. Initialise ESP-IDF console REPL (`esp_console`) and register Lightwave commands.
8. Main thread idles; REPL runs in its own task.

**Code anchors**
- app_main init flow (boot → ActorSystem init/start): `firmware/v2/esp-idf-p4/main/main.cpp:44-112`.
- REPL initialisation and rationale (“replaces handleSerialCommands() approach that caused resets”): `firmware/v2/esp-idf-p4/main/main.cpp:113-176`.

**Failure behaviour**
- Fail-fast (hard halt) on `actors.init()`, missing renderer, or `actors.start()` failure: `firmware/v2/esp-idf-p4/main/main.cpp:71-84`, `firmware/v2/esp-idf-p4/main/main.cpp:106-111`.
- NVS / ZoneComposer failures degrade (warnings, continue): `firmware/v2/esp-idf-p4/main/main.cpp:89-104`.

### 2.3 RTOS, Clock, and Power Baseline (P4 Build)

The P4 application does not explicitly configure clocks or power domains in `app_main()`. Those are set by ESP-IDF before `app_main()` runs and by the project configuration (`sdkconfig`).

Key configuration facts (as checked into this repo):
- Default CPU frequency is **360 MHz** (not 400): `firmware/v2/esp-idf-p4/sdkconfig:2030-2033`.
- FreeRTOS tick rate is **100 Hz** (10 ms tick resolution): `firmware/v2/esp-idf-p4/sdkconfig:2216`.
- ESP power management (dynamic frequency scaling) is **disabled**: `firmware/v2/esp-idf-p4/sdkconfig:1995-1998`.
- Task watchdog is **enabled** with a **5 s** timeout: `firmware/v2/esp-idf-p4/sdkconfig:2094-2099`.

Clock API note (P4 build):
- `micros()` and `millis()` are wrappers around `esp_timer_get_time()` in the Arduino-compat layer, so renderer and audio actors share the same monotonic wall-clock source on P4: `firmware/v2/esp-idf-p4/components/arduino-compat/include/Arduino.h:91-103`.

### 2.4 Initialisation Order Dependencies (Observed)

**Enforced order (by usage constraints, not strict runtime assertions)**
- Effects must be registered *before* `ActorSystem::start()` so RendererActor can render valid effect IDs.
  - S3: `registerAllEffects()` occurs before `actors.start()` at `firmware/v2/src/main.cpp:173-223`.
  - P4: `registerAllEffects()` occurs before `actors.start()` at `firmware/v2/esp-idf-p4/main/main.cpp:86-111`.
- ZoneComposer must be initialised and attached before any zone mode rendering can occur.
  - S3: `renderer->setZoneComposer(&zoneComposer)` at `firmware/v2/src/main.cpp:200-214`.
  - P4: `renderer->setZoneComposer(&g_zoneComposer)` at `firmware/v2/esp-idf-p4/main/main.cpp:93-104`.

**What happens if violated?**
- Renderer will render “no effect” (clears buffer) if the current effect is not active: `firmware/v2/src/core/actors/RendererActor.cpp:1136-1143`.
- Zone mode simply won’t be available if ZoneComposer is null or disabled: `firmware/v2/src/core/actors/RendererActor.cpp:1122-1134`.

### 2.5 Initialisation Sequence Diagram (P4)

```
ESP-IDF boot + FreeRTOS bring-up
  → app_main()                                      [firmware/v2/esp-idf-p4/main/main.cpp]
    → Serial.begin()
    → (optional) Stack/Heap/Leak/Validation profilers
    → ActorSystem::instance().init()
      → new RendererActor
      → new ShowDirectorActor
      → (optional) new AudioActor
    → registerAllEffects(renderer)
    → NVS_MANAGER.init()                            (warning on failure)
    → ZoneComposer.init(renderer)                   (warning on failure)
      → renderer->setZoneComposer(&g_zoneComposer)
      → new ZoneConfigManager + loadFromNVS()
    → ActorSystem::start()
      → RendererActor::start() (task create)
      → ShowDirectorActor::start()
      → (optional) AudioActor::start() (non-fatal if fail)
    → esp_console REPL init + start (interactive control plane)
    → app_main() idle loop
```

---

## 3. ActorSystem Control Flow

### 3.1 ActorSystem Lifecycle

**Create actors**: `ActorSystem::init()` in `firmware/v2/src/core/actors/ActorSystem.cpp`.
- RendererActor created first (dependency root): `ActorSystem.cpp:71-88`.
- ShowDirectorActor created next: `ActorSystem.cpp:90-99`.
- AudioActor created when `FEATURE_AUDIO_SYNC`: `ActorSystem.cpp:101-115`.

**Start actors**: `ActorSystem::start()`
- `RendererActor::start()` then `ShowDirectorActor::start()`; audio start failures are non-fatal: `ActorSystem.cpp:163-200`.
- Audio integration wiring into renderer (SnapshotBuffer + TempoTracker pointer): `ActorSystem.cpp:190-199`.

**Shutdown**: reverse stop order; disconnects renderer’s audio pointer first: `ActorSystem.cpp:231-239`.

### 3.2 Actor Base Class Scheduling Model

The actor runtime is implemented in `firmware/v2/src/core/actors/Actor.cpp`.

**Task model**
- Each actor is a FreeRTOS task pinned to a specified core (via `xTaskCreatePinnedToCore`): `Actor.cpp:55-106`.

**Tick semantics**
- `tickInterval > 0`: `xQueueReceive(..., tickInterval)` then `onTick()` on timeout.
- `tickInterval == 0`: non-blocking poll of the queue then `onTick()` every loop; comments explicitly expect onTick to block for self-clocked sources like I2S.
  - See `Actor.cpp:326-365`.

**Queue backpressure behaviour**
- If queue utilisation exceeds 50%, the actor drains up to 8 messages using non-blocking receives to avoid command rejection: `Actor.cpp:294-324`.

### 3.3 Key Data Structures (As Implemented)

The `z_changes.md` task list calls out concepts such as `ActorHandle`, `MessageQueue<T>`, and a formal `ActorState` enum. In the current codebase these do **not** exist as first-class types.

Instead:
- “Actor handle” is effectively a raw pointer owned by `ActorSystem` (e.g., `RendererActor*` returned from `ActorSystem::getRenderer()`): `firmware/v2/src/core/actors/ActorSystem.h` and `firmware/v2/src/core/actors/ActorSystem.cpp:71-115`.
- The message queue is a FreeRTOS `QueueHandle_t` created per actor, storing `Message` structs (not templated, not lock-free):
  - `QueueHandle_t m_queue`: `firmware/v2/src/core/actors/Actor.h:439-445`.
  - `m_queue = xQueueCreate(..., sizeof(Message))`: `firmware/v2/src/core/actors/Actor.cpp:36-52`.

Operationally, this means queue semantics (thread safety, blocking, backpressure) are the standard FreeRTOS queue semantics rather than a bespoke lock-free queue implementation.

### 3.4 Actor “State” and Transitions (As Implemented)

The codebase does not implement a formal `ActorState` enum for lifecycle.

Practical lifecycle state is represented by:
- `m_running` and `m_shutdownRequested` flags in the base actor.
- The task handle being non-null.

Conceptual state diagram (what the flags imply):
```
Constructed
  → start() creates FreeRTOS task
    → Running (m_running=true)
      → shutdown requested (m_shutdownRequested=true)
        → task exits
          → Stopped (m_running=false, task handle cleared)
```

Note: some actors implement additional internal states (e.g., AudioActor sets an ERROR-like internal state when capture init fails), but that is not reflected in the base actor lifecycle API.

### 3.5 Threading Model (P4 Build)

At runtime (P4 build), the system typically consists of:
- One Renderer task on Core 1 (priority 5) — 120 FPS render loop.
- One Audio task on Core 0 (priority 4) — hop capture + DSP + publish snapshots.
- One ShowDirector task on Core 0 (priority 2) — show playback and parameter sweeps.

Concrete pinning examples:
- Renderer config pins Core 1: `firmware/v2/src/core/actors/Actor.h:464-472`.
- ShowDirector pins Core 0: `firmware/v2/src/core/actors/ShowDirectorActor.cpp:96-104`.
- Audio config pins Core 0: `firmware/v2/src/audio/AudioActor.h:815-823`.

### 3.6 Concurrency Risks (Actor Layer)

| Issue | Severity | Priority | Evidence | Notes |
|---|---|---|---|---|
| MessageBus publishes without locking while subscribe/unsubscribe mutate entries in-place | High | P1 | `firmware/v2/src/core/bus/MessageBus.cpp:206-260` plus subscribe/unsubscribe logic in same file | Likely benign if subscriptions are only mutated at start/stop, but unsafe if actors are created/destroyed dynamically. See Document 2 for deep analysis. |

---

## 4. Renderer Execution Path

### 4.1 Frame Tick Call Path

Nominal call chain (Renderer):
```
FreeRTOS scheduler
  → Actor::run() [Renderer task]
    → xQueueReceive(..., tickInterval)
      → timeout
        → RendererActor::onTick()
          → RendererActor::renderFrame()
          → ColorCorrectionEngine::processBuffer() (optional)
          → RendererActor::showLeds()
            → ILedDriver::show()
```

Code anchors:
- `Actor::run` tick dispatch: `firmware/v2/src/core/actors/Actor.cpp:326-365`.
- `RendererActor::onTick`: `firmware/v2/src/core/actors/RendererActor.cpp:617-711`.
- `RendererActor::renderFrame`: `firmware/v2/src/core/actors/RendererActor.cpp:961-1273`.
- `RendererActor::showLeds`: `firmware/v2/src/core/actors/RendererActor.cpp:1275-1317`.

### 4.2 Latency-Critical (Hot) Path Identification

Hot path operations per frame:
- Effect render (plugin `IEffect::render(ctx)`) in `renderFrame()`.
- Optional colour correction in `onTick()`.
- Buffer split (`memcpy` into per-strip buffers) and optional silence scaling.
- LED driver “show” (RMT transmit or FastLED.show), plus any required yields.

Notes:
- The renderer explicitly yields before and after `showLeds()` on embedded targets to avoid watchdog starvation while the LED transmission blocks: `RendererActor.cpp:646-652` and `RendererActor.cpp:704-710`.
- Centre-origin compliance is supported by the effect API: RendererActor sets `EffectContext::centerPoint = LedConfig::CENTER_POINT` (79/80 split) before calling `IEffect::render(ctx)`, and the context provides helpers like `getDistanceFromCenter()` and `mirrorIndex()` for symmetric, centre-out patterns:
  - context helpers: `firmware/v2/src/plugins/api/EffectContext.h:501-624`.
  - centrePoint set per-frame: `firmware/v2/src/core/actors/RendererActor.cpp:1155-1160`.

### 4.3 Timing Constraints

| Item | Target / Constraint | Evidence |
|---|---|---|
| Target FPS | 120 FPS | `RendererActor.h` defines `TARGET_FPS=120`, frame time 8333us |
| Frame budget | ~8333us | `firmware/v2/src/core/actors/RendererActor.h` and `CONSTRAINTS.md` |
| Effect code budget | ~2ms (rule of thumb) | `CONSTRAINTS.md` |
| FreeRTOS tick resolution (P4 build) | `CONFIG_FREERTOS_HZ=100` ⇒ 10ms tick; `pdMS_TO_TICKS(8)` becomes 0 and is clamped to 1 tick (10ms) in `ActorConfigs::Renderer()` | `firmware/v2/esp-idf-p4/sdkconfig:2216`, `firmware/v2/src/core/actors/Actor.h:464-472` |

Practical implication (P4 build): unless the renderer switches to a self-clocked loop (tickInterval=0 + explicit micro-sleep) or the project uses a higher FreeRTOS tick rate, the tick-driven renderer cannot reach 120 FPS; it will be quantised to ~100 Hz.

### 4.4 Validation and Profiling Hooks

- Render stats updated per-frame and expose FPS and frame drop count: `RendererActor.cpp:1319-1357`.
- Frame drop defined as raw frame time exceeding budget: `RendererActor.cpp:1323-1326`.
- ValidationProfiler integration around effect ID validation and per-frame updates (build-flag gated): e.g. `RendererActor.cpp:1313-1316`.

---

## 5. Memory Allocation Patterns

### 5.1 Non-Render Allocations Observed

The following allocations occur during startup / configuration, not in the 120 FPS render hot path:
- Actor creation uses `std::make_unique` in `ActorSystem::init()` (`ActorSystem.cpp:79-115`).
- ZoneConfigManager created on heap in S3 and P4 entry points:
  - S3: `firmware/v2/src/main.cpp:203-205`.
  - P4: `firmware/v2/esp-idf-p4/main/main.cpp:93-101`.
- TransitionEngine allocated on heap inside RendererActor constructor (when transitions enabled): `firmware/v2/src/core/actors/RendererActor.cpp:149-152`.

### 5.2 Render-Path Allocation Check (Heuristic)

A repository scan for obvious heap allocators in effect implementations (`new`, `malloc`, `String`, `std::vector`) did not find direct code-level matches inside `firmware/v2/src/effects/ieffect/`.

Important caveat: this is a *heuristic* scan. It cannot prove that no effect triggers allocation indirectly (e.g., via library calls). The primary enforcement remains: code review of new effects and (ideally) runtime heap delta contracts (see Task 3).

### 5.3 Heap Monitoring Support

Heap monitoring hooks exist behind feature flags:
- `HeapMonitor` reports free heap, largest free block, and fragmentation; and provides heap corruption and malloc failure hooks: `firmware/v2/src/core/system/HeapMonitor.cpp`.

### 5.4 Heap Churn Risk Outside Render (S3 vs P4)

This does not violate the “no heap alloc in render paths” constraint, but it affects long-uptime stability and fragmentation risk.

- S3 serial command parsing uses Arduino `String` and substring operations in the main loop, which can create allocation churn during interactive use: e.g. `firmware/v2/src/main.cpp:475-554` and `firmware/v2/src/main.cpp:728-760`.
- P4 replaces the ad-hoc serial parser with an ESP-IDF console REPL (`esp_console`), so the same interactive control surface is less likely to fragment the heap via repeated `String` allocations: `firmware/v2/esp-idf-p4/main/main.cpp:113-176`.

---

## 6. Error Handling and Propagation

### 6.1 Fail-Fast vs Degrade Patterns

- Entry points (S3 and P4) halt the system if ActorSystem init/start fails (fail-fast):
  - S3: `firmware/v2/src/main.cpp:165-169`, `firmware/v2/src/main.cpp:219-222`.
  - P4: `firmware/v2/esp-idf-p4/main/main.cpp:71-84`, `firmware/v2/esp-idf-p4/main/main.cpp:106-111`.
- Audio startup failures are treated as non-fatal at the ActorSystem level (degrade): `firmware/v2/src/core/actors/ActorSystem.cpp:181-199`.

### 6.2 Cross-Actor Error Visibility Gaps

- `Actor::start()` only reports FreeRTOS task creation success; it does not reflect `onStart()` success/failure.
  - Example: AudioActor may fail `m_capture.init()` inside `AudioActor::onStart()` and switch to `ERROR` state, but `AudioActor::start()` still returns success to ActorSystem.
  - Evidence: `AudioActor.cpp:356-362`.

This can produce “silent degradation” (system appears running but has no audio frames).

---

## 7. Integration Points

High-level dependency diagram:
```
            (commands/events)
REPL/Serial ────────────────→ MessageBus / Actor Queues
                                  │
                                  ▼
                         ShowDirectorActor
                                  │ (messages)
                                  ▼
AudioCapture → AudioActor → SnapshotBuffer<ControlBusFrame> → RendererActor → ILedDriver → LEDs
                                  ▲
                                  │
                         ZoneComposer (optional)
```

### 7.1 Audio → Renderer Integration

- Audio publishes `ControlBusFrame` snapshots via `SnapshotBuffer<ControlBusFrame>`:
  - publish: `firmware/v2/src/audio/AudioActor.cpp:1215-1249`.
  - read/extrapolate + freshness gate: `firmware/v2/src/core/actors/RendererActor.cpp:1016-1095`.

- Renderer also receives a TempoTracker pointer to advance phase at 120 FPS:
  - wiring: `firmware/v2/src/core/actors/ActorSystem.cpp:190-199`.
  - usage: `firmware/v2/src/core/actors/RendererActor.cpp:966-999`.

### 7.2 Zones

Renderer delegates to ZoneComposer when enabled:
- `RendererActor.cpp:1122-1134`.

### 7.3 LED Output

Renderer uses HAL-selected `ILedDriver`:
- driver init + buffers acquired: `RendererActor.cpp:927-959`.
- per-frame output: `RendererActor.cpp:1275-1317`.

---

## 8. Summary of Findings

| Finding | Severity | Priority | Evidence | Recommendation |
|---|---|---|---|---|
| MessageBus publish is lock-free but subscribe/unsubscribe mutate the table in-place, creating a data race if subscriptions change at runtime | High | P1 | `firmware/v2/src/core/bus/MessageBus.cpp:206-260` | Either enforce “subscriptions only mutate at init/stop” as a contract, or make publish concurrency-safe via copy-on-write or mutex/critical section. |
| P4 FreeRTOS tick rate (100 Hz) quantises the renderer’s 8ms tickInterval to 10ms (clamped to 1 tick), making 120 FPS unattainable via tick-driven scheduling | High | P1 | `firmware/v2/esp-idf-p4/sdkconfig:2216`, `firmware/v2/src/core/actors/Actor.h:464-472` | Either increase `CONFIG_FREERTOS_HZ` (if acceptable), or change RendererActor scheduling to be self-clocked using a high-resolution timebase (e.g., `esp_timer_get_time()`) and bounded sleeps. |
| RendererActor allocates `TransitionEngine` via `new` (lifetime-managed manually) | Medium | P2 | `firmware/v2/src/core/actors/RendererActor.cpp:149-152` | Prefer `std::unique_ptr` (or static storage) to avoid leaks and simplify ownership. |
| Audio onStart failure is not surfaced to ActorSystem (task creation success ≠ actor readiness) | High | P1 | `firmware/v2/src/audio/AudioActor.cpp:356-362` | Add an explicit “ready” state/handshake or health status that ActorSystem can query/log. |
| RendererActor palette message handling includes host-path `fopen()` debug logging blocks that are compiled for embedded targets | Medium | P2 | `firmware/v2/src/core/actors/RendererActor.cpp:439-456`, `RendererActor.cpp:502-519` | Remove the debug logging, or gate it behind `#ifdef NATIVE_BUILD` (or a dedicated debug flag) and avoid blocking file I/O in actor message handlers. |

---

## 9. Cross-References

- HAL and audio pipeline deep scan (threat model, determinism, DMA timing, concurrency): `docs/analysis/P4_HAL_Audio_Pipeline_Deep_Scan.md`.
- Contracts layer recommendation (how to prevent regressions for timing/memory/concurrency): `docs/analysis/P4_Contracts_Layer_Recommendation.md`.
