# P4 HAL and Audio Pipeline Deep Scan (ESP32-P4 LightwaveOS v2)

## 1. Scope and Threat Model

### 1.1 Scope

This audit focuses on the ESP32-P4 build’s hardware boundary and real-time audio→render integration:
- HAL selection and platform divergence risks (S3 vs P4).
- LED output determinism and the P4 parallel RMT driver.
- Audio capture (I2S std-mode) and DSP pipeline (AudioActor).
- Cross-core data sharing (SnapshotBuffer) and concurrency correctness.
- Resource exhaustion risk (queues, buffers, watchdog interactions).
- Real-time correctness and instrumentation gaps.

Network / OTA subsystems are largely excluded from the P4 build by CMake filters (`/network/`, `/sync/`, `/plugins/`, etc.): `firmware/v2/esp-idf-p4/main/CMakeLists.txt:8-20`.

### 1.2 Threat Model

**Inputs and trust boundaries (P4)**
- Primary control input is local console/serial via `esp_console` REPL: `firmware/v2/esp-idf-p4/main/main.cpp:113-176`.
- No WiFi/web API on P4 (`FEATURE_WEB_SERVER=0`): `firmware/v2/esp-idf-p4/main/CMakeLists.txt:27-40`.

**Attack vectors (realistic)**
- Command spam over REPL causing queue saturation and dropped control messages.
- Malformed/edge-case command parameters (e.g., effect ID bounds).

**Failure modes (engineering threats)**
- Silent degradation: audio capture fails but system continues rendering without audio.
- Real-time deadline violations: LED transmission blocking + insufficient yielding → watchdog resets.
- Concurrency defects: pub/sub table races, stale pointer use, torn reads.
- Resource exhaustion: queue fills, memory fragmentation, long-uptime drift.

---

## 2. HAL Interface Analysis

### 2.1 Compile-Time Selection

HAL selection is compile-time via `firmware/v2/src/hal/HalFactory.h`.
- On P4, default LED driver is the custom parallel RMT implementation (`LedDriver_P4_RMT`) unless `USE_FASTLED_DRIVER=1`: `HalFactory.h:49-70`.
- On S3, LED driver is FastLED-based `LedDriver_S3`: `HalFactory.h:21-33`.

### 2.2 HAL Coverage (Current State)

The interfaces exist, but adoption is partial:
- `ILedDriver` is actively used by RendererActor (`m_ledDriver.initDual(...)`, `m_ledDriver.show()`): `firmware/v2/src/core/actors/RendererActor.cpp:927-959`, `RendererActor.cpp:1310-1312`.
- `IAudioCapture` exists in `firmware/v2/src/hal/interface/IAudioCapture.h` but the audio stack currently uses `audio::AudioCapture` directly (not HAL-polymorphic).
- `INetworkDriver` exists but P4 excludes networking at build time.

Requested-but-missing HAL components (per `z_changes.md`):
- No `ClockHAL`, `PowerHAL`, or `DMAController` abstractions were found under `firmware/v2/src/hal/interface/` (or elsewhere). On P4, clock/power/DMA behaviour is therefore primarily an ESP-IDF concern, with only local policy layered on top (e.g., renderer yields to satisfy WDT; LED driver implementation choices).

### 2.3 Platform Divergence Risks

| Divergence | Severity | Priority | Evidence | Risk |
|---|---|---|---|---|
| Audio contract mentions/assumptions diverge across files (hop rate comments vs actual config) | Medium | P2 | `firmware/v2/src/config/audio_config.h:72-97` vs `firmware/v2/src/config/chip_esp32p4.h:167-179` | Documentation drift can hide timing regressions; contracts should source-of-truth from config headers. |
| P4 build CMake requires `FastLED` even when P4 LED driver is non-FastLED RMT | Low | P2 | `firmware/v2/esp-idf-p4/main/CMakeLists.txt:16-20` | Mostly harmless (CRGB type), but increases dependency surface area. |

---

## 3. Audio Buffer and DMA Pipeline

### 3.1 Components and Data Flow

```
I2S peripheral (ESP-IDF driver)
  → DMA descriptors (internal ring)
    → i2s_channel_read() (AudioCapture::captureHop)
      → hop buffer (PCM samples)
        → AudioActor::processHop (DSP)
          → SnapshotBuffer<ControlBusFrame>::publish
            → RendererActor::renderFrame (read + extrapolate + freshness gate)
```

