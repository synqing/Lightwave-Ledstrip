# Codex Agent Prompt — STM Dual-Edge Implementation

> Copy everything below this line as the Codex agent task prompt.

---

## Task

Implement a Spectrotemporal Modulation (STM) feature extractor for the K1 audio-reactive LED controller. This adds a new audio analysis module that decomposes music into temporal modulation (rhythm/beat patterns) and spectral modulation (harmonic/tonal movement), enabling the K1's dual-edge Light Guide Plate to drive each LED strip with a different perceptual dimension.

**Read the full engineering spec FIRST:** `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` — this contains the complete algorithm, interface design, memory budget, timing budget, and integration plan. Do not deviate from this spec without documenting why.

---

## Context

The K1 is an ESP32-S3 LED controller running LightwaveOS. It drives 320 WS2812 LEDs (2×160) on a dual-edge Light Guide Plate. Audio is captured via I2S at 32 kHz, processed on Core 0 (AudioActor), and the resulting features are published via a lock-free ControlBus to the renderer on Core 1.

The audio pipeline already extracts: RMS loudness, 8 octave bands, 12 pitch class chroma, 64-bin Goertzel spectrum, 256-bin FFT magnitude, onset detection (kick/snare/hihat), beat tracking, tempo estimation, chord state, and musical saliency. STM adds two new feature vectors to this pipeline.

The EdgeMixer component already differentiates Edge A (strip1) from Edge B (strip2) with configurable colour/spatial/temporal transforms. STM integration adds a new mode where Edge A is driven by temporal modulation and Edge B by spectral modulation.

---

## Mandatory Pre-Reading

Read these files in this order BEFORE writing any code:

### Tier 1 — Architecture (understand the data flow)
1. `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` — **THE SPEC. Read every section.**
2. `firmware-v3/docs/audio-visual/AUDIO_SYSTEM_ARCHITECTURE.md` — end-to-end data flow
3. `firmware-v3/CONSTRAINTS.md` — timing and memory budgets

### Tier 2 — Integration Points (the code you will modify)
4. `firmware-v3/src/audio/contracts/ControlBus.h` — struct layout, all existing fields
5. `firmware-v3/src/audio/ChromaAnalyzer.h` and `ChromaAnalyzer.cpp` — **THE PATTERN TO FOLLOW.** Your STMExtractor class must mirror this architecture: static buffers, called from AudioActor::tick(), writes to ControlBusRawInput
6. `firmware-v3/src/audio/AudioActor.h` and `AudioActor.cpp` — where you wire in STMExtractor
7. `firmware-v3/src/audio/pipeline/FFT.h` — existing FFT implementation (you may use this or esp-dsp)
8. `firmware-v3/src/effects/enhancement/EdgeMixer.h` — where you add the STM_DUAL mix mode

### Tier 3 — Constraints
9. `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` — code conventions, GEMS factor tagging
10. `CLAUDE.md` (project root) — hard constraints including centre-origin, no heap in render, British English

---

## Deliverables

### Phase 1: Benchmark Harness

Create `harness/stm-feasibility/` as a standalone PlatformIO project:

```
harness/stm-feasibility/
├── platformio.ini
├── src/
│   └── main.cpp
└── README.md
```

**Requirements:**
- Target: `esp32-s3-devkitc-1` with esp-dsp library
- Generate synthetic 256-bin FFT data (pink noise spectrum)
- Implement mel filterbank (256→16 bands)
- Implement sliding buffer (16 frames × 16 bands)
- Implement temporal modulation extraction (Goertzel method)
- Implement spectral modulation extraction (16-point FFT)
- Measure each step independently using `esp_timer_get_time()` over 1000 iterations
- Report min/median/p99 for each step and full pipeline via serial
- Print PASS/FAIL against 100 µs p99 threshold for full pipeline

**Serial output format:**
```
[STM-BENCH] mel_filterbank: min=Xus med=Xus p99=Xus
[STM-BENCH] buffer_update: min=Xus med=Xus p99=Xus
[STM-BENCH] temporal_mod: min=Xus med=Xus p99=Xus
[STM-BENCH] spectral_mod: min=Xus med=Xus p99=Xus
[STM-BENCH] full_pipeline: min=Xus med=Xus p99=Xus
[STM-BENCH] RESULT: PASS|FAIL (p99=Xus vs threshold=100us)
```

