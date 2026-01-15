# TempoTracker Hardware Verification Checklist

**Date:** 2026-01-07  
**Purpose:** Verify onset-timing tempo tracker works correctly on hardware (no harmonic aliasing)

---

## Pre-Flight Checks

- [x] Code compiles successfully (`pio run -e esp32dev_audio`)
- [x] Documentation cleanup completed (AudioNode.cpp comments updated)
- [ ] Hardware connected and serial port identified

---

## Build & Upload

```bash
cd firmware/v2

# Build firmware
pio run -e esp32dev_audio

# Upload to hardware (replace port with your device)
pio run -e esp32dev_audio -t upload --upload-port /dev/cu.usbmodem2101

# Monitor serial output
pio device monitor -b 115200
```

**Expected:** Build succeeds, upload completes, serial monitor shows boot messages.

---

## Test Cases

### Test 1: Steady Electronic Music (120-140 BPM)

**Setup:**
- Play electronic track with steady kick drum
- Track should have clear, distinct beats
- Reference BPM: 120-140 BPM (verify with external tool if needed)

**Expected Behavior:**
- BPM locks to actual tempo within 5-10 seconds
- Confidence rises to >0.7 within 5 onsets
- BPM variance <2 BPM after lock
- **NO harmonic jumping** (e.g., 120 BPM should NOT jump to 60 or 240)

**Pass Criteria:**
- ✅ BPM stable (no 155→77→81 jumping)
- ✅ Confidence >0.7
- ✅ LED effects sync correctly to beats

**Serial Output to Watch:**
```
BPM: 120.0 → 120.1 → 119.9 (stable, small variance)
Confidence: 0.0 → 0.5 → 0.8 → 0.9 (rising)
```

---

### Test 2: Rock Music (Syncopated, 100-130 BPM)

**Setup:**
- Play rock track with syncopated rhythm
- May have irregular beat patterns

**Expected Behavior:**
- BPM locks to average tempo
- Confidence 0.5-0.7 (lower than steady electronic)
- BPM remains stable despite syncopation
- **NO lock to 2× tempo** (e.g., 120 BPM should NOT lock to 240 BPM)

**Pass Criteria:**
- ✅ BPM stable (no harmonic doubling)
- ✅ Confidence 0.5-0.7 (reasonable for syncopated music)
- ✅ Effects still sync (may be less precise)

---

### Test 3: Ambient/Sparse Music

**Setup:**
- Play ambient track with sparse, irregular beats
- May have long gaps between beats

**Expected Behavior:**
- BPM holds last estimate when beats are sparse
- Confidence decays during gaps
- Confidence rises when beats resume
- BPM doesn't jump wildly

**Pass Criteria:**
- ✅ BPM holds during gaps (doesn't reset to 120)
- ✅ Confidence decays appropriately
- ✅ No false tempo locks

---

### Test 4: Silence Dropout

**Setup:**
- Play music for 10 seconds (let BPM lock)
- Then play 5 seconds of silence
- Resume music

**Expected Behavior:**
- Confidence decays from 1.0 → 0.0 over ~4 seconds (confFall = 0.8/sec)
- BPM holds at last estimate during silence
- Phase continues advancing (beat clock keeps running)
- Confidence rises again when music resumes

**Pass Criteria:**
- ✅ Confidence decays smoothly (not instant drop)
- ✅ BPM holds during silence
- ✅ Phase continues (beat tick still fires)

---

### Test 5: Tempo Change (120→140 BPM)

**Setup:**
- Play track at 120 BPM for 10 seconds
- Switch to track at 140 BPM

**Expected Behavior:**
- BPM smoothly transitions (EMA smoothing)
- Transition completes within 10 onsets
- Confidence maintains >0.5 during transition
- No sudden jumps or oscillations

**Pass Criteria:**
- ✅ Smooth transition (no sudden jumps)
- ✅ Completes within 10 onsets
- ✅ Confidence remains stable

---

## Failure Modes to Watch For

### ❌ Harmonic Aliasing (CRITICAL - This is what we're fixing)

**Symptom:**
```
BPM: 155.0 → 77.0 → 81.0 → 155.0 (jumping between harmonics)
```

**Action:** This indicates the fix didn't work. Check:
- Is onset detection firing? (check serial for onset events)
- Are inter-onset intervals being calculated correctly?
- Is the period clamping working? (minP/maxP in updateBeat)

---

### ⚠️ Confidence Never Rises

**Symptom:**
```
Confidence: 0.0 → 0.05 → 0.08 → 0.05 (stays low)
```

**Possible Causes:**
- Onset detection too sensitive (false positives)
- Onset detection not sensitive enough (misses beats)
- Inter-onset intervals outside plausible range

**Debug Steps:**
- Check onset detection threshold (tuning_.onsetThreshK)
- Verify onsets are firing (add debug logging)
- Check refractory period (tuning_.refractoryMs)

---

### ⚠️ Phase Drifts Out of Sync

**Symptom:**
- Beat ticks don't align with actual beats
- Effects are consistently early or late

**Possible Causes:**
- Phase correction too weak (lockStrength too low)
- Timestamp accuracy issues
- Phase wrap-around logic bug

**Debug Steps:**
- Increase lockStrength (0.35 → 0.5)
- Verify esp_timer_get_time() is monotonic
- Check phase error calculation

---

## Serial Monitoring Commands

If the firmware has serial commands for tempo debugging:

```bash
# Check current tempo state
s  # (if available - status command)

# List effects (verify MusicalGrid still works)
l  # (if available)

# Validate tempo tracker
validate tempo  # (if available)
```

---

## Success Criteria Summary

### Must Pass (Critical)
- ✅ No harmonic aliasing (155→77→81 jumping)
- ✅ BPM locks to actual tempo
- ✅ Confidence >0.7 for steady music
- ✅ LED effects sync correctly

### Should Pass (Important)
- ✅ Smooth tempo transitions
- ✅ Appropriate confidence for different music styles
- ✅ BPM holds during silence
- ✅ No false positives

### Nice to Have
- ✅ Fast lock time (<5 seconds)
- ✅ Low BPM variance (<2 BPM after lock)

---

## Post-Verification

If all tests pass:

1. **Update Plan:** Mark hardware verification complete in `~/.claude/plans/zippy-puzzling-lampson.md`
2. **Document Results:** Record BPM stability, confidence behavior, any edge cases
3. **Close Out:** Mark TempoTracker fix as fully complete

If tests fail:

1. **Document Failure:** Record exact symptoms, BPM values, confidence behavior
2. **Debug:** Check onset detection, beat tracking logic, tuning parameters
3. **Iterate:** Adjust tuning parameters or fix bugs as needed

---

## Tuning Parameters Reference

If adjustments are needed, these are in `TempoTrackerTuning`:

```cpp
// Beat tracking
minBpm = 60.0f;              // Minimum BPM
maxBpm = 180.0f;             // Maximum BPM
lockStrength = 0.35f;        // Phase correction gain
confRise = 0.4f;             // Confidence rise per good onset
confFall = 0.8f;             // Confidence fall per second

// Onset detection
onsetThreshK = 2.5f;         // Multiplier over baseline for onset
refractoryMs = 150;          // Minimum time between onsets (ms)
baselineAlpha = 0.08f;       // Baseline smoothing (EMA alpha)
```

---

**Good luck! 🎵**

