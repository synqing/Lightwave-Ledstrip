# STM Spectral Axis Shootout — A/B Comparative Benchmark

**Status:** SPECIFICATION — Ready for implementation
**Supersedes:** The 16-point FFT spectral modulation in the initial STM implementation
**Date:** 2026-04-01

---

## 1. Problem Statement

The current STMExtractor uses a 16-point FFT on 16 mel bands for spectral modulation extraction. This resolves **1.1 cycles/octave**. The STM literature (Chang et al. 2025) establishes that spectral modulation up to **6 cycles/octave** is perceptually meaningful. The current implementation captures 18% of the required resolution. That's inadequate.

Two replacement candidates will be implemented and tested head-to-head. The winner replaces the 16-point FFT in STMExtractor's spectral path. The temporal path (Goertzel on mel bands) is unchanged — it's the right tool for that axis.

---

## 2. Candidates

### Candidate A: Full-Resolution STM (128 Mel Bands → 128-Point FFT)

**What it is:** Scale the mel filterbank from 16 to 128 bands across bins256. Apply a 128-point real FFT across the mel bands. Keep bins 0–42 (≤6 cycles/octave across 7 octaves = 42 cycles total / 128 bins × Nyquist = ~42 bins needed).

**Spectral resolution:** 128 bands across 7 octaves = 18.3 bands/octave. Nyquist = 9.1 cycles/octave. **Exceeds the 6 cycles/octave requirement.**

**Computation:**
- Mel filterbank: 256→128 dot products (sparse — ~20 non-zero weights per band avg) ≈ 128 × 20 = 2,560 multiply-adds
- 128-point FFT: 37.5 µs (radix-2 f32) or 6.7 µs (int16) per ESP-DSP benchmarks
- Magnitude extraction: 64 sqrt operations ≈ 5 µs
- **Total estimate: 50–60 µs (f32) or 15–20 µs (int16)**

**Memory:**
- Mel filterbank weights: 128 bands × ~20 weights × 4 bytes = ~10 KB
- Mel history buffer: 128 × 16 frames × 4 bytes = 8 KB (if temporal axis also scales)
- FFT buffer: 128 × 4 bytes = 512 bytes
- **Total: ~19 KB** (breaks the 4 KB class budget)

**Memory solution:** Store mel filterbank weights as `static const` (flash/PROGMEM), not class members. The weights are precomputed constants — they belong in flash, not SRAM. Class footprint then drops to ~9 KB (mostly the history buffer). If temporal axis stays at 16 mel bands (only spectral axis uses 128), the history buffer stays at 1 KB and total class SRAM is ~1.5 KB.

**Architecture note:** The temporal and spectral axes do NOT need the same mel resolution. Temporal modulation (Goertzel) operates on per-band envelopes over time — 16 bands gives adequate temporal tracking. Spectral modulation (FFT across bands) needs fine frequency resolution — 128 bands is correct here. This means:
- Temporal path: 16 mel bands (unchanged) → Goertzel → stmTemporal[16]
- Spectral path: 128 mel bands (new) → 128-point FFT → stmSpectral[42]
- Two separate mel filterbanks, different resolutions, different purposes

### Candidate B: Direct Feature Composite (No Spectral FFT)

**What it is:** Replace the spectral FFT entirely with a vector of directly computed spectral features, each derived from data already in ControlBus (bins256, chroma[12]) or cheaply computable from bins256.

**Feature vector (6 elements):**

| Feature | Formula | What it captures | Source | Cost |
|---------|---------|-----------------|--------|------|
| **Spectral centroid** | Σ(f × mag) / Σ(mag) over bins256 | Brightness / timbral colour | bins256 | ~2 µs |
| **Spectral flatness** | exp(Σ(log(mag))/N) / (Σ(mag)/N) | Tonal vs noise-like | bins256 | ~4 µs |
| **Spectral crest** | max(mag) / mean(mag) | Pure tone vs complex mixture | bins256 | ~1 µs |
| **Roughness** | Σ(|mag[i] - mag[i+1]|) for adjacent bins in 200–2000 Hz | Sensory dissonance | bins256 | ~2 µs |
| **Chroma flux** | Σ(|chroma[i]_now - chroma[i]_prev|) | Chord/key change rate | chroma[12] | ~1 µs |
| **Spectral spread** | Σ((f - centroid)² × mag) / Σ(mag) | Bandwidth / fullness | bins256 | ~3 µs |

**Total estimate: 13–15 µs.** No FFT required. No mel filterbank required for spectral path.

**Memory:** ~100 bytes (one float per feature × 2 for current + previous frame for delta computation). Negligible.

**Resolution question:** These features don't have "cycles/octave" resolution — they're aggregate statistics. But they capture the perceptual dimensions that actually matter for visual mapping:
- Centroid → "how bright/dark does this sound?" → maps to hue shift or brightness
- Flatness → "is this tonal or noisy?" → maps to saturation
- Roughness → "consonant or dissonant?" → maps to visual texture/interference
- Chroma flux → "are chords changing?" → maps to colour movement rate
- Crest → "single instrument or ensemble?" → maps to visual complexity
- Spread → "narrow or wide frequency content?" → maps to spatial spread on edge