**Build and verify:**
```bash
cd harness/stm-feasibility
pio run -e stm_benchmark
# If hardware available: pio run -e stm_benchmark -t upload && pio device monitor -b 115200
```

### Phase 2: STMExtractor Class

Create these files in the firmware:

```
firmware-v3/src/audio/pipeline/STMExtractor.h
firmware-v3/src/audio/pipeline/STMExtractor.cpp
```

**Requirements (from spec §5):**
- Follow `ChromaAnalyzer` pattern exactly
- Static buffers only — no heap allocation
- `process(const float* bins256, float* temporalOut, float* spectralOut)` → returns bool (false during warmup)
- `reset()` for state machine transitions
- Mel filterbank: 256 FFT bins → 16 mel bands (precomputed sparse weights in constructor)
- Sliding circular buffer: 16 frames × 16 mel bands = 1 KB
- Temporal modulation: Goertzel accumulators per mel band, targeting ≤4 Hz
- Spectral modulation: 16-point FFT on current mel frame, keep first 7 bins
- Normalise outputs to [0, 1] range
- Total memory footprint: ≤4 KB

### Phase 3: ControlBus Integration

Modify `firmware-v3/src/audio/contracts/ControlBus.h`:

Add to `ControlBusRawInput`:
```cpp
static constexpr uint8_t STM_MEL_BANDS = 16;
static constexpr uint8_t STM_SPECTRAL_BINS = 7;

float stmTemporal[STM_MEL_BANDS];    // [0,1] temporal modulation per mel band
float stmSpectral[STM_SPECTRAL_BINS]; // [0,1] spectral modulation magnitude
float stmTemporalEnergy;              // Scalar sum of temporal modulation
float stmSpectralEnergy;              // Scalar sum of spectral modulation
bool  stmReady;                        // False until warmup complete (~128ms)
```

Add matching fields to `ControlBusFrame` with smoothing applied (use the same asymmetric attack/release as `bands[8]`).

### Phase 4: AudioActor Wiring

Modify `firmware-v3/src/audio/AudioActor.cpp`:

1. Instantiate `STMExtractor` as a member (static allocation)
2. Call `m_stmExtractor.process(rawInput.bins256, rawInput.stmTemporal, rawInput.stmSpectral)` inside `tick()`, after the FFT computation but before ControlBus publish
3. Compute scalar energies:
   ```cpp
   float tEnergy = 0.0f, sEnergy = 0.0f;
   for (int i = 0; i < STMExtractor::MEL_BANDS; i++) tEnergy += rawInput.stmTemporal[i];
   for (int i = 0; i < STMExtractor::SPECTRAL_BINS; i++) sEnergy += rawInput.stmSpectral[i];
   rawInput.stmTemporalEnergy = tEnergy / STMExtractor::MEL_BANDS;
   rawInput.stmSpectralEnergy = sEnergy / STMExtractor::SPECTRAL_BINS;
   rawInput.stmReady = result;  // from process() return value
   ```

### Phase 5: EdgeMixer STM Mode

Modify `firmware-v3/src/effects/enhancement/EdgeMixer.h`:

1. Add `STM_DUAL` to the `MixMode` enum
2. In `STM_DUAL` mode:
   - Edge A (strip1): multiply brightness by `audio.stmTemporalEnergy` (scaled 0–255)
   - Edge B (strip2): multiply brightness by `audio.stmSpectralEnergy` (scaled 0–255)
   - Optionally: rotate Edge B hue by dominant spectral modulation bin index (proportional to spectral shift)
3. Add WebSocket command handler in `WsEdgeMixerCommands.cpp`:
   - `{"cmd": "edgemixer_mode", "mode": "stm_dual"}` to activate
   - `{"cmd": "edgemixer_mode", "mode": "mirror"}` to deactivate

---

## Hard Constraints (Violating Any of These Is a Failure)

1. **No heap allocation** in any code path called from `AudioActor::tick()` or `RendererActor::render()`. No `new`, `malloc`, `String`, `std::vector`, `std::string`. Static buffers only.
2. **Centre-origin preserved.** Both edges inject light from LED 79/80 (or `Math.floor(LED_COUNT / 2)`) outward symmetrically. STM modulates intensity/colour, not spatial origin.
3. **Core affinity:** AudioActor on Core 0. RendererActor on Core 1. Do not change this.
4. **2.0 ms effect budget.** STM extraction must not push AudioActor::tick() processing time beyond the hop budget (8 ms). Target: <100 µs for STM specifically.
5. **Zero warnings.** Build must pass with `-Wall -Werror`. No unused variables, no sign comparison warnings, no implicit narrowing.
6. **British English** in all comments, doc strings, log messages, and UI strings: centre, colour, initialise, serialise, behaviour, normalise.
7. **Existing functionality unchanged.** All current effects, EdgeMixer modes, audio features must continue working identically. STM is purely additive.
8. **No external dependencies** beyond what's already in `platformio.ini`. esp-dsp is acceptable (already a lib_dep in the project).

