# Tempo Tracker Acceptance Tests

This document defines pass/fail criteria for validating the beat/tempo tracker after fixes.

## Test Environment

- **Sample Rate:** 16 kHz
- **Hop Rate:** 125 Hz (8ms per hop)
- **Expected Tempo Range:** 60-180 BPM
- **Target Lock Time:** <5 seconds (ideal: <2 seconds)

## Test Cases

### Test 1: Click Track 152 BPM

**Setup:**
- Play a click track at exactly 152 BPM
- Clear, consistent beat with no transients

**Pass Criteria:**
- Lock confidence > 0.7 within 5-8 seconds
- BPM estimate within ±1.0 of 152 BPM (151-153 BPM)
- Lock status transitions from `locked=0` to `locked=1`
- Density peak bin stabilizes at bin 92 (60 + 92 = 152 BPM)
- Density peak value > 0.1 (accumulates over time)

**Fail Criteria:**
- Confidence never exceeds 0.5
- BPM estimate stuck at default (63.0) or wrong value
- Lock never achieved after 10 seconds
- Density peak value remains at 0.00

**Validation:**
- Check serial logs for `tempo_summary` messages
- Verify `conf` field rises above 0.7
- Verify `bpm` field stabilizes at 151-153
- Verify `locked` field transitions to 1
- Verify `density_peak_val` > 0.1

---

### Test 2: Silence

**Setup:**
- No audio input (silence or microphone muted)
- System should not lock to noise

**Pass Criteria:**
- Confidence stays < 0.1 throughout test
- No lock achieved (`locked=0`)
- BPM estimate may drift but should not lock
- Density peak value remains low (< 0.01)

**Fail Criteria:**
- Confidence rises above 0.1
- Lock achieved on silence
- BPM estimate locks to spurious value

**Validation:**
- Check serial logs for `tempo_summary` messages
- Verify `conf` field stays < 0.1
- Verify `locked` field remains 0
- Verify `density_peak_val` stays low

---

### Test 3: Hat-Heavy Loop

**Setup:**
- Play music with heavy hi-hat/cymbal content
- Minimal kick drum presence
- Tempo should be known (e.g., 120 BPM)

**Pass Criteria (Option A - No Kick):**
- No lock achieved if kick is absent
- Confidence stays low (< 0.3)
- System correctly rejects hat transients

**Pass Criteria (Option B - Kick Present):**
- Lock achieved but slower than click track test (may take 10-15 seconds)
- Once locked, BPM estimate is stable and correct
- Confidence > 0.5 once locked

**Fail Criteria:**
- Locks to hat transients (wrong BPM, typically 2× or 4× actual tempo)
- Confidence rises quickly on hats alone
- BPM estimate jumps erratically

**Validation:**
- Check serial logs for `interval_rejected` messages (should see many "too_fast" rejections)
- Verify `onsets_total` is high but `intervals_valid` is low (rejection working)
- If kick present, verify eventual lock to correct BPM

---

## Performance Metrics

### Onset Detection

**Target Metrics:**
- Onset rate: ~2-6 onsets/second (beat-like, not transient-like)
- Valid interval rate: ~2.5 intervals/second (for 152 BPM)
- Rejection rate: <50% (down from 99.7%)

**Validation:**
- Check `onsets_total` in tempo_summary (should be ~250-750 over 125 seconds)
- Check `intervals_valid` (should be ~312 over 125 seconds for 152 BPM)
- Check `rejection_rate_pct` (should be <50%)

### Density Buffer

**Target Metrics:**
- Peak value: >0.1 after 10-20 seconds
- Peak bin: Stabilizes at correct BPM bin
- Second peak: <0.5 × peak (good separation)

**Validation:**
- Check `density_peak_val` (should rise from 0.00 to >0.1)
- Check `density_peak_bin` (should match expected BPM)
- Check `density_second_peak` (should be <0.5 × peak_val)

### Tempo Estimation

**Target Metrics:**
- BPM accuracy: ±1 BPM of actual tempo
- Lock time: <5 seconds (ideal: <2 seconds)
- Confidence: >0.7 once locked
- Jitter: <±1 BPM once locked

**Validation:**
- Check `bpm` field (should match actual tempo ±1 BPM)
- Check `lock_time_ms` (should be <5000ms)
- Check `conf` field (should be >0.7 once locked)
- Check `bpm_jitter` (should be <1.0 once locked)

---

## Debug Logging

### Key Log Messages

1. **K1_TEMPO_TRACKER_V2** - Confirms K1 path is active (Phase A verification)
2. **k1_novelty** - Shows K1 novelty flux values (should be ~1-6 for music)
3. **peak_check** - Shows onset detection threshold comparison
4. **interval_valid** - Shows valid intervals being added to density buffer
5. **density_update** - Shows density buffer votes being added
6. **density_decay** - Shows density buffer decay (should be 0.995 per hop)
7. **tempo_summary** - Shows overall tempo tracking state

### Log Precision

- Density values use `%.5f` precision (Phase E fix)
- This allows seeing accumulation from 0.00001 to 0.1

---

## Manual Test Procedure

1. **Build firmware** with all fixes applied
2. **Flash to ESP32-S3**
3. **Open serial monitor** (115200 baud)
4. **Run Test 1** (click track 152 BPM):
   - Play click track
   - Wait 10 seconds
   - Check logs for lock achievement
5. **Run Test 2** (silence):
   - Mute audio or disconnect microphone
   - Wait 10 seconds
   - Verify no lock
6. **Run Test 3** (hat-heavy loop):
   - Play hat-heavy music
   - Wait 15 seconds
   - Check if lock occurs and if BPM is correct

---

## Expected Results After All Fixes

### Before Fixes (Current State)
- Onset rate: 421/sec (over-detection)
- Valid interval rate: 1.1/sec (too sparse)
- Density peak: 0.00 (empty)
- BPM estimate: 63.0 (stuck at default)
- Confidence: 0.00 (never rises)
- Lock: Never achieved

### After Fixes (Target State)
- Onset rate: ~2-6/sec (beat-like)
- Valid interval rate: ~2.5/sec (matches tempo)
- Density peak: >0.1 (accumulates)
- BPM estimate: 152.0 ±1 BPM (correct)
- Confidence: >0.7 (enables lock)
- Lock time: <5 seconds (target: <2s)

---

## Regression Tests

After fixes, verify these don't regress:

1. **Audio processing** - No dropouts or glitches
2. **CPU usage** - Stays within budget (<1.1ms per hop)
3. **Memory usage** - No heap leaks
4. **Beat tick generation** - Consistent timing (no 4× corruption)
5. **Phase jitter** - Low once locked (<10ms)

---

## Notes

- These are **manual tests**, not automated
- Tests should be run in order (Test 1 → Test 2 → Test 3)
- Each test should run for at least 10 seconds
- Logs should be captured for analysis
- Pass/fail is determined by criteria above, not "vibes"

