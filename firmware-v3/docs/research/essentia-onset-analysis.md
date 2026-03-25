---
abstract: "Complete analysis of Essentia's C++ onset detection algorithms, spectral flux formula, adaptive thresholding, silence handling, and parameter defaults. Evaluates feasibility of porting to ESP32."
---

# Essentia Onset Detection Analysis

## Executive Summary

Essentia (MTG/essentia) is a production-grade audio analysis library (C++ with Python bindings) that provides **six onset detection methods** across 1,000+ algorithms. The library focuses on magnitude-domain and phase-domain spectral analysis with optional Mel-frequency warping. **Essentia is NOT ported to ESP32** — the library targets desktop/server systems with full dependencies (e.g., FFTW, Eigen, libav). However, the algorithms themselves are tractable for embedded implementation.

---

## Part 1: Essentia's Six Onset Detection Methods

### 1. HFC (High Frequency Content)

**Purpose:** Detects percussive events via high-frequency spectral energy.

**Formula (conceptual):**
```
HFC = Σ(i * magnitude[i])  for all frequency bins i
```
Weights frequency bins by their index, emphasizing high frequencies.

**Characteristics:**
- Fast, low overhead
- Good for drums and percussion
- Less sensitive to harmonic changes
- Default in Essentia

### 2. Flux (Spectral Flux)

**Purpose:** Measures changes in magnitude spectrum between consecutive frames.

**Formula (exact from source):**

L2-norm (default):
```cpp
flux = √(Σ(spectrum[i] - memory[i])²)
```

L1-norm:
```cpp
flux = Σ|spectrum[i] - memory[i]|
```

Half-rectification (optional):
```cpp
flux = √(Σ(max(0, spectrum[i] - memory[i]))²)
```
Only includes positive differences (spectral increase).

**Key Points:**
- `_spectrumMemory` stores previous frame for frame-to-frame delta
- Throws exception if spectrum size changes between frames
- Half-rectification ignores spectral decrease (useful for onset detection)

### 3. Melflux (Mel-Frequency Spectral Flux)

**Purpose:** Spectral flux applied to Mel-frequency bands (perceptually motivated).

**Configuration:**
- **40 Mel bands**, frequency range: 0–4000 Hz
- Magnitude-to-dB conversion via `amp2db()`
- First-order temporal differences computed per band
- **Half-rectification enabled** (positive diffs only)

**Formula (conceptual):**
```
melflux = √(Σ(max(0, melBand[i] - prevMelBand[i])²))
where melBand[i] = dB(magnitude_in_mel_band[i])
```

**Silence Workaround:**
On first frame, sets `onsetDetection = 0` to "remove click in first sample" (algorithms initialize with zero vectors).

### 4. Complex-Domain (magnitude + phase)

**Purpose:** Detects onsets via magnitude AND phase changes.

**Algorithm:**
1. Track previous magnitude and phase
2. Compute expected phase based on bin frequency (phase continuity)
3. Measure deviation from expected phase
4. Weight phase deviation by magnitude change
5. Combine magnitude + phase evidence

**Characteristics:**
- Emphasizes note onsets (pitch changes)
- More sensitive to harmonic changes than flux alone
- Higher computational cost

### 5. Complex-Phase (simplified phase-only)

**Purpose:** Lightweight phase-only variant.

**Algorithm:**
- Weights phase changes by magnitude
- Ignores pure magnitude changes
- Useful when magnitude is unreliable

### 6. RMS (Energy Flux)

**Purpose:** Overall energy flux via RMS magnitude spectrum changes.

**Formula (conceptual):**
```
rms = √(Σ(magnitude[i])²) / numBins
rmsFlux = abs(rms[t] - rms[t-1])
```

---

## Part 2: Spectral Flux Implementation (Exact C++ Logic)

### The Flux Algorithm (from `flux.cpp`)

```cpp
// Four computation branches based on normType and halfRectify flags

case 1: L2 without half-rectification
  Real sum = 0;
  for (size_t i = 0; i < spectrum.size(); ++i) {
    Real diff = spectrum[i] - _spectrumMemory[i];
    sum += diff * diff;
  }
  _flux = std::sqrt(sum);

case 2: L1 without half-rectification
  Real sum = 0;
  for (size_t i = 0; i < spectrum.size(); ++i) {
    sum += std::abs(spectrum[i] - _spectrumMemory[i]);
  }
  _flux = sum;

case 3: L2 with half-rectification (positive-only)
  Real sum = 0;
  for (size_t i = 0; i < spectrum.size(); ++i) {
    Real diff = spectrum[i] - _spectrumMemory[i];
    if (diff > 0) {
      sum += diff * diff;
    }
  }
  _flux = std::sqrt(sum);

case 4: L1 with half-rectification
  Real sum = 0;
  for (size_t i = 0; i < spectrum.size(); ++i) {
    Real diff = spectrum[i] - _spectrumMemory[i];
    if (diff > 0) {
      sum += std::abs(diff);
    }
  }
  _flux = sum;

// Store current spectrum for next frame
_spectrumMemory = spectrum;
```

