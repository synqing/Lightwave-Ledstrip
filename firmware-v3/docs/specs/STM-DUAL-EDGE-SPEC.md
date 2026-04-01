# STM Dual-Edge Visual Decomposition — Engineering Specification

**Status:** DRAFT — Pending feasibility benchmark
**Domain:** Audio Pipeline → Visual Engine
**Hardware:** ESP32-S3 (K1 v2), with ESP32-P4 as potential future target
**Author:** War Room pipeline (synthesised from 3 research papers + firmware analysis)
**Date:** 2026-04-01

---

## 1. What This Is

Spectrotemporal Modulation (STM) is a signal processing technique that decomposes audio into two orthogonal perceptual axes:

- **Temporal modulation (0–15 Hz):** How fast amplitude envelopes change — rhythm, beat patterns, onset sharpness
- **Spectral modulation (0–9 cycles/kHz):** How fast pitch/formant content shifts — harmonic movement, chord changes, tonal colour

This mirrors how the human auditory cortex represents sound (established neuroscience — Chi et al. 2005, Dau et al. 1997).

**Why it matters for K1:** The K1's dual-edge Light Guide Plate (LGP) is a 2D surface with two independent LED strips (2×160 WS2812, centre-origin outward). Currently both edges receive identical signals. STM provides a principled way to drive each edge with a different perceptual dimension:

- **Edge A (strip1):** Temporal modulation features → rhythm, beat, onset patterns
- **Edge B (strip2):** Spectral modulation features → harmonic movement, tonal shifts
- **LGP surface:** Optical mixing zone where both channels blend physically

This replaces ad-hoc "mirror with hue shift" (current EdgeMixer ANALOGOUS mode) with a perceptually grounded decomposition.

---

## 2. Research Foundation (Distilled)

Three papers converge on the same architectural insight:

| Paper | Contribution | Key Finding |
|-------|-------------|-------------|
| **Chang et al. 2025** (arXiv 2505.23509) — Spectrotemporal Modulation | Formal computation | STM classification matches pretrained DNNs without pretraining. Only temporal ≤±4 Hz and spectral ≤6 cycles/octave are critical (ablation study). |
| **Behringer** — 2D FFT on Piano Roll Plots | Conceptual demonstration | 2D FFT on time×pitch grid produces two orthogonal axes: horizontal = rhythmic periodicity, vertical = harmonic interval structure. Transform is invertible (preserves all musical information). |
| **K1 Hardware** | Physical implementation | Dual-edge LGP with independent strips IS a 2D display surface. EdgeMixer already differentiates Edge A from Edge B. The plumbing exists. |

**Critical ablation result:** Temporal modulation above ±4 Hz and spectral modulation above 6 cycles/octave contribute <5% to audio classification accuracy. This means we only need the lowest-frequency components of each axis — dramatically reducing computation.

---

## 3. Computation Approach — Reduced STM

**This is NOT a monolithic 2D FFT.** The STM computation is separable and reducible:

### 3.1 Input: Mel-Filtered FFT Frames

The K1 already computes `bins256[256]` (256-bin FFT magnitude at 62.5 Hz spacing). We apply a mel filterbank to reduce this to ~16 mel bands. This is a matrix multiply of 256→16 values (precomputed mel weights, ~16 dot products of ~16 non-zero weights each).

**Cost estimate:** 16 × 16 = 256 multiply-adds ≈ 1 µs on S3.

### 3.2 Sliding Buffer

Maintain a circular buffer of the last N mel frames (N = 16 recommended, covering ~128 ms at 125 Hz hop rate).

```
float melHistory[STM_TEMPORAL_FRAMES][STM_MEL_BANDS];  // 16 × 16 = 256 floats = 1 KB
uint8_t writeIndex;  // Current position in circular buffer
```

Each hop: overwrite `melHistory[writeIndex]` with the new mel frame, increment writeIndex mod N.

**Cost:** 16 float copies + 1 index increment ≈ negligible.

### 3.3 Temporal Modulation Extraction (Column Operation)

For each mel band, compute the low-frequency modulation across the 16 time frames. Since we only need ≤4 Hz components and our hop rate is 125 Hz, we need FFT bins 0–1 (DC and ~7.8 Hz). A sliding Goertzel or a simple 16-point FFT per column works.

**Option A — 16 sliding Goertzel accumulators:**
- One per mel band, tracking the ≤4 Hz envelope
- ~2 multiply-adds per band per hop = 32 multiply-adds total
- Cost: <1 µs

