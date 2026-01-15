# Synesthesia Beat Detection Algorithm Flow
**Implementation**: Lightwave-Ledstrip v2 TempoTracker
**Date**: January 9, 2026

---

## Complete Pipeline (Phases 3-5 Integrated)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AUDIO INPUT (16 kHz)                              │
│                        128 samples per hop (~8ms)                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         LAYER 1: ONSET DETECTION                            │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ Spectral Flux Calculation                                          │    │
│  │ • 8-band pre-AGC Goertzel analysis                                 │    │
│  │ • Half-wave rectified flux: Σ max(0, M[t] - M[t-1])               │    │
│  │ • VU derivative: max(0, RMS[t] - RMS[t-1])                         │    │
│  │ • Combined: 50% spectral + 50% VU                                  │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ PHASE 3: Flux History Buffer (Circular)                           │    │
│  │ • Size: 40 frames (≈230ms @ 173 Hz hop rate)                      │    │
│  │ • flux_history[flux_idx] = current_flux                            │    │
│  │ • flux_idx = (flux_idx + 1) % 40                                   │    │
│  │ • Warmup: First 40 hops use legacy baseline                        │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ PHASE 4: Adaptive Threshold (Synesthesia Formula)                 │    │
│  │                                                                     │    │
│  │   median = median(flux_history[40])                                │    │
│  │   stddev = √(Σ(flux[i] - median)² / 40)                            │    │
│  │   threshold = median + 1.5 × stddev                                │    │
│  │                                                                     │    │
│  │ • Robust to outliers (uses median, not mean)                       │    │
│  │ • Adapts to noise floor dynamically                                │    │
│  │ • Sensitivity: 1.5 (tunable)                                       │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ Peak Detection & Refractory Period                                 │    │
│  │ • Local peak: flux[t-1] > flux[t-2] AND flux[t-1] > flux[t]       │    │
│  │ • Above threshold: flux[t-1] > threshold                           │    │
│  │ • Refractory: time_since_last > 150ms                              │    │
│  │ • Onset strength: (flux - threshold) / threshold                   │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│                            [ONSET DETECTED: YES/NO]                         │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        LAYER 2: BEAT TRACKING                               │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ Inter-Onset Interval (IOI) Analysis                                │    │
│  │ • IOI = t_onset[i+1] - t_onset[i]                                  │    │
│  │ • BPM_raw = 60.0 / IOI_seconds                                      │    │
│  │ • Validate: 60 BPM ≤ BPM_raw ≤ 180 BPM                             │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ Density Buffer Voting (Histogram)                                  │    │
│  │ • tempoDensity[BPM - 60] += weight × strength                      │    │
│  │ • Triangular kernel spread: ±2 BPM bins                            │    │
│  │ • Decay per hop: density × 0.995                                   │    │
│  │ • Winner: argmax(tempoDensity[60-180])                             │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ PHASE 5: Exponential Smoothing (Attack/Release)                   │    │
│  │                                                                     │    │
│  │   if (BPM_raw > BPM_prev):                                         │    │
│  │       α = 0.15  // Attack coefficient (fast response)              │    │
│  │   else:                                                             │    │
│  │       α = 0.05  // Release coefficient (slow decay)                │    │
│  │                                                                     │    │
│  │   BPM_smooth = α × BPM_raw + (1 - α) × BPM_prev                    │    │
│  │                                                                     │    │
│  │ • Attack time constant: τ ≈ 111ms (6.7 frames @ 60fps)             │    │
│  │ • Release time constant: τ ≈ 333ms (20 frames @ 60fps)             │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │ Confidence Scoring                                                  │    │
│  │ • Peak sharpness: (max - second_peak) / max                        │    │
│  │ • Interval consistency: 1 - CoV(recent_intervals)                  │    │
│  │ • Confidence = sharpness × consistency                              │    │
│  │ • Locked: confidence > 0.5                                          │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                      │                                      │
│                                      ▼                                      │
│                          [BPM, PHASE, CONFIDENCE]                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       LAYER 3: OUTPUT FORMATTING                            │
│                                                                             │
│  TempoOutput:                                                               │
│  • bpm: float (60.0 - 180.0)                                                │
│  • phase01: float (0.0 - 1.0, wraps at beat)                                │
│  • confidence: float (0.0 - 1.0)                                            │
│  • beat_tick: bool (true for 1 frame at beat)                              │
│  • beat_strength: float (0.0 - 5.0+)                                        │
│  • onset_strength: float (0.0 - 5.0+)                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 3: Onset Detection Detail

