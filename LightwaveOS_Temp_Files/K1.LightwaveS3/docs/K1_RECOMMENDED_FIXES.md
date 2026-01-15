# K1 Beat Tracker Recommended Fixes

**Document Version:** 1.0
**Date:** December 31, 2025
**Author:** Claude Code
**Status:** Ready for Implementation

---

## Overview

This document provides actionable implementation details for fixing K1 beat tracker reliability issues. Based on the investigation in `K1_RELIABILITY_INVESTIGATION.md`, the recommended approach is **Option A: Targeted Novelty Improvement**.

---

## Priority 1: Perceptually-Weighted Spectral Flux

### Problem

All frequency bands contribute equally to spectral flux, causing treble transients (hi-hats) to dominate over bass (kicks).

### Solution

Modify `K1Pipeline.cpp` to apply frequency weighting to flux calculation.

### Implementation

**File:** `v2/src/audio/k1/K1Pipeline.cpp`

**Find:** The `computeNovelty()` or equivalent function that calculates spectral flux.

**Add to K1Config.h:**
```cpp
// Perceptual frequency weights for onset detection
// Higher weight = more contribution to onset signal
static constexpr float K1_BAND_WEIGHTS[8] = {
    1.00f,  // Band 0: 0-250 Hz (sub-bass) - kick drums
    0.85f,  // Band 1: 250-500 Hz (bass) - bass guitar, low synth
    0.60f,  // Band 2: 500-1000 Hz (low-mid) - body of instruments
    0.40f,  // Band 3: 1000-2000 Hz (mid) - vocal fundamentals
    0.25f,  // Band 4: 2000-4000 Hz (high-mid) - vocal presence
    0.15f,  // Band 5: 4000-6000 Hz (high) - hi-hats, cymbals
    0.08f,  // Band 6: 6000-8000 Hz (air) - sibilance
    0.04f   // Band 7: 8000+ Hz (ultrasonic) - near-noise
};
```

**Modify flux calculation:**
```cpp
float computeWeightedFlux(const float* currentBands, const float* prevBands) {
    float weightedFlux = 0.0f;
    float totalWeight = 0.0f;

    for (int i = 0; i < 8; ++i) {
        float diff = currentBands[i] - prevBands[i];
        // Half-wave rectification: only positive changes (onsets)
        if (diff > 0.0f) {
            weightedFlux += diff * K1_BAND_WEIGHTS[i];
        }
        totalWeight += K1_BAND_WEIGHTS[i];
    }

    // Normalize by total weight for consistent scaling
    return weightedFlux / totalWeight;
}
```

### Expected Outcome

- Bass-heavy transients (kick drums) produce larger flux values
- Treble transients (hi-hats) contribute minimally
- Reduced subdivision confusion (locks on quarter notes, not 16ths)

### Testing Criteria

| Test Track | Before | Target |
|------------|--------|--------|
| EDM (4-on-floor kick) | Locks on hi-hat | Locks on kick |
| Rock (kick/snare) | Unstable | Stable lock |
| Jazz (brushes/ride) | False triggers | Fewer triggers |

---

## Priority 2: Multi-Band Onset Detection

### Problem

Single-threshold onset detection treats all frequency content identically.

### Solution

Independent onset detection per frequency band with band-specific thresholds.

### Implementation

**Add to K1Config.h:**
```cpp
// Multi-band onset configuration
static constexpr int K1_ONSET_BANDS = 3;

struct K1OnsetBandConfig {
    float freq_low;      // Hz
    float freq_high;     // Hz
    float weight;        // Contribution to combined onset
    float threshold_k;   // Standard deviations for threshold
};

static constexpr K1OnsetBandConfig K1_ONSET_BANDS_CONFIG[K1_ONSET_BANDS] = {
    { 0.0f,    150.0f,  0.50f, 1.8f },  // Sub-bass: sensitive, high weight
    { 150.0f,  2000.0f, 0.35f, 2.0f },  // Mid: standard threshold
    { 2000.0f, 8000.0f, 0.15f, 2.5f },  // High: conservative, low weight
};
```