**Option B — 16-point FFT per mel band (column FFTs):**
- 16 × 16-point FFTs per hop
- ESP-DSP benchmark: 16-point FFT ≈ 3–5 µs each
- Total: 16 × 5 µs = 80 µs (still within budget but less efficient)

**Recommendation:** Option A (Goertzel) for Phase 1. Option B if finer temporal resolution needed.

**Output:** `stmTemporal[STM_MEL_BANDS]` — float array of temporal modulation magnitude per band.

### 3.4 Spectral Modulation Extraction (Row Operation)

> **⚠️ SUPERSEDED:** The 16-point FFT approach described below resolves only 1.1 cycles/octave — 18% of the 6 cycles/octave required by Chang et al. (2025) ablation results. This section is retained for historical context only. The spectral extraction method is being replaced via a head-to-head A/B shootout between two candidates:
>
> - **Candidate A:** 128 mel bands → 128-point esp-dsp FFT (9.1 cycles/octave — exceeds requirement)
> - **Candidate B:** 6 direct spectral features (centroid, flatness, crest, roughness, chroma flux, spread) computed from existing bins256/chroma
>
> See: `STM-SPECTRAL-SHOOTOUT-SPEC.md` for the full comparison protocol and pass criteria.
> See: `STM-SPECTRAL-SHOOTOUT-CODEX-PROMPT.md` for the implementation agent prompt.
>
> The winning candidate will replace `computeSpectralModulation()` in STMExtractor and may resize `stmSpectral[]` (42 bins if A wins, 6 if B wins).

~~For the current mel frame (16 values), compute the FFT to extract spectral modulation. Keep bins 0–6 (≤6 cycles/octave per ablation result).~~

- ~~One 16-point real FFT per hop~~
- ~~ESP-DSP radix-2 f32 benchmark for 64-point: 16.5 µs. 16-point will be ~4 µs.~~
- ~~Keep first 7 magnitude bins~~

**Output (pending shootout):** `stmSpectral[N]` where N = 42 (Candidate A) or 6 (Candidate B).

### 3.5 Total Computation Budget

| Step | Operation | Cost (S3 @ 240 MHz) |
|------|-----------|-------------------|
| Mel filterbank | 256→16 dot products | ~1 µs |
| Buffer update | 16 float copies | <1 µs |
| Temporal modulation | 16 Goertzel accumulators | <1 µs |
| Spectral modulation | ~~1 × 16-point FFT + magnitude~~ **SUPERSEDED — see §3.4** | ~5 µs (A: ~50 µs, B: ~15 µs) |
| **Total** | | **~8 µs (A: ~53 µs, B: ~18 µs)** |

**Budget note:** The original 8 µs estimate was based on the inadequate 16-point FFT. With the replacement candidates, worst case (Candidate A) is ~53 µs — still well within the 100 µs target and 0.6% of the 8.33 ms frame budget. Candidate B at ~18 µs is negligibly different from the original estimate.

### 3.6 ESP32-P4 Comparison

On P4 @ 400 MHz: all operations ~40% faster wall-clock. The reduced STM is not the reason to upgrade to P4. P4 becomes relevant only if we want full (non-reduced) STM with larger grids (64×32+) or neural feature extraction.

---

## 4. ControlBus Integration

### 4.1 New Fields

Add to `ControlBusRawInput` in `src/audio/contracts/ControlBus.h`:

```cpp
// --- STM Dual-Edge Decomposition (Phase 1: reduced) ---
static constexpr uint8_t STM_MEL_BANDS = 16;
static constexpr uint8_t STM_SPECTRAL_BINS = 7;
static constexpr uint8_t STM_TEMPORAL_FRAMES = 16;

float stmTemporal[STM_MEL_BANDS];      // Temporal modulation per mel band [0,1]
float stmSpectral[STM_SPECTRAL_BINS];  // Spectral modulation magnitude [0,1]
float stmTemporalEnergy;               // Sum of temporal modulation (scalar for quick access)
float stmSpectralEnergy;               // Sum of spectral modulation (scalar for quick access)
bool  stmReady;                         // False until sliding buffer is full (~128ms warmup)
```

**Memory cost:** (16 + 7 + 2) × 4 + 1 = 101 bytes per frame. Negligible against 280 KB available.

### 4.2 Smoothed Fields

Add to `ControlBusFrame`:

```cpp
float stmTemporal[STM_MEL_BANDS];      // Smoothed temporal modulation
float stmSpectral[STM_SPECTRAL_BINS];  // Smoothed spectral modulation
float stmTemporalEnergy;               // Smoothed scalar
float stmSpectralEnergy;               // Smoothed scalar
bool  stmReady;
```

