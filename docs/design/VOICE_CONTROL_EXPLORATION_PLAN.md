# Voice Control Exploration Plan

> **Mode:** EXPLORATION — research, prototyping, feasibility validation
> **Branch:** `feature/voice-control-exploration`
> **Created:** 2026-03-08
> **Product:** SpectraSynq K1 (ESP32-S3 N16R8, 16MB flash, 8MB PSRAM OPI)

---

## Executive Summary

Add offline voice control to K1 using Espressif's ESP-SR stack: a wake word ("Lightwave") plus a tight grammar of 15–20 commands, with confirmation via LED light response — no speaker, no cloud, no chatbot.

**Design Principle:** Voice is a control layer, not the brain.

**Approach:** Clean-room harness first. Strip out all LightwaveOS code. Prove ESP-SR runs on the exact K1 hardware in isolation before touching the main firmware.

---

## 0. IDF Version Constraint

**ESP-SR v2.x requires ESP-IDF >= 5.0.** Our main firmware uses `espressif32@6.9.0` (IDF 4.4.7).

This is a **confirmed hard dependency** — ESP-SR's `idf_component.yml` declares `idf: ">=5.0"`.

**Resolution:** The clean-room harness uses Arduino ESP32 v3.x (IDF 5.x) via the [pioarduino](https://github.com/pioarduino/platform-espressif32) community fork. This completely bypasses the main firmware's IDF version. The harness is a separate PlatformIO project — it does not touch `firmware-v3/` at all.

The main firmware IDF upgrade is a **separate, future milestone** that only becomes justified if the harness proves voice is viable.

### References
- [ESP-SR `idf_component.yml`](https://github.com/espressif/esp-sr/blob/master/idf_component.yml) — `idf: ">=5.0"`
- [Arduino ESP32 v3.3.5](https://github.com/espressif/arduino-esp32/releases) — based on IDF 5.5.1+
- [pioarduino platform](https://github.com/pioarduino/platform-espressif32/releases) — Arduino v3.x for PlatformIO
- [ESP-SR Getting Started](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/getting_started/readme.html)

---

## 1. Resource Budget

### 1.1 K1 Hardware (ESP32-S3 N16R8)

| Resource | Total | Harness Overhead | Available for ESP-SR |
|----------|-------|------------------|----------------------|
| **Flash** | 16 MB (15 MB app partition) | ~200 KB (minimal firmware) | ~14.8 MB |
| **SRAM** | 384 KB | ~40 KB (Arduino + Serial) | ~340 KB |
| **PSRAM** | 8 MB OPI | ~0 | ~8 MB |
| **CPU** | 2 cores @ 240 MHz | negligible | both cores free |

### 1.2 ESP-SR Published Requirements

| Resource | ESP-SR Estimate | Source |
|----------|-----------------|-------|
| **CPU** | ~22% one core | Espressif AI solution docs |
| **SRAM** | ~48 KB | AFE state, ringbuffers |
| **PSRAM** | ~1.1 MB | WakeNet + MultiNet model weights |
| **Flash** | ~1.0–1.5 MB | Model binaries + library code |

**Verdict:** The harness has massive headroom on all axes. The real question is whether these numbers hold on our specific hardware/mic configuration.

---

## 2. Harness Architecture

### 2.1 What the Harness Contains

```
harness/voice-feasibility/
├── platformio.ini          # Arduino v3.x via pioarduino, ESP32-S3 N16R8
├── partitions_16mb_voice.csv  # 15 MB app partition (room for models)
├── idf_component.yml       # ESP-SR v2.2.0 dependency
└── src/
    └── main.cpp            # Mic init → ESP-SR → serial log + LED pulse
```

**Included:**
- Board target, flash/PSRAM mode, CPU frequency (exact K1 match)
- I2S pin mapping for SPH0645 microphone (GPIO 13/11/14)
- ESP-SR AFE + WakeNet + optional MultiNet
- Serial logging (resource reports, wake word events, command recognition)
- Single LED confirmation stub (GPIO 48 onboard NeoPixel)
- 30-second periodic resource reports (soak monitoring)

**Excluded (deliberately):**
- Effects, renderer, LED strip driver, FastLED
- WiFi AP, WebServer, mDNS
- Actor system, CQRS command bus
- Audio-reactive pipeline (PipelineCore, ESV11, ControlBus)
- Capture tooling, serial command handler
- Everything not required to prove voice works

### 2.2 What the Harness Cannot Prove

| Question | Harness Answers? | Why Not |
|----------|------------------|---------|
| Can ESP-SR build on this hardware? | **YES** | Direct test |
| What are real resource costs? | **YES** | Measured, not marketing |
| Does SPH0645 mic work for wake words? | **YES** | Same wiring, same enclosure |
| Can wake word + commands run stably? | **YES** | 30-minute soak |
| Can voice coexist with audio-reactive pipeline? | **NO** | Pipeline not present |
| Can voice coexist with 120 FPS render? | **NO** | Renderer not present |
| Does IDF 5.x break existing firmware? | **NO** | Different project entirely |

This is intentional. Coexistence is Phase 1 (integration). Feasibility is Phase 0 (harness).

---

## 3. Phased Execution Plan

### Phase 0: Clean-Room Voice Harness (2–3 sessions)

**Goal:** ESP-SR boots, detects wake word, and recognises 3–5 commands on K1 hardware.

**Location:** `harness/voice-feasibility/` — completely isolated from `firmware-v3/`.

#### Step 0a: Build Verification (Session 1)
1. Verify pioarduino platform resolves and downloads
   - If pioarduino fails: try plain `espressif32` with `platform_packages` override
   - If all PlatformIO paths fail: fall back to native `idf.py` build
2. Verify ESP-SR component resolves via `idf_component.yml`
   - If component registry fails: `git clone --depth 1` into `components/esp-sr`
3. Build `voice_harness` environment — goal is a successful link, not a working product
4. Flash to dev board (NOT K1 — use expendable hardware first)
5. Serial output: boot message, resource report, stub mode confirmation

**Exit criteria:** Binary compiles, links, boots, and prints resource report.

#### Step 0b: I2S Microphone Bring-Up (Session 1–2)
1. Implement IDF 5.x I2S channel driver in `initMicrophone()`
   - `driver/i2s_std.h` API (new channel-based driver, replaces `i2s_driver_install()`)
   - 16 kHz sample rate, 32-bit slots, mono (left or right per SPH0645 datasheet)
2. Read raw I2S samples, print amplitude to serial
3. Verify signal is not all-zeros (the trap that killed 3 sessions in Feb 2026)
4. If zero: check channel selection (SPH0645 = RIGHT on ESP32-S3 legacy driver — verify on new driver)

**Exit criteria:** Serial prints live amplitude values that respond to sound.

#### Step 0c: ESP-SR Integration (Session 2–3)
1. Configure ESP-SR AFE:
   - Single mic, no reference channel (no speaker)
   - `SR_MODE_LOW_COST` for minimal CPU
   - PSRAM allocation mode (`AFE_MEMORY_ALLOC_MORE_PSRAM`)
   - 16 kHz sample rate
2. Load WakeNet model:
   - Use built-in "Hi Lexin" or "Alexa" for initial test
   - Document available English wake word models
3. Feed I2S audio into AFE, poll for wake word detection
4. On detection: print to serial + LED pulse
5. Resource report: before init, after init, during active detection
6. Run 30-minute soak: stability, heap, stack, no resets

**Exit criteria:**
- Say wake word → serial prints detection + LED pulses
- 30-minute soak: zero resets, stable heap
- Resource report shows actual SRAM/PSRAM/flash consumed

#### Step 0d: MultiNet Command Grammar (Session 3, optional)
1. Load MultiNet English model
2. Register 5 test commands: "next effect", "brighter", "dimmer", "lights off", "lights on"
3. Wake word → enter command window (3 sec) → MultiNet recognition → serial log
4. Measure recognition accuracy across 20 attempts per command
5. Document latency: wake word detection to command recognition

**Exit criteria:**
- 5 commands recognised with > 80% accuracy in quiet room
- End-to-end latency (wake word → command ID) < 2 seconds

---

### Phase 1: Integration Branch (3–5 sessions)

**Goal:** Voice running inside LightwaveOS alongside effects and audio-reactive pipeline.

**Prerequisite:** Phase 0 passes.

**WARNING:** This phase requires upgrading `firmware-v3/` to IDF 5.x. That is a large effort with regression risk across:
- FastLED RMT driver (dual-strip parallel TX)
- I2S legacy driver → channel-based driver
- WiFi AP APIs
- FreeRTOS minor changes

Phase 0's success justifies this investment. Phase 0's failure kills it.

**Tasks:**
1. **IDF 5.x upgrade** for `firmware-v3/` in isolated worktree
   - Fix compilation errors (I2S, RMT, WiFi, GPIO API changes)
   - 30-minute soak test on dev board
   - If FastLED breaks: investigate FastLED's IDF 5.x support, patch if needed
2. **VoiceActor** (Actor model, Core 0, priority 3)
3. **Strategy C** (time multiplexing): mic ownership transfer between AudioActor and VoiceActor
4. **CQRS integration**: voice commands dispatch to same command bus as REST/Serial/WebSocket
5. **Feature flag**: `FEATURE_VOICE_CONTROL` compile-time gate
6. **Stress test**: voice + render + audio coexistence

**Deliverables:**
- `src/voice/VoiceActor.h` / `.cpp`
- `src/voice/VoiceConfig.h`
- `src/voice/VoiceCommandParser.h`

---

### Phase 2: Command Grammar + UX (2–3 sessions)

**Goal:** Full 15-command grammar with LED confirmation animations.

**Prerequisite:** Phase 1 passes coexistence stress test.

**Tasks:**
1. 15-command grammar registration (see Section 4)
2. LED confirmation overlays per command type
3. NVS persistence for voice enable/disable state
4. Serial commands: `voice on` / `voice off` / `voice status`
5. REST API: `POST /api/voice/enable`, `GET /api/voice/status`

---

### Phase 3: Concurrent Voice (OPTIONAL)

**Goal:** Voice works without freezing effects (Strategy A: I2S fork).

**Only pursue if:** Strategy C's frozen-effect trade-off is unacceptable in user testing.

**Tasks:**
1. I2S buffer fork: single DMA read, dual consumer
2. 32kHz → 16kHz decimation filter for ESP-SR feed
3. ESP-SR AFE fed via callback, not I2S ownership
4. Full regression: audio-reactive quality unchanged

---

## 4. Command Grammar (15 Commands)

### Wake Word
- **"Lightwave"** (or closest available) — enters 3-second command window. LED: centre-out white pulse.

### Brightness
| Command | Action | LED Confirmation |
|---------|--------|--------------------|
| "brighter" | Brightness += 30 | Flash at new level |
| "dimmer" | Brightness -= 30 | Dim pulse |
| "brightness [N]" | Set to % | Ramp to target |

### Effect Navigation
| Command | Action | LED Confirmation |
|---------|--------|--------------------|
| "next effect" | Next effect | Transition plays |
| "previous effect" | Previous effect | Transition plays |
| "studio mode" | Studio preset | Smooth transition |
| "party mode" | High-energy preset | Smooth transition |
| "calm mode" | Ambient preset | Smooth transition |

### Audio Reactivity
| Command | Action | LED Confirmation |
|---------|--------|--------------------|
| "start listening" | Enable audio mode | Spectrum pulse |
| "stop listening" | Disable audio mode | Fade to static |
| "more bass" | Boost bass sensitivity | Low-frequency throb |
| "more treble" | Boost treble sensitivity | High shimmer |

### System
| Command | Action | LED Confirmation |
|---------|--------|--------------------|
| "lights off" | Standby | Slow fade out |
| "lights on" | Resume | Slow fade in |
| "reset" | Defaults | Centre-out pulse |

---

## 5. Risks and Mitigations

| # | Risk | Severity | Mitigation |
|---|------|----------|------------|
| 1 | **pioarduino platform unstable** | HIGH | Fallback: native `idf.py` build. Harness is small enough to port. |
| 2 | **ESP-SR component won't resolve** | MEDIUM | Manual `git clone` into `components/`. Documented in platformio.ini. |
| 3 | **SPH0645 mic channel wrong on IDF 5.x** | MEDIUM | Phase 0b explicitly tests this. SPH0645 RIGHT channel may map differently under new I2S driver. |
| 4 | **All-zero DMA** (same trap as Feb 2026) | HIGH | Phase 0b checks for this before proceeding. Known failure mode. |
| 5 | **WakeNet doesn't support "Lightwave"** | MEDIUM | Use "Hi Lexin" for prototype. Custom training documented by Espressif. |
| 6 | **IDF 5.x upgrade breaks main firmware** | HIGH | Only attempted in Phase 1, only after Phase 0 proves voice is viable. Done in isolated worktree. |
| 7 | **CPU contention on Core 0** | HIGH | Phase 1 stress test. Fallback: wake-word-only (no MultiNet) cuts CPU. |
| 8 | **FastLED incompatible with IDF 5.x RMT** | **BLOCKER for Phase 1** | Research FastLED's IDF 5.x support before starting Phase 1. If broken, Phase 1 cannot proceed without FastLED patch. |

---

## 6. Success Criteria

### Harness Pass (Phase 0)
- ESP-SR boots and runs on K1 hardware
- Wake word detected repeatedly
- 3–5 commands recognised > 80% accuracy
- 30-minute soak: zero resets, stable heap
- Resource report: actual numbers documented

### Integration Pass (Phase 1)
- Voice works inside LightwaveOS
- 120 FPS maintained during voice session
- Effects resume within 500ms after voice session
- No heap leaks across 1-hour soak

### Production Ready (Phase 2)
- 15 commands, reliable recognition
- LED confirmations polished
- Feature flag gate, REST API, serial commands

---

## 7. Decision Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-03-08 | Clean-room harness before integration | Isolates ESP-SR feasibility from IDF upgrade risk. Answers 4 questions without touching main firmware. |
| 2026-03-08 | Separate PlatformIO project (not build env in main platformio.ini) | Zero risk of polluting main firmware build. Different IDF version, different dependencies. |
| 2026-03-08 | pioarduino for Arduino v3.x | Community fork with Arduino ESP32 v3.x support. Official PlatformIO espressif32 still on v2.x. |
| 2026-03-08 | Strategy C (time multiplex) for initial integration | Simplest mic sharing. Validates UX before committing to I2S fork or second mic. |
| 2026-03-08 | No speaker — LED-only confirmation | On-brand (light is the medium). No audio output hardware needed. |
| 2026-03-08 | VoiceActor on Core 0 priority 3 | Below AudioActor (4), above ShowDirector (2). Core 1 sacred for render. |
| 2026-03-08 | Defer IDF 5.x upgrade until Phase 0 proves voice viable | IDF upgrade is large effort (~2 weeks). Only justified if voice works on the hardware. |

---

## 8. Open Questions (Phase 0 Will Resolve)

1. ~~**IDF version:** Does `esp-sr` work with IDF 4.4.7?~~ **ANSWERED: No.** IDF >= 5.0 required. Harness uses Arduino v3.x.
2. **pioarduino stability:** Does the community fork build reliably for ESP32-S3 N16R8?
3. **ESP-SR component resolution:** Does `idf_component.yml` work under PlatformIO, or is manual clone needed?
4. **SPH0645 on IDF 5.x:** Does the new I2S channel driver produce correct audio from this mic?
5. **Available wake words:** Which English wake word models ship with ESP-SR v2.2.0?
6. **MultiNet English model:** Command registration API, max commands, phoneme format?
7. **PSRAM allocation:** Does ESP-SR auto-use PSRAM, or does it need explicit configuration?
8. **FastLED + IDF 5.x:** Does FastLED support the new RMT driver? (Blocks Phase 1)

---

*Next action: `cd harness/voice-feasibility && pio run` — attempt first build.*