---

## Validation Checklist

Before declaring done, verify:

- [ ] `harness/stm-feasibility/` builds and reports timing via serial
- [ ] Full STM pipeline p99 < 100 µs on ESP32-S3
- [ ] `STMExtractor` class compiles with zero warnings
- [ ] `STMExtractor` follows ChromaAnalyzer pattern (static buffers, reset(), process())
- [ ] ControlBus fields added (stmTemporal, stmSpectral, stmTemporalEnergy, stmSpectralEnergy, stmReady)
- [ ] AudioActor calls STMExtractor in tick() after FFT, before publish
- [ ] EdgeMixer has STM_DUAL mode
- [ ] WebSocket command enables/disables STM_DUAL mode
- [ ] `pio run -e esp32dev_audio_esv11_k1v2_32khz` builds with zero warnings
- [ ] No heap allocation in any new code (verified by inspection)
- [ ] Centre-origin constraint not violated
- [ ] British English in all new comments and strings
- [ ] All pre-existing `pio test` tests still pass

---

## What NOT to Do

- Do not redesign the audio pipeline. STMExtractor slots into the existing architecture.
- Do not modify ChromaAnalyzer, BeatTracker, OnsetDetector, or TempoTracker.
- Do not change FFT.h. Use its output (`bins256`) as input.
- Do not use heap allocation "temporarily" with plans to optimise later.
- Do not add Python/Node dependencies. This is C++ on bare metal.
- Do not change the WiFi mode. K1 is AP-only. (This is unrelated but a hard constraint.)
- Do not modify any effect's render() method. EdgeMixer is a post-process; effects are unaware of it.

---

## Escalation Triggers

Stop and report if:

1. Benchmark p99 exceeds 500 µs (5× the expected budget) — indicates a fundamental algorithmic issue, not just optimisation needed
2. ControlBus struct size exceeds 4 KB total — indicates memory pressure, need architectural review
3. AudioActor tick() total time exceeds 6 ms with STM enabled — indicates pipeline budget is exhausted
4. Any existing test fails after integration — indicates regression, do not proceed
5. You discover EdgeMixer processes strip1 (Edge A) as well as strip2 — the spec assumes strip1 passes through unchanged. If this is wrong, the integration approach changes.

---

## File Manifest (All Files You Will Create or Modify)

### Create:
- `harness/stm-feasibility/platformio.ini`
- `harness/stm-feasibility/src/main.cpp`
- `harness/stm-feasibility/README.md`
- `firmware-v3/src/audio/pipeline/STMExtractor.h`
- `firmware-v3/src/audio/pipeline/STMExtractor.cpp`

### Modify:
- `firmware-v3/src/audio/contracts/ControlBus.h` — add STM fields
- `firmware-v3/src/audio/AudioActor.h` — add STMExtractor member
- `firmware-v3/src/audio/AudioActor.cpp` — wire STMExtractor into tick()
- `firmware-v3/src/effects/enhancement/EdgeMixer.h` — add STM_DUAL mode enum + process logic
- `firmware-v3/src/network/webserver/ws/WsEdgeMixerCommands.h` — add STM_DUAL command handling
- `firmware-v3/src/network/webserver/ws/WsEdgeMixerCommands.cpp` — implement command

### Do Not Touch:
- `firmware-v3/src/audio/ChromaAnalyzer.*` (read-only reference)
- `firmware-v3/src/audio/pipeline/FFT.h` (read-only, consume its output)
- `firmware-v3/src/audio/pipeline/BeatTracker.*`
- `firmware-v3/src/audio/onset/OnsetDetector.*`
- `firmware-v3/src/audio/tempo/TempoTracker.*`
- Any file in `firmware-v3/src/effects/ieffect/` (effects are unaware of EdgeMixer)

---

## Changelog
- 2026-04-01: Initial prompt — synthesised from War Room research pipeline + firmware analysis
