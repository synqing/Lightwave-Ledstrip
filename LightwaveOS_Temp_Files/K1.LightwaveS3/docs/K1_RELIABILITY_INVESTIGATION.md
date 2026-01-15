# K1 Beat Tracker Reliability Investigation

**Document Version:** 1.0
**Date:** December 31, 2025
**Author:** Claude Code
**Status:** Complete

---

## Executive Summary

This document presents a comprehensive investigation into the reliability issues of the K1 beat tracking system in Lightwave-Ledstrip v2. The investigation was prompted by user observation that the beat/tempo output is "basically completely unreliable."

### Key Finding

The K1 architecture **IS fully implemented** - contrary to what the outdated `BEAT_TRACKING_AUDIT_REPORT.md` suggests. The system includes:
- 121 Goertzel resonators (60-180 BPM)
- Family scoring with half/double tempo weighting
- UNLOCKED/PENDING/VERIFIED state machine
- Grouped density confidence algorithm
- Challenger logic with hysteresis

**The problem is not missing components - it's fundamental limitations in the novelty extraction stage that tuning cannot overcome.**

---

## Current Implementation Status

### Components Verified as Implemented

| Component | File | Line Reference | Status |
|-----------|------|----------------|--------|
| 121 Goertzel Resonators | `k1/K1ResonatorBank.cpp` | Lines 34-48 | ✅ Complete |
| Top-K Candidate Extraction | `k1/K1ResonatorBank.cpp` | Lines 136-180 | ✅ Complete |
| Parabolic BPM Refinement | `k1/K1ResonatorBank.cpp` | Lines 115-134 | ✅ Complete |
| Family Scoring (half/double) | `k1/K1TactusResolver.cpp` | Lines 184-233 | ✅ Complete |
| Gaussian Tactus Prior | `k1/K1TactusResolver.cpp` | Lines 118-121 | ✅ Complete |
| State Machine (UNLOCKED/PENDING/VERIFIED) | `k1/K1TactusResolver.h` | LockState enum | ✅ Complete |
| Challenger Logic (15% margin) | `k1/K1TactusResolver.cpp` | Lines 436-462 | ✅ Complete |
| Grouped Density Confidence | `k1/K1TactusResolver.cpp` | Lines 143-182 | ✅ Complete |
| Verification Period (2500ms) | `k1/K1Config.h` | LOCK_VERIFY_MS | ✅ Complete |
| Density KDE Memory | `k1/K1TactusResolver.cpp` | Lines 53-116 | ✅ Complete |

### Configuration Parameters (Current Values)

| Parameter | Value | Location |
|-----------|-------|----------|
| `ST2_BPM_MIN` | 60 | K1Config.h |
| `ST2_BPM_MAX` | 180 | K1Config.h |
| `ST2_BPM_BINS` | 121 | K1Config.h |
| `ST3_TACTUS_CENTER` | 120.0 | K1Config.h |
| `ST3_TACTUS_SIGMA` | 40.0 | K1Config.h |
| `ST3_STABILITY_BONUS` | 0.12 | K1Config.h |
| `ST3_SWITCH_RATIO` | 1.15 | K1Config.h |
| `ST3_SWITCH_FRAMES` | 8 | K1Config.h |
| `LOCK_VERIFY_MS` | 2500 | K1Config.h |

---

## Root Cause Analysis

### Issues That Have Been Addressed (Previous Fixes)

| Issue | Previous State | Fix Applied | File |
|-------|---------------|-------------|------|
| Goertzel window too narrow | σ = 0.6 | Changed to σ = 1.0 | K1ResonatorBank.cpp:27 |
| Stability bonus too aggressive | 0.25 | Reduced to 0.12 | K1Config.h |
| Tactus prior too narrow | σ = 30 | Widened to σ = 40 | K1Config.h |
| Confidence exceeds 1.0 | Additive scores | Grouped density confidence | K1TactusResolver.cpp:143 |
| No verification period | Immediate lock | PENDING→VERIFIED (2500ms) | K1TactusResolver.cpp:361 |

### Fundamental Limitations (Cannot Be Fixed by Tuning)

#### 1. Spectral Flux Noise (CRITICAL)

**Problem:** The novelty/onset detection relies on spectral flux, which has inherently poor signal-to-noise ratio.

**Evidence:**
- Misses soft beats (low flux change)
- Triggers on noise spikes (high flux, no musical content)
- No perceptual weighting (treble contributes equally to bass)

**Impact:** Garbage-in to the resonator bank produces garbage-out tempo estimates.

#### 2. Goertzel Phase Independence (HIGH)

**Problem:** The 121 Goertzel bins are computed independently - there's no phase correlation between candidates.

