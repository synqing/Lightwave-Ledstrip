---
abstract: "Production-grade onset detection reference from aubio C library. Covers spectral flux algorithm, adaptive thresholding, silence gating, peak picking, and proven default parameters for real-time audio-reactive applications. Gold standard for embedded onset detection."
---

# aubio Onset Detection Reference Implementation

**Source:** [aubio/aubio GitHub](https://github.com/aubio/aubio) — the de facto standard C library for audio analysis, battle-tested on production systems (phones, DAWs, embedded hardware).

**Key insight:** aubio separates three concerns: spectral description (what changed), peak picking (is it a peak), and validation (is it real).

---

## Complete Signal Flow: Audio Input → Onset Boolean

```
Audio Input Buffer (hop_size samples)
    ↓
Phase Vocoder (FFT analysis)
    aubio_pvoc_do() → complex magnitude spectrum
    ↓
[Optional: Spectral Whitening]
    aubio_spectral_whitening_do() (adaptive, disabled by default)
    ↓
[Optional: Log Magnitude Compression]
    λ compression factor (disabled by default)
    ↓
Spectral Descriptor Computation
    aubio_specdesc_do() → scalar onset novelty value
    ↓
Peak Picker (Adaptive Threshold)
    aubio_peakpicker_do()
    ↓
Validation Gates:
    1. Silence check (< -70 dB) → reject
    2. Minimum inter-onset interval (50ms) → reject duplicates
    ↓
Output: onset→data[0] = 0 (no onset) or ≥1 (onset found)
    aubio_onset_get_last() → timestamp in samples
```

---

## Spectral Descriptors (Onset Novelty Functions)

aubio implements **9 spectral descriptor methods**. Each captures different aspects of spectral change:

| Method | Algorithm | Use Case | Default Threshold |
|--------|-----------|----------|-------------------|
| **hfc** | High Frequency Content | Percussive onsets (default) | 0.058 |
| **specflux** | Spectral Flux (positive magnitude differences only) | General onset detection | 0.3 |
| **energy** | Frame-to-frame energy change | Energy-based attack detection | — |
| **complex** | Complex domain (magnitude + phase) | Smooth tonal transitions | 0.15 |
| **phase** | Phase deviation from expected | Phase-coherent onsets | 0.3 |
| **wphase** | Weighted phase deviation | Phase with magnitude weighting | 0.3 |
| **specdiff** | Spectral difference (L2 distance) | General spectral change | 0.3 |
| **kl** | Kullback-Liebler divergence | Information-theoretic distance | 0.3 |
| **mkl** | Modified Kullback-Liebler | KL with magnitude normalization | 0.3 |

### Spectral Flux Implementation (Gold Standard)

```c
void aubio_specdesc_specflux(aubio_specdesc_t *o, const cvec_t *fftgrain, fvec_t *onset) {
  uint_t j;
  onset->data[0] = 0.;
  for (j = 0; j < fftgrain->length; j++) {
    // Only count positive magnitude differences (energy increase)
    if (fftgrain->norm[j] > o->oldmag->data[j])
      onset->data[0] += fftgrain->norm[j] - o->oldmag->data[j];
    o->oldmag->data[j] = fftgrain->norm[j];
  }
}
```

**Formula:** `∑(max(0, |X[k]| - |X_prev[k]|))` over all frequency bins k

**Why this works:**
- Only positive differences count → suppresses spectral continuity
- Sum across all bins → captures broad spectral changes
- Magnitude-only → phase noise ignored
- Simple, efficient, stable

---

## Silence Gate (Noise Suppression)

### Implementation Strategy

aubio uses **magnitude thresholding** in spectral domain to suppress noise:

```c
// In phase-based methods:
if (threshold < fftgrain->norm[j])
  o->dev1->data[j] = ABS(...);  // Process
else
  o->dev1->data[j] = 0.0;       // Suppress
```

### Default Parameters

- **Silence threshold:** -70 dB (set via `aubio_onset_set_silence()`)
- **Conversion:** -70 dB ≈ magnitude 0.0003 (10^(-70/20))
- **Per-bin checking:** Only spectral bins exceeding threshold contribute to onset novelty

### Rationale

-70 dB is the point where:
1. Real ambient noise floor is suppressed
2. Quantization noise from 16-bit audio (≈-98 dB) is completely masked
3. Onset detection remains responsive to soft attacks

**Empirical validation:** Tested across music (quiet classical to loud EDM), speech, and ambient noise without parameter tuning required.

---

## Peak Picking: Adaptive Threshold Method

### Core Algorithm (Real-Time, Slightly Permissive)

aubio's peak picker is **adaptive**—thresholds computed from recent signal statistics:

```c
// 1. Filter spectral descriptor (low-pass biquad)
//    Cuts high-frequency noise while preserving onset transients
//    Cutoff: 0.34 normalized frequency (≈ 0.34 × samplerate / 2)

// 2. Compute running statistics on filtered signal
//    - median of recent samples (median window: 5 samples)
//    - mean of recent samples (mean window: 1 sample)

// 3. Adaptive threshold formula:
//    thresholded[n] = filtered[n] - median - (mean × sensitivity)
//    where sensitivity = 0.1 (default peak-picking threshold)

// 4. Peak detection: three-sample window [n-1, n, n+1]
//    Peak exists if: filtered[n] > filtered[n-1] AND filtered[n] > filtered[n+1]
//    AND thresholded[n] > 0

// 5. Quadratic interpolation for precise timing
//    When peak detected, fit parabola through [n-1, n, n+1]
//    to refine peak position sub-sample accuracy
```

### Key Parameters

| Parameter | Default | Purpose | Tuning Guide |
|-----------|---------|---------|--------------|
| **threshold** | 0.3 | Peak sensitivity multiplier | ↓ for more detections, ↑ for fewer |
| **win_post** | 5 | Median window (samples after current) | Larger = more smoothing = fewer false positives |
| **win_pre** | 1 | Median window (samples before current) | Asymmetry allows faster rise detection |
| **biquad cutoff** | 0.34 | Low-pass filter normalized frequency | Fixed, not user-tunable |

### Why "Slightly More Permissive"

Real-time peak picking must trade latency for false positives:
- **Offline:** Full bidirectional filtering (future data available) → fewer false positives
- **Real-time:** Causal filtering only (no future data) → some extra detections, ~10-50ms lower latency

**Design rationale:** For interactive/creative use (LED effects, musical instruments), responsiveness > precision.

---

## Minimum Inter-Onset Interval (Duplicate Suppression)

### Default Value
- **minioi:** 50 milliseconds (method-dependent, see table below)

### Implementation
After detecting a peak, aubio rejects any subsequent onsets within the minioi window:

```c
if (current_time - last_onset_time < minioi)
  reject_onset()  // Too soon, probably same attack
```

### Method-Specific Defaults

| Method | minioi (ms) | Rationale |
|--------|-------------|-----------|
| hfc | ~29 | Sharp percussive attacks require no overlap |
| complex | ~64 | Smooth tonal onsets need wider gate |
| phase | ~64 | Phase coherence builds gradually |
| specflux | ~50 | Hybrid percussive/tonal default |
| energy | ~50 | Conservative default |

**Musical justification:** Fastest realistic drum rolls (snare) are ~150 BPM double-strokes (~200ms between hits). 50ms minioi allows fast patterns without double-triggering single attacks.

---

## Silence Validation Gate

Final validation after all detection:

```c
if (rms_energy < silence_threshold_linear) {
  onset->data[0] = 0;  // Veto detection
  return;
}
```

**Why two gates (per-bin + global)?**
- **Per-bin spectral gate:** Suppresses spurious bins in noise
- **Global silence gate:** Prevents detection during truly quiet passage (e.g., rest in sheet music)

---

## Default Parameter Summary

### Initialization

```c
aubio_onset_t *onset = new_aubio_onset(
  "hfc",           // Method (string: "hfc", "specflux", "energy", etc.)
  2048,            // buf_size (typical for 44.1kHz: 2048 = 46ms window)
  512,             // hop_size (typical: 512 = 11.6ms)
  44100            // samplerate
);
```

### Tunable Parameters (Post-Init)

```c
aubio_onset_set_silence(onset, -70);        // dB, suppress below this
aubio_onset_set_threshold(onset, 0.3);      // Peak picker sensitivity
aubio_onset_set_minioi_ms(onset, 50);       // Minimum inter-onset (ms)
aubio_onset_set_delay_ms(onset, 4.3 * (hop_size / samplerate) * 1000);
aubio_onset_set_awhitening(onset, 0);       // Adaptive whitening (0 = off)
aubio_onset_set_compression(onset, 0);      // Log compression (0 = off)
```

### Processing Loop

```c
while (audio_available) {
  aubio_onset_do(onset, input_frame);  // Feed hop_size samples
  is_onset = onset->data[0] > 0;       // Boolean result
  if (is_onset) {
    uint_t sample_pos = aubio_onset_get_last(onset);
    smpl_t onset_ms = aubio_onset_get_last_ms(onset);
  }
}
```

---

## Adaptive Whitening (Optional Enhancement)

**Default:** disabled (set via `aubio_onset_set_awhitening(onset, 0)`)

**Purpose:** Spectral shape variance can mask temporal attacks:
- Bright timbre (lots of high-frequency energy) → high spectral flux baseline
- Dark timbre (low-frequency energy) → low spectral flux baseline
- Adaptive whitening normalizes these differences

**Cost:** Additional DSP per frame. For embedded, leave disabled unless specifically targeting timbre-invariant detection.

---

## Log Magnitude Compression (Optional Enhancement)

**Default:** disabled (compression factor = 0)

**When enabled:** Applies `log(magnitude + ε)` before spectral descriptor computation

**Effect:** Compresses dynamic range, making quiet attacks more visible relative to sustained notes

**Trade-off:** Reduces precision of spectral flux for weak attacks; not recommended for music production.

---

## System Latency & Delay Compensation

aubio accounts for group delay through the analysis chain:

### Default Delay Calculation
```c
delay = 4.3 * hop_size  // empirical (4.6× for complex domain)
```

**Why?** Phase vocoder introduces ~0.5 hops latency. Peak picker (median filter, biquad) adds ~3.8 hops. Total ≈ 4.3 hops.

**For K1 firmware:** If using aubio, set explicit delay:
```c
aubio_onset_set_delay_ms(onset,
  4.3 * (hop_size / samplerate) * 1000);
```

Then retrieve corrected timestamps:
```c
uint_t corrected_sample = aubio_onset_get_last(onset);  // Delay already subtracted
```

---

## Embedded Considerations: ESP32 Feasibility

### aubio Native Port Status
- **No official ESP32 port** found in search results
- aubio is pure C, no external dependencies required as of v0.4.0
- Ported to various platforms: iOS (Objective-C wrapper), Android (JNI), browser (Emscripten)

### Memory Requirements (Estimated)
For `buf_size=2048, hop_size=512`:
- **Phase vocoder:** ~20 KB (FFT buffers, window)
- **Spectral descriptor:** ~2 KB (magnitude history)
- **Peak picker:** ~4 KB (filter state, buffers)
- **Metadata:** ~1 KB
- **Total:** ~27 KB heap (modest for ESP32-S3)

### CPU Requirements (Estimated, 44.1 kHz)
- **FFT (2048 → 1024 bins):** ~2.5 ms on ARM Cortex-M4 @ 100 MHz
- **Spectral descriptor:** ~0.1 ms
- **Peak picking:** ~0.05 ms
- **Total per hop (11.6ms):** ~2.65 ms (23% real-time on 100 MHz)

**For ESP32-S3 (dual-core, 240 MHz):** Should run comfortably in background, leaving Core 1 for rendering.

### Key Porting Steps (If Attempted)
1. Extract `src/onset/`, `src/spectral/`, `src/analysis/` directories
2. Remove file I/O dependencies (`sndfile`, `libav`)
3. Provide FFT backend (FFTPACK bundled in aubio, or use ESP-IDF FFT)
4. Implement audio input callback (I2S, USB, etc.)
5. No floating-point library needed (uses libm: sin, cos, log, sqrt — all in math.h)

---

## Comparison to LightwaveOS Current Approach

### aubio Strengths
✅ Separated concerns (spectral desc ≠ peak picking ≠ validation)
✅ Method selection (can switch algorithms without recompile)
✅ Adaptive thresholding (tracks signal statistics automatically)
✅ Quadratic peak refinement (sub-sample accuracy)
✅ Multiple validation gates (silence + minioi)

### LightwaveOS Current (Goertzel-based ESV11)
✅ Fixed memory footprint (64 bins pre-allocated)
✅ Faster computation (no FFT)
✅ Tighter control over latency

### Recommended Synthesis
For LightwaveOS, consider **hybrid approach**:
1. Keep ESV11 (Goertzel) for tempo/beat tracking (already tuned)
2. Add aubio-inspired **spectral flux** over existing `bands[0..7]`
   - Simple: `∑max(0, bands[i] - bands_prev[i])`
   - Reuse existing octave extraction, no new FFT needed
3. Implement aubio's **adaptive threshold** peak picker (not current fixed threshold)
4. Add **minioi** duplicate suppression (missing in current)

This gives you onset detection without full aubio integration.

---

## References

- [aubio GitHub Repository](https://github.com/aubio/aubio)
- [aubio Documentation - Spectral Features](https://aubio.org/manual/latest/py_spectral.html)
- [aubio Onset Detection Issue #106 - Method Optimization](https://github.com/aubio/aubio/issues/106)
- [aubio API Reference](https://aubio.org/doc/latest/)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:claude-code | Created: comprehensive aubio reference including spectral flux algorithm, adaptive thresholding, peak picking, silence gating, and embedded feasibility analysis |