```
         flux[t]
            │
            ▼
    ┌──────────────┐
    │ Add to Buffer│ ◄── Circular buffer: flux_history[40]
    └──────────────┘
            │
            ▼
    ┌──────────────┐
    │ Buffer Full? │ ◄── flux_history_count >= 40?
    └──────────────┘
        YES │      NO
            │      │
            │      └──► [Use Legacy Baseline × 1.8]
            │
            ▼
    ┌──────────────┐
    │ PHASE 4:     │
    │ Adaptive     │
    │ Threshold    │
    └──────────────┘
```

### Refractory Period Gate
```
time_since_last_onset > 150ms?
    │
    ├── YES ──► Continue to peak detection
    │
    └── NO  ──► Reject onset (too soon)
```

---

## Phase 4: Adaptive Threshold Calculation

```
flux_history[40] = [0.5, 0.7, 0.3, 1.2, ..., 0.8]
                          │
                          ▼
                  ┌───────────────┐
                  │  Sort Buffer  │
                  └───────────────┘
                          │
                          ▼
         sorted = [0.1, 0.2, 0.3, ..., 3.5, 4.2]
                          │
                          ▼
                  ┌───────────────┐
                  │ Find Median   │ ◄── Middle value (20th element)
                  └───────────────┘
                          │
                          ▼
                    median = 0.6
                          │
                          ▼
                  ┌───────────────┐
                  │  Calculate    │ ◄── σ = √(Σ(x - median)² / N)
                  │  Std Dev      │
                  └───────────────┘
                          │
                          ▼
                    stddev = 0.3
                          │
                          ▼
                  ┌───────────────┐
                  │  Apply        │ ◄── threshold = median + 1.5σ
                  │  Formula      │
                  └───────────────┘
                          │
                          ▼
              threshold = 0.6 + 1.5 × 0.3
                        = 1.05
```

### Peak Detection Logic
```
flux[t-2] < flux[t-1] > flux[t]  AND  flux[t-1] > threshold
    │                                        │
    └────────────────────────────────────────┘
                    │
                    ▼
              [ONSET DETECTED]
```

---

## Phase 5: BPM Smoothing Decision Tree

```
                    BPM_raw estimated
                           │
                           ▼
                ┌──────────────────┐
                │ BPM_raw >        │
                │ BPM_prev ?       │
                └──────────────────┘
                    │          │
              YES   │          │ NO
                    │          │
                    ▼          ▼
        ┌──────────────┐  ┌──────────────┐
        │ ATTACK MODE  │  │ RELEASE MODE │
        │ α = 0.15     │  │ α = 0.05     │
        │              │  │              │
        │ Fast response│  │ Slow decay   │
        │ to tempo     │  │ prevents     │
        │ increase     │  │ jitter       │
        └──────────────┘  └──────────────┘
                    │          │
                    └────┬─────┘
                         │
                         ▼
            BPM_smooth = α × BPM_raw + (1 - α) × BPM_prev
                         │
                         ▼
                  ┌──────────────┐
                  │ Update State │
                  │ bpm_prev =   │
                  │ BPM_smooth   │
                  └──────────────┘
```

### Time Constants
```
Attack (α = 0.15):
    τ = -1 / ln(1 - 0.15) = 6.67 frames
    At 60fps: 6.67 × 16.7ms ≈ 111ms response time

Release (α = 0.05):
    τ = -1 / ln(1 - 0.05) = 20.0 frames
    At 60fps: 20.0 × 16.7ms ≈ 333ms decay time
```

---

## State Machine Integration