Apply the same asymmetric attack/release smoothing used for `bands[8]`.

---

## 5. New Class: STMExtractor

### 5.1 Pattern

Follow `ChromaAnalyzer` exactly:
- Statically allocated buffers (no heap)
- Called from `AudioActor::tick()` after FFT computation
- Writes to `ControlBusRawInput` fields
- Has `reset()` method for state machine transitions

### 5.2 Interface

```cpp
// File: src/audio/pipeline/STMExtractor.h

#pragma once
#include <cstdint>
#include <cstddef>

namespace audio {

class STMExtractor {
public:
    static constexpr uint8_t MEL_BANDS = 16;
    static constexpr uint8_t SPECTRAL_BINS = 7;
    static constexpr uint8_t TEMPORAL_FRAMES = 16;

    STMExtractor();

    /// Call once per hop with the current FFT magnitude bins.
    /// Applies mel filterbank, updates sliding buffer, extracts STM.
    /// @param bins256  256-bin FFT magnitude spectrum [0,1]
    /// @param temporalOut  Output: temporal modulation per mel band [MEL_BANDS]
    /// @param spectralOut  Output: spectral modulation magnitudes [SPECTRAL_BINS]
    /// @return true if buffer is warmed up and outputs are valid
    bool process(const float* bins256,
                 float* temporalOut,
                 float* spectralOut);

    /// Reset all internal state (call on mode change or silence reset)
    void reset();

private:
    // Mel filterbank weights (precomputed in constructor)
    // Sparse representation: for each mel band, store start bin, end bin, weights
    struct MelBand {
        uint16_t startBin;
        uint16_t endBin;
        float weights[32];  // Max 32 non-zero weights per band
    };
    MelBand m_melBands[MEL_BANDS];

    // Sliding buffer of mel frames
    float m_melHistory[TEMPORAL_FRAMES][MEL_BANDS];
    uint8_t m_writeIndex;
    uint8_t m_framesFilled;  // Warmup counter (0 to TEMPORAL_FRAMES)

    // Goertzel accumulators for temporal modulation
    float m_goertzelState[MEL_BANDS][2];  // [band][real, imag] for target frequency

    // Precomputed Goertzel coefficient for ≤4 Hz at 125 Hz hop rate
    float m_goertzelCoeff;

    // Internal methods
    void applyMelFilterbank(const float* bins256, float* melOut);
    void updateTemporalModulation(const float* melFrame, float* temporalOut);
    void computeSpectralModulation(const float* melFrame, float* spectralOut);
};

}  // namespace audio
```

### 5.3 Memory Budget

| Buffer | Size | Bytes |
|--------|------|-------|
| `m_melHistory[16][16]` | 256 floats | 1,024 |
| `m_melBands[16]` (sparse weights) | 16 × (4 + 4 + 128) | ~2,176 |
| `m_goertzelState[16][2]` | 32 floats | 128 |
| Scratch buffers | ~64 floats | 256 |
| **Total** | | **~3.6 KB** |

Against 280 KB available and current 30.3% utilisation: negligible.

---

## 6. EdgeMixer Integration

### 6.1 Current State

EdgeMixer already has a 3-stage pipeline: Temporal → Colour → Spatial. It reads `audio.rms` for the temporal stage (RMS_GATE mode).

### 6.2 New Mode: STM_DECOMPOSITION

Add a new EdgeMixer mode where:

- **Edge A (strip1):** Brightness/intensity modulated by `stmTemporalEnergy` (rhythmic energy)
- **Edge B (strip2):** Brightness/intensity modulated by `stmSpectralEnergy` (harmonic energy)
- **Colour shift:** Optionally, Edge B's hue rotated proportional to dominant spectral modulation bin

This could be a new `MixMode` enum value (e.g., `STM_DUAL`) or a modifier on existing modes.

### 6.3 Interaction with Existing Effects

Effects continue to render to both strips identically (centre-origin outward). EdgeMixer applies the STM decomposition as a post-process, the same way it currently applies hue shift and spatial gradient. No effect code changes required.

---

## 7. Benchmark Harness Specification

### 7.1 Location

`harness/stm-feasibility/` — standalone PlatformIO project.

### 7.2 What It Tests