**Add new struct to K1Pipeline or K1Types.h:**
```cpp
struct K1BandOnsetState {
    float history[16];      // Rolling window
    int historyIdx;
    float mean;
    float stdDev;
    bool triggered;

    void update(float value) {
        // Update rolling statistics
        float oldVal = history[historyIdx];
        history[historyIdx] = value;
        historyIdx = (historyIdx + 1) % 16;

        // Compute mean
        float sum = 0.0f;
        for (int i = 0; i < 16; ++i) sum += history[i];
        mean = sum / 16.0f;

        // Compute std dev
        float sqSum = 0.0f;
        for (int i = 0; i < 16; ++i) {
            float d = history[i] - mean;
            sqSum += d * d;
        }
        stdDev = sqrtf(sqSum / 16.0f);
    }

    bool checkTrigger(float value, float k) {
        float threshold = mean + k * stdDev;
        if (threshold < 0.02f) threshold = 0.02f;  // Floor
        triggered = (value > threshold);
        return triggered;
    }
};
```

**Modify onset detection:**
```cpp
float computeMultiBandOnset(K1BandOnsetState states[K1_ONSET_BANDS]) {
    float combinedOnset = 0.0f;

    for (int b = 0; b < K1_ONSET_BANDS; ++b) {
        const K1OnsetBandConfig& cfg = K1_ONSET_BANDS_CONFIG[b];

        // Get band energy (sum of Goertzel bins in frequency range)
        float bandEnergy = getBandEnergy(cfg.freq_low, cfg.freq_high);

        // Update state and check trigger
        states[b].update(bandEnergy);
        if (states[b].checkTrigger(bandEnergy, cfg.threshold_k)) {
            combinedOnset += cfg.weight;
        }
    }

    return combinedOnset;  // 0.0 to 1.0
}
```

### Expected Outcome

- Each frequency region has appropriate sensitivity
- Sub-bass (kicks) detected reliably with lower threshold
- High frequencies (hi-hats) require stronger signal to trigger
- Combined onset reflects musical importance, not just energy

---

## Priority 3: Onset Debouncing

### Problem

Single-frame noise spikes can trigger false onsets.

### Solution

Require onset to persist across consecutive frames.

### Implementation

**Add to K1Config.h:**
```cpp
// Onset debounce configuration
static constexpr int K1_ONSET_DEBOUNCE_FRAMES = 2;  // Require 2 consecutive frames
```

**Add debounce logic:**
```cpp
class K1OnsetDebouncer {
public:
    bool process(bool rawOnset) {
        // Shift history
        for (int i = K1_ONSET_DEBOUNCE_FRAMES - 1; i > 0; --i) {
            history_[i] = history_[i - 1];
        }
        history_[0] = rawOnset;

        // Check if all frames in window show onset
        bool debounced = true;
        for (int i = 0; i < K1_ONSET_DEBOUNCE_FRAMES; ++i) {
            if (!history_[i]) {
                debounced = false;
                break;
            }
        }

        // Edge detection: only trigger on rising edge
        bool trigger = debounced && !wasDebounced_;
        wasDebounced_ = debounced;

        return trigger;
    }

private:
    bool history_[K1_ONSET_DEBOUNCE_FRAMES] = {false};
    bool wasDebounced_ = false;
};
```

### Expected Outcome

- Single-frame noise spikes rejected
- Only sustained onsets (real musical events) pass through
- Slight increase in onset latency (~32ms) - acceptable tradeoff

---

## Priority 4: Debug Logging for Validation

### Problem

No visibility into onset detection quality during development.

### Solution

Add conditional debug output to measure onset detection performance.

### Implementation

**Add to K1Config.h:**
```cpp
// Debug flags (set to 0 for production)
#define K1_DEBUG_ONSET_DETECTION 1
#define K1_DEBUG_ONSET_RATE_LIMIT_MS 500  // Print every 500ms max
```