```
    INITIALIZING (hops 0-50)
            │
            │ flux_history fills
            │
            ▼
    SEARCHING (confidence < 0.3)
            │
            │ Adaptive threshold activates
            │ Onset detection stabilizes
            │
            ▼
    LOCKING (0.3 ≤ confidence < 0.5)
            │
            │ BPM smoothing active
            │ Density buffer converging
            │
            ▼
    LOCKED (confidence ≥ 0.5)
            │
            │ Phase 5 smoothing minimizes jitter
            │ Stable output for effects
            │
            ▼
    [If confidence drops]
            │
            ▼
    UNLOCKING → SEARCHING (cycle repeats)
```

---

## Memory Layout

```
OnsetState {
    // Legacy fields
    baseline_vu: 4 bytes
    baseline_spec: 4 bytes
    flux_prev: 4 bytes
    flux_prevprev: 4 bytes
    lastOnsetUs: 8 bytes
    bands_last[8]: 32 bytes
    rms_last: 4 bytes

    // Phase 3-4 additions
    flux_history[40]: 160 bytes  ◄── NEW
    flux_history_idx: 1 byte     ◄── NEW
    flux_history_count: 1 byte   ◄── NEW
}
Total: 222 bytes

BeatState {
    // Legacy fields
    bpm: 4 bytes
    phase01: 4 bytes
    conf: 4 bytes
    ...

    // Phase 5 additions
    bpm_raw: 4 bytes       ◄── NEW
    bpm_prev: 4 bytes      ◄── NEW
}
Additional: 8 bytes

Grand Total: ~230 bytes added (0.22 KB)
```

---

## Performance Characteristics

### Computational Cost per Hop
```
Phase 3: Circular buffer insert
    - Array write: O(1)
    - Index increment: O(1)
    - Cost: ~3 CPU cycles

Phase 4: Adaptive threshold
    - Median calculation: O(N log N) = O(40 log 40) ≈ 213 comparisons
    - Std dev calculation: O(N) = 40 iterations
    - Total: ~250 operations
    - Cost: ~2 μs @ 240 MHz

Phase 5: Exponential smoothing
    - Branch: 1 comparison
    - FMA operations: 2 multiply-adds
    - Cost: ~10 CPU cycles

Total overhead: ~3 μs per hop
(Negligible compared to 8ms hop duration)
```

### Latency Budget
```
Audio buffer: 8ms (128 samples @ 16 kHz)
    ├── FFT analysis: 0.5ms
    ├── Phase 3 buffer: 0.001ms
    ├── Phase 4 threshold: 0.002ms
    ├── Peak detection: 0.1ms
    ├── Phase 5 smoothing: 0.01ms
    └── Other processing: 0.5ms
────────────────────────────────────
Total: ~1.1ms per hop (13.8% of budget) ✅
```

---

## Test Validation Checklist

### Phase 3: Onset Detection
- [ ] Flux history buffer fills correctly (40 frames warmup)
- [ ] Legacy fallback works during warmup
- [ ] Refractory period enforces 150ms minimum interval
- [ ] Peak detection triggers on local maxima
- [ ] Onset strength calculated correctly

### Phase 4: Adaptive Threshold
- [ ] Median calculation matches reference implementation
- [ ] Standard deviation robust to outliers
- [ ] Threshold adapts to silence (low median, low σ)
- [ ] Threshold adapts to loud music (high median, high σ)
- [ ] False positive rate < 5% on noise input

### Phase 5: Exponential Smoothing
- [ ] Attack mode activates when BPM increasing
- [ ] Release mode activates when BPM decreasing
- [ ] BPM jitter < ±5 BPM after lock
- [ ] Response time ≤ 200ms for tempo changes
- [ ] No oscillation or instability

### Integration Tests
- [ ] Lock time ≤ 2.0s on EDM test cases (120-140 BPM)
- [ ] Confidence score > 0.7 after lock
- [ ] Beat phase alignment within ±50ms
- [ ] TempoOutput interface unchanged (backward compatible)

---

**End of Algorithm Flow Document**
