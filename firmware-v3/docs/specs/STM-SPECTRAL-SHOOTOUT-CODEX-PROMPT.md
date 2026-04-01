# Codex Agent Prompt — STM Spectral Axis A/B Shootout

> Copy everything below this line as the Codex agent task prompt.

---

## Task

The current STMExtractor uses a 16-point FFT on 16 mel bands for spectral modulation. This resolves 1.1 cycles/octave. The research requires 6 cycles/octave. The current implementation is inadequate and must be replaced.

You will implement **two competing candidates** for the spectral modulation axis, benchmark them head-to-head on synthetic test stimuli, and report a winner. The winner will replace the current spectral extraction in STMExtractor.

**Read the shootout spec FIRST:** `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-SPEC.md` — contains the full algorithm for both candidates, test protocol, pass criteria, and implementation notes. Follow it exactly.

**Also read the parent spec for context:** `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md`

---

## Context

The K1 is an ESP32-S3 LED controller (240 MHz Xtensa LX7, 320 KB SRAM). It drives 320 WS2812 LEDs on a dual-edge Light Guide Plate. The STM system decomposes audio into temporal modulation (rhythm — drives Edge A) and spectral modulation (harmony/timbre — drives Edge B).

The temporal axis (Goertzel on 16 mel bands) is correct and stays unchanged. Only the spectral axis is being replaced.

**Candidate A:** 128 mel bands → 128-point FFT (esp-dsp). True spectrotemporal modulation at 9.1 cycles/octave resolution. Higher memory cost (~10 KB for filterbank weights in flash).

**Candidate B:** Direct feature composite — 6 spectral features (centroid, flatness, crest, roughness, chroma flux, spread) computed from existing bins256[256] and chroma[12]. No FFT needed. Near-zero memory cost. More interpretable but not "true STM."

---

## Mandatory Pre-Reading

Read these files before writing any code:

1. `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-SPEC.md` — **THE SPEC. Every section.**
2. `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` — parent spec (sections 1-3 for context)
3. `firmware-v3/src/audio/pipeline/STMExtractor.h` — current implementation you're replacing part of
4. `firmware-v3/src/audio/pipeline/STMExtractor.cpp` — current spectral extraction method to replace
5. `firmware-v3/src/audio/contracts/ControlBus.h` — bins256[256] and chroma[12] field definitions
6. `firmware-v3/src/audio/pipeline/FFT.h` — existing FFT (do NOT use this for Candidate A — use esp-dsp instead)
7. `firmware-v3/CONSTRAINTS.md` — timing and memory budgets

---

## Deliverables

### 1. Benchmark Harness

Create `harness/stm-spectral-shootout/` as a standalone PlatformIO project:

```
harness/stm-spectral-shootout/
├── platformio.ini
├── src/
│   ├── main.cpp              # Test runner
│   ├── candidate_a.h         # 128-band STM
│   ├── candidate_a.cpp
│   ├── candidate_b.h         # Direct feature composite
│   ├── candidate_b.cpp
│   ├── test_stimuli.h        # Synthetic signal generators
│   └── test_stimuli.cpp
└── README.md
```

**platformio.ini:**
```ini
[env:spectral_shootout]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags =
    -DSPECTRAL_SHOOTOUT
    -O2
    -Wall -Werror
lib_deps =
    espressif/esp-dsp@^1.4.0
monitor_speed = 115200
```

### 2. Test Stimuli (test_stimuli.h/.cpp)

Generate 5 synthetic test signals, each producing 256-bin FFT magnitude arrays and 12-element chroma arrays (simulated ControlBus data):

1. **Sine sweep** (200 Hz → 4 kHz exponential over 500 frames)
2. **Major chord → minor chord** (C major sustained 250 frames → C minor sustained 250 frames)
3. **Drum loop** (kick/snare/hihat pattern at 120 BPM, 500 frames at 125 Hz hop rate)
4. **Complex orchestral** (20 harmonically related sines with vibrato, 500 frames)
5. **Silence → burst → silence** (200 frames silence, 100 frames full-scale noise, 200 frames silence)

Each stimulus function returns a struct:
```cpp
struct TestFrame {
    float bins256[256];
    float chroma[12];
};
```

Generate frames on-the-fly (one at a time) to avoid storing 500 × 256 × 4 bytes = 512 KB. The generator should be stateful:
```cpp
class SineSweepGenerator {
public:
    SineSweepGenerator();
    TestFrame nextFrame();
    void reset();
private:
    int m_frameIndex;
    // ...
};
```

### 3. Candidate A Implementation (candidate_a.h/.cpp)

**128 mel bands → 128-point FFT using esp-dsp.**

Requirements:
- Mel filterbank weights as `static const` in flash (NOT SRAM class members)
- Triangular mel filterbank: 128 bands, 50 Hz to 7 kHz, 32 kHz sample rate
- Use `dsps_fft2r_fc32_ae32` (or `dsps_fft2r_fc32`) from esp-dsp for the 128-point FFT
- Extract magnitudes for bins 0–42 (covers 6 cycles/octave across 7 octaves)
- Normalise output to [0, 1] using peak normalisation
- `process(const float* bins256, float* spectralOut, uint8_t& outCount)` interface
- `reset()` method
- No heap allocation

**Mel filterbank generation:** Precompute at compile time. For each of 128 bands:
- Compute mel-spaced centre frequencies from 50 Hz to 7 kHz
- Triangular filter: rises from left edge to centre, falls from centre to right edge
- Map frequency to bin index: `bin = freq / (sampleRate / fftSize)` = `freq / 62.5`
- Store as sparse representation: start bin, weight count, weight values