**Add debug output:**
```cpp
#if K1_DEBUG_ONSET_DETECTION
void logOnsetDiagnostics(const K1BandOnsetState states[], bool triggered) {
    static uint32_t lastLogMs = 0;
    uint32_t nowMs = millis();

    if (nowMs - lastLogMs < K1_DEBUG_ONSET_RATE_LIMIT_MS) return;
    lastLogMs = nowMs;

    Serial.printf("[K1-ONSET] Bass: %.2f (thr: %.2f %s) | "
                  "Mid: %.2f (thr: %.2f %s) | "
                  "High: %.2f (thr: %.2f %s) → %s\n",
        states[0].history[states[0].historyIdx - 1],
        states[0].mean + 1.8f * states[0].stdDev,
        states[0].triggered ? "TRIG" : "----",
        states[1].history[states[1].historyIdx - 1],
        states[1].mean + 2.0f * states[1].stdDev,
        states[1].triggered ? "TRIG" : "----",
        states[2].history[states[2].historyIdx - 1],
        states[2].mean + 2.5f * states[2].stdDev,
        states[2].triggered ? "TRIG" : "----",
        triggered ? "ONSET" : "-----");
}
#endif
```

---

## Implementation Roadmap

### Week 1: Core Changes

| Day | Task | Files |
|-----|------|-------|
| 1-2 | Implement perceptual weights | K1Config.h, K1Pipeline.cpp |
| 3-4 | Add multi-band onset detection | K1Pipeline.cpp, K1Types.h |
| 5 | Add onset debouncing | K1Pipeline.cpp |

### Week 2: Testing & Tuning

| Day | Task | Method |
|-----|------|--------|
| 1-2 | Test with reference tracks | Serial debug output |
| 3 | Tune threshold values | Iterative adjustment |
| 4-5 | Measure validation criteria | Before/after comparison |

### Validation Test Suite

Create test playlist with known BPM:

| Track | Genre | BPM | Challenge |
|-------|-------|-----|-----------|
| Four Tet - "Parallel" | EDM | 120 | Clear kick, complex high-end |
| Daft Punk - "Around the World" | House | 121 | Steady groove |
| Red Hot Chili Peppers - "Can't Stop" | Rock | 92 | Syncopated kick |
| Miles Davis - "So What" | Jazz | 136 | Ride cymbal, brush |
| Massive Attack - "Teardrop" | Trip-hop | 80 | Slow tempo, electronic |

---

## Files to Modify

| File | Changes |
|------|---------|
| `v2/src/audio/k1/K1Config.h` | Add weight arrays, onset band config, debug flags |
| `v2/src/audio/k1/K1Pipeline.cpp` | Modify novelty calculation, add multi-band onset |
| `v2/src/audio/k1/K1Types.h` | Add K1BandOnsetState struct if not in K1Pipeline |

## Files to Reference

| File | Purpose |
|------|---------|
| `v2/src/audio/GoertzelAnalyzer.cpp` | Current band energy calculation |
| `v2/src/audio/k1/K1ResonatorBank.cpp` | How novelty feeds into resonators |
| `v2/src/audio/k1/K1TactusResolver.cpp` | How candidates are scored |

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Weights too aggressive | Start conservative, tune with test tracks |
| Increased latency from debouncing | Limit to 2 frames (32ms), acceptable |
| Multi-band adds CPU overhead | Profile before/after, should be <5% increase |
| Threshold tuning takes time | Document iterative process, use debug logging |

---

## Success Metrics

| Metric | Current | Target | How to Measure |
|--------|---------|--------|----------------|
| EDM lock accuracy | ~60% | >85% | Test playlist, manual verification |
| Rock lock accuracy | ~40% | >75% | Test playlist, manual verification |
| Lock time | 3-5 sec | <2 sec | Timer from track start |
| False positive rate | High | <20% | Count triggers vs actual beats |
| Subdivision confusion | Frequent | <10% | Track locks on 2x tempo |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-31 | Claude Code | Initial implementation guide |
