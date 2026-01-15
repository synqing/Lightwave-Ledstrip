# K1 Front-End Acceptance Tests

## Front-End Tests

### Test 1: Idle Room
**Setup**: Record 10 seconds of quiet room noise
**Expected**:
- Novelty flux: < 0.1 (quiet)
- Noise floor: Converges within 10 seconds
- Harmony bins: Low magnitude, stable
- Chroma: Unstable (no clear key)

**Pass Criteria**: Novelty stays low, noise floor converges

### Test 2: Kick Loop
**Setup**: Play 4/4 kick drum loop at known BPM (e.g., 128 BPM)
**Expected**:
- Novelty flux: Peaks align with kick transients
- Rhythm bins: Strong response in low-frequency bins (35-120 Hz)
- Tempo tracker: Locks to correct BPM within 2-5 seconds

**Pass Criteria**: Novelty peaks match kick timing, tempo locks correctly

### Test 3: Hat-Only
**Setup**: Play hi-hat pattern without kick/snare
**Expected**:
- Novelty flux: Detects hat transients but does not dominate
- Beat tracker: Does not lock to hat-only pattern (confidence stays low)
- Rhythm bins: Response in high-frequency bins (2-8 kHz) but not overwhelming

**Pass Criteria**: Hats detected but do not dominate beat evidence

### Test 4: Sustained Chord
**Setup**: Play sustained major/minor chord
**Expected**:
- Chroma: Stable distribution (high stability metric)
- Key clarity: High (clear pitch center)
- Harmony bins: Strong response in semitone bins matching chord notes
- Novelty: Low (no transients)

**Pass Criteria**: Chroma stability > 0.7, key clarity > 0.5

## Tempo Tracker Tests

### Test 5: EDM Lock Time
**Setup**: Play typical EDM track (128-140 BPM)
**Expected**:
- Lock time: ≤2 seconds typical, ≤5 seconds worst-case
- BPM accuracy: Within ±1 BPM of actual tempo
- Confidence: Rises to >0.5 within lock time

**Pass Criteria**: Locks within time limits, accurate BPM

### Test 6: Jitter Test
**Setup**: Play steady tempo track for 30 seconds after lock
**Expected**:
- BPM jitter: ≤±1 BPM once locked
- Phase jitter: ≤±10 ms
- Confidence: Stable >0.5

**Pass Criteria**: Jitter within limits

### Test 7: Octave Flip Test
**Setup**: Play steady tempo track for 2 minutes
**Expected**:
- Octave flips: ≤1 per minute in steady sections
- BPM stability: No sudden 2× or 0.5× jumps

**Pass Criteria**: Minimal octave flips

### Test 8: Silence/Speech
**Setup**: Record silence, then speech (no music)
**Expected**:
- Confidence: <0.1 and stays low
- Novelty: Low (no strong transients)
- Tempo tracker: Does not lock

**Pass Criteria**: Confidence stays low, no false locks

## Native Determinism Test

### Test 9: Determinism
**Setup**: Process same WAV file multiple times
**Expected**:
- Identical outputs across runs
- No system timer dependencies
- Sample counter produces consistent timestamps

**Pass Criteria**: Bit-identical outputs

## Implementation Status
- [ ] Test framework created
- [ ] Tests implemented
- [ ] Results documented