**Note on esp-dsp initialisation:** `dsps_fft2r_init_fc32` must be called once before first use. Call it in the constructor. The init function allocates twiddle factors — verify this uses static storage (it does in esp-dsp ≥1.4).

### 4. Candidate B Implementation (candidate_b.h/.cpp)

**6 direct spectral features from bins256 and chroma.**

Requirements:
- Compute: spectral centroid, spectral flatness, spectral crest, roughness, chroma flux, spectral spread
- Formulas are in the shootout spec §4 — implement them exactly
- Store previous chroma frame for flux computation (12 floats = 48 bytes)
- `process(const float* bins256, const float* chroma, float* featuresOut, uint8_t& outCount)` interface
- `reset()` method
- No heap allocation
- Output: 6 floats, each normalised to [0, 1]

### 5. Test Runner (main.cpp)

For each stimulus, for each candidate:

**Timing (1000 iterations):**
- Run candidate.process() 1000 times on the same frame
- Measure each call with `esp_timer_get_time()`
- Report min / median / p99

**Quality (full stimulus playback):**
- Play through all frames of the stimulus
- Record output vector at each frame
- Compute:
  - **Dynamic range:** max(output_magnitude) - min(output_magnitude) across all frames
  - **Discrimination:** Euclidean distance between average output vector for chord stimulus vs drum stimulus
  - **Stability:** Variance of output during sustained chord (frames 50–200 of chord stimulus)

**ASCII visualisation (one stimulus only — chord→chord transition):**
- Print one line per frame
- Each character represents one output element: `·` for <0.1, `▪` for 0.1–0.3, `▬` for 0.3–0.6, `█` for >0.6
- Print frame number at start of each line

**Final verdict:**
- Apply pass criteria from spec §3.4
- Print winner with reasoning

### 6. Serial Output Format

Follow the format in the shootout spec §3.5 exactly. The output must be machine-parseable.

---

## Hard Constraints

1. **No heap allocation.** No `new`, `malloc`, `String`, `std::vector`. Static buffers only. esp-dsp's init functions are allowed (they use internal static storage).
2. **British English** in all comments and strings: centre, colour, normalise, behaviour.
3. **Zero warnings** on build with `-Wall -Werror`.
4. **Both candidates must be standalone modules.** No dependencies on AudioActor, ControlBus, or EdgeMixer. They take float arrays in and produce float arrays out. This keeps the benchmark clean and makes the winner trivially swappable into STMExtractor.
5. **Do not modify any existing firmware code.** This is a standalone benchmark. The winner will be integrated separately.
6. **esp-dsp is the ONLY external dependency.** Do not add others.

---

## Validation Checklist

- [ ] `harness/stm-spectral-shootout/` builds with `pio run -e spectral_shootout` and zero warnings
- [ ] All 5 test stimuli generate plausible synthetic spectra
- [ ] Candidate A correctly applies 128-band mel filterbank and 128-point esp-dsp FFT
- [ ] Candidate B correctly computes all 6 spectral features with formulas matching spec §4
- [ ] Timing reported for both candidates across all stimuli
- [ ] Quality metrics (dynamic range, discrimination, stability) reported for both candidates
- [ ] ASCII visualisation printed for chord stimulus
- [ ] Winner declared with reasoning per spec §3.4 criteria
- [ ] No heap allocation in either candidate (verified by inspection)
- [ ] British English in all comments

---

## What NOT to Do

- Do not modify STMExtractor.h/.cpp, ControlBus.h, AudioActor.cpp, or EdgeMixer.h
- Do not use the custom FFT.h for Candidate A — use esp-dsp (`dsps_fft2r_fc32`)
- Do not hardcode mel filterbank weights by hand — compute them programmatically (constexpr or static init)
- Do not skip any of the 5 test stimuli
- Do not declare a winner without computing all quality metrics
- Do not use heap allocation "temporarily" — not even in test stimulus generation

---

## Escalation Triggers

Stop and report if:

1. esp-dsp's `dsps_fft2r_init_fc32` requires heap allocation that you cannot avoid — this changes the feasibility of Candidate A
2. 128-band mel filterbank weights exceed 16 KB in flash — may need to reduce to 96 or 64 bands
3. Candidate A p99 exceeds 500 µs — indicates algorithmic issue, not just optimisation
4. Neither candidate passes the discrimination threshold (0.3) — indicates the test stimuli are flawed, not the candidates
5. Build fails due to esp-dsp API incompatibility — check esp-dsp version and API docs

---

## File Manifest

### Create:
- `harness/stm-spectral-shootout/platformio.ini`
- `harness/stm-spectral-shootout/src/main.cpp`
- `harness/stm-spectral-shootout/src/candidate_a.h`
- `harness/stm-spectral-shootout/src/candidate_a.cpp`
- `harness/stm-spectral-shootout/src/candidate_b.h`
- `harness/stm-spectral-shootout/src/candidate_b.cpp`
- `harness/stm-spectral-shootout/src/test_stimuli.h`
- `harness/stm-spectral-shootout/src/test_stimuli.cpp`
- `harness/stm-spectral-shootout/README.md`

### Do Not Touch:
- Everything in `firmware-v3/` (read-only reference)
- Everything in `harness/stm-feasibility/` (previous benchmark, leave intact)

---

## Changelog
- 2026-04-01: Initial prompt — A/B comparison of spectral modulation candidates