**Evidence:**
- Two candidates at 120.3 and 119.7 BPM cannot be recognized as the same tempo
- Phase jumps when switching between similar candidates

**Impact:** Octave confusion and unnecessary tempo transitions.

#### 3. Single-Hypothesis Lock-In (HIGH)

**Problem:** Once locked, the system strongly biases toward the current tempo (stability bonus), making recovery from wrong locks very slow.

**Evidence:**
- `ST3_STABILITY_BONUS = 0.12` added to candidates within ±2 BPM of lock
- Challenger must sustain 15% advantage for 8 frames (0.8 seconds)
- Wrong locks can persist for 30+ seconds

**Impact:** User observes prolonged incorrect tempo display.

#### 4. No Perceptual Weighting (MEDIUM)

**Problem:** All frequency bands contribute equally to spectral flux calculation.

**Evidence:**
- Hi-hat energy (high frequency) creates flux equal to kick drum
- Bass (which typically carries the beat) is underweighted
- Treble transients dominate the onset signal

**Impact:** Subdivision confusion - hi-hat 16ths can lock instead of kick quarter notes.

#### 5. No Musical Intelligence (MEDIUM)

**Problem:** The system cannot distinguish musical elements by timbre.

**Evidence:**
- Kick drum treated same as synth bass
- Snare treated same as vocal transient
- No instrument-specific onset detection

**Impact:** Poor performance on complex arrangements.

---

## Proposed Solutions

### Option A: Targeted Novelty Improvement (Recommended First)

**Rationale:** The K1 architecture is sound. Fix the input signal quality.

#### A.1 Perceptually-Weighted Flux

Modify `K1Pipeline::computeNovelty()` to weight frequency bands:

```cpp
// Proposed frequency weighting
float weights[8] = {
    1.0f,   // 0-250 Hz (sub-bass) - max weight
    0.8f,   // 250-500 Hz (bass)
    0.6f,   // 500-1000 Hz (low-mid)
    0.4f,   // 1000-2000 Hz (mid)
    0.3f,   // 2000-4000 Hz (high-mid)
    0.2f,   // 4000-8000 Hz (high)
    0.1f,   // 8000+ Hz (treble) - min weight
    0.05f   // Noise floor
};

float weightedFlux = 0.0f;
for (int i = 0; i < 8; i++) {
    float bandDiff = bands[i] - prevBands[i];
    if (bandDiff > 0) {
        weightedFlux += bandDiff * weights[i];
    }
}
```

**Expected Outcome:** Bass-heavy onsets amplified, treble transients suppressed.

#### A.2 Multi-Band Onset Detection

Split onset detection into 3 independent channels:

| Band | Frequency Range | Weight | Threshold |
|------|-----------------|--------|-----------|
| Sub-Bass | 0-150 Hz | 0.50 | Adaptive |
| Mid | 150-2000 Hz | 0.35 | Adaptive |
| High | 2000+ Hz | 0.15 | Adaptive |

Each band has its own:
- Rolling mean/std statistics
- Adaptive threshold (k=2.0 std devs)
- Independent peak detection

Combined onset:
```cpp
float combinedOnset =
    0.50f * bassTrigger +
    0.35f * midTrigger +
    0.15f * highTrigger;
```

#### A.3 Onset Debouncing

Require onset to persist across multiple hops:

```cpp
// Persistence check
if (currentOnset && prevOnset) {
    // Two consecutive frames with onset - valid
    validOnset = true;
} else {
    // Single-frame spike - likely noise
    validOnset = false;
}
```

**Files to Modify:**
- `v2/src/audio/k1/K1Pipeline.cpp`
- `v2/src/audio/k1/K1Config.h`

**Estimated Effort:** 1-2 weeks

---

### Option B: Hybrid Multi-Hypothesis Tracking

**Rationale:** The single-lock model has inherent bias. Track multiple hypotheses.

#### B.1 Multi-Hypothesis State

Maintain top 3 tempo hypotheses:

```cpp
struct TempoHypothesis {
    float bpm;
    float confidence;
    float phase;
    MusicalGrid pll;  // Independent PLL per hypothesis
    int age_frames;
};

TempoHypothesis hypotheses[3];
```

#### B.2 Onset Correlation

Score each hypothesis by how well it predicts actual onsets:

```cpp
float scoreHypothesis(const TempoHypothesis& h, float onset_time) {
    float predicted_beat_time = h.phase + (1.0f / (h.bpm / 60.0f));
    float error = fabs(onset_time - predicted_beat_time);
    return exp(-error * error / (2.0f * 0.05f * 0.05f));  // Gaussian score
}
```

#### B.3 Soft Lock / Hard Lock