**Capture**: `audio::AudioCapture` (P4 path uses I2S std-mode).
- Initialisation: `AudioCapture::init()` calls `configureI2S()` then enables the channel: `firmware/v2/src/audio/AudioCapture.cpp:62-105`.
- Per-hop capture: `AudioCapture::captureHop()` calls `i2s_channel_read(...)` and post-processes samples (DC-block + clamp): `AudioCapture.cpp:125-200`.

DMA/ISR note:
- The “I2S ISR → DMA ping/pong → app buffer” details are implemented inside the ESP-IDF I2S driver. This codebase interacts via `i2s_channel_read(...)` (task context) rather than providing a custom ISR. Practically, the determinism questions become: how deep the driver buffering is, how often reads slip, and what the application does when it is behind.

**Processing**: `audio::AudioActor` owns the capture device and DSP.
- Starts capture in `AudioActor::onStart()` and enters RUNNING state only on success: `firmware/v2/src/audio/AudioActor.cpp:345-376`.
- Each tick performs `captureHop()` and then `processHop()` on success: `AudioActor.cpp:406-486`, `AudioActor.cpp:514-581`.

**Publish**: AudioActor publishes a `ControlBusFrame` snapshot to the renderer using a lock-free `SnapshotBuffer`.
- Publish call: `firmware/v2/src/audio/AudioActor.cpp:1215-1249`.
- Snapshot buffer design: `firmware/v2/src/audio/contracts/SnapshotBuffer.h`.

**Renderer side**: `RendererActor::renderFrame()` reads snapshots by value, extrapolates AudioTime, and gates freshness.
- Read + extrapolate + freshness: `firmware/v2/src/core/actors/RendererActor.cpp:1016-1057`.

### 3.2 Timing Constraints

From config:
- P4 sample rate: 16 kHz.
- P4 hop size: 160 samples (10ms) to align with 100 Hz FreeRTOS tick.
  - `firmware/v2/src/config/audio_config.h:82-91`.

AudioActor tick interval is ceiling-rounded to avoid 0 ticks at 100 Hz:
- `LW_MS_TO_TICKS_CEIL_MIN1(AUDIO_ACTOR_TICK_MS)` in `firmware/v2/src/audio/AudioActor.h:791-823`.

### 3.3 DMA Overrun / Backlog Handling

At the I2S layer, the driver provides buffering via DMA descriptors; at the application layer:
- `AudioCapture` exposes non-blocking and short-timeout hop reads for backlog draining (`captureHopNonBlocking`, `captureHopWithTimeout`), but the main AudioActor tick path currently uses the blocking `captureHop()` call: `firmware/v2/src/audio/AudioCapture.h:52-84` and `firmware/v2/src/audio/AudioActor.cpp:521-581`.

Recommendation (if overruns observed): integrate catch-up reads into AudioActor when behind (bounded work per tick), and record overrun counters into the published ControlBusFrame.

### 3.4 Failure Modes

| Failure mode | Severity | Priority | Evidence | Notes |
|---|---|---|---|---|
| Capture init fails → AudioActor goes to ERROR; ActorSystem cannot detect because Actor::start only reflects task creation | High | P1 | `AudioActor.cpp:356-362` and `Actor.cpp:55-106` | Results in “audio unavailable” downstream while renderer continues. |
| Snapshot staleness logic depends on correct `AudioTime` extrapolation and `audioStalenessMs` tuning | Medium | P2 | `RendererActor.cpp:1028-1057` | Needs instrumentation to avoid “looks OK but is drifting”. |

---

## 4. Clock Synchronisation

### 4.1 Clock Domains

- Audio time base: `esp_timer_get_time()` + monotonically increasing `sample_index` (AudioActor): `firmware/v2/src/audio/AudioActor.cpp:636-643`.
- Render time base: `micros()` in RendererActor, with extrapolation from last audio timestamp: `firmware/v2/src/core/actors/RendererActor.cpp:1028-1046`.
  - On the P4 ESP-IDF build, `micros()` is implemented as a wrapper over `esp_timer_get_time()`, so AudioActor and RendererActor share the same monotonic wall clock in practice: `firmware/v2/esp-idf-p4/components/arduino-compat/include/Arduino.h:91-103`.