**Advantages over Candidate A:**
- Every feature has a direct, interpretable mapping to a visual parameter
- Maps directly to the research chain (Palmer: centroid→brightness, McPherson: roughness→dissonance, GEMS: flux→arousal)
- Near-zero memory cost
- Computed from data that already exists in ControlBus — no new extraction pipeline

**Disadvantages vs Candidate A:**
- Not "true STM" — loses the principled 2D decomposition
- Features are hand-picked, not data-driven
- May miss emergent spectral patterns that the FFT would catch

---

## 3. Test Protocol

### 3.1 Harness Location

`harness/stm-spectral-shootout/`

### 3.2 Test Stimuli

Generate or capture 5 test signals that exercise different spectral characteristics:

| Stimulus | What it tests | Expected behaviour |
|----------|--------------|-------------------|
| **Pure sine sweep** (200 Hz → 4 kHz, 5 sec) | Spectral centroid tracking, single-tone response | Both candidates should show smooth spectral movement |
| **Major chord → minor chord** (C major → C minor, sustained) | Harmonic change detection, mode sensitivity | Should produce clear step change in spectral output |
| **Drum loop** (kick + snare + hihat, 120 BPM) | Rhythmic transient response, percussion vs tonal separation | Temporal axis should dominate; spectral axis should show low tonal content |
| **Complex orchestral** (30-sec clip, strings + brass + woodwinds) | Timbral complexity, multiple simultaneous voices | Spectral output should show high complexity/richness |
| **Silence → loud burst → silence** | Warmup, transient response, decay | Clean onset, no artefacts, clean return to baseline |

For the benchmark harness (no hardware audio input), generate these synthetically:
- Sine sweep: `sin(2π × f(t) × t)` where f(t) sweeps exponentially
- Chords: sum of sine waves at chord intervals
- Drum loop: white noise bursts (kick: 60 Hz LP, snare: 200 Hz BP, hihat: 8 kHz HP) at 120 BPM intervals
- Orchestral: sum of 20 harmonically related sines with vibrato and amplitude envelopes
- Silence/burst: zero → full-scale white noise → zero

### 3.3 Measurements

For each candidate, on each stimulus:

**Timing (quantitative):**
- Per-call: min / median / p99 over 1000 iterations
- Must report: mel filterbank time (A only), FFT time (A only), feature computation time (B only), total pipeline time

**Output quality (quantitative):**
- **Dynamic range:** max output - min output across the stimulus. Higher = more expressive.
- **Responsiveness:** Time from stimulus change to output change exceeding 10% of dynamic range. Lower = faster reaction.
- **Discrimination:** Euclidean distance between output vectors for chord vs drum stimuli. Higher = better separation of tonal vs rhythmic content.
- **Stability:** Variance of output during sustained chord (should be low). Lower = less jitter.

**Output quality (visual — for human evaluation):**
- Print a simple ASCII visualisation of the spectral output vector over time (one line per frame, bars proportional to magnitude). This lets Captain eyeball whether the output is producing meaningful visual differentiation.

### 3.4 Pass Criteria

| Metric | Candidate A threshold | Candidate B threshold |
|--------|----------------------|----------------------|
| p99 latency | < 200 µs | < 50 µs |
| Dynamic range (sine sweep) | > 0.6 | > 0.6 |
| Discrimination (chord vs drum) | > 0.3 | > 0.3 |
| Stability (sustained chord variance) | < 0.05 | < 0.05 |

**Winner selection:** If both pass all thresholds, the candidate with higher discrimination score wins. If discrimination is within 10%, the faster candidate wins. If one fails timing but the other passes all, the passer wins regardless of quality scores.

### 3.5 Output Format

```
=== STM SPECTRAL AXIS SHOOTOUT ===

--- Candidate A: 128-Band STM ---
[TIMING]
  mel_filterbank_128: min=Xus med=Xus p99=Xus
  fft_128:            min=Xus med=Xus p99=Xus
  magnitude:          min=Xus med=Xus p99=Xus
  total:              min=Xus med=Xus p99=Xus
[QUALITY]
  dynamic_range(sweep):    X.XXX
  dynamic_range(chord):    X.XXX
  discrimination(chord_vs_drum): X.XXX
  stability(sustained):    X.XXX
[RESULT] PASS|FAIL

--- Candidate B: Direct Feature Composite ---
[TIMING]
  centroid:     min=Xus med=Xus p99=Xus
  flatness:     min=Xus med=Xus p99=Xus
  crest:        min=Xus med=Xus p99=Xus
  roughness:    min=Xus med=Xus p99=Xus
  chroma_flux:  min=Xus med=Xus p99=Xus
  spread:       min=Xus med=Xus p99=Xus
  total:        min=Xus med=Xus p99=Xus
[QUALITY]
  dynamic_range(sweep):    X.XXX
  dynamic_range(chord):    X.XXX
  discrimination(chord_vs_drum): X.XXX
  stability(sustained):    X.XXX
[RESULT] PASS|FAIL

=== WINNER: Candidate [A|B] ===
Reason: [discrimination|speed|only_passer]
```

