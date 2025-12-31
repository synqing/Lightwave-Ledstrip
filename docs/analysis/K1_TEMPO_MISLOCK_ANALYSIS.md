# K1 Beat Tracker Tempo Mis-Lock Analysis & Fix

## Problem Statement

The K1 beat tracker was locking onto ~123-129 BPM with 100% confidence on a reference track verified at 138 BPM. The track has clear, distinct percussion without vocals, making this an "easy" case that should lock correctly.

**Evidence from logs:**
- `BPM: 123.2 | Conf: 1.00 | DConf: 0.12 | Phase: 0.49 | LOCKED`
- `Top3: 88(1.00) 88(0.99) 89(0.98)` (Stage-2 candidates, not the winner)
- `flux=1.000` (hard saturation)
- Stage-2 Top3 never showed ~138 BPM

## Root Causes Identified

### 1. Novelty Saturation & Poor Scaling (CRITICAL)
- **Location**: `v2/src/audio/AudioActor.cpp` (flux computation) + `v2/src/audio/k1/K1Pipeline.cpp` (flux→z mapping)
- **Problem**: Flux hard-clamped to [0,1], then linearly mapped to fake "z-score" [-6,+6] with assumed midpoint at 0.5
- **Impact**: Destroyed periodic structure, created spurious resonator peaks, made Stage-2 miss true tempo

### 2. Confidence Semantics Are Broken (CRITICAL)
- **Location**: `v2/src/audio/k1/K1TactusResolver.cpp` (confidence assignment)
- **Problem**: `confidence = clamp01(family_score)`, but `family_score` can exceed 1.0 (additive bonuses, density multiplier)
- **Impact**: Always reported 1.00 confidence even when density agreement was low (DConf: 0.12)

### 3. Misleading Debug Output (HIGH)
- **Location**: `v2/src/audio/k1/K1DebugCli.cpp`
- **Problem**: `k1` printed Stage-4 BPM (from beat clock) alongside Stage-2 Top3 (from resonators), creating confusion
- **Impact**: Could not diagnose which stage was failing

### 4. Tactus Prior Bias (MEDIUM)
- **Location**: `v2/src/audio/k1/K1TactusResolver.cpp:109-112`
- **Problem**: Gaussian prior centered at 120 BPM with σ=30 gave ~14% advantage to 129.2 over 138
- **Impact**: Once wrong tempo appeared, prior reinforced it

### 5. Stability Bonus Lock-In (MEDIUM)
- **Location**: `v2/src/audio/k1/K1TactusResolver.cpp:215-221`
- **Problem**: +0.25 additive bonus for candidates within ±2 BPM of current lock
- **Impact**: Made wrong tempo "sticky", resisted correction

### 6. First Lock Is Immediate (MEDIUM)
- **Location**: `v2/src/audio/k1/K1TactusResolver.cpp:302-323`
- **Problem**: First candidate meeting minimum confidence locked immediately
- **Impact**: No verification period, wrong initial lock persisted

## Fixes Applied

### Phase 1: Diagnostic Observability
- **Updated `k1_print_full()`** to show:
  - Stage-3 tactus winner: `Tactus: 123.2 (score=1.23, dconf=0.12)`
  - Stage-2 Top3 with raw magnitudes: `Top3: 88(mag=0.92,raw=1.45) 89(...) 90(...)`
  - Stage-4 beat clock: `Clock: 123.2 phase=0.49`
- **Added verbose diagnostics** (gated by `K1_DEBUG_TACTUS_SCORES`) showing top 5 candidates with BPM, raw magnitude, prior value, family score

### Phase 2: Fix Novelty Scaling
- **Separated flux for K1** from UI flux in `AudioActor.cpp`:
  - UI/effects get hard-clamped flux [0,1]
  - K1 gets soft-clipped flux (compresses beyond 1.0 instead of hard clamp)
- **Replaced linear flux→z mapping** with running-stat normaliser in `K1Pipeline.cpp`:
  - Tracks running mean/variance (EWMA with 2s time constants)
  - Computes true z-score: `(flux - mean) / sqrt(variance)`
  - Clamps to ±4 sigma max
  - Reduces sensitivity to AGC/gating baseline shifts