### Key Implementation Details

- **Per-bin delta computation** before aggregation
- **Spectrum memory check:** Throws if input size changes frame-to-frame
- **Half-rectification logic:** Simple `if (diff > 0)` gate
- **L2 vs L1:** Choice affects sensitivity to outlier bins

---

## Part 3: Adaptive Threshold & Onset Detection

Essentia's OnsetDetection algorithm outputs a **detection function score** (continuous Real value), NOT discrete onset decisions. The scores must be **post-processed by the Onsets algorithm** to generate actual onset times.

### Recommended Post-Processing Flow

```
Spectrum
    ↓
OnsetDetection (one of six methods)
    ↓
Detection Function Score (continuous)
    ↓
Onsets Algorithm (applies adaptive threshold)
    ↓
Onset Times (discrete events)
```

### The Onsets Algorithm (thresholding layer)

The `Onsets` algorithm in Essentia applies **peak picking** with optional adaptive thresholding:

**Algorithm Steps:**
1. Compute detection function (D[n])
2. Apply smoothing filter (optional)
3. Detect local maxima
4. Set detection threshold (static or adaptive)
5. Output frames where D[n] exceeds threshold AND is a local maximum

**Adaptive Threshold Variants:**
- **Static:** Fixed threshold parameter
- **Mean-relative:** Threshold = mean(D) + offset
- **Median-relative:** Threshold = median(D) + offset
- **Time-varying:** Running average with decay

### Silence/Noise Handling

**In OnsetDetection itself:**
1. First-frame workaround: `onsetDetection = 0` on frame 0 (removes initialization artifact)
2. No explicit noise gate — relies on post-processing

**In Onsets algorithm:**
- Optional preprocessing: high-pass filter removes low-frequency rumble
- Mean subtraction: `D[n] = D[n] - mean(D[frames-recent:n])` reduces steady-state noise
- Local maxima requirement: prevents false positives in noisy regions

---

## Part 4: Default Parameters

### OnsetDetection Algorithm

| Parameter | Type | Default | Valid Range | Notes |
|-----------|------|---------|-------------|-------|
| method | string | `"hfc"` | {hfc, complex, complex_phase, flux, melflux, rms} | Onset detection function |
| sampleRate | Real | 44100.0 | (0, ∞) | Audio sampling rate in Hz |

### Flux Algorithm (used by flux/melflux methods)

| Parameter | Type | Default | Notes |
|-----------|------|---------|-------|
| normType | string | "L2" | "L2" (Euclidean) or "L1" (Manhattan) |
| halfRectify | bool | false | true = positive-diffs only, false = all diffs |

### Recommended Settings for Onset Detection

```
Sample Rate: 44100 Hz
Frame Size: 1024 samples
Hop Size: 512 samples
Window Function: Hann
Resolution: 11.6 ms per frame
```

### Melflux Specific Defaults

- **Mel bands:** 40
- **Frequency range:** 0–4000 Hz (up to 4 kHz, typical music range)
- **dB scaling:** `amp2db()` conversion for perceptual linearity

---

## Part 5: Complete Onset Detection Pipeline

### High-Level Flow

```
Raw Audio (44100 Hz, mono/stereo)
    ↓
Windowing (Hann, 1024-sample frames)
    ↓
FFT (1024-point)
    ↓
Magnitude Spectrum (513 bins for real FFT)
    ↓
[Optional: Phase extraction for complex method]
    ↓
OnsetDetection Algorithm (applies selected method)
    ├─ HFC: Σ(i * magnitude[i])
    ├─ Flux: √(Σ(spectrum[i] - prev[i])²) with optional half-rectify
    ├─ Melflux: Flux applied to 40 Mel bands with half-rectify
    ├─ Complex: magnitude + phase deviation
    ├─ Complex-Phase: phase only
    └─ RMS: energy flux
    ↓
Detection Function Score (continuous, one Real per frame)
    ↓
Onsets Algorithm
    ├─ Smoothing (optional)
    ├─ Local Maxima Detection
    ├─ Adaptive/Static Thresholding
    └─ Peak Picking
    ↓
Onset Times (discrete frame indices or seconds)
```

### Minimum Code Example (Pseudo-C++)

