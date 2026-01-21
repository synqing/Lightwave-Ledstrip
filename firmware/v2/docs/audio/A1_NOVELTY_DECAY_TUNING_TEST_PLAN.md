# A1 Novelty Decay Tuning - Test Plan

**Task**: Phase A Quick Win - Novelty Decay Tuning
**Date**: 2026-01-07
**Status**: Implementation Complete, Testing Required
**Risk**: Low
**Complexity**: Low
**Duration**: 2 days

---

## Changes Implemented

### 1. TempoTracker Novelty Decay
**File**: `src/audio/tempo/TempoTracker.h`

```cpp
// BEFORE: constexpr float NOVELTY_DECAY = 0.999f;
// AFTER:  constexpr float NOVELTY_DECAY = 0.996f;
```

**Impact**: Faster decay of stale beat events in history buffers (both spectral and VU curves)
**Expected Result**: Reduced ghost beat influence, ~15% improvement in tempo lock stability

### 2. TempoTracker Hysteresis Thresholds
**File**: `src/audio/tempo/TempoTracker.h`

```cpp
// BEFORE: float hysteresisThreshold = 1.1f;  // 10% advantage required
// AFTER:  float hysteresisThreshold = 1.08f; // 8% advantage required

// BEFORE: uint8_t hysteresisFrames = 5;      // Consecutive frames to switch
// AFTER:  uint8_t hysteresisFrames = 4;      // Faster lock acquisition
```

**Impact**: Faster tempo lock-on by reducing switching resistance
**Expected Result**: Beat detection confidence >0.7 within 4 bars for steady-tempo tracks

### 3. MusicalGrid Confidence and Phase Correction
**File**: `src/audio/contracts/MusicalGrid.h`

```cpp
// BEFORE: float confidenceTau = 1.00f;
// AFTER:  float confidenceTau = 0.75f;

// BEFORE: float phaseCorrectionGain = 0.35f;
// AFTER:  float phaseCorrectionGain = 0.40f;
```

**Impact**:
- Faster confidence response to tempo changes
- Stronger phase correction for better beat alignment

**Expected Result**: BPM stability variance <2% after phase lock

### 4. ControlBus Attack/Release Envelopes
**File**: `src/audio/contracts/ControlBus.h`

```cpp
// BEFORE: m_band_attack = 0.15f, m_band_release = 0.03f
// AFTER:  m_band_attack = 0.18f, m_band_release = 0.04f

// BEFORE: m_heavy_band_attack = 0.08f, m_heavy_band_release = 0.015f
// AFTER:  m_heavy_band_attack = 0.10f, m_heavy_band_release = 0.020f
```

**Impact**: Smoother feature transitions with better balance between responsiveness and stability
**Expected Result**: Smoother visual transitions without flicker

---

## Test Suite

### Test Environment
- **Hardware**: ESP32-S3-DevKitC-1 @ 240MHz
- **Firmware**: esp32dev_audio build
- **Serial Monitor**: 115200 baud
- **Duration**: 5 music styles × 30 seconds each = 2.5 minutes minimum

### Test Sequence

#### 1. EDM (Electronic Dance Music)
**Track**: Steady 128 BPM, 4/4 time signature
**Duration**: 30 seconds

**Metrics to Monitor**:
```
Serial Output:
- tempo_confidence: Should reach >0.7 within 4 bars (~8 seconds at 128 BPM)
- bpm_smoothed: Should stabilize at 128 ± 2 BPM
- beat_tick: Should align with audible beats (visual inspection)
```

**Success Criteria**:
- [ ] Confidence >0.7 within 8 seconds
- [ ] BPM variance <2% after lock (126-130 BPM)
- [ ] No false beat_ticks during sustained sections

#### 2. Vocal Pop
**Track**: Variable dynamics, 90-110 BPM, 4/4 time signature
**Duration**: 30 seconds

**Metrics to Monitor**:
```
- tempo_confidence: Should recover quickly after vocal breaks
- bpm_smoothed: Should track tempo through dynamic changes
- beat_strength: Should correlate with kick drum hits
```

**Success Criteria**:
- [ ] Confidence recovers >0.5 within 2 bars after breaks
- [ ] BPM tracks correctly through verse/chorus transitions
- [ ] Beat strength modulates correctly with dynamics

#### 3. Ambient/Atmospheric
**Track**: Minimal percussion, slow tempo (60-80 BPM)
**Duration**: 30 seconds

**Metrics to Monitor**:
```
- tempo_confidence: May be low (<0.3) - this is CORRECT
- silence_detected: Should NOT trigger false positives
- beat_tick: Should be sparse or absent
```

**Success Criteria**:
- [ ] No false beats on ambient drone sections
- [ ] Confidence remains low (<0.3) - indicates correct rejection
- [ ] Silence detection doesn't trigger prematurely

#### 4. Instrumental (Jazz/Classical)
**Track**: Complex rhythms, variable tempo, syncopation
**Duration**: 30 seconds

**Metrics to Monitor**:
```
- tempo_confidence: May fluctuate (0.3-0.7)
- bpm_smoothed: Should follow tempo changes gradually
- beat_tick: Should align with downbeats, not every subdivision
```

**Success Criteria**:
- [ ] Tempo tracking follows conductor/drummer
- [ ] No excessive jitter in BPM estimate
- [ ] Beat ticks align with strong beats (1 and 3 in 4/4)