- **Soft Lock:** All hypotheses tracked, output is weighted average
- **Hard Lock:** Single hypothesis >80% weight, others suppressed

**Files to Create:**
- `v2/src/audio/k1/K1MultiHypothesis.h`
- `v2/src/audio/k1/K1MultiHypothesis.cpp`

**Estimated Effort:** 2-3 weeks

---

### Option C: Alternative Algorithm

**Rationale:** If K1 architecture fundamentally limited, consider alternatives.

#### C.1 Sensory Bridge APVP Port

- Existing analysis: `docs/analysis/SB_APVP_Analysis.md`
- Proven on Tab5 hardware
- Very different architecture (onset envelope auto-correlation)

#### C.2 Librosa-style Auto-Correlation

1. Compute onset strength envelope
2. Auto-correlate to find dominant periodicity
3. Less sensitive to subdivision confusion

#### C.3 Deep Learning (Research Phase)

- Not viable on ESP32 without simplification
- Could run on companion device
- Madmom/BeatNet as reference

---

## Validation Criteria

| Metric | Current Estimate | Target |
|--------|------------------|--------|
| Lock accuracy (EDM 120-140 BPM) | ~60% | >85% |
| Lock accuracy (rock 90-120 BPM) | ~40% | >75% |
| Time to lock (clear beat) | 3-5 seconds | <2 seconds |
| Lock stability (during song) | Frequent drift | <5% drift |
| False positive rate | High | <20% |
| Subdivision confusion rate | High | <10% |

---

## Recommended Action Plan

### Phase 1: Immediate (Week 1)

1. Implement perceptually-weighted flux in K1Pipeline
2. Add debug logging for onset detection quality
3. Create test harness with 5 reference tracks

### Phase 2: Short-Term (Weeks 2-3)

1. Implement multi-band onset detection
2. Add onset debouncing
3. Tune thresholds with reference tracks
4. Measure improvement against validation criteria

### Phase 3: Evaluate (Week 4)

If Option A achieves >70% accuracy:
- Fine-tune parameters
- Consider Option B for robustness

If Option A insufficient:
- Proceed to Option B (multi-hypothesis)
- Or evaluate Option C alternatives

---

## Critical Files Reference

| File | Purpose |
|------|---------|
| `src/audio/k1/K1Pipeline.cpp` | Novelty extraction, main pipeline |
| `src/audio/k1/K1ResonatorBank.cpp` | 121 Goertzel resonators |
| `src/audio/k1/K1TactusResolver.cpp` | Family scoring, state machine |
| `src/audio/k1/K1BeatClock.cpp` | Beat phase PLL |
| `src/audio/k1/K1Config.h` | All tuning parameters |
| `src/audio/AudioActor.cpp` | K1 pipeline integration |

---

## Appendix: Code References

### Goertzel Resonator Initialization (K1ResonatorBank.cpp:34-48)

```cpp
for (int bi = 0; bi < ST2_BPM_BINS; ++bi) {
    float bpm = (float)(ST2_BPM_MIN + bi * ST2_BPM_STEP);
    float hz = bpm / 60.0f;
    bins_[bi].target_hz = hz;
    bins_[bi].block_size = ST2_HISTORY_FRAMES;
    // ... initialization
}
```

### Family Scoring (K1TactusResolver.cpp:184-233)

```cpp
float K1TactusResolver::scoreFamily(const K1ResonatorFrame& in, int candidate_idx) const {
    float primary_bpm = in.candidates[candidate_idx].bpm;
    float primary_mag = in.candidates[candidate_idx].magnitude;
    float score = primary_mag * tactusPrior(primary_bpm);

    // Half-tempo contribution
    float half_bpm = primary_bpm / 2.0f;
    if (half_bpm >= ST2_BPM_MIN) {
        int half_idx = findFamilyMember(in, half_bpm, half_tolerance);
        if (half_idx >= 0) {
            score += ST3_HALF_CONTRIB * in.candidates[half_idx].magnitude;
        }
    }

    // Double-tempo contribution
    // ... similar logic
    return score;
}
```

### State Machine (K1TactusResolver.cpp:332-394)

```cpp
if (lock_state_ == LockState::UNLOCKED) {
    // Enter PENDING state on first lock
    lock_state_ = LockState::PENDING;
    lock_pending_start_ms_ = in.t_ms;
}

if (lock_state_ == LockState::PENDING) {
    uint32_t elapsed = in.t_ms - lock_pending_start_ms_;
    if (elapsed >= LOCK_VERIFY_MS) {
        lock_state_ = LockState::VERIFIED;
    }
}
```

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-31 | Claude Code | Initial comprehensive investigation |