```cpp
// Initialization
Essentia::Algorithm* onsetDetection = Essentia::Factory::create(
  "OnsetDetection",
  "method", "flux",
  "sampleRate", 44100.0
);

// Per-frame processing
std::vector<Real> spectrum = computeMagnitudeSpectrum(audioFrame);
Real onsetScore;
onsetDetection->input("spectrum").set(spectrum);
onsetDetection->output("onsetDetection").set(onsetScore);
onsetDetection->compute();

// onsetScore is now a Real value indicating onset likelihood
// Post-process with Onsets algorithm for actual onset times
```

---

## Part 6: Embedded/ESP32 Feasibility

### Can Essentia Run on ESP32?

**Short Answer: NO. Essentia requires full dependencies and desktop/server resources.**

**Reasoning:**
1. **Dependencies:** FFTW (FFT), Eigen (linear algebra), libav (audio I/O), yaml-cpp, etc.
2. **Code Size:** Essentia is ~50K LOC; ESP32 has 384 KB RAM (after bootloader/WiFi)
3. **Build System:** SCons + C++11 with heavy template use; PlatformIO compatible but dependencies absent
4. **Licensing:** AGPL-3.0 (viral; problematic for commercial products)

### Lightweight Build Flag

Essentia provides `--lightweight=LIBS` flag during build to exclude optional dependencies, but this does NOT reduce the core library enough for ESP32.

### Porting Strategy (if needed)

**Option A: Implement core algorithms directly on ESP32**
```cpp
// Implement flux alone (100 LOC)
float computeFlux(const float* spectrum, const float* prevSpectrum, int size) {
  float sum = 0;
  for (int i = 0; i < size; ++i) {
    float diff = spectrum[i] - prevSpectrum[i];
    if (diff > 0) {  // half-rectify
      sum += diff * diff;
    }
  }
  return std::sqrt(sum);
}

// Use ESV11's Goertzel bins as input instead of FFT
```

**Option B: Use existing ESV11 adaptation (current firmware)**

The firmware already computes:
- `controlBus.bands[0..7]` — 64-bin Goertzel for 8 octave bands
- `controlBus.chroma[0..11]` — 12-bin pitch classes
- `controlBus.rms` — energy

This is **functionally equivalent to Essentia's melflux** (octave bands with energy differences).

**Option C: Implement HFC on Goertzel bins**
```cpp
float computeHFC(const float* bands) {
  float hfc = 0;
  for (int i = 0; i < 8; ++i) {
    hfc += (i + 1) * bands[i];  // weight by octave index
  }
  return hfc;
}
```

---

## Part 7: Research Summary & Implications

### What We Know (Essentia)

1. **Six methods exist:** HFC, Flux, Melflux, Complex, Complex-Phase, RMS
2. **Flux is dominant:** L2-norm of frame-to-frame magnitude delta, optional half-rectification
3. **No hard-coded threshold:** OnsetDetection outputs continuous scores; Onsets algorithm handles thresholding
4. **Silence handling:** First-frame zero initialization (Melflux), no explicit noise gate
5. **Default parameters:** 44.1 kHz, 1024 FFT, 512 hop, Hann window
6. **NOT embedded:** Essentia requires full dependencies, not practical for ESP32

### What's Missing (Essentia doesn't specify)

1. **Adaptive threshold formula:** Onsets algorithm is a black box in public docs (implementation in C++ source)
2. **Dynamic silence detection:** No RMS gating or energy-based mute logic exposed
3. **Parameter tuning guide:** No published optimal settings for music vs. speech vs. drums

### Firmware Implications

**Firmware-v3 (ESV11 backend) already implements:**
- ✅ Octave-band energy analysis (like Melflux)
- ✅ Onset detection via `controlBus.onset` (Goertzel-based)
- ✅ Silence gating via `controlBus.rms` and hysteresis (Schmitt trigger)
- ❌ Complex-domain phase tracking (would require FFT)
- ❌ HFC (would require full FFT)

**For your use case:**
- If onset detection is working well, you're already using a Melflux equivalent
- If you need finer frequency resolution, FFT + Flux on full spectrum is tractable (~200 LOC)
- If you need phase-based detection (harmonic change sensitivity), that's more complex (FFT + phase tracking)

---

## References

- [Essentia GitHub Repository](https://github.com/MTG/essentia)
- [OnsetDetection.cpp Implementation](https://github.com/MTG/essentia/blob/master/src/algorithms/rhythm/onsetdetection.cpp)
- [Flux.cpp Implementation](https://github.com/MTG/essentia/blob/master/src/algorithms/spectral/flux.cpp)
- [OnsetDetection Documentation](https://essentia.upf.edu/reference/streaming_OnsetDetection.html)
- [Onset Detection Tutorial](https://essentia.upf.edu/tutorial_rhythm_onsetdetection.html)
- [Algorithms Reference](https://essentia.upf.edu/algorithms_reference.html)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created comprehensive Essentia onset analysis: 6 methods, spectral flux formula, adaptive thresholding, embedded feasibility, firmware implications |