| Test | Input | Measurement | Pass Criteria |
|------|-------|-------------|--------------|
| Mel filterbank | Synthetic 256-bin spectrum | µs per call | < 10 µs |
| Sliding buffer update | 16 floats | µs per call | < 2 µs |
| Temporal modulation (Goertzel) | 16 bands × buffer | µs per call | < 10 µs |
| Spectral modulation (16-pt FFT) | 16 mel values | µs per call | < 20 µs |
| Full STM pipeline | 256-bin input → STM output | µs per call (p99 over 1000 iterations) | < 100 µs |
| Full system (STM + existing AudioActor pipeline) | Real I2S audio | Frame timing p99 | < 2.0 ms total effect budget |

### 7.3 Build Target

```
[env:stm_benchmark]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags =
    -DSTM_BENCHMARK
    -O2
lib_deps =
    espressif/esp-dsp@^1.4.0
```

### 7.4 Output Format

Serial output, parseable:

```
[STM-BENCH] mel_filterbank: min=0.8us med=1.1us p99=1.4us
[STM-BENCH] buffer_update: min=0.1us med=0.2us p99=0.3us
[STM-BENCH] temporal_mod: min=0.5us med=0.7us p99=1.2us
[STM-BENCH] spectral_mod: min=3.1us med=3.8us p99=5.2us
[STM-BENCH] full_pipeline: min=5.2us med=6.4us p99=9.1us
[STM-BENCH] PASS: p99 < 100us threshold
```

---

## 8. Hard Constraints (Non-Negotiable)

1. **No heap allocation in the audio/render path.** All STMExtractor buffers are statically allocated class members. No `new`, `malloc`, or `String`.
2. **Centre-origin preserved.** Both edges still inject light from LED 79/80 outward. STM decomposition is an intensity/colour modulation, not a spatial remapping.
3. **AudioActor stays on Core 0.** STMExtractor runs inside AudioActor::tick(), same as ChromaAnalyzer.
4. **Effect budget: 2.0 ms maximum.** STM extraction must not push the total AudioActor processing time beyond the hop budget.
5. **Zero warnings.** `-Wall -Werror` on build. No unused variables, no implicit conversions.
6. **British English** in all comments, docs, logs: centre, colour, initialise, behaviour.
7. **Existing effects unchanged.** STM is additive — no regressions to current effect rendering.

---

## 9. Implementation Phases

### Phase 1: Benchmark Harness (2–3 hours)
- Build `harness/stm-feasibility/`
- Test all grid sizes: 8×4, 8×8, 16×8, 16×16
- Report timing via serial
- Gate: p99 < 100 µs for at least 16×16

### Phase 2: STMExtractor Class (4–6 hours)
- Implement `STMExtractor` following ChromaAnalyzer pattern
- Add ControlBus fields
- Wire into AudioActor::tick()
- Unit tests for mel filterbank accuracy, Goertzel convergence

### Phase 3: EdgeMixer STM Mode (2–3 hours)
- Add `STM_DUAL` mix mode
- Wire stmTemporalEnergy → Edge A modulation
- Wire stmSpectralEnergy → Edge B modulation
- WebSocket command to enable/disable and configure

### Phase 4: Integration Testing (2–3 hours)
- Full system timing validation (AudioActor + RendererActor)
- Verify existing effects unchanged
- Test with real music (varied genres)
- `pio test` pass

---

## 10. References

| Resource | Location |
|----------|----------|
| Chang et al. 2025 — STM paper | arXiv 2505.23509 |
| Behringer — 2D FFT paper | War Room: `SRC · VIS · 2D Fourier Transform Music Visualisation — Behringer.md` |
| ESP-DSP FFT benchmarks (S3) | War Room: `SRC · FW · ESP-DSP Library FFT Benchmarks ESP32-S3 — Espressif Official.md` |
| ESP-DSP FFT benchmarks (P4) | War Room: `SRC · FW · ESP-DSP ESP32-P4 FFT Benchmark Data — Espressif Official.md` |
| ChromaAnalyzer (pattern to follow) | `firmware-v3/src/audio/ChromaAnalyzer.h/.cpp` |
| EdgeMixer (integration target) | `firmware-v3/src/effects/enhancement/EdgeMixer.h` |
| ControlBus (publish target) | `firmware-v3/src/audio/contracts/ControlBus.h` |
| AudioActor (host for STMExtractor) | `firmware-v3/src/audio/AudioActor.h/.cpp` |

---

## Changelog
- 2026-04-01: Superseded §3.4 spectral extraction — 16-point FFT (1.1 cy/oct) replaced by A/B shootout (128-band STM vs direct features). Updated §3.5 budget estimates. Added cross-references to STM-SPECTRAL-SHOOTOUT-SPEC.md.
- 2026-04-01: Initial draft — synthesised from War Room research pipeline (GEMS chain + Behringer + Chang STM + ESP-DSP benchmarks)