- LED time base: WS2812 bit timings via RMT; output time is dominated by wire protocol and driver implementation.

### 4.2 Synchronisation Mechanism

Renderer extrapolates AudioTime forward based on wall-clock delta (`now_us - lastAudioMicros`) and sample rate. This is effectively a “soft PLL” without explicit drift correction.

**Risk**: even if Audio and Renderer share the same wall clock, that wall clock can still drift relative to the I2S peripheral clock domain (sample clock). Over long periods, extrapolation may slowly misalign beat phase if the effective sample rate differs from the nominal configured sample rate.

Recommendation: choose one monotonic clock API for both actors (prefer `esp_timer_get_time()` for IDF builds), and add explicit drift telemetry (see Section 8).

---

## 5. Power and Thermal Behaviour

### 5.1 Power Management

No explicit dynamic frequency scaling / ESP power management (`esp_pm_*`) usage was found in the firmware sources.
Project configuration also has ESP power management disabled (`CONFIG_PM_ENABLE` is not set): `firmware/v2/esp-idf-p4/sdkconfig:1995-1998`.

CPU clock note (P4 build): default CPU frequency is configured as 360 MHz: `firmware/v2/esp-idf-p4/sdkconfig:2030-2033`.

LED power limiting:
- Renderer initialises LED driver with a max power budget `setMaxPower(5, 3000)`: `firmware/v2/src/core/actors/RendererActor.cpp:954-956`.
- P4 RMT driver currently stores max power but does not implement scaling (TODO present): `firmware/v2/src/hal/esp32p4/LedDriver_P4_RMT.cpp` (TODO near end of file).

### 5.2 Thermal Behaviour

No explicit thermal throttling detection or reporting is present.

Recommendation: if long-uptime stability is required, add periodic temperature telemetry (if supported) and surface it via console diagnostics.

---

## 6. Concurrency and Race Conditions

### 6.1 SnapshotBuffer (Audio → Renderer)

`SnapshotBuffer<T>` is a lock-free double buffer with sequence numbering and acquire/release fences.
- Defensive index validation is included for corruption resilience: `firmware/v2/src/audio/contracts/SnapshotBuffer.h:28-66`, `SnapshotBuffer.h:79-105`.

Risk profile: low (well-contained), assuming `T` is trivially copyable or has safe copy semantics.

### 6.2 MessageBus Pub/Sub

`MessageBus::publish()` reads the subscription table without locking for speed: `firmware/v2/src/core/bus/MessageBus.cpp:206-260`.

However, `subscribe()` and `unsubscribe()` mutate the entries in-place (shifting subscriber pointers and decrementing count): `MessageBus.cpp:65-187`.

| Issue | Severity | Priority | Impact |
|---|---|---|---|
| Data race between publish and subscribe/unsubscribe | High | P1 | Potential send to stale pointers; potential duplicate sends; worst-case use-after-free if an actor is destroyed while still referenced. |

Mitigations:
- Enforce a contract: subscriptions mutate only during init/stop, never concurrently with publish.
- Or implement copy-on-write subscription snapshots (double-buffer the table similar to SnapshotBuffer).
- Or lock publish (costlier) but safe.

### 6.3 Watchdog and Yielding

Renderer explicitly yields before and after LED output to allow IDLE task to reset watchdog because `show()` may block for milliseconds:
- `RendererActor.cpp:646-652` and `RendererActor.cpp:704-710`.

P4 build watchdog baseline:
- Task watchdog is enabled with a 5 s timeout: `firmware/v2/esp-idf-p4/sdkconfig:2094-2099`.
- RendererActor explicitly registers itself with the task WDT on embedded builds: `firmware/v2/src/core/actors/RendererActor.cpp:405-410`.

This is a strong indicator that LED output time is a primary real-time hazard.

---

## 7. Memory and Resource Exhaustion

### 7.1 Queues

- Each actor has a fixed-size FreeRTOS queue; sends can fail when full and are logged: `firmware/v2/src/core/actors/Actor.cpp:149-187`.
- Renderer queue size is 32 by config: `firmware/v2/src/core/actors/Actor.h:464-472`.