---

## 4. Implementation Notes

### Candidate A — 128 Mel Filterbank

The mel filterbank weights MUST be `static const` (stored in flash). Do NOT allocate 10 KB of SRAM for precomputed constants.

```cpp
// In STMExtractor.cpp (or a separate STMMelWeights.cpp)
static const float kMelWeights128[128][MAX_WEIGHTS_PER_BAND] = { ... };
static const uint16_t kMelStartBin128[128] = { ... };
static const uint8_t kMelWeightCount128[128] = { ... };
```

Generate these at compile time or via a constexpr function. The triangular mel filterbank is deterministic given sample rate and band count.

Use `dsps_fft2r_fc32` from esp-dsp for the 128-point FFT — do NOT use the custom FFT.h (which maxes out at 512-point radix-2 and is unoptimised). esp-dsp's radix-2 at 128-point = 37.5 µs; the custom implementation will be slower.

### Candidate B — Direct Features

All features are computed from `bins256[256]` (already normalised [0, 1]) and `chroma[12]`.

**Spectral centroid:**
```cpp
float centroid = 0.0f, totalMag = 0.0f;
for (int i = 0; i < 256; i++) {
    centroid += i * bins256[i];
    totalMag += bins256[i];
}
centroid = (totalMag > 1e-6f) ? centroid / totalMag : 0.0f;
centroid /= 255.0f;  // Normalise to [0, 1]
```

**Spectral flatness:**
```cpp
float logSum = 0.0f, linSum = 0.0f;
int count = 0;
for (int i = 1; i < 256; i++) {  // Skip DC
    float v = fmaxf(bins256[i], 1e-10f);
    logSum += logf(v);
    linSum += v;
    count++;
}
float flatness = expf(logSum / count) / (linSum / count);
// flatness ∈ [0, 1]: 0 = pure tone, 1 = white noise
```

**Roughness (adjacent bin interference, 200–2000 Hz):**
```cpp
// bins256 at 62.5 Hz spacing: 200 Hz = bin 3, 2000 Hz = bin 32
float roughness = 0.0f;
for (int i = 3; i < 32; i++) {
    roughness += fabsf(bins256[i] - bins256[i + 1]);
}
roughness /= 29.0f;  // Normalise by bin count
```

**Chroma flux (requires previous frame storage):**
```cpp
float chromaFlux = 0.0f;
for (int i = 0; i < 12; i++) {
    chromaFlux += fabsf(chroma[i] - m_prevChroma[i]);
}
chromaFlux /= 12.0f;
memcpy(m_prevChroma, chroma, sizeof(float) * 12);
```

**Spectral crest:**
```cpp
float maxMag = 0.0f, meanMag = 0.0f;
for (int i = 1; i < 256; i++) {
    maxMag = fmaxf(maxMag, bins256[i]);
    meanMag += bins256[i];
}
meanMag /= 255.0f;
float crest = (meanMag > 1e-6f) ? maxMag / meanMag : 0.0f;
crest = fminf(crest / 10.0f, 1.0f);  // Normalise (crest can be large)
```

**Spectral spread:**
```cpp
float spread = 0.0f;
for (int i = 0; i < 256; i++) {
    float diff = (i / 255.0f) - centroid;
    spread += diff * diff * bins256[i];
}
spread = (totalMag > 1e-6f) ? sqrtf(spread / totalMag) : 0.0f;
// spread ∈ [0, ~0.5]: normalise to [0, 1]
spread = fminf(spread * 2.0f, 1.0f);
```

---

## 5. Hard Constraints (Same as Parent Spec)

1. No heap allocation in any benchmark or implementation code
2. British English in all comments and strings
3. Zero warnings on build (`-Wall -Werror`)
4. Both candidates must be implemented as standalone, swappable modules — the winner slots into STMExtractor without touching AudioActor or ControlBus wiring
5. The benchmark harness must be self-contained in `harness/stm-spectral-shootout/`

---

## 6. After the Shootout

The winning candidate replaces the current `computeSpectralModulation()` method in `STMExtractor`. The ControlBus fields may need resizing:
- If Candidate A wins: `stmSpectral[42]` (from current `stmSpectral[7]`)
- If Candidate B wins: `stmSpectral[6]` (from current `stmSpectral[7]`) — or rename to `stmSpectralFeatures[6]` for clarity

EdgeMixer's STM_DUAL mode already reads `stmSpectralEnergy` (a scalar). The scalar computation (sum/average of the spectral vector) works regardless of vector length. No EdgeMixer changes needed.

---

## Changelog
- 2026-04-01: Initial spec — replaces inadequate 16-point FFT spectral extraction