#### 5. Silence
**Track**: No audio input (mute/zero signal)
**Duration**: 30 seconds

**Metrics to Monitor**:
```
- tempo_confidence: Should decay to ~0.0
- silence_detected: Should be TRUE
- beat_tick: Should NOT fire
```

**Success Criteria**:
- [ ] Zero false positives (no beat_ticks)
- [ ] Confidence decays to 0 within ~10 seconds
- [ ] silence_detected flag is TRUE

---

## Performance Regression Test

### CPU Overhead Measurement

**Baseline** (pre-A1):
```
Frame Time Breakdown (120 FPS target = 8.33ms budget):
- Audio processing: ~1.5-2.0 ms
- Effect rendering: 2.0-4.0 ms
- FastLED.show(): 1.5-2.0 ms
- Total: 5.0-8.0 ms
```

**Test Procedure**:
1. Flash firmware with A1 optimizations
2. Run serial monitor with `FEATURE_PERFORMANCE_MONITOR=1`
3. Monitor frame timing during 5-style test suite
4. Compare CPU overhead vs. baseline

**Success Criteria**:
- [ ] CPU overhead increase <5% from baseline
- [ ] Frame time remains <8.33ms at 120 FPS
- [ ] No dropped frames during steady-state operation

**Performance Monitor Commands**:
```
Serial Menu:
- 'p' - Toggle performance stats
- 'm' - Show memory usage
```

Expected output:
```
FPS: 120.0 | Frame: 8.2ms | CPU: 73% | Heap: 245KB free
```

---

## Validation Checklist

### Pre-Test Setup
- [ ] Firmware flashed successfully (`pio run -e esp32dev_audio -t upload`)
- [ ] Serial monitor connected at 115200 baud
- [ ] Audio source configured (line-in or I2S microphone)
- [ ] 5 test tracks prepared (EDM, Vocal, Ambient, Instrumental, Silence)

### During Testing
- [ ] Record serial output for each test track
- [ ] Note timestamps when confidence threshold reached
- [ ] Visually verify beat_tick alignment with audio
- [ ] Monitor for any crashes or restarts

### Post-Test Analysis
- [ ] Compare confidence lock-on times (should be faster)
- [ ] Analyze BPM stability variance (should be <2%)
- [ ] Verify zero false positives on silence/ambient
- [ ] Confirm CPU overhead <5% increase

---

## Expected Results Summary

| Metric | Before A1 | After A1 | Target |
|--------|-----------|----------|--------|
| Confidence lock-on time (EDM) | ~12 seconds | ~8 seconds | <10 seconds |
| BPM variance after lock | ±3 BPM | ±2 BPM | <2% |
| False positives (silence) | Occasional | Zero | Zero |
| CPU overhead | Baseline | Baseline + <5% | <5% increase |
| Frame time (120 FPS) | 5-8 ms | 5-8.4 ms | <8.33 ms |

---

## Rollback Plan

If validation fails any critical success criteria:

1. **Revert novelty decay**:
   ```cpp
   // TempoTracker.h line 91
   constexpr float NOVELTY_DECAY = 0.999f; // Restore original
   ```

2. **Revert hysteresis thresholds**:
   ```cpp
   // TempoTracker.h lines 46-48
   float hysteresisThreshold = 1.1f;
   uint8_t hysteresisFrames = 5;
   ```

3. **Revert MusicalGrid tuning**:
   ```cpp
   // MusicalGrid.h lines 45-48
   float confidenceTau = 1.00f;
   float phaseCorrectionGain = 0.35f;
   ```

4. **Revert ControlBus envelopes**:
   ```cpp
   // ControlBus.h lines 326-329
   float m_band_attack = 0.15f;
   float m_band_release = 0.03f;
   float m_heavy_band_attack = 0.08f;
   float m_heavy_band_release = 0.015f;
   ```

5. **Rebuild and re-flash**:
   ```bash
   pio run -e esp32dev_audio -t upload
   ```

---

## Next Steps

After successful validation:
1. Document final results in this file
2. Update `.claude/harness/feature_list.json` with PASSING status
3. Commit changes with evidence
4. Proceed to Phase B (Wave Propagation Optimization)

---

## Test Results

### EDM Test
**Date**: _____________
**Track**: _____________
**Results**:
```
Confidence lock-on: _____ seconds
BPM stability: _____ ± _____ BPM
False positives: _____
Notes: _____________
```

### Vocal Pop Test
**Date**: _____________
**Track**: _____________
**Results**:
```
Confidence recovery: _____ seconds
BPM tracking: _____
Beat strength correlation: _____
Notes: _____________
```

### Ambient Test
**Date**: _____________
**Track**: _____________
**Results**:
```
False positives: _____
Confidence level: _____
Silence detection: _____
Notes: _____________
```

### Instrumental Test
**Date**: _____________
**Track**: _____________
**Results**:
```
Tempo tracking: _____
BPM jitter: _____
Beat alignment: _____
Notes: _____________
```

### Silence Test
**Date**: _____________
**Results**:
```
False positives: _____
Confidence decay: _____ seconds
silence_detected flag: _____
Notes: _____________
```

### Performance Regression
**Results**:
```
CPU overhead: _____ %
Frame time: _____ ms
Dropped frames: _____
Notes: _____________
```

---

## Approval

**Implementation Complete**: ✓ 2026-01-07
**Testing Complete**: _______________
**Validation Passed**: _______________
**Approved By**: _______________