### 7.2 Heap Fragmentation

- HeapMonitor provides free heap, largest free block, and fragmentation percent: `firmware/v2/src/core/system/HeapMonitor.cpp:70-120`.

Risk note: the S3 Arduino main loop uses `String` heavily for serial parsing (heap churn), but the P4 build uses `esp_console` instead.

---

## 8. Real-Time Performance Metrics

### 8.1 Existing Instrumentation

Renderer:
- Frame timing, FPS, drops: `firmware/v2/src/core/actors/RendererActor.cpp:1319-1357`.

LED driver (P4 RMT):
- `show()` timing includes wait, quantise, transmit-start, total, logged periodically: `firmware/v2/src/hal/esp32p4/LedDriver_P4_RMT.cpp:417-499`.

Audio:
- Capture latency and process latency tracked in diagnostics: `firmware/v2/src/audio/AudioActor.cpp:514-570`.
- Perfetto trace scopes (`TRACE_SCOPE("audio_pipeline")`): `firmware/v2/src/audio/AudioActor.cpp:589-595`.

### 8.2 Metrics Missing (Recommended)

- p99/p999 jitter for audio hop completion time.
- Worst-case (max) LED transmit completion duration for both channels.
- Explicit “audio freshness” histogram (age_s) and sequence gap counters surfaced in a single status command.
- MessageBus failed delivery counts surfaced in status.
- “Effective scheduler resolution” telemetry: log `CONFIG_FREERTOS_HZ` (P4 is 100 Hz in repo `sdkconfig`) and confirm the renderer’s observed FPS matches the 120 FPS target (or document an intentional downgrade): `firmware/v2/esp-idf-p4/sdkconfig:2216`.

---

## 9. Deprecated APIs and Legacy Paths

Inventory (P4 relevant):
- FastLED-based P4 driver exists as a fallback (`LedDriver_P4`), but HalFactory defaults to custom RMT: `firmware/v2/src/hal/HalFactory.h:49-70`, `firmware/v2/src/hal/esp32p4/LedDriver_P4.cpp`.
- `firmware/v2/esp-idf-p4/main/main.c` is a legacy bring-up test using `led_strip` component directly (not used by the main application): `firmware/v2/esp-idf-p4/main/main.c`.
- The older “ad-hoc serial command parser” approach is explicitly avoided (commented) in favour of the REPL due to reset/input conflicts; the main loop now just idles: `firmware/v2/esp-idf-p4/main/main.cpp:113-185`.

---

## 10. Summary of Findings

| Finding | Severity | Priority | Evidence | Recommendation |
|---|---|---|---|---|
| MessageBus publish/subscribe data race risk | High | P1 | `firmware/v2/src/core/bus/MessageBus.cpp:65-187` and `MessageBus.cpp:206-260` | Enforce “mutate only at init/stop” or implement copy-on-write subscriptions. |
| Audio readiness is not observable at ActorSystem level (task creation success ≠ pipeline running) | High | P1 | `Actor.cpp:55-106`, `AudioActor.cpp:345-376` | Add explicit readiness/health reporting and a renderer-visible “audio pipeline state” contract. |
| Multiple timing/config docs are out of sync (e.g., hop rate comments) | Medium | P2 | `audio_config.h`, `chip_esp32p4.h` | Establish one source of truth and add static asserts where possible. |
| P4 RMT power limiting not implemented (budget stored but not enforced) | Medium | P2 | `RendererActor.cpp:954-956`, `LedDriver_P4_RMT.cpp` | Implement optional power limiting or make budget explicit “advisory only”. |
| RTOS tick resolution may constrain scheduling (P4 repo `sdkconfig` uses 100 Hz tick rate) | Medium | P2 | `firmware/v2/esp-idf-p4/sdkconfig:2216` | Confirm this is intentional; if 120 FPS rendering is required, revisit tick rate or move renderer to a self-clocked loop using a high-resolution timer. |

---

## 11. Cross-References

- Control flow mapping and Renderer hot path: `docs/analysis/P4_Control_Flow_Mapping.md`.
- Contracts/invariants recommendation (prevents regression): `docs/analysis/P4_Contracts_Layer_Recommendation.md`.
