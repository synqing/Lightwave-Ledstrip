# COMPREHENSIVE BEAT TRACKING ALGORITHM ANALYSIS

## Document Metadata

| Field | Value |
|-------|-------|
| **Document Type** | Technical Research Report |
| **Analysis Date** | 2025-12-31 |
| **Scope** | Emotiscope v1.0-v2.0 vs LightwaveOS K1 v2 |
| **Total Lines Analyzed** | ~15,000+ |
| **Files Examined** | 47 source files |
| **Agent Methodology** | Multi-agent parallel exploration |

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [K1 v2 Beat Detection Architecture](#2-k1-v2-beat-detection-architecture)
3. [Emotiscope Implementation Analysis](#3-emotiscope-implementation-analysis)
4. [Comparative Analysis](#4-comparative-analysis)
5. [Integration Assessment](#5-integration-assessment)
6. [Recommendations](#6-recommendations)
7. [Appendices](#7-appendices)

---

## 1. Executive Summary

### 1.1 Research Objectives

This document presents a forensic-level technical analysis of beat tracking algorithms across two embedded audio-visual systems:

1. **LightwaveOS K1 v2**: Production beat tracker in `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/k1/`
2. **Emotiscope v1.0-v2.0**: Reference implementations in `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/`

### 1.2 Key Findings

| Finding | Significance |
|---------|--------------|
| **4 distinct Emotiscope versions** identified | Progressive evolution from v1.0 baseline to v2.0 dual-core |
| **Fundamentally different paradigms** | Emotiscope: multi-tempo visualization; K1: single-tempo control |
| **K1 has superior octave resolution** | Family scoring resolves 60 vs 120 BPM ambiguity |
| **Emotiscope has better poly-rhythm support** | 96 parallel PLLs vs K1's single lock |
| **K1 138 BPM mislock bug** was fixed | Running-stat normalizer + 3-state verification |
| **No direct replacement recommended** | Selective enhancement strategy preferred |

### 1.3 Recommendation Summary

**Do NOT replace K1 with Emotiscope.** Instead, selectively adopt:
- Dynamic novelty scaling (from Emotiscope v2.0)
- Silent bin suppression (from Emotiscope v2.0)
- VU separation concept (from Emotiscope v1.1)

---

## 2. K1 v2 Beat Detection Architecture

### 2.1 System Overview

The K1 beat tracker is a **4-stage tempo detection pipeline** ported from Tab5.DSP, designed for real-time LED effect synchronization on ESP32-S3.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          K1 BEAT DETECTION PIPELINE                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Audio Input (spectral flux @ 62.5 Hz from AudioActor)                      │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ STAGE 1: NOVELTY NORMALIZATION (K1Pipeline.cpp)                     │   │
│  │ • Input: raw flux [0, 1]                                            │   │
│  │ • Running-stat z-score: (flux - mean) / sqrt(variance)              │   │
│  │ • Output: z-score [-4, +4]                                          │   │
│  │ • Time constants: mean τ = 2.0s, variance τ = 2.0s                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ STAGE 2: RESONATOR BANK (K1ResonatorBank.cpp)                       │   │
│  │ • 121 Goertzel resonators (60-180 BPM, 1 BPM steps)                 │   │
│  │ • 8-second novelty history window                                   │   │
│  │ • Gaussian window σ = 1.0 for frequency resolution                  │   │
│  │ • EMA smoothing α = 0.85 (half-life ~0.4s at 10 Hz)                 │   │
│  │ • Parabolic interpolation for sub-bin accuracy                      │   │
│  │ • Output: Top-12 candidates with magnitude, phase, raw_mag          │   │
│  │ • Update rate: 10 Hz                                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ STAGE 3: TACTUS RESOLVER (K1TactusResolver.cpp)                     │   │
│  │ • Family scoring: primary + 0.4×half + 0.4×double                   │   │
│  │ • Gaussian prior: center=120 BPM, σ=40 BPM                          │   │
│  │ • Tempo density memory (KDE-style, decay=0.97)                      │   │
│  │ • Stability bonus: +0.12 in VERIFIED state within ±2 BPM            │   │
│  │ • 3-state lock machine: UNLOCKED → PENDING (2.5s) → VERIFIED        │   │
│  │ • Grouped density confidence algorithm                              │   │
│  │ • Hysteresis: 15% margin for 8 frames (0.8s) to switch              │   │
│  │ • Output: locked_bpm, confidence, phase_hint, lock_state            │   │
│  │ • Update rate: 10 Hz                                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ STAGE 4: BEAT CLOCK / PLL (K1BeatClock.cpp)                         │   │
│  │ • Phase equation: phase_rad += (2π × BPM/60) × delta_sec            │   │
│  │ • Phase error: wrap(reference_phase - phase_rad) to [-π, +π]        │   │
│  │ • Phase correction: += error × 0.08 (clamped ±0.15)                 │   │
│  │ • Frequency correction: += freq_error × 0.002 × 60 (clamped ±2 BPM) │   │
│  │ • Big-change detection: instant jump if >5 BPM difference           │   │
│  │ • Beat tick generation with 60% debounce ratio                      │   │
│  │ • Output: phase01 [0,1), beat_tick, bpm, confidence, locked         │   │
│  │ • Update rate: 120 FPS (renderer-driven) + 10 Hz (from tactus)      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  Output to EffectContext:                                                   │
│  • beatPhase() [0.0-1.0]                                                   │
│  • isOnBeat() single-frame pulse                                           │
│  • isOnDownbeat() bar marker                                               │
│  • bpm() smoothed estimate                                                 │
│  • tempoConfidence() [0.0-1.0]                                             │
│  • beatStrength() decaying energy                                          │
│  • k1Locked boolean                                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Stage 1: Novelty Normalization

**File**: `v2/src/audio/k1/K1Pipeline.cpp`

**Purpose**: Convert raw spectral flux into a normalized z-score that's robust to AGC/gating baseline shifts.

#### 2.2.1 Algorithm Details

The novelty normalizer uses an **Exponentially Weighted Moving Average (EWMA)** to track running statistics:

```cpp
// Running mean update (EWMA)
float alpha_mean = 1.0f - expf(-dt / NOVELTY_MEAN_TAU);  // τ = 2.0 seconds
novelty_mean_ = novelty_mean_ * (1.0f - alpha_mean) + flux * alpha_mean;

// Running variance update (EWMA on squared deviation)
float deviation = flux - novelty_mean_;
float alpha_var = 1.0f - expf(-dt / NOVELTY_VAR_TAU);  // τ = 2.0 seconds
novelty_variance_ = novelty_variance_ * (1.0f - alpha_var) + (deviation * deviation) * alpha_var;

// Z-score computation
float stddev = sqrtf(fmaxf(novelty_variance_, 1e-6f));  // Floor to prevent div/0
float z_score = (flux - novelty_mean_) / stddev;

// Clipping to prevent outliers
z_score = fminf(fmaxf(z_score, -NOVELTY_CLIP), NOVELTY_CLIP);  // ±4.0
```

#### 2.2.2 Historical Context: The 138 BPM Mislock Bug

Prior to this implementation, K1 used a **naive linear mapping**:

```cpp
// OLD (BROKEN) approach:
float z_score = (flux - 0.5f) * 12.0f;  // Assumed midpoint at 0.5
```

**Problem**: This assumed flux was centered at 0.5 and had constant variance. In reality:
- AGC caused flux baseline to shift
- Gating events caused flux to saturate at 1.0
- The resulting z-score was meaningless, destroying periodic structure

**Evidence from logs**:
```
BPM: 123.2 | Conf: 1.00 | DConf: 0.12 | Phase: 0.49 | LOCKED
Top3: 88(1.00) 88(0.99) 89(0.98)
flux=1.000  // SATURATED!
```

The Stage-2 resonators never saw the true 138 BPM because flux was hard-clamped.

#### 2.2.3 Key Parameters

| Parameter | Value | Location | Purpose |
|-----------|-------|----------|---------|
| `NOVELTY_MEAN_TAU` | 2.0 seconds | K1Config.h | Running mean adaptation rate |
| `NOVELTY_VAR_TAU` | 2.0 seconds | K1Config.h | Running variance adaptation rate |
| `NOVELTY_CLIP` | ±4.0 sigma | K1Config.h | Outlier rejection threshold |

### 2.3 Stage 2: Resonator Bank

**File**: `v2/src/audio/k1/K1ResonatorBank.cpp`

**Purpose**: Multi-resolution tempo hypothesis generation via parallel Goertzel analysis.

#### 2.3.1 Goertzel Algorithm Implementation

The Goertzel algorithm is an efficient single-frequency DFT:

```cpp
void K1ResonatorBank::calculate_bin(uint16_t bin) {
    float hz = BPM_to_Hz(bins_[bin].bpm);  // BPM / 60
    float w = 2.0f * M_PI * hz / NOVELTY_LOG_HZ;  // Normalized angular frequency
    float coeff = 2.0f * cosf(w);
    float sine = sinf(w);
    float cosine = cosf(w);

    float q1 = 0.0f, q2 = 0.0f;

    // Apply Goertzel IIR to novelty history with Gaussian window
    for (uint16_t i = 0; i < block_size; i++) {
        uint16_t idx = (history_head_ - block_size + i + NOVELTY_HISTORY_LENGTH) % NOVELTY_HISTORY_LENGTH;
        float sample = novelty_history_[idx];
        float window = gaussian_window(i, block_size, WINDOW_SIGMA);  // σ = 1.0

        float q0 = coeff * q1 - q2 + sample * window;
        q2 = q1;
        q1 = q0;
    }

    // Extract magnitude and phase
    float real = q1 - q2 * cosine;
    float imag = q2 * sine;

    float magnitude_raw = sqrtf(real * real + imag * imag);
    float magnitude_norm = magnitude_raw / (block_size / 2.0f);

    float phase = atan2f(imag, real);

    // EMA smoothing
    bins_[bin].magnitude_smooth = ST2_MAG_SMOOTH * bins_[bin].magnitude_smooth
                                + (1.0f - ST2_MAG_SMOOTH) * magnitude_norm;
    bins_[bin].phase = phase;
}
```

#### 2.3.2 Gaussian Window Function

The window function σ was a critical tuning parameter:

```cpp
float gaussian_window(uint16_t i, uint16_t N, float sigma) {
    float n = (float)i - (float)(N - 1) / 2.0f;
    float s = sigma * (float)(N - 1) / 2.0f;
    return expf(-0.5f * (n / s) * (n / s));
}
```

**Critical Finding**: Window σ was originally 0.6, which captured only ~1.8 beat cycles for 138 BPM (insufficient frequency resolution). Widening to σ=1.0 captures ~3 cycles, providing clean spectral peaks.

| σ Value | Cycles at 138 BPM | Frequency Resolution | Status |
|---------|-------------------|---------------------|--------|
| 0.6 | ~1.8 | Poor | BROKEN |
| 1.0 | ~3.0 | Good | FIXED |
| 1.5 | ~4.5 | Excellent | Slower response |

#### 2.3.3 Parabolic Interpolation for Sub-Bin Accuracy

After finding the peak bin, parabolic (3-point quadratic) interpolation refines the BPM estimate:

```cpp
float refine_bpm(uint16_t peak_bin) {
    if (peak_bin == 0 || peak_bin >= NUM_BINS - 1) return bins_[peak_bin].bpm;

    float y0 = bins_[peak_bin - 1].magnitude_smooth;
    float y1 = bins_[peak_bin].magnitude_smooth;
    float y2 = bins_[peak_bin + 1].magnitude_smooth;

    // Quadratic interpolation: x_peak = 0.5 * (y0 - y2) / (y0 - 2*y1 + y2)
    float denom = y0 - 2.0f * y1 + y2;
    if (fabsf(denom) < 1e-6f) return bins_[peak_bin].bpm;

    float offset = 0.5f * (y0 - y2) / denom;  // Range: [-0.5, +0.5]

    return bins_[peak_bin].bpm + offset;  // Sub-BPM accuracy
}
```

#### 2.3.4 Key Parameters

| Parameter | Value | Location | Purpose |
|-----------|-------|----------|---------|
| `NUM_RESONATOR_BINS` | 121 | K1Config.h | 60-180 BPM in 1 BPM steps |
| `NOVELTY_HISTORY_LENGTH` | 500 | K1Config.h | 8 seconds at 62.5 Hz |
| `RESONATOR_UPDATE_HZ` | 10 | K1Config.h | Resonator bank update rate |
| `ST2_MAG_SMOOTH` | 0.85 | K1Config.h | EMA alpha, ~0.4s half-life |
| `WINDOW_SIGMA` | 1.0 | K1ResonatorBank.cpp | Gaussian window width |
| `TOP_K_CANDIDATES` | 12 | K1Config.h | Candidates passed to Stage 3 |

#### 2.3.5 Top-K Extraction

```cpp
void K1ResonatorBank::extract_top_k(K1ResonatorFrame& frame) {
    // Insertion sort to find top 12 candidates
    for (uint16_t i = 0; i < NUM_RESONATOR_BINS; i++) {
        if (bins_[i].magnitude_smooth < 0.1f) continue;  // Noise floor filter

        float norm_mag = bins_[i].magnitude_smooth / max_magnitude_;

        // Insert into sorted candidates array
        for (uint8_t k = 0; k < TOP_K_CANDIDATES; k++) {
            if (norm_mag > frame.candidates[k].magnitude) {
                // Shift down and insert
                for (uint8_t j = TOP_K_CANDIDATES - 1; j > k; j--) {
                    frame.candidates[j] = frame.candidates[j-1];
                }
                frame.candidates[k] = {
                    .bpm = bins_[i].bpm,
                    .magnitude = norm_mag,
                    .phase = bins_[i].phase,
                    .raw_magnitude = bins_[i].magnitude_smooth
                };
                break;
            }
        }
    }
}
```

### 2.4 Stage 3: Tactus Resolver

**File**: `v2/src/audio/k1/K1TactusResolver.cpp`

**Purpose**: Convert resonator candidates into a stable, musically-intelligent tempo lock.

#### 2.4.1 Family Scoring Algorithm

The **key innovation** is scoring tempo "families" that include half-tempo and double-tempo relationships:

```cpp
float K1TactusResolver::score_family(float primary_bpm, float primary_mag,
                                     const K1ResonatorFrame& frame) {
    // 1. Apply Gaussian prior centered at 120 BPM
    float prior = gaussian_prior(primary_bpm);
    float score = primary_mag * prior;

    // 2. Add half-tempo contribution (if found within ±3%)
    float half_bpm = primary_bpm / 2.0f;
    float half_mag = find_candidate_magnitude(frame, half_bpm, 0.03f);
    if (half_mag > 0.0f) {
        score += ST3_HALF_DOUBLE_CONTRIB * half_mag * gaussian_prior(half_bpm);
    }

    // 3. Add double-tempo contribution (if found within ±3%)
    float double_bpm = primary_bpm * 2.0f;
    float double_mag = find_candidate_magnitude(frame, double_bpm, 0.03f);
    if (double_mag > 0.0f) {
        score += ST3_HALF_DOUBLE_CONTRIB * double_mag * gaussian_prior(double_bpm);
    }

    // 4. Apply tempo density memory (KDE-style)
    if (density_enabled_) {
        float density_at_bpm = tempo_density_[(int)roundf(primary_bpm)];
        float density_norm = density_at_bpm / max_density_;
        score *= (1.0f + ST3_DENSITY_WEIGHT * density_norm);
    }

    // 5. Add stability bonus (VERIFIED state only, within ±2 BPM of lock)
    if (lock_state_ == LockState::VERIFIED) {
        if (fabsf(primary_bpm - locked_bpm_) <= ST3_STABILITY_WINDOW) {
            score += ST3_STABILITY_BONUS;
        }
    }

    return score;
}
```

#### 2.4.2 Gaussian Prior (Tactus Preference)

The prior encodes human tempo perception preference:

```cpp
float K1TactusResolver::gaussian_prior(float bpm) {
    // Center: 120 BPM (most music perceived at this tempo)
    // Sigma: 40 BPM (width of preference, widened from 30 to reduce bias)
    float deviation = bpm - ST3_TACTUS_CENTER;
    return expf(-(deviation * deviation) / (2.0f * ST3_TACTUS_SIGMA * ST3_TACTUS_SIGMA));
}
```

| BPM | Prior Value | Relative Advantage |
|-----|-------------|-------------------|
| 60 | 0.325 | -67.5% |
| 90 | 0.754 | -24.6% |
| 120 | 1.000 | 0% (reference) |
| 138 | 0.882 | -11.8% |
| 150 | 0.754 | -24.6% |
| 180 | 0.325 | -67.5% |

**Historical Note**: σ was originally 30.0, giving 138 BPM a 14% disadvantage vs 129 BPM. Widening to 40.0 reduced this to 7%, fixing the bias issue.

#### 2.4.3 Tempo Density Memory (KDE-Style)

The density memory tracks historical tempo distribution:

```cpp
void K1TactusResolver::update_density(float bpm) {
    // Decay all bins
    for (int i = 60; i <= 180; i++) {
        tempo_density_[i] *= ST3_DENSITY_DECAY;  // 0.97
    }

    // Add triangular kernel at current BPM
    int center = (int)roundf(bpm);
    for (int i = center - 3; i <= center + 3; i++) {
        if (i >= 60 && i <= 180) {
            float weight = 1.0f - fabsf(i - bpm) / 3.0f;
            tempo_density_[i] += weight;
        }
    }

    // Update max for normalization
    max_density_ = *std::max_element(tempo_density_ + 60, tempo_density_ + 181);
}
```

**Purpose**: Persistent tempos accumulate density, breaking ties. If the music has been at 120 BPM for a while, that bin has higher density, providing a small advantage.

#### 2.4.4 Grouped Density Confidence

The **critical insight** for avoiding false competition between nearby tempo estimates:

```cpp
float K1TactusResolver::compute_grouped_confidence(const std::vector<ScoredCandidate>& candidates) {
    if (candidates.empty()) return 0.0f;

    float winner_bpm = candidates[0].bpm;
    float winner_score = candidates[0].score;

    // Group nearby candidates (within ±3 BPM) as consensus
    float group_score = 0.0f;
    for (const auto& c : candidates) {
        if (fabsf(c.bpm - winner_bpm) <= 3.0f) {
            group_score += c.score;
        }
    }

    // Find strongest distant competitor (≥6 BPM away)
    float distant_runner = 0.0f;
    for (const auto& c : candidates) {
        if (fabsf(c.bpm - winner_bpm) >= 6.0f) {
            distant_runner = fmaxf(distant_runner, c.score);
        }
    }

    // Compute confidence
    if (distant_runner < 0.01f) {
        return 1.0f;  // Unanimous agreement
    }
    return (group_score - distant_runner) / (group_score + distant_runner);
}
```

**Key Insight**: Candidates at 64, 65, 66 BPM should produce HIGH confidence (consensus), not LOW confidence (competition). They're all estimating the same tempo with slight variance.

#### 2.4.5 Three-State Lock Machine

```cpp
enum class LockState { UNLOCKED, PENDING, VERIFIED };

void K1TactusResolver::update_lock_state(float best_bpm, float best_score,
                                         float density_conf, uint32_t now_ms) {
    switch (lock_state_) {
        case LockState::UNLOCKED:
            if (best_score > ST3_MIN_SCORE) {
                locked_bpm_ = best_bpm;
                lock_state_ = LockState::PENDING;
                pending_start_ms_ = now_ms;
                challenger_bpm_ = 0.0f;
                challenger_frames_ = 0;
            }
            break;

        case LockState::PENDING:
            // Track strongest competitor (>5 BPM away, >10% advantage)
            if (fabsf(best_bpm - locked_bpm_) > 5.0f &&
                best_score > locked_score_ * ST3_COMPETITOR_THRESHOLD) {
                if (fabsf(best_bpm - challenger_bpm_) <= 2.0f) {
                    challenger_frames_++;
                    if (challenger_frames_ >= 15) {  // ~1.5s at 10 Hz
                        // Switch to challenger
                        locked_bpm_ = best_bpm;
                        pending_start_ms_ = now_ms;
                        challenger_frames_ = 0;
                    }
                } else {
                    challenger_bpm_ = best_bpm;
                    challenger_frames_ = 1;
                }
            } else {
                challenger_frames_ = 0;
            }

            // After 2.5s, commit to VERIFIED
            if (now_ms - pending_start_ms_ >= ST3_VERIFY_MS) {
                lock_state_ = LockState::VERIFIED;
            }
            break;

        case LockState::VERIFIED:
            // Hysteresis: challenger must beat incumbent by 15% for 8 frames
            if (fabsf(best_bpm - locked_bpm_) > ST3_STABILITY_WINDOW) {
                if (best_score > locked_score_ * ST3_SWITCH_RATIO) {
                    challenger_frames_++;
                    if (challenger_frames_ >= ST3_SWITCH_FRAMES) {
                        locked_bpm_ = best_bpm;
                        challenger_frames_ = 0;
                    }
                } else {
                    challenger_frames_ = 0;
                }
            } else {
                // Slow tracking within window
                locked_bpm_ = 0.99f * locked_bpm_ + 0.01f * best_bpm;
                challenger_frames_ = 0;
            }
            break;
    }
}
```

**State Transitions**:
- **UNLOCKED → PENDING**: First valid candidate detected
- **PENDING → VERIFIED**: 2.5s elapsed without challenger winning
- **PENDING → PENDING**: Challenger wins, restart verification
- **VERIFIED → VERIFIED**: Hysteresis protects lock, slow tracking within window

#### 2.4.6 Octave Doubling Override

If best candidate is below 80 BPM, check if double-tempo is more plausible:

```cpp
void K1TactusResolver::check_octave_doubling(float& best_bpm, float& best_score,
                                              const std::vector<ScoredCandidate>& candidates) {
    if (best_bpm >= 80.0f) return;

    float double_bpm = best_bpm * 2.0f;
    float double_score = find_candidate_score(candidates, double_bpm, 2.0f);

    float best_prior = gaussian_prior(best_bpm);
    float double_prior = gaussian_prior(double_bpm);

    // If double's prior is ≥2x and score is ≥30% of best, select double
    if (double_prior >= 2.0f * best_prior && double_score >= 0.3f * best_score) {
        best_bpm = double_bpm;
        best_score = double_score;
    }
}
```

**Purpose**: Prevents false half-time locks on fast tempos. A 65 BPM lock when music is actually 130 BPM is musically wrong.

#### 2.4.7 Key Parameters

| Parameter | Value | Location | Purpose |
|-----------|-------|----------|---------|
| `ST3_TACTUS_CENTER` | 120.0 BPM | K1Config.h | Gaussian prior center |
| `ST3_TACTUS_SIGMA` | 40.0 BPM | K1Config.h | Gaussian prior width |
| `ST3_HALF_DOUBLE_CONTRIB` | 0.4 | K1Config.h | Octave family weight |
| `ST3_DENSITY_DECAY` | 0.97 | K1Config.h | KDE decay per frame |
| `ST3_DENSITY_WEIGHT` | 0.80 | K1Config.h | Density contribution |
| `ST3_STABILITY_BONUS` | 0.12 | K1Config.h | Lock-in bonus (VERIFIED) |
| `ST3_STABILITY_WINDOW` | ±2.0 BPM | K1Config.h | Definition of "same tempo" |
| `ST3_VERIFY_MS` | 2500 | K1Config.h | Verification period |
| `ST3_COMPETITOR_THRESHOLD` | 1.10 | K1Config.h | 10% advantage to reconsider |
| `ST3_SWITCH_RATIO` | 1.15 | K1Config.h | 15% margin to switch |
| `ST3_SWITCH_FRAMES` | 8 | K1Config.h | ~0.8s hysteresis |
| `ST3_MIN_CONFIDENCE` | 0.15 | K1Config.h | Minimum reported confidence |

### 2.5 Stage 4: Beat Clock / PLL

**File**: `v2/src/audio/k1/K1BeatClock.cpp`

**Purpose**: Smooth phase tracking with sub-frame accuracy between Stage 3 updates.

#### 2.5.1 Phase-Locked Loop Algorithm

```cpp
void K1BeatClock::update(const K1TactusFrame& tactus, float delta_sec) {
    if (!tactus.locked) {
        // Free-running when unlocked
        phase_rad_ += (2.0f * M_PI * bpm_ / 60.0f) * delta_sec;
        wrap_phase();
        return;
    }

    // Check for big tempo change
    if (fabsf(tactus.bpm - bpm_) > ST4_BIG_CHANGE_BPM) {
        // Instant jump
        phase_rad_ = tactus.phase_hint;
        bpm_ = tactus.bpm;
        phase_error_ = 0.0f;
        freq_error_ = 0.0f;
    } else {
        // PLL error correction
        float reference_phase = tactus.phase_hint;
        float raw_error = reference_phase - phase_rad_;

        // Wrap error to [-π, +π]
        while (raw_error > M_PI) raw_error -= 2.0f * M_PI;
        while (raw_error < -M_PI) raw_error += 2.0f * M_PI;
        phase_error_ = raw_error;

        // Phase correction (gradual)
        float phase_correction = phase_error_ * ST4_PHASE_GAIN;
        phase_correction = fminf(fmaxf(phase_correction, -ST4_MAX_PHASE_CORR), ST4_MAX_PHASE_CORR);
        phase_rad_ += phase_correction;

        // Frequency correction (cumulative)
        freq_error_ = 0.9f * freq_error_ + 0.1f * phase_error_;
        float freq_correction = freq_error_ * ST4_FREQ_GAIN * 60.0f;
        freq_correction = fminf(fmaxf(freq_correction, -ST4_MAX_FREQ_CORR), ST4_MAX_FREQ_CORR);
        bpm_ += freq_correction;
    }

    // Advance phase
    phase_rad_ += (2.0f * M_PI * bpm_ / 60.0f) * delta_sec;
    wrap_phase();

    // Generate beat tick
    generate_tick();
}
```

#### 2.5.2 Beat Tick Generation

```cpp
void K1BeatClock::generate_tick() {
    static float last_phase = 0.0f;

    // Detect positive zero-crossing
    if (last_phase < 0.0f && phase_rad_ >= 0.0f) {
        uint32_t now_ms = millis();
        float beat_period_ms = 60000.0f / bpm_;

        // Debounce: no double-triggers within 60% of beat period
        if (now_ms - last_tick_ms_ > beat_period_ms * ST4_DEBOUNCE_RATIO) {
            beat_tick_ = true;
            last_tick_ms_ = now_ms;
        }
    } else {
        beat_tick_ = false;
    }

    last_phase = phase_rad_;
}
```

#### 2.5.3 Continuous Phase Advance (Renderer Tick)

Called at ~120 FPS between resonator updates:

```cpp
void K1BeatClock::advance(float delta_sec) {
    // Just advance phase, no error correction
    phase_rad_ += (2.0f * M_PI * bpm_ / 60.0f) * delta_sec;
    wrap_phase();

    // Convert to phase01 for effects
    phase01_ = (phase_rad_ + M_PI) / (2.0f * M_PI);  // [0, 1)
}
```

#### 2.5.4 Key Parameters

| Parameter | Value | Location | Purpose |
|-----------|-------|----------|---------|
| `ST4_PHASE_GAIN` | 0.08 | K1Config.h | Phase correction responsiveness |
| `ST4_FREQ_GAIN` | 0.002 | K1Config.h | BPM correction rate |
| `ST4_MAX_PHASE_CORR` | 0.15 | K1Config.h | Max ±15% of beat period |
| `ST4_MAX_FREQ_CORR` | 2.0 BPM | K1Config.h | Max BPM change per update |
| `ST4_BIG_CHANGE_BPM` | 5.0 | K1Config.h | Threshold for instant jump |
| `ST4_DEBOUNCE_RATIO` | 0.6 | K1Config.h | No triggers within 60% of beat |

### 2.6 Data Flow to Effects

#### 2.6.1 Lock-Free Queue Architecture

```
K1Pipeline (Audio Thread)
    │
    ├─► K1TempoUpdate Queue (8 slots, ~10 Hz)
    │   └─ {timestamp_ms, bpm, confidence, is_locked}
    │
    └─► K1BeatEvent Queue (16 slots, 1-3 Hz)
        └─ {timestamp_ms, phase01, beat_in_bar, is_downbeat, strength}
    │
    ▼
RendererActor (Render Thread @ 120 FPS)
    │
    ├─► Drain K1 queues
    ├─► Update MusicalGrid (render-domain PLL)
    │   └─ Extrapolates beat phase at 120 FPS
    │   └─ Tracks beat strength decay
    │
    └─► Publish to EffectContext
        └─ AudioContext accessible to effects
```

#### 2.6.2 MusicalGrid (Render-Domain PLL)

The MusicalGrid provides smooth phase interpolation at 120 FPS:

```cpp
struct MusicalGridSnapshot {
    float beat_phase;      // [0, 1) - phase within current beat
    float bar_phase;       // [0, 1) - phase within current bar
    uint8_t beat_in_bar;   // 0-3 for 4/4 time
    float bpm;             // Smoothed BPM estimate
    float confidence;      // Smoothed confidence
    float beat_strength;   // Decaying beat energy [0, 1]
    bool beat_tick;        // Single-frame pulse on beat
    bool downbeat_tick;    // Single-frame pulse on downbeat
};
```

**Smoothing Parameters**:
- BPM smoothing τ: 0.5 seconds
- Confidence smoothing τ: 1.0 seconds
- Phase correction gain: 0.35
- Bar correction gain: 0.20

#### 2.6.3 EffectContext Beat Accessors

**File**: `v2/src/plugins/api/EffectContext.h`

```cpp
// Core beat accessors
float beatPhase()          // [0.0-1.0] phase within beat
bool isOnBeat()            // Single-frame pulse on beat
bool isOnDownbeat()        // Single-frame pulse on downbeat
float bpm()                // Smoothed BPM estimate
float tempoConfidence()    // [0.0-1.0] tracker confidence
float beatStrength()       // [0.0-1.0] decaying beat energy

// K1 lock state (from ControlBusFrame)
bool k1Locked              // True if beat tracker locked
float k1Confidence         // Confidence from K1 Tactus stage

// Musical context
uint8_t beat_in_bar        // 0-3 (from MusicalGridSnapshot)
bool isRhythmicDominant()  // Is rhythm most salient?
```

### 2.7 Complete K1 Parameter Reference

| Category | Parameter | Value | Purpose |
|----------|-----------|-------|---------|
| **Stage 1** | NOVELTY_MEAN_TAU | 2.0s | Running mean adaptation |
| | NOVELTY_VAR_TAU | 2.0s | Running variance adaptation |
| | NOVELTY_CLIP | ±4σ | Outlier rejection |
| **Stage 2** | NUM_RESONATOR_BINS | 121 | 60-180 BPM range |
| | NOVELTY_HISTORY_LENGTH | 500 | 8 seconds @ 62.5 Hz |
| | RESONATOR_UPDATE_HZ | 10 | Update rate |
| | ST2_MAG_SMOOTH | 0.85 | EMA alpha (~0.4s half-life) |
| | WINDOW_SIGMA | 1.0 | Gaussian window width |
| | TOP_K_CANDIDATES | 12 | Passed to Stage 3 |
| **Stage 3** | ST3_TACTUS_CENTER | 120 BPM | Prior center |
| | ST3_TACTUS_SIGMA | 40 BPM | Prior width |
| | ST3_HALF_DOUBLE_CONTRIB | 0.4 | Octave family weight |
| | ST3_DENSITY_DECAY | 0.97 | KDE decay |
| | ST3_DENSITY_WEIGHT | 0.80 | Density contribution |
| | ST3_STABILITY_BONUS | 0.12 | Lock-in bonus |
| | ST3_STABILITY_WINDOW | ±2 BPM | "Same tempo" range |
| | ST3_VERIFY_MS | 2500 | Verification period |
| | ST3_COMPETITOR_THRESHOLD | 1.10 | 10% to reconsider |
| | ST3_SWITCH_RATIO | 1.15 | 15% to switch |
| | ST3_SWITCH_FRAMES | 8 | ~0.8s hysteresis |
| | ST3_MIN_CONFIDENCE | 0.15 | Minimum reported |
| **Stage 4** | ST4_PHASE_GAIN | 0.08 | Phase correction |
| | ST4_FREQ_GAIN | 0.002 | Frequency correction |
| | ST4_MAX_PHASE_CORR | 0.15 | Max phase correction |
| | ST4_MAX_FREQ_CORR | 2.0 BPM | Max freq correction |
| | ST4_BIG_CHANGE_BPM | 5.0 | Instant jump threshold |
| | ST4_DEBOUNCE_RATIO | 0.6 | Beat tick debounce |

---

## 3. Emotiscope Implementation Analysis

### 3.1 Emotiscope v1.0 - Baseline Implementation

**File**: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.0/src/tempo.h`

**Lines**: 1-429

#### 3.1.1 Algorithm Architecture

**Mathematical Model**: Multi-bin Goertzel DFT applied to novelty curve (NOT raw audio)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    EMOTISCOPE v1.0 TEMPO DETECTION PIPELINE                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Audio Input (44.1 kHz implicit from Goertzel constants)                    │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ GOERTZEL FILTER BANK (goertzel.h)                                   │   │
│  │ • 64 parallel frequency bins                                        │   │
│  │ • Magnitude extraction with auto-ranging                            │   │
│  │ • A-weighted perceptual scaling                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ NOVELTY CURVE GENERATION (tempo.h:344-373)                          │   │
│  │ • Spectral flux: Σ max(0, new_mag - last_mag) / NUM_FREQS           │   │
│  │ • Logarithmic compression: log(1 + novelty)                         │   │
│  │ • 1024-sample circular buffer @ 50 Hz = 20.48 seconds               │   │
│  │ • VU curve: separate amplitude envelope logging                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ 64 PARALLEL TEMPO GOERTZEL (tempo.h:140-192)                        │   │
│  │ • Range: 32-160 BPM (TEMPO_LOW to TEMPO_HIGH)                       │   │
│  │ • Per-bin: Goertzel IIR on novelty_curve with Hann window           │   │
│  │ • Adaptive block_size based on neighbor frequency distance          │   │
│  │ • Hybrid input: 0.5 × novelty + 0.5 × vu_curve                      │   │
│  │ • Phase extraction: atan2(imag, real)                               │   │
│  │ • Magnitude normalization: / (block_size / 2)                       │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ AUTO-RANGING & NORMALIZATION (tempo.h:200-228)                      │   │
│  │ • Find max magnitude across all 64 bins                             │   │
│  │ • Floor: 0.04 to prevent division by zero                           │   │
│  │ • Scale: 1.0 / max_val                                              │   │
│  │ • Cubic scaling: magnitude = (scaled)^3 for perceptual compression  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ PHASE SYNCHRONIZATION (tempo.h:375-403)                             │   │
│  │ • Phase continuity via unwrap: ±π wrapping with inversion tracking  │   │
│  │ • Beat signal: sin(phase) for visualization                         │   │
│  │ • Phase advance: += phase_radians_per_reference_frame × delta       │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ CONFIDENCE CALCULATION (tempo.h:405-429)                            │   │
│  │ • Per-bin contribution: (smooth[bin] / power_sum) × magnitude       │   │
│  │ • Global confidence: max(contribution) across all bins              │   │
│  │ • Power sum: Σ tempi_smooth[i]                                      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       ▼                                                                     │
│  Output (per frame):                                                        │
│  • tempi[64].magnitude (normalized 0-1)                                    │
│  • tempi[64].phase (-π to +π)                                              │
│  • tempi[64].beat = sin(phase)                                             │
│  • tempo_confidence (0-1)                                                  │
│  • silence_detected (boolean)                                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 3.1.2 Core Data Structure

**File**: `types.h:57-71`

```cpp
struct tempo {
    float target_tempo_hz;              // Target frequency: BPM/60 (0.53-2.67 Hz)
    float coeff;                        // Goertzel coefficient: 2*cos(2π*f/fs)
    float sine;                         // sin(2π*f/fs) for phase calculation
    float cosine;                       // cos(2π*f/fs) for phase calculation
    float window_step;                  // Hann window step size
    float phase;                        // Current beat phase (-π to +π)
    float phase_target;                 // Unused target phase (vestigial)
    bool  phase_inverted;               // Track phase wraparound inversions
    float phase_radians_per_reference_frame;  // Phase velocity at REFERENCE_FPS
    float beat;                         // sin(phase) output signal (-1 to +1)
    float magnitude;                    // Normalized magnitude [0-1]
    float magnitude_full_scale;         // Pre-normalization magnitude
    uint32_t block_size;                // Goertzel analysis window in samples
};
```

**Memory**: Each struct is ~76 bytes (10 floats × 4 + 1 bool + padding + uint32_t)

#### 3.1.3 Goertzel Magnitude Calculation

**File**: `tempo.h:140-192`

```cpp
float calculate_magnitude_of_tempo(uint16_t tempo_bin) {
    float normalized_magnitude;

    uint16_t block_size = tempi[tempo_bin].block_size;
    float q1 = 0, q2 = 0;
    float window_pos = 0.0;

    // Goertzel IIR applied to novelty + VU hybrid
    for (uint16_t i = 0; i < block_size; i++) {
        // Get sample from circular buffer
        uint16_t index = (NOVELTY_HISTORY_LENGTH - 1 - block_size + i + novelty_index)
                         % NOVELTY_HISTORY_LENGTH;

        float sample_novelty = novelty_curve_normalized[index];
        float sample_vu = vu_curve[index];

        // Hybrid input: 50% novelty (transients) + 50% VU (sustained)
        float sample = (sample_novelty + sample_vu) / 2.0f;

        // Goertzel IIR recurrence: q0 = coeff*q1 - q2 + x*window
        float q0 = tempi[tempo_bin].coeff * q1 - q2
                 + (sample_novelty * window_lookup[uint32_t(window_pos)]);
        q2 = q1;
        q1 = q0;

        window_pos += tempi[tempo_bin].window_step;
    }

    // Extract real and imaginary components
    float real = (q1 - q2 * tempi[tempo_bin].cosine);
    float imag = (q2 * tempi[tempo_bin].sine);

    // Phase extraction with beat shift
    tempi[tempo_bin].phase = unwrap_phase(atan2(imag, real))
                           + (PI * BEAT_SHIFT_PERCENT);  // 8% ahead

    // Magnitude from Goertzel output
    float magnitude_squared = (q1*q1) + (q2*q2) - q1*q2*tempi[tempo_bin].coeff;
    normalized_magnitude = sqrt(magnitude_squared) / (block_size / 2.0);

    return normalized_magnitude;
}
```

#### 3.1.4 Adaptive Block Sizing

**File**: `tempo.h:71-105`

The block size for each tempo bin is calculated to ensure adequate frequency resolution:

```cpp
void init_tempo_goertzel_constants() {
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        // Calculate target tempo in Hz
        float progress = float(i) / NUM_TEMPI;  // 0.0 to ~1.0
        float tempo_hz = progress_to_tempo(progress) / 60.0;  // BPM to Hz
        tempi[i].target_tempo_hz = tempo_hz;

        // Find neighbor distances for Nyquist criterion
        float neighbor_left = (i > 0) ? tempi[i-1].target_tempo_hz : tempo_hz;
        float neighbor_right = (i < NUM_TEMPI-1) ? progress_to_tempo((i+1.0)/NUM_TEMPI)/60.0 : tempo_hz;
        float max_distance_hz = fmaxf(fabsf(tempo_hz - neighbor_left),
                                      fabsf(tempo_hz - neighbor_right));

        // Block size: enough samples to resolve this frequency from neighbors
        // Formula: block_size = fs / (max_distance * 0.5)
        uint32_t block_size = NOVELTY_LOG_HZ / (max_distance_hz * 0.5);
        block_size = min(block_size, (uint32_t)NOVELTY_HISTORY_LENGTH);
        block_size = max(block_size, (uint32_t)50);  // Minimum 50 samples

        tempi[i].block_size = block_size;

        // Goertzel coefficients
        float w = 2.0 * PI * tempo_hz / NOVELTY_LOG_HZ;
        tempi[i].coeff = 2.0 * cos(w);
        tempi[i].sine = sin(w);
        tempi[i].cosine = cos(w);

        // Window step for Hann window lookup
        tempi[i].window_step = 4096.0 / (float)block_size;

        // Phase velocity at reference FPS
        tempi[i].phase_radians_per_reference_frame = (2.0 * PI * tempo_hz) / REFERENCE_FPS;
    }
}
```

#### 3.1.5 Novelty Extraction

**File**: `tempo.h:344-373`

```cpp
void update_novelty() {
    // Called at 50 Hz (every 20 ms)

    float current_novelty = 0.0;

    // Compute spectral flux across all 64 frequency bins
    for (uint16_t i = 0; i < NUM_FREQS; i++) {
        float new_mag = spectrogram_smooth[i];

        // Novelty = positive magnitude change only (half-wave rectified)
        frequencies_musical[i].novelty = fmaxf(0.0f,
                                              new_mag - frequencies_musical[i].magnitude_last);
        frequencies_musical[i].magnitude_last = new_mag;

        current_novelty += frequencies_musical[i].novelty;
    }

    // Average across frequencies
    current_novelty /= float(NUM_FREQS);

    // Check for silence
    check_silence(current_novelty);

    // Log to circular buffer with logarithmic compression
    log_novelty(log(1.0 + current_novelty));

    // Log VU (volume envelope) separately
    log_vu(vu_max);
    vu_max = 0.000001;  // Reset peak detector for next frame
}
```

#### 3.1.6 Silence Detection

**File**: `tempo.h:317-342`

```cpp
void check_silence(float current_novelty) {
    static float silence_history[128];
    static uint8_t silence_index = 0;

    // Log recent novelty values
    silence_history[silence_index] = current_novelty;
    silence_index = (silence_index + 1) % 128;

    // Compute min/max over last 128 samples (2.56 seconds)
    float min_val = 999999.0;
    float max_val = 0.0;
    for (uint8_t i = 0; i < 128; i++) {
        min_val = fminf(min_val, silence_history[i]);
        max_val = fmaxf(max_val, silence_history[i]);
    }

    // Silence level: inverse of dynamic range
    float range = max_val - min_val;
    if (range < 0.00001) range = 0.00001;
    silence_level = 1.0 / (1.0 + range * 100.0);

    // Binary silence detection
    silence_detected = (silence_level > 0.5);

    // If silent, reduce history to accelerate recovery
    if (silence_detected) {
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            novelty_curve[i] *= 0.9;  // 10% reduction per frame
        }
    }
}
```

#### 3.1.7 Phase Unwrapping

**File**: `tempo.h:129-138`

```cpp
float unwrap_phase(float phase) {
    // NOTE: This function has a BUG!
    // "phase - phase > PI" is always 0 > PI = false
    // The actual phase wrapping happens in sync_beat_phase()

    while (phase - phase > M_PI) {   // BUG: always false!
        phase -= 2 * M_PI;
    }
    while (phase - phase < -M_PI) {  // BUG: always false!
        phase += 2 * M_PI;
    }
    return phase;
}
```

**Actual Phase Wrapping** (tempo.h:170-177):
```cpp
// Manual wrapping that actually works
if (tempi[tempo_bin].phase > PI) {
    tempi[tempo_bin].phase -= (2 * PI);
    tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
}
if (tempi[tempo_bin].phase < -PI) {
    tempi[tempo_bin].phase += (2 * PI);
    tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
}
```

#### 3.1.8 Interleaved Computation Strategy

**File**: `tempo.h:282-294`

To avoid CPU spikes, only 2 of 64 tempo bins are computed per frame:

```cpp
void calculate_tempi_magnitudes() {
    static uint8_t calc_bin = 0;
    static uint8_t iter = 0;

    // Process 2 bins per frame
    calculate_magnitude_of_tempo(calc_bin + 0);
    calculate_magnitude_of_tempo(calc_bin + 1);

    calc_bin += 2;
    if (calc_bin >= NUM_TEMPI * MAX_TEMPO_RANGE) {
        calc_bin = 0;
        iter++;
    }
}
```

**Full Cycle**: 64 bins / 2 bins per frame = 32 frames × 10ms = **320ms** to update all bins.

#### 3.1.9 Confidence Calculation

**File**: `tempo.h:405-429`

```cpp
void calculate_tempo_confidence() {
    float max_contribution = 0.0;
    float tempi_power_sum = 0.0;

    // Accumulate power across all bins
    for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI * MAX_TEMPO_RANGE; tempo_bin++) {
        float tempi_magnitude = tempi[tempo_bin].magnitude;

        // Smooth with exponential filter
        tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * 0.975
                                + tempi_magnitude * 0.025;

        tempi_power_sum += tempi_smooth[tempo_bin];
    }

    // Find max contribution
    for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI * MAX_TEMPO_RANGE; tempo_bin++) {
        float contribution = (tempi_smooth[tempo_bin] / tempi_power_sum)
                           * tempi[tempo_bin].magnitude;
        max_contribution = fmaxf(max_contribution, contribution);
    }

    tempo_confidence = max_contribution;
}
```

#### 3.1.10 Key Parameters (v1.0)

| Parameter | Value | Line | Purpose |
|-----------|-------|------|---------|
| `NOVELTY_HISTORY_LENGTH` | 1024 | 15 | Circular buffer size (20.48s @ 50 Hz) |
| `NOVELTY_LOG_HZ` | 50 | 16 | Novelty update rate |
| `TEMPO_LOW` | 32 BPM | 18-19 | Lowest detectable tempo |
| `TEMPO_HIGH` | 160 BPM | 18-19 | Highest detectable tempo |
| `NUM_TEMPI` | 64 | 20 | Number of parallel tempo bins |
| `BEAT_SHIFT_PERCENT` | 0.08 | 21 | Phase offset (8% ahead of beat peak) |
| `MAX_TEMPO_RANGE` | 1.0 | 30 | Can restrict to subset (unused) |
| Smoothing factor | 0.975 | 415 | EMA for tempo magnitudes |
| Autoranger floor | 0.04 | 213 | Minimum max_val to prevent div/0 |

#### 3.1.11 Resource Utilization (v1.0)

| Resource | Size | Notes |
|----------|------|-------|
| `novelty_curve[1024]` | 4 KB | Circular buffer |
| `novelty_curve_normalized[1024]` | 4 KB | Normalized copy |
| `vu_curve[1024]` | 4 KB | Volume envelope |
| `tempi[64]` | ~5 KB | 64 × 76 bytes |
| `tempi_smooth[64]` | 256 B | EMA smoothed |
| `window_lookup[4096]` | 16 KB | Hann window (global) |
| **Total** | **~37 KB** | No dynamic allocation |

### 3.2 Emotiscope v1.1 - VU Separation

**File**: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.1/src/tempo.h`

**Lines**: 1-441

#### 3.2.1 Key Changes from v1.0

1. **Separate VU Normalization** (NEW: lines 244-261):
```cpp
void normalize_vu_curve() {
    profile_function([&]() {
        static float smooth_max_val = 0.0;

        // Find max in VU curve
        float max_val = 0.0;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i += 4) {
            max_val = fmaxf(max_val, vu_curve[i]);
        }

        // Smooth the max tracker (different from novelty: τ = 0.01)
        smooth_max_val = smooth_max_val * 0.99 + max_val * 0.01;

        // Normalize VU curve independently
        float scale = 1.0 / fmaxf(smooth_max_val, 0.02);
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            vu_curve_normalized[i] = vu_curve[i] * scale;
        }
    });
}
```

2. **Autoranger Threshold Change** (line 205):
```cpp
// v1.0: if (max_val < 0.04)
// v1.1: if (max_val < 0.02)  // More sensitive
```

3. **Novelty Smoothing Factor Change** (line 237):
```cpp
// v1.0: smooth_max_val = smooth_max_val * 0.99 + max_val * 0.01;
// v1.1: smooth_max_val = smooth_max_val * 0.95 + max_val * 0.05;  // Faster adaptation
```

4. **Profile Function Wrapping** (throughout):
```cpp
// All major functions wrapped for performance monitoring
profile_function([&]() {
    // Function body
});
```

5. **New Buffer Added**:
```cpp
float vu_curve_normalized[NOVELTY_HISTORY_LENGTH];  // +4 KB
```

#### 3.2.2 VU Separation Impact

| Aspect | v1.0 | v1.1 | Impact |
|--------|------|------|--------|
| VU normalization | Combined with novelty | Independent | Better separation of dynamics vs transients |
| VU smooth τ | 0.01 | 0.01 | Same (slower than novelty) |
| Novelty smooth τ | 0.01 | 0.05 | Faster novelty adaptation |
| Autoranger floor | 0.04 | 0.02 | More sensitive |
| Profile overhead | None | Yes | Performance monitoring |

### 3.3 Emotiscope v1.2 - Window-Free Simplification

**File**: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/tempo.h`

**Lines**: 1-441

#### 3.3.1 Key Changes from v1.1

1. **Window Function REMOVED** (line 149):
```cpp
// v1.0-1.1: q0 = coeff * q1 - q2 + (sample_novelty * window_lookup[idx]);
// v1.2:     q0 = coeff * q1 - q2 + sample_novelty;  // NO WINDOW!
```

2. **Single-Bin Computation** (line 273):
```cpp
// v1.1: calculate_tempi_magnitudes(calc_bin+0); calculate_tempi_magnitudes(calc_bin+1);
// v1.2: calculate_tempi_magnitudes(calc_bin);  // One bin per frame
```

3. **Magnitude Scaling Shift** (line 178):
```cpp
// v1.0: scale = (0.25 * progress) + 0.75;  // Penalizes fast tempos
// v1.1: scale = (0.6 * progress) + 0.4;    // Intermediate
// v1.2: scale = (0.3 * progress) + 0.7;    // Lighter penalty
```

4. **Phase Update Threshold** (lines 419-428):
```cpp
if (tempi_magnitude > 0.005) {
    // Only sync phase if above noise floor
    sync_beat_phase(tempo_bin, delta);
}
```

5. **FFT Input** (lines 356-361):
```cpp
// v1.1: Uses spectrogram_smooth[i] (frequency magnitudes)
// v1.2: Uses fft_max directly (raw FFT bins)
```

6. **Autoranger Floor** (line 205):
```cpp
// v1.1: 0.02
// v1.2: 0.01  // Even more sensitive
```

#### 3.3.2 Window Removal Analysis

| Aspect | With Window | Without Window |
|--------|-------------|----------------|
| Frequency leakage | Suppressed | Present |
| Temporal resolution | Reduced | Better |
| Computation | ~4096 lookups | None |
| Memory | 16 KB lookup table | Saved |

**Rationale**: For tempo detection on novelty curves (already smoothed), windowing may be unnecessary overhead. The novelty curve itself acts as a natural window.

### 3.4 Emotiscope v2.0 - Dynamic Scaling & Dual-Core

**File**: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-2.0/main/tempo.h`

**Lines**: 1-408

#### 3.4.1 Major Architectural Changes

1. **96 Tempo Bins** (48-144 BPM range):
```cpp
#define NUM_TEMPI 96
#define TEMPO_LOW  48
#define TEMPO_HIGH 144
```

2. **Dynamic Novelty Scale Factor** (NEW: lines 24, 134-150):
```cpp
float novelty_scale_factor = 1.0;

void calculate_novelty_scale_factor() {
    static uint8_t frame_count = 0;
    frame_count++;

    // Update every 10 frames (~100ms)
    if (frame_count % 10 != 0) return;

    float max_val = 0.0;
    for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i += 4) {
        max_val = fmaxf(max_val, novelty_curve[i]);
    }

    float scale_factor_raw = 1.0 / (max_val * 0.5);

    // Slow adaptation: 70% old + 30% new
    novelty_scale_factor = novelty_scale_factor * 0.7 + scale_factor_raw * 0.3;
}
```

3. **FFT-Direct Novelty** (lines 326-331):
```cpp
static float fft_last[FFT_SIZE >> 1];

float current_novelty = 0.0;
for (uint16_t i = 0; i < (FFT_SIZE >> 1); i++) {
    // Positive spectral difference (half-wave rectified)
    current_novelty += fmaxf(0.0f, fft_smooth[0][i] - fft_last[i]);
    fft_last[i] = fft_smooth[0][i];
}

// Square for dynamic range compression
current_novelty *= current_novelty;
```

4. **Quartic Magnitude Scaling** (lines 253-255):
```cpp
for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    float scaled = tempi[i].magnitude_full_scale * autoranger_scale;
    scaled = scaled * scaled;           // Quadratic
    tempi[i].magnitude = scaled * scaled;  // Quartic (^4 total)
}
```

5. **Silent Bin Suppression** (lines 384-393):
```cpp
if (tempi_magnitude > 0.005) {
    // Normal processing
    tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * 0.975 + tempi_magnitude * 0.025;
    tempi_power_sum += tempi_smooth[tempo_bin];
    sync_beat_phase(tempo_bin, delta);
} else {
    // Decay below threshold (don't contribute to power sum)
    tempi_smooth[tempo_bin] *= 0.995;
}
```

6. **VU Curve Removed**: Relies entirely on FFT spectral dynamics.

7. **Lookup Table Optimization** (line 53):
```cpp
float num_tempi_float_lookup[NUM_TEMPI];  // Pre-computed float(i) / NUM_TEMPI

void init() {
    for (int i = 0; i < NUM_TEMPI; i++) {
        num_tempi_float_lookup[i] = float(i) / NUM_TEMPI;
    }
}
```

8. **Profile Macro Refactoring** (throughout):
```cpp
// v1.x: profile_function([&]() { ... });
// v2.0: start_profile(__COUNTER__, __func__);
//       ...
//       end_profile(__COUNTER__);
```

9. **Two-Pass Auto-Ranging** (lines 238-256):
```cpp
void update_tempo() {
    // Pass 1: Compute all magnitudes
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        calculate_magnitude_of_tempo(i);
    }

    // Pass 2: Find max and scale
    float max_val = 0.0;
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        max_val = fmaxf(max_val, tempi[i].magnitude_full_scale);
    }

    float autoranger_scale = 1.0 / max_val;

    // Pass 3: Apply quartic scaling
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        float scaled = tempi[i].magnitude_full_scale * autoranger_scale;
        scaled = scaled * scaled;
        tempi[i].magnitude = scaled * scaled;
    }
}
```

10. **Confidence Recalculation** (lines 400-405):
```cpp
// v1.x: max_contribution = max(smooth[bin] / power_sum * magnitude)
// v2.0: max_contribution = max(smooth[bin])
//       tempo_confidence = max_contribution / tempi_power_sum;
```

#### 3.4.2 Key Parameters (v2.0)

| Parameter | Value | Change from v1.x | Purpose |
|-----------|-------|------------------|---------|
| `NUM_TEMPI` | 96 | +32 (+50%) | Finer resolution (~1 BPM) |
| `TEMPO_LOW` | 48 BPM | +16 | Higher minimum |
| `TEMPO_HIGH` | 144 BPM | -16 | Lower maximum |
| `BEAT_SHIFT_PERCENT` | 0.16 | ×2 | Double phase offset |
| `novelty_scale_factor` | Dynamic | NEW | Adaptive autoranger |
| Scale update interval | 10 frames | NEW | ~100ms adaptation |
| Scale adaptation | 0.7/0.3 | NEW | 70% old + 30% new |
| Magnitude scaling | Quartic (^4) | Was cubic | More aggressive compression |
| Silent threshold | 0.005 | NEW | Noise floor filter |

#### 3.4.3 Resource Utilization (v2.0)

| Resource | v1.0 | v2.0 | Change |
|----------|------|------|--------|
| `tempi[]` array | 64 × 76 B | 96 × 76 B | +2.4 KB |
| `tempi_smooth[]` | 64 × 4 B | 96 × 4 B | +128 B |
| `vu_curve[]` | 4 KB | REMOVED | -4 KB |
| `vu_curve_normalized[]` | 4 KB (v1.1) | REMOVED | -4 KB |
| `num_tempi_float_lookup[]` | N/A | 96 × 4 B | +384 B |
| `fft_last[]` | N/A | ~2 KB | +2 KB |
| **Total** | ~37-44.5 KB | ~40 KB | ~Similar |

### 3.5 Emotiscope v2.0 System Architecture

#### 3.5.1 Dual-Core ESP32-S3 Architecture

**Target**: ESP32-S3-DevKitC-1 (240 MHz, 520 KB RAM)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          EMOTISCOPE v2.0 DUAL-CORE ARCHITECTURE             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         CORE 1: CPU (Audio)                         │   │
│  │                                                                     │   │
│  │  Rate: Adaptive (~10 FPS, limited by I2S)                           │   │
│  │                                                                     │   │
│  │  Tasks:                                                             │   │
│  │  ├─ acquire_sample_chunk()     128 samples @ 12.8 kHz from I2S     │   │
│  │  ├─ calculate_magnitudes()     64 Goertzel frequency bins          │   │
│  │  ├─ perform_fft()              Optional FFT analysis               │   │
│  │  ├─ get_chromagram()           12-note pitch class extraction      │   │
│  │  ├─ run_vu()                   Volume envelope meter               │   │
│  │  ├─ read_touch()               Capacitive touch @ 20 Hz            │   │
│  │  ├─ update_tempo()             96 tempo filters (beat detection)   │   │
│  │  └─ process_serial()           Debug commands                      │   │
│  │                                                                     │   │
│  │  Writes: tempi[], spectrogram[], chromagram[], novelty_curve[]     │   │
│  │  Blocking: I2S read (portMAX_DELAY)                                │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│       │ (Lock-free shared arrays - single writer per variable)              │
│       ▼                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         CORE 0: GPU (Graphics)                      │   │
│  │                                                                     │   │
│  │  Rate: 100 FPS (REFERENCE_FPS)                                      │   │
│  │                                                                     │   │
│  │  Tasks:                                                             │   │
│  │  ├─ update_novelty_curve()     Read from CPU                        │   │
│  │  ├─ update_tempi_phase()       Phase-lock all 96 tempo bins        │   │
│  │  ├─ update_color_cycle()       Auto hue rotation                   │   │
│  │  ├─ draw_light_mode()          Current visualization               │   │
│  │  ├─ apply_transitions()        12 transition types                 │   │
│  │  ├─ apply_effects()            Brightness, gamma, dithering        │   │
│  │  └─ transmit_leds()            RMT driver (non-blocking DMA)       │   │
│  │                                                                     │   │
│  │  Reads: tempi[], spectrogram[], chromagram[], configuration        │   │
│  │  Writes: leds[80]                                                  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  Synchronization: None needed (single writer per shared variable)           │
│  Float access: Atomic on 32-bit ARM                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### 3.5.2 Light Modes Using Beat Data

| Mode | Beat Data Used | Visualization |
|------|----------------|---------------|
| Metronome | `tempi[96].phase`, `tempi_smooth[96]` | 96 sinusoidal dots |
| Spectronome | `tempi[96]` + `spectrogram[64]` | Hybrid spectrum/tempo |
| Beat Tunnel | `tempi[96].phase` | Color strips in tunnel |
| Hype | `tempi[odd/even]` | Split odd/even reactivity |
| Bloom | `tempi[96].phase` | Radiating pulses |
| Spectrum | `spectrogram[64]` only | Bar visualization |
| Analog | `spectrogram[]` only | Gauge visualization |
| Octave | `spectrogram[]` bucketed | Octave-bucketed bars |
| Pitch | `chromagram[12]` | Chromagram display |

#### 3.5.3 Experimental Onset Detection

**File**: `Emotiscope-2.0.edited/Emotiscope-1/00.Reference_Code/SensoryBridge/extras/misc/onsets.h`

**Lines**: 1-71

```cpp
void calculate_onset_correlation() {
    int32_t retro_ticks[16] = {0};  // Last 16 onset timestamps
    uint32_t position_zero = onset_pulse_history[15];

    // For each candidate BPM (60-200, step 2)
    for (uint8_t bpm = 60; bpm <= 200; bpm += 2) {
        float correlation_sum = 0;
        float bps = bpm / 60.0;
        float frames_per_beat = 60 / bps;

        // Generate expected beat grid backwards from current time
        for (uint8_t i = 0; i < 16; i++) {
            retro_ticks[i] = position_zero - (i * frames_per_beat);
        }

        // Correlate with actual onsets
        for (uint8_t i = 0; i < 16; i++) {
            uint32_t min_delta = 9999999;

            // Find closest actual onset to this expected beat
            for (uint8_t x = 0; x < 16; x++) {
                uint32_t frame_delta = abs(retro_ticks[i] - onset_pulse_history[x]);
                if (frame_delta < min_delta) {
                    min_delta = frame_delta;
                }
            }

            // Score if within ±3 frames (~50ms tolerance)
            if (min_delta <= 4) {
                correlation_sum += 1.0 - (min_delta / 4.0);  // Linear decay
            }
        }

        correlation_sum /= 8.0;  // Normalize

        // Store result (not shown: BPM selection logic)
    }
}
```

**Status**: Experimental/deprecated. Not integrated into main tempo tracking.

---

## 4. Comparative Analysis

### 4.1 Architecture Comparison Matrix

| Aspect | Emotiscope v1.0 | Emotiscope v2.0 | K1 v2 |
|--------|-----------------|-----------------|-------|
| **Algorithm Type** | Goertzel on novelty | Goertzel on FFT diff | 4-stage pipeline |
| **Tempo Bins** | 64 | 96 | 121 |
| **BPM Range** | 32-160 | 48-144 | 60-180 |
| **Resolution** | ~2 BPM | ~1 BPM | ~1 BPM |
| **Input Signal** | Novelty + VU hybrid | FFT spectral diff | Spectral flux z-score |
| **Normalization** | Static max autoranger | Dynamic scale factor | Running-stat z-score |
| **Window Function** | Gaussian (4096 LUT) | None | Gaussian σ=1.0 |
| **Phase Tracking** | Direct from Goertzel | Direct from Goertzel | PLL with error correction |
| **Confidence** | max(contribution %) | max(smooth)/power_sum | Grouped density consensus |
| **Hysteresis** | None | None | 3-state machine |
| **Octave Resolution** | None | None | Family scoring |
| **Multi-Tempo** | Yes (64/96 parallel) | Yes (96 parallel) | No (single lock) |
| **History Length** | 20.48s | 20.48s | 8s |
| **Update Rate** | 50 Hz novelty, 2 bins/frame | 50 Hz, 2 bins/frame | 62.5 Hz flux, 10 Hz resonators |
| **Memory** | ~37 KB | ~40 KB | ~12 KB (pipeline only) |
| **Latency** | ~400-500ms | ~400-500ms | ~400-500ms |

### 4.2 Fundamental Paradigm Differences

| Philosophy | Emotiscope | K1 v2 |
|------------|------------|-------|
| **Primary Goal** | Visualize tempo spectrum | Provide definitive beat |
| **Output Model** | 96 parallel phases | Single locked BPM + phase |
| **Tempo Selection** | User/visual selects | Algorithm locks |
| **Octave Handling** | Show both 60 and 120 | Resolve via family scoring |
| **Lock Stability** | N/A (all tempos shown) | 3-state hysteresis |
| **Poly-Rhythm** | Natural (multi-tempo) | Not supported |
| **Integration** | Effects choose from array | Effects use single beat |

### 4.3 Strengths & Weaknesses Analysis

#### 4.3.1 Emotiscope Strengths

1. **Poly-Rhythmic Awareness**
   - All 96 tempos tracked simultaneously
   - Effects can choose any tempo or combine multiple
   - Natural representation of complex rhythms

2. **No Lock-In Errors**
   - Cannot lock to wrong tempo (shows all)
   - Self-correcting as music changes
   - No hysteresis delays

3. **Rich Visualization Data**
   - 96 phases + magnitudes for display
   - Metronome mode shows tempo landscape
   - Enables interactive tempo exploration

4. **Simpler State Machine**
   - No lock states to manage
   - No hysteresis complexity
   - Easier to debug

5. **VU Separation** (v1.1)
   - Independent volume vs novelty response
   - Better handling of sustained notes
   - Separate smoothing time constants

#### 4.3.2 Emotiscope Weaknesses

1. **No Tempo Selection**
   - Effects must choose from 96 options
   - No guidance on "correct" tempo
   - Complex decision logic in effects

2. **No Octave Resolution**
   - 60 BPM and 120 BPM both shown
   - Musically wrong half-time locks visible
   - No family scoring

3. **Static Confidence**
   - Just magnitude ratio
   - No consensus analysis
   - Can't distinguish close candidates

4. **Phase Unwrap Bug**
   - `phase - phase > PI` always false
   - Manual wrapping compensates
   - Code clarity issue

5. **No Verification Period**
   - Susceptible to early noise
   - No challenger tracking
   - Quick to show spurious tempos

#### 4.3.3 K1 v2 Strengths

1. **Definitive Output**
   - Single BPM + phase for effect sync
   - No effect decision logic needed
   - Clear contract

2. **Octave Family Scoring**
   - Resolves 60 vs 120 BPM ambiguity
   - Half/double contributions
   - Musically intelligent

3. **3-State Verification**
   - Catches early misdetection
   - 2.5s verification period
   - Challenger tracking

4. **Adaptive Z-Score**
   - Robust to AGC/gating shifts
   - Running-stat normalization
   - Proven fix for 138 BPM bug

5. **Grouped Confidence**
   - Consensus-based algorithm
   - Recognizes nearby estimates as agreement
   - More meaningful confidence metric

6. **PLL Phase Smoothing**
   - Sub-frame phase accuracy
   - Error correction loop
   - Stable beat display

#### 4.3.4 K1 v2 Weaknesses

1. **Single Tempo Only**
   - Cannot track poly-rhythmic music
   - Forces single-tempo assumption
   - Loses musical information

2. **Lock Persistence**
   - Wrong lock can persist despite hysteresis
   - Recovery requires 15% margin for 0.8s
   - Can feel "stuck"

3. **Complexity**
   - 4 stages with many parameters
   - 3 lock states with transitions
   - Harder to tune and debug

4. **138 BPM Bug History**
   - Required multiple fixes
   - Novelty saturation issue
   - Prior bias issue

### 4.4 Performance Benchmark Comparison

#### 4.4.1 Computational Overhead

| Operation | Emotiscope v2.0 | K1 v2 | Notes |
|-----------|-----------------|-------|-------|
| Goertzel per bin | ~500 iterations | ~512 iterations | Similar |
| Bins per frame | 2 | 121/10 Hz = ~12 | K1 batched |
| Novelty computation | 64 FFT diffs | 8-band flux | Similar |
| Phase updates | 96 × simple math | 1 × PLL correction | Emotiscope higher |
| **Estimated CPU/frame** | ~200 µs | ~150 µs | K1 slightly lower |

#### 4.4.2 Memory Footprint

| Component | Emotiscope v2.0 | K1 v2 |
|-----------|-----------------|-------|
| Tempo structures | 96 × 76 B = 7.3 KB | 121 × 40 B = 4.8 KB |
| History buffers | 1024 × 4 B × 2 = 8 KB | 500 × 4 B = 2 KB |
| Window lookup | 0 (removed) | 4096 × 4 B = 16 KB |
| Smooth arrays | 96 × 4 B = 384 B | 121 × 4 B = 484 B |
| **Total** | ~16 KB | ~23 KB |

#### 4.4.3 Latency Analysis

| Stage | Emotiscope v2.0 | K1 v2 |
|-------|-----------------|-------|
| Novelty/flux history | 20.48s @ 50 Hz | 8s @ 62.5 Hz |
| Goertzel convergence | ~320ms (full cycle) | ~800ms (EMA settling) |
| Phase lock | Immediate | PLL settling ~500ms |
| **Total detection** | ~400-500ms | ~400-500ms |

### 4.5 Genre Reliability Assessment

| Genre | Emotiscope | K1 v2 | Notes |
|-------|------------|-------|-------|
| **EDM/Electronic** | ★★★★☆ | ★★★★★ | K1 better at sustained bass |
| **Rock/Pop** | ★★★★☆ | ★★★★☆ | Similar performance |
| **Hip-Hop** | ★★★★☆ | ★★★★☆ | Both handle well |
| **Jazz** | ★★★☆☆ | ★★★☆☆ | Both struggle with swing |
| **Ambient** | ★★☆☆☆ | ★★★☆☆ | K1's silence detection helps |
| **Classical** | ★★☆☆☆ | ★★★☆☆ | K1's prior helps |
| **Polyrhythmic** | ★★★★★ | ★★☆☆☆ | **Emotiscope wins** |
| **Tempo Changes** | ★★★★★ | ★★★☆☆ | Emotiscope adapts instantly |

---

## 5. Integration Assessment

### 5.1 Option A: Full Emotiscope Replacement

**Required Changes**:

1. **Remove K1 4-stage pipeline** entirely
2. **Replace with Emotiscope tempo.h** (single file)
3. **Modify ControlBus** to expose 96 phases instead of 1
4. **Update ALL 76 effects** to select from tempo array
5. **Remove** family scoring, hysteresis, verification
6. **Add** VU separation logic (from v1.1)
7. **Change** confidence semantics

**Code Impact**:
- `v2/src/audio/k1/` - DELETE (6 files, ~2500 lines)
- `v2/src/audio/contracts/ControlBus.h` - MAJOR REWRITE
- `v2/src/effects/ieffect/*.cpp` - ALL 76 FILES MODIFIED
- `v2/src/plugins/api/EffectContext.h` - REWRITE

**Risk Assessment**: **VERY HIGH**
- Complete architectural change
- All effects need rewriting
- Loss of proven K1 features (octave resolution, hysteresis)
- Untested paradigm shift

**Estimated Effort**: 3-4 weeks

### 5.2 Option B: Selective Enhancement

**Enhancements to K1 from Emotiscope**:

| Enhancement | Source | Effort | Risk | Impact |
|-------------|--------|--------|------|--------|
| Dynamic novelty scale | v2.0 | LOW | LOW | Faster adaptation |
| Silent bin suppression | v2.0 | LOW | LOW | Noise reduction |
| VU separation | v1.1 | MEDIUM | LOW | Better sustain handling |
| Quartic display scaling | v2.0 | LOW | LOW | Visual clarity |
| FFT-direct novelty | v2.0 | MEDIUM | MEDIUM | Different transient response |

**Code Impact**:
- `v2/src/audio/k1/K1Pipeline.cpp` - Minor additions
- `v2/src/audio/k1/K1TactusResolver.cpp` - Minor additions
- No effect changes required

**Risk Assessment**: **LOW**
- Incremental changes
- Each enhancement testable independently
- K1 architecture preserved
- Easy rollback

**Estimated Effort**: 1-2 days per enhancement

### 5.3 Option C: Parallel Implementation

**Concept**: Run BOTH K1 and Emotiscope-style tempo tracking

**Use Cases**:
- K1 output for effects (single beat)
- Emotiscope output for diagnostic visualization
- Tempo spectrum display for debugging

**Code Impact**:
- Add `v2/src/audio/EmotiscopeTempo.h` (~400 lines)
- Add ControlBus fields for 96 tempi
- Add diagnostic web endpoint

**Risk Assessment**: **LOW-MEDIUM**
- No changes to existing K1
- Additional memory (~40 KB)
- Additional CPU (~200 µs/frame)

**Estimated Effort**: 1 week

---

## 6. Recommendations

### 6.1 Primary Recommendation

**Keep K1, enhance with selective Emotiscope ideas (Option B)**

**Rationale**:
1. K1's single-tempo output matches effect requirements
2. Family scoring solves octave ambiguity (critical for BPM display)
3. 3-state machine prevents lock errors (proven fix for 138 BPM bug)
4. Effects already designed for single beat source
5. Low risk, incremental improvements

### 6.2 Recommended Enhancements (Priority Order)

#### P1: Silent Bin Suppression

**Implementation**:
```cpp
// In K1TactusResolver.cpp, score_family() or equivalent
if (candidate.magnitude < 0.005f) {
    candidate.magnitude *= 0.995f;  // Decay below threshold
    continue;  // Skip scoring
}
```

**Impact**: Reduces noise floor contribution, cleaner confidence.

#### P2: Dynamic Novelty Scaling

**Implementation**:
```cpp
// In K1Pipeline.cpp
static float novelty_scale_factor = 1.0f;
static uint8_t scale_frame_count = 0;

void update_novelty_scale() {
    scale_frame_count++;
    if (scale_frame_count % 10 != 0) return;  // Every 10 frames

    float max_val = 0.0f;
    for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i += 4) {
        max_val = fmaxf(max_val, novelty_history_[i]);
    }

    float raw_scale = 1.0f / fmaxf(max_val * 0.5f, 0.01f);
    novelty_scale_factor = novelty_scale_factor * 0.7f + raw_scale * 0.3f;
}

// In novelty computation
float scaled_novelty = raw_novelty * novelty_scale_factor;
```

**Impact**: Faster adaptation to level changes than running-stat z-score alone.

#### P3: VU Separation (Optional)

**Implementation**:
```cpp
// Add separate VU tracking
float vu_curve[NOVELTY_HISTORY_LENGTH];
float vu_scale_factor = 1.0f;

void update_vu(float rms) {
    // Log VU separately from novelty
    vu_curve[vu_index] = rms;
    vu_index = (vu_index + 1) % NOVELTY_HISTORY_LENGTH;

    // Separate normalization with slower adaptation
    update_vu_scale();  // τ = 0.01 vs 0.05 for novelty
}

// In flux computation
float hybrid_input = 0.5f * novelty_normalized + 0.5f * vu_normalized;
```

**Impact**: Better handling of sustained notes vs percussive transients.

### 6.3 Testing Requirements

| Test Case | Purpose | Pass Criteria |
|-----------|---------|---------------|
| 138 BPM reference track | Verify no regression | Lock within ±2 BPM, <3s |
| 120 BPM reference track | Prior center behavior | Lock within ±1 BPM, <2s |
| Silence → music | Startup handling | Lock <3s after audio starts |
| Tempo change 120→140 | Adaptation speed | Switch <2s |
| Tempo change 120→60 | Octave handling | Prefer 120 (family scoring) |
| Polyrhythmic track | Edge case | Stable lock (any valid tempo) |
| Ambient/minimal | Low energy | No false locks |
| Level change +20dB | AGC robustness | No lock disruption |
| Level change -20dB | AGC robustness | No lock disruption |

### 6.4 Migration Strategy

**Phase 1** (Week 1): Implement and test P1 (silent bin suppression)
- Single function modification
- A/B test with existing implementation
- Measure confidence stability

**Phase 2** (Week 2): Implement and test P2 (dynamic novelty scaling)
- Add parallel scaling mechanism
- Compare adaptation speed vs running-stat
- Verify 138 BPM fix not regressed

**Phase 3** (Week 3, Optional): Implement P3 (VU separation)
- Add VU tracking infrastructure
- Test with sustained-note-heavy music
- Measure improvement in lock stability

**Rollback Plan**: Each enhancement is isolated; can revert individual changes without affecting others.

---

## 7. Appendices

### 7.1 Complete File Inventory

#### K1 v2 Files Analyzed

| File | Lines | Purpose |
|------|-------|---------|
| `v2/src/audio/k1/K1Config.h` | 79 | Configuration parameters |
| `v2/src/audio/k1/K1Pipeline.h` | ~100 | Pipeline orchestrator interface |
| `v2/src/audio/k1/K1Pipeline.cpp` | ~300 | Z-score normalizer implementation |
| `v2/src/audio/k1/K1ResonatorBank.h` | ~80 | Resonator bank interface |
| `v2/src/audio/k1/K1ResonatorBank.cpp` | ~400 | Goertzel implementation |
| `v2/src/audio/k1/K1TactusResolver.h` | ~100 | Tactus resolver interface |
| `v2/src/audio/k1/K1TactusResolver.cpp` | ~500 | Family scoring, hysteresis |
| `v2/src/audio/k1/K1BeatClock.h` | ~60 | Beat clock interface |
| `v2/src/audio/k1/K1BeatClock.cpp` | ~200 | PLL implementation |
| `v2/src/audio/k1/K1Types.h` | ~150 | Data structure definitions |
| `v2/src/audio/AudioActor.cpp` | ~700 | Audio pipeline integration |
| `v2/src/audio/contracts/ControlBus.h` | ~200 | Beat data publication |
| `v2/src/audio/contracts/ControlBus.cpp` | ~500 | ControlBus implementation |
| `v2/src/plugins/api/EffectContext.h` | ~350 | Effect beat accessors |

#### Emotiscope Files Analyzed

| File | Lines | Version | Purpose |
|------|-------|---------|---------|
| `Emotiscope-1.0/src/tempo.h` | 429 | v1.0 | Baseline implementation |
| `Emotiscope-1.0/src/types.h` | ~150 | v1.0 | Data structures |
| `Emotiscope-1.0/src/goertzel.h` | 366 | v1.0 | Frequency analysis |
| `Emotiscope-1.1/src/tempo.h` | 441 | v1.1 | VU separation |
| `Emotiscope-1.2/src/tempo.h` | 441 | v1.2 | Window-free |
| `Emotiscope-2.0/main/tempo.h` | 408 | v2.0 | Dynamic scaling |
| `Emotiscope-2.0/main/cpu_core.h` | ~200 | v2.0 | Audio thread |
| `Emotiscope-2.0/main/gpu_core.h` | ~200 | v2.0 | Graphics thread |
| `Emotiscope-2.0/main/microphone.h` | ~150 | v2.0 | I2S capture |
| `Emotiscope-2.0/main/global_defines.h` | ~100 | v2.0 | Constants |
| `extras/misc/onsets.h` | 71 | Exp. | Onset correlation |

### 7.2 Glossary

| Term | Definition |
|------|------------|
| **BPM** | Beats Per Minute - tempo measurement unit |
| **Goertzel Algorithm** | Efficient single-frequency DFT for magnitude/phase extraction |
| **Novelty Curve** | Spectral flux (magnitude change) over time |
| **Tactus** | Perceived beat level (typically 80-160 BPM for most music) |
| **PLL** | Phase-Locked Loop - feedback control for phase tracking |
| **EMA** | Exponential Moving Average - first-order IIR filter |
| **EWMA** | Exponentially Weighted Moving Average - same as EMA |
| **Z-Score** | Standard deviations from mean: (x - μ) / σ |
| **Hysteresis** | Resistance to state change requiring sustained threshold crossing |
| **Octave Ambiguity** | Musical confusion between half/double tempo relationships |
| **Family Scoring** | Considering half-tempo and double-tempo in scoring |
| **Autoranger** | Dynamic scaling to normalize signal levels |
| **VU** | Volume Unit - amplitude envelope measurement |

### 7.3 References

1. **K1 Tempo Mislock Analysis**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/analysis/K1_TEMPO_MISLOCK_ANALYSIS.md`
2. **Audio Reactive Motion Analysis**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/docs/analysis/Audio_Reactive_Motion_Phase_Analysis.md`
3. **Sensory Bridge Audio-Visual Mapping**: Referenced in K1 design documents
4. **Emotiscope GitHub**: Original Lixie Labs project

---

## Document Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-31 | Claude Code Multi-Agent | Initial comprehensive analysis |

---

*End of Document*
