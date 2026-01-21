# TempoTracker Tuning Parameter Guide

**Version:** 3.0.0
**Target**: LightwaveOS v2 Onset-Timing Tempo Tracker
**Last Updated:** 2026-01-08

This guide documents all tuning parameters in the `TempoTrackerTuning` struct, providing range recommendations, effects, and tuning strategies for each parameter.

---

## Parameter Categories

- [BPM Range](#bpm-range)
- [Onset Detection](#onset-detection)
- [Flux Combination](#flux-combination)
- [Beat Tracking](#beat-tracking)
- [BPM Smoothing](#bpm-smoothing)
- [Confidence Calculation](#confidence-calculation)
- [Density Buffer](#density-buffer)
- [Interval Voting](#interval-voting)
- [PLL (Phase-Locked Loop)](#pll-phase-locked-loop)
- [Phase Advancement](#phase-advancement)
- [Low-Confidence Reset](#low-confidence-reset)
- [Interval Mismatch Reset](#interval-mismatch-reset)
- [Interval Weighting (Consistency Boost)](#interval-weighting-consistency-boost)
- [Octave Flip Detection](#octave-flip-detection)
- [Outlier Rejection (P1-D)](#outlier-rejection-p1-d)
- [Onset Strength Weighting (P1-A)](#onset-strength-weighting-p1-a)
- [Conditional Octave Voting (P1-B)](#conditional-octave-voting-p1-b)
- [Interval Validation](#interval-validation)
- [K1 Front-End Initialization](#k1-front-end-initialization)
- [Peak Gating](#peak-gating)
- [Onset Strength Limits](#onset-strength-limits)
- [Triangular Kernel Weights](#triangular-kernel-weights)
- [Spectral Weights](#spectral-weights)

---

## BPM Range

### minBpm

- **Range**: [40.0, 100.0]
- **Default**: 60.0
- **Effect**: Minimum detectable BPM (60 BPM = 1.0 sec beat period)
  - **Lower (40)**: Allows slower tempos (downtempo, ambient)
  - **Higher (80)**: Rejects slow tempos, focuses on dance music
- **Tuning**: Set based on music genre:
  - **Ambient/downtempo**: 40-60 BPM
  - **Pop/rock**: 60-80 BPM
  - **EDM/dance**: 80-100 BPM

### maxBpm

- **Range**: [150.0, 300.0]
- **Default**: 180.0
- **Effect**: Maximum detectable BPM (180 BPM = 0.333 sec beat period)
  - **Lower (150)**: Rejects fast tempos (prevents drum&bass detection)
  - **Higher (300)**: Allows faster tempos (drum&bass, gabber)
- **Tuning**: Set based on music genre:
  - **Pop/rock**: 120-160 BPM
  - **EDM/house**: 120-180 BPM
  - **Drum&bass**: 160-300 BPM

**Note**: The refractory period (`refractoryMs`) must be compatible with `maxBpm`:
- 200 ms refractory = 300 BPM max (current default)
- 150 ms refractory = 400 BPM max (for faster genres)

---

## Onset Detection

### onsetThreshK

- **Range**: [1.2, 3.0]
- **Default**: 1.8
- **Effect**: Multiplier over baseline for onset detection
  - **Lower (1.2)**: More sensitive (detects weak beats, more false positives)
  - **Higher (3.0)**: Less sensitive (only strong beats, fewer false positives)
- **Tuning**:
  - Decrease if missing beats in quiet passages
  - Increase if detecting ghost beats (hats, snares as beats)

### refractoryMs

- **Range**: [100, 300] ms
- **Default**: 200 ms
- **Effect**: Minimum time between onsets (prevents subdivision detection)
  - **Lower (100)**: Allows faster tempos (400 BPM max)
  - **Higher (300)**: Prevents subdivisions (200 BPM max)
- **Tuning**:
  - **P0-F FIX**: Increased to 200 ms to prevent hi-hat subdivision detection
  - Increase if detecting subdivisions (hats as beats)
  - Decrease if missing fast tempos (drum&bass, gabber)

### baselineAlpha

- **Range**: [0.01, 0.5]
- **Default**: 0.22
- **Effect**: Baseline adaptation speed (EMA smoothing factor)
  - **Lower (0.01)**: Slower adaptation, better for music with dynamics
  - **Higher (0.5)**: Faster adaptation, better for noisy environments
- **Tuning**:
  - Decrease if baseline overshoots during loud passages
  - Increase if baseline lags during quiet-to-loud transitions

### minBaselineInit, minBaselineVu, minBaselineSpec

- **Range**: [0.0001, 0.01]
- **Default**: 0.001
- **Effect**: Minimum baseline floor (prevents decay to zero)
- **Tuning**: Generally leave at default (safety mechanism)

---

## Flux Combination

### fluxWeightVu

- **Range**: [0.0, 1.0]
- **Default**: 0.5
- **Effect**: Weight for VU delta in combined flux
- **Tuning**: Must satisfy `fluxWeightVu + fluxWeightSpec = 1.0`

### fluxWeightSpec

- **Range**: [0.0, 1.0]
- **Default**: 0.5
- **Effect**: Weight for spectral flux in combined flux
- **Tuning**:
  - Increase for spectral-heavy detection (focus on timbre changes)
  - Decrease for energy-heavy detection (focus on loudness changes)

### fluxNormalizedMax

- **Range**: [5.0, 20.0]
- **Default**: 10.0
- **Effect**: Maximum normalized flux value (clamp outliers)
- **Tuning**: Increase if strong beats are being clamped prematurely

### fluxBaselineEps

- **Range**: [1e-9, 1e-3]
- **Default**: 1e-6
- **Effect**: Epsilon for baseline division (prevent divide-by-zero)
- **Tuning**: Generally leave at default (numerical stability)

---

## Beat Tracking

### lockStrength

- **Range**: [0.0, 1.0]
- **Default**: 0.35 (DEPRECATED - not used)
- **Effect**: Phase correction gain (legacy parameter)
- **Tuning**: Not used in current implementation

### confRise

- **Range**: [0.01, 0.5]
- **Default**: 0.1
- **Effect**: Confidence rise per good onset
- **Tuning**: Increase for faster lock acquisition

### confFall

- **Range**: [0.05, 0.5]
- **Default**: 0.2
- **Effect**: Confidence fall per second without support
- **Tuning**: Increase for faster loss of lock when beats stop

### lockThreshold

- **Range**: [0.3, 0.8]
- **Default**: 0.5
- **Effect**: Confidence threshold for "locked" state
  - **Lower (0.3)**: Lock earlier (less stable)
  - **Higher (0.8)**: Lock later (more stable)
- **Tuning**: Increase if beat ticks occur before confident lock

---

## BPM Smoothing

### bpmAlpha

- **Range**: [0.01, 0.5]
- **Default**: 0.1
- **Effect**: BPM EMA smoothing factor
  - **Lower (0.01)**: Slower BPM changes (more stable)
  - **Higher (0.5)**: Faster BPM changes (tracks tempo changes quickly)
- **Tuning**:
  - Decrease for stable tempos (less BPM jitter)
  - Increase for music with tempo variations (DJ mixes, live performances)

---

## Confidence Calculation

### confAlpha

- **Range**: [0.05, 0.5]
- **Default**: 0.2
- **Effect**: Confidence EMA smoothing factor
- **Tuning**: Similar to `bpmAlpha` - controls confidence update speed

---

## Density Buffer

### densityDecay

- **Range**: [0.98, 0.999]
- **Default**: 0.995
- **Effect**: Density buffer decay per hop (0.995 = 0.5% decay/hop, ~200s time constant at 125 Hz)
  - **Lower (0.98)**: Faster decay (forgets old tempos quickly)
  - **Higher (0.999)**: Slower decay (remembers old tempos longer)
- **Tuning**:
  - Decrease for music with frequent tempo changes
  - Increase for stable tempos (reduces jitter)

---

## Interval Voting

### kernelWidth

- **Range**: [1.0, 4.0]
- **Default**: 2.0
- **Effect**: Triangular kernel width for density voting (BPM bins)
- **Tuning**: Increase for more spread-out votes (smoother density buffer)

### octaveVariantWeight

- **Range**: [0.25, 0.75]
- **Default**: 0.5
- **Effect**: Weight for octave variants (0.5×, 2×) during search mode
- **Tuning**: Increase if octave confusion persists during search

---

## PLL (Phase-Locked Loop)

### pllKp

- **Range**: [0.01, 0.5]
- **Default**: 0.1
- **Effect**: PLL proportional gain (phase correction)
  - **Lower (0.01)**: Slower phase correction
  - **Higher (0.5)**: Faster phase correction (may overshoot)
- **Tuning**:
  - Increase if phase drifts significantly
  - Decrease if phase oscillates

### pllKi

- **Range**: [0.001, 0.1]
- **Default**: 0.01
- **Effect**: PLL integral gain (tempo correction)
  - **Lower (0.001)**: Slower tempo correction
  - **Higher (0.1)**: Faster tempo correction (may cause tempo instability)
- **Tuning**:
  - Increase if tempo drifts over time
  - Decrease if tempo oscillates

### pllMaxIntegral

- **Range**: [0.5, 5.0]
- **Default**: 2.0
- **Effect**: PLL integral windup limit
- **Tuning**: Increase if tempo correction saturates too early

### pllMaxPhaseCorrection

- **Range**: [0.01, 0.3]
- **Default**: 0.1
- **Effect**: Maximum phase correction per onset (clamp)
- **Tuning**: Increase if phase corrections are too small

### pllMaxTempoCorrection

- **Range**: [1.0, 20.0]
- **Default**: 5.0
- **Effect**: Maximum tempo correction per onset (BPM, clamp)
- **Tuning**: Increase if tempo corrections are too small

---

## Phase Advancement

### phaseWrapHighThreshold

- **Range**: [0.8, 0.95]
- **Default**: 0.9
- **Effect**: High threshold for beat tick detection (wrap from high to low)
- **Tuning**: Adjust if beat ticks fire too early/late

### phaseWrapLowThreshold

- **Range**: [0.05, 0.2]
- **Default**: 0.1
- **Effect**: Low threshold for beat tick detection
- **Tuning**: Adjust if beat ticks fire too early/late

### beatTickDebounce

- **Range**: [0.3, 0.8]
- **Default**: 0.6
- **Effect**: Debounce factor (60% of beat period minimum between ticks)
- **Tuning**: Increase if multiple beat ticks fire per beat

---

## Low-Confidence Reset

### lowConfThreshold

- **Range**: [0.05, 0.3]
- **Default**: 0.15
- **Effect**: Confidence below which we consider "lost"
- **Tuning**: Lower to trigger reset earlier

### lowConfResetTimeSec

- **Range**: [2.0, 30.0]
- **Default**: 8.0
- **Effect**: Seconds of low confidence before soft-reset
- **Tuning**: Decrease for faster recovery from loss of lock

### densitySoftResetFactor

- **Range**: [0.1, 0.5]
- **Default**: 0.3
- **Effect**: Multiply density buffer by this on soft-reset (not full clear)
- **Tuning**: Lower for more aggressive reset (faster re-acquisition)

---

## Interval Mismatch Reset

### intervalMismatchThreshold

- **Range**: [5.0, 30.0]
- **Default**: 10.0
- **Effect**: BPM difference to trigger mismatch check
- **Tuning**: Increase if false resets occur

### intervalMismatchCount

- **Range**: [3, 10]
- **Default**: 5
- **Effect**: Number of consecutive mismatched intervals before reset
- **Tuning**: Increase for more tolerance before reset

---

## Interval Weighting (Consistency Boost)

### consistencyBoostThreshold

- **Range**: [5.0, 30.0]
- **Default**: 15.0
- **Effect**: BPM difference for consistency boost (within N BPM = boost)
- **Tuning**: Increase for looser consistency criteria

### consistencyBoostMultiplier

- **Range**: [1.5, 5.0]
- **Default**: 3.0
- **Effect**: Multiply weight by this if interval matches recent ones
- **Tuning**: Increase to amplify consistent intervals (faster lock)

### recentIntervalWindow

- **Range**: [3, 10]
- **Default**: 5
- **Effect**: Number of recent intervals to check for consistency
- **Tuning**: Increase for more robust consistency check (slower lock)

---

## Octave Flip Detection

### octaveFlipRatioHigh

- **Range**: [1.5, 2.0]
- **Default**: 1.8
- **Effect**: Ratio threshold for octave flip detection (near 2×)
- **Tuning**: Tighten (lower) to detect smaller jumps as octave flips

### octaveFlipRatioLow

- **Range**: [0.4, 0.6]
- **Default**: 0.55
- **Effect**: Ratio threshold for octave flip detection (near 0.5×)
- **Tuning**: Tighten (raise) to detect smaller jumps as octave flips

---

## Outlier Rejection (P1-D)

### outlierStdDevThreshold

- **Range**: [1.5, 3.0]
- **Default**: 2.0
- **Effect**: Standard deviation threshold for outlier rejection
- **Tuning**:
  - Lower (1.5σ): Aggressive outlier rejection (may reject valid beats)
  - Higher (3.0σ): Lenient outlier rejection (may accept outliers)

### outlierMinConfidence

- **Range**: [0.1, 0.5]
- **Default**: 0.3
- **Effect**: Minimum confidence to enable outlier rejection
- **Tuning**: Raise to delay outlier rejection until more confident

---

## Onset Strength Weighting (P1-A)

### onsetStrengthWeightBase

- **Range**: [0.5, 2.0]
- **Default**: 1.0
- **Effect**: Base weight for onset strength
- **Tuning**: Leave at 1.0 unless scaling entire weight range

### onsetStrengthWeightScale

- **Range**: [0.1, 1.0]
- **Default**: 0.5
- **Effect**: Scale factor for onset strength (1.0-3.5× range)
- **Tuning**:
  - Increase to amplify strong beats more (sharper density peaks)
  - Decrease for more uniform weighting (flatter density buffer)

---

## Conditional Octave Voting (P1-B)

### octaveVotingConfThreshold

- **Range**: [0.1, 0.5]
- **Default**: 0.3
- **Effect**: Confidence threshold - vote octaves only below this
- **Tuning**:
  - Lower to stop octave voting sooner (faster lock, less octave confusion)
  - Higher to continue octave voting longer (slower lock, more flexibility)

---

## Interval Validation

### periodAlpha

- **Range**: [0.05, 0.3]
- **Default**: 0.15
- **Effect**: EMA alpha for period estimation
- **Tuning**: Similar to `bpmAlpha` - controls period update speed

### periodInitSec

- **Range**: [0.3, 1.0]
- **Default**: 0.5
- **Effect**: Initial period estimate (0.5 sec = 120 BPM)
- **Tuning**: Set based on expected initial tempo

---

## K1 Front-End Initialization

### k1BaselineInit

- **Range**: [0.5, 2.0]
- **Default**: 1.0
- **Effect**: K1 normalized baseline initialization (novelty ≈ 1.0)
- **Tuning**: Match to typical K1 novelty range (usually 1.0-6.0)

### k1BaselineAlpha

- **Range**: [0.01, 0.2]
- **Default**: 0.05
- **Effect**: K1 baseline adaptation alpha (5% new, 95% history)
- **Tuning**: Similar to `baselineAlpha` - controls baseline update speed

### k1BaselineCheckThreshold

- **Range**: [0.05, 0.5]
- **Default**: 0.1
- **Effect**: Threshold to detect legacy baselines (< 0.1 = reinit)
- **Tuning**: Generally leave at default (migration safety mechanism)

---

## Peak Gating

### peakGatingCapMultiplier

- **Range**: [1.2, 3.0]
- **Default**: 1.5
- **Effect**: Cap peak contributions to prevent baseline contamination
- **Tuning**:
  - Lower (1.2×): More aggressive peak gating (baseline adapts slower during peaks)
  - Higher (3.0×): Less aggressive peak gating (baseline tracks peaks more)

---

## Onset Strength Limits

### onsetStrengthMin

- **Range**: [0.0, 1.0]
- **Default**: 0.0
- **Effect**: Minimum onset strength (clamped)
- **Tuning**: Generally leave at 0.0

### onsetStrengthMax

- **Range**: [3.0, 10.0]
- **Default**: 5.0
- **Effect**: Maximum onset strength (clamped)
- **Tuning**: Increase if very strong beats are being clamped

---

## Triangular Kernel Weights

### kernelWeightCenter

- **Range**: [0.5, 1.0]
- **Default**: 1.0
- **Effect**: Weight for center bin in triangular kernel
- **Tuning**: Generally leave at 1.0 (full weight for exact BPM)

### kernelWeightPlus1

- **Range**: [0.2, 0.8]
- **Default**: 0.5
- **Effect**: Weight for ±1 bin
- **Tuning**: Increase for more spread-out votes (smoother density buffer)

### kernelWeightPlus2

- **Range**: [0.1, 0.5]
- **Default**: 0.25
- **Effect**: Weight for ±2 bin
- **Tuning**: Increase for more spread-out votes (smoother density buffer)

---

## Spectral Weights

### spectralWeights[8]

- **Range**: [0.0, 2.0] per band
- **Default**: `{1.2, 1.1, 1.0, 0.8, 0.7, 0.5, 0.5, 0.5}`
- **Effect**: Per-band weights for spectral flux calculation (8 bands)
- **Tuning**:
  - **Band 0-1 (low)**: Increase for bass-heavy detection
  - **Band 2-3 (mid)**: Increase for vocal/melodic detection
  - **Band 4-7 (high)**: Increase for hi-hat/cymbal detection
  - **Disparity**: Current 2.4× disparity (1.2÷0.5) balances bass vs treble
    - Reduce disparity (e.g., 1.5×) for more uniform sensitivity
    - Increase disparity (e.g., 4.0×) for strong bass bias

---

## Tuning Workflow

### 1. Baseline Tuning (Start Here)

1. Set BPM range to match your music genre (`minBpm`, `maxBpm`)
2. Adjust onset sensitivity (`onsetThreshK`) - watch for ghost beats vs missed beats
3. Set refractory period (`refractoryMs`) to prevent subdivisions
4. Tune baseline adaptation (`baselineAlpha`) for your music's dynamic range

### 2. Lock Acquisition Tuning

1. Adjust confidence rise/fall (`confRise`, `confFall`) for lock speed
2. Set lock threshold (`lockThreshold`) for stability vs responsiveness
3. Tune BPM smoothing (`bpmAlpha`) for jitter vs tracking speed
4. Configure reset mechanisms (`lowConfThreshold`, `lowConfResetTimeSec`)

### 3. Advanced Tuning

1. PLL gains (`pllKp`, `pllKi`) for phase alignment
2. Consistency boost (`consistencyBoostMultiplier`) for faster lock
3. Outlier rejection (`outlierStdDevThreshold`) for stability
4. Spectral weights (`spectralWeights`) for frequency-specific sensitivity

### 4. Validation

Use these metrics (from `diagnostics_` struct) to validate tuning:

- **Lock time**: `lockTimeMs` (target: <2 seconds)
- **BPM jitter**: `bpmJitter` (target: <1 BPM RMS after lock)
- **Phase jitter**: `phaseJitter` (target: <5 ms RMS)
- **Octave flips**: `octaveFlips` (target: 0 after lock)
- **Interval rejection rate**: `intervalsRejected / (intervalsValid + intervalsRejected)` (target: <50%)

---

## Common Issues & Solutions

### Issue: Detecting hi-hats as beats

**Solution**: Increase `refractoryMs` to 250-300 ms

### Issue: Missing beats in quiet passages

**Solution**: Decrease `onsetThreshK` to 1.4-1.6

### Issue: Slow lock acquisition (>3 seconds)

**Solution**:
1. Increase `confRise` to 0.15-0.2
2. Increase `consistencyBoostMultiplier` to 4.0-5.0
3. Decrease `lockThreshold` to 0.4

### Issue: BPM oscillates after lock

**Solution**:
1. Decrease `bpmAlpha` to 0.05
2. Decrease `pllKi` to 0.005

### Issue: Phase drifts over time

**Solution**:
1. Increase `pllKp` to 0.15-0.2
2. Increase `pllKi` to 0.015-0.02

### Issue: False resets during breakdowns

**Solution**:
1. Increase `lowConfResetTimeSec` to 15-20 seconds
2. Decrease `lowConfThreshold` to 0.1

### Issue: Octave confusion (locks to half/double tempo)

**Solution**:
1. Decrease `octaveVotingConfThreshold` to 0.2 (stop octave voting sooner)
2. Adjust `spectralWeights` to favor bass (increase band 0-1 weights)

---

## References

- **P0 Fixes**: Basic onset timing fixes (refractory period, interval validation)
- **P1 Fixes**: Advanced onset fixes (strength weighting, conditional octave voting, harmonic filtering, outlier rejection)
- **K1 Front-End**: K1 normalized novelty integration
- **Phase 2 Integration**: Dual-bank AudioFeatureFrame processing

**For implementation details, see**: `src/audio/tempo/TempoTracker.h` and `TempoTracker.cpp`