### Phase 3: Fix Confidence Semantics
- **Redefined confidence derivation** in `K1TactusResolver.cpp`:
  - Changed from `clamp01(family_score)` to `density_conf * (1.0f - 0.2f * (1.0f - density_conf))`
  - Confidence now reflects actual grouped consensus, not clamped composite score
  - `family_score` kept as separate field for debugging

### Phase 4: Reduce Tactus Prior Bias
- **Widened tactus prior sigma**: `ST3_TACTUS_SIGMA` from 30.0 → 40.0
  - Reduces 129.2 vs 138 advantage from ~14% to ~7%
- **Reduced stability bonus**: `ST3_STABILITY_BONUS` from 0.25 → 0.12
  - Reduces lock-in strength

### Phase 5: Add Pending Lock Verification
- **Added lock state machine** with three states:
  - `UNLOCKED`: No tempo detected yet
  - `PENDING`: Initial lock, still verifying (2.5s period)
  - `VERIFIED`: Committed lock, full hysteresis active
- **Verification logic**:
  - Tracks strongest competitor (>5 BPM away, >10% advantage)
  - If competitor sustains for 1.5s during verification, switches to competitor
  - After 2.5s, commits to VERIFIED state
  - Stability bonus only applies in VERIFIED state

## Tuning Parameters

### K1 Config (`v2/src/audio/k1/K1Config.h`)
- `ST3_TACTUS_SIGMA = 40.0f` (was 30.0f)
- `ST3_STABILITY_BONUS = 0.12f` (was 0.25f)
- `LOCK_VERIFY_MS = 2500` (2.5 second verification period)
- `COMPETITOR_THRESHOLD = 1.10f` (10% advantage to reconsider)

### Novelty Normaliser (`v2/src/audio/k1/K1Pipeline.cpp`)
- `NOVELTY_MEAN_TAU = 2.0f` (seconds)
- `NOVELTY_VAR_TAU = 2.0f` (seconds)
- `NOVELTY_CLIP = 4.0f` (±4 sigma max)

### Debug Flags
- `K1_DEBUG_TACTUS_SCORES = 0` (set to 1 to enable verbose scoring diagnostics)

## Testing Recommendations

1. **Reference track (138 BPM)**:
   - Verify Stage-2 shows ~138 BPM candidate
   - Verify Stage-3 locks to ~138 BPM (or at least shows it as strong competitor)
   - Verify confidence reflects density agreement (not always 1.00)

2. **120 BPM track** (near prior center):
   - Should still lock quickly
   - Verification period shouldn't cause issues

3. **Ambiguous polyrhythmic track**:
   - Should show diagnostic output of competing candidates
   - Lock should go to perceptually correct tempo

## Rollback Plan

If changes cause regressions:
1. Set `K1_DEBUG_TACTUS_SCORES 0` to disable verbose logging
2. Revert `ST3_TACTUS_SIGMA` to 30 if new tracks fail
3. Set `LOCK_VERIFY_MS 0` to skip verification (instant lock) - requires code change
4. Revert novelty normaliser to linear mapping if unstable

## Files Modified

- `v2/src/audio/k1/K1DebugCli.cpp` - Updated debug output
- `v2/src/audio/k1/K1Config.h` - Added debug flag, changed σ and stability bonus, added lock verification constants
- `v2/src/audio/k1/K1TactusResolver.h` - Added `LockState` enum, pending lock members
- `v2/src/audio/k1/K1TactusResolver.cpp` - Fixed confidence semantics, added diagnostics, implemented verification state machine
- `v2/src/audio/AudioActor.cpp` - Separated flux for K1 (soft-clipped)
- `v2/src/audio/k1/K1Pipeline.h` - Added running-stat normaliser members
- `v2/src/audio/k1/K1Pipeline.cpp` - Replaced linear flux→z with running-stat normaliser

## Expected Outcomes

After these fixes:
- Novelty signal should be stable and non-saturating
- Confidence should reflect actual agreement (density_conf), not always 1.00
- Debug output should clearly show which stage is making decisions
- Tactus prior bias reduced from ~14% to ~7%
- Wrong initial locks should be caught during 2.5s verification period
- Stability bonus reduced to prevent excessive lock-in

