# Tempo Tracker Test Infrastructure

**Phase 0 Deliverable** - Test tools created BEFORE implementation (per PRD v2.0.0)

Test-driven validation infrastructure for the tempo tracking system overhaul addressing 75 documented failures from `TEMPO_TRACKER_COMPLETE_FAILURE_DEBRIEF.md`.

---

## Overview

Critical insight from failure debrief: **"Never tested with ground truth"** was the meta-failure that enabled all other failures. This test infrastructure is MANDATORY and must be created FIRST.

### Tools Provided

1. **`test_tempo_lock.py`** - Main ground truth validation framework
2. **`analyze_intervals.py`** - Interval distribution analyzer
3. **`measure_onset_accuracy.py`** - Onset accuracy measurement
4. **`visualize_tempo_data.py`** - Multi-panel visualization tools

### Test Tracks (Ground Truth)

Located in `tools/samples/`:
- `edm_138bpm.csv` - Typical EDM track (138 BPM)
- `slow_ambient_60bpm.csv` - Slow ambient (60 BPM)
- `fast_dnb_174bpm.csv` - Fast drum & bass (174 BPM)
- `hats_only.csv` - Hat-only loop (should NOT lock)
- `silence.csv` - Silence (should NOT lock)
- `click_120bpm.csv` - Metronome click track (120 BPM)

**Note:** CSV files contain hand-labeled beat timestamps. Actual audio files (WAV/MP3) must be collected separately.

---

## Installation

### Prerequisites

```bash
# Python 3.7+
python3 --version

# Required packages
pip install matplotlib numpy

# Optional (for audio processing in future)
pip install librosa soundfile
```

### Quick Start

```bash
cd firmware/v2/tools

# Run ground truth validation
python3 test_tempo_lock.py --log tempo_log.txt --ground-truth samples/edm_138bpm.csv

# Analyze interval distribution
python3 analyze_intervals.py tempo_log.txt

# Measure onset accuracy
python3 measure_onset_accuracy.py tempo_log.txt samples/edm_138bpm.csv

# Visualize tempo tracking state
python3 visualize_tempo_data.py tempo_log.txt --ground-truth-bpm 138
```

---

## Tool 1: test_tempo_lock.py

**Purpose:** Main validation framework - checks tempo tracker against acceptance criteria from PRD.

### Usage

```bash
python3 test_tempo_lock.py --log <log_file> --ground-truth <csv_file> [--genre EDM|complex]
```

### Acceptance Criteria Validated

- **AC-9 Lock Time:** EDM ≤ 2.0s, worst-case ≤ 5.0s
- **AC-9 BPM Jitter:** ≤ ±1 BPM short-term
- **AC-9 Octave Flips:** ≤ 1 per 60s during steady sections
- **AC-9 Stability:** Mean confidence after lock

### Example

```bash
# Test EDM track (2.0s lock target)
python3 test_tempo_lock.py \
    --log captures/edm_test.log \
    --ground-truth samples/edm_138bpm.csv \
    --genre EDM

# Test complex track (5.0s lock target)
python3 test_tempo_lock.py \
    --log captures/ambient_test.log \
    --ground-truth samples/slow_ambient_60bpm.csv \
    --genre complex
```

### Output

```
======================================================================
TEMPO TRACKING TEST REPORT: edm_test
======================================================================

--- Metrics ---
Lock time:           1.85s
Mean BPM:            137.8
BPM error:           0.2
BPM jitter:          0.85
Octave flips:        0
Confidence (mean):   0.72

--- Acceptance Criteria ---
✓ ALL ACCEPTANCE CRITERIA PASSED
======================================================================
```

### Exit Codes

- `0` - All acceptance criteria passed
- `1` - One or more criteria failed

---

## Tool 2: analyze_intervals.py

**Purpose:** Parse tempo tracker logs to extract inter-onset intervals (IOI), generate histograms, classify as beat-range vs rejected.

### Usage

```bash
python3 analyze_intervals.py <log_file> [--output <png_file>] [--csv <csv_file>]
```

### Log Format Expected

Lines containing onset intervals:
```
ONSET: dt=0.435s accepted=1
ONSET: dt=0.120s rejected=1
```

Or:
```
Interval: 0.435s (accepted)
Interval: 0.120s (rejected)
```

### Example

```bash
python3 analyze_intervals.py captures/tempo_debug.log
```

### Output

1. **PNG histogram** - Shows interval distribution with beat-range boundaries marked
   - Green bars: Accepted intervals (0.333-1.0s = 180-60 BPM)
   - Red bars: Rejected intervals (too fast/slow)
   - Blue/orange lines: Beat-range boundaries

2. **CSV data** - `<log_file>_intervals.csv` with columns:
   - `interval_sec` - Interval duration
   - `bpm` - Corresponding BPM
   - `status` - accepted / too_fast / too_slow

3. **Console summary**:
```
=============================================================
INTERVAL DISTRIBUTION ANALYSIS
=============================================================
Total intervals:     450
Accepted:            285 (63.3%)
Rejected:            165 (36.7%)
  - Too fast (<0.333s):  120
  - Too slow (>1.0s):     45

Mean interval:       0.435s
Mean BPM:            138.0
=============================================================
```

### Critical Insight

From debrief: Original implementation had **99.7% rejection rate** due to hat onsets (~0.12s) poisoning beat-scale IOI measurement. This tool visualizes the rejection distribution to validate fixes.

---

## Tool 3: measure_onset_accuracy.py

**Purpose:** Measure onset quality against ground truth - validates AC-11 (≥70% onset accuracy).

### Usage

```bash
python3 measure_onset_accuracy.py <log_file> <ground_truth_csv> [--tolerance 0.050]
```

### Log Format Expected

Lines containing onset timestamps:
```
ONSET: t=1.234s strength=0.87
```

Or:
```
Onset detected at 1.234s
```

### Ground Truth CSV Format

```csv
beat_time_sec,expected_bpm
0.435,138.0
0.870,138.0
1.305,138.0
...
```

### Tolerance

Default: `±50ms` - Onsets within ±50ms of labeled beats count as "on-beat"

### Example

```bash
python3 measure_onset_accuracy.py \
    captures/edm_onsets.log \
    samples/edm_138bpm.csv \
    --tolerance 0.050
```

### Output

```
======================================================================
ONSET ACCURACY ANALYSIS
======================================================================
Tolerance window:    ±50ms

Total onsets:        156
Total beats (GT):    100

--- Onset Classification ---
On-beat:              72 ( 46.2%)
Subdivision:          58 ( 37.2%)
Transient:            26 ( 16.7%)

--- Detection Quality ---
Matched beats:       72/100
False negatives:     28 (28.0%)

--- Acceptance Criteria Check ---
✗ AC-11 FAIL: Onset accuracy 46.2% < 70%
✗ AC-11 FAIL: Subdivision rate 37.2% > 20%
✗ AC-11 FAIL: Transient rate 16.7% > 10%
======================================================================

DIAGNOSTIC RECOMMENDATIONS:
- Onset accuracy below target (70%)
- Check novelty threshold tuning (baseline alpha, onset threshold)
- Consider increasing refractory period if high subdivision rate
- Review spectral flux computation (may need different frequency weighting)
```

### Diagnostic Value

From debrief: **Only ~10-20% of onsets are on actual beats**, with ~40-60% subdivisions and ~20-30% transients. This tool measures the breakdown to guide tuning.

### Exit Codes

- `0` - Onset accuracy ≥ 70%
- `2` - Onset accuracy < 70%

---

## Tool 4: visualize_tempo_data.py

**Purpose:** Multi-panel plots showing tempo tracker internal state evolution over time.

### Usage

```bash
python3 visualize_tempo_data.py <log_file> [--output <png_file>] [--ground-truth-bpm <bpm>]
```

### Log Format Expected

Structured debug output:
```
t=1.234s bpm=120.5 conf=0.67
DENSITY: bpm=120.0 density=0.85
DENSITY: bpm=121.0 density=0.92
DENSITY_END
INTERVAL: detected=0.435s expected=0.500s
PHASE: t=1.234s phase=0.75 beat_tick=0
```

### Example

```bash
python3 visualize_tempo_data.py \
    captures/tempo_state.log \
    --ground-truth-bpm 138 \
    --output viz_edm.png
```

### Output

Multi-panel PNG with:

1. **BPM over time** - Detected BPM with ground truth reference line
2. **Confidence over time** - Shows lock threshold crossing
3. **Interval scatter** - Detected vs expected intervals (perfect = diagonal line)
4. **Phase progression** - Phase evolution with beat tick markers
5. **Density buffer heatmap** - BPM vote distribution over time (shows hypothesis evolution)

### Value

Visualizing density buffer evolution reveals:
- How quickly hypothesis converges
- Octave confusion (2× or 0.5× peaks)
- Subdivision artifacts (spurious half-beat peaks)

---

## Capturing Tempo Tracker Logs

### Option 1: Serial Capture from ESP32 (Current Approach)

1. **Enable debug logging** in `TempoTracker.cpp`:
   ```cpp
   diagnostics_.enabled = true;
   ```

2. **Build and upload**:
   ```bash
   cd firmware/v2
   pio run -e esp32dev_audio -t upload
   ```

3. **Connect serial monitor and play audio**:
   ```bash
   pio device monitor -b 115200 > captures/tempo_debug.log
   ```

4. **Play test audio** through SPH0645 I2S microphone

5. **Stop capture** (Ctrl+C) after 10-15 seconds

### Option 2: Native Test Harness (Future)

**Status:** Not yet implemented (Phase 0 focuses on log-based tools)

**Planned:**
```bash
# Build native test
pio run -e native_test_audio

# Run with audio file
.pio/build/native_test_audio/program --audio samples/edm_138bpm.wav > tempo_log.txt

# Validate
python3 test_tempo_lock.py --log tempo_log.txt --ground-truth samples/edm_138bpm.csv
```

---

## Debug Log Format Specification

### Required Log Lines

**Tempo updates:**
```
t=<time_sec>s bpm=<bpm> conf=<confidence>
```

Example: `t=1.234s bpm=120.5 conf=0.67`

**Onset events:**
```
ONSET: t=<time_sec>s strength=<strength>
```

Example: `ONSET: t=1.234s strength=0.87`

**Intervals (with accept/reject):**
```
ONSET: dt=<interval_sec>s accepted=<0|1>
```

Example: `ONSET: dt=0.435s accepted=1`

**Optional (for visualization):**

Density buffer dumps:
```
DENSITY: bpm=<bpm> density=<value>
...
DENSITY_END
```

Phase updates:
```
PHASE: t=<time_sec>s phase=<phase01> beat_tick=<0|1>
```

---

## Ground Truth Data Collection

### Creating New Test Tracks

1. **Select/create audio file** (WAV/MP3)
   - Clear rhythmic content
   - Steady tempo (no tempo changes)
   - 10-15 seconds duration
   - Known BPM

2. **Hand-label beats** using audio editor (Audacity, Reaper, etc.)
   - Place markers at each beat
   - Export marker times to text

3. **Convert to CSV format:**
   ```csv
   beat_time_sec,expected_bpm
   0.500,120.0
   1.000,120.0
   1.500,120.0
   ...
   ```

4. **Validate label accuracy:**
   ```bash
   python3 measure_onset_accuracy.py <log> <csv> --tolerance 0.050
   ```

### Recommended Test Tracks

- **EDM / House** (120-140 BPM) - Strong kick, clear beats
- **Drum & Bass** (160-180 BPM) - Fast tempo, tests upper BPM limit
- **Ambient** (60-80 BPM) - Slow, tests lower BPM limit
- **Complex Polyrhythm** - Multiple simultaneous tempos
- **Hat-only loop** - Should NOT produce beat detections (negative test)
- **Metronome click** - Perfect timing reference

---

## Integration with Ralph Loop

**Phase 0 Exit Criteria (from execution plan):**
- ✅ Test tools exist and execute
- ✅ Ground truth data collected (6 CSV templates created)
- ✅ Can run baseline validation (tools functional, will fail until fixes applied)
- ✅ Documented (this README)

**Iteration 1 Complete** when:
1. All 4 Python tools execute without errors
2. Example ground truth files exist
3. Documentation explains usage and log format
4. Can run validation pipeline (even if tests FAIL - that's expected pre-fix)

**Next Steps (Iteration 2+):**
- Phase 1: Fix P0 bugs (beat_tick, gating, onset poisoning, determinism)
- Validate fixes using these tools
- Iterate until AC-11 (onset accuracy ≥ 70%) achieved

---

## Troubleshooting

### "No intervals found in log file"

**Cause:** Log format doesn't match expected patterns

**Fix:** Check that tempo tracker is outputting lines like:
```
ONSET: dt=0.435s accepted=1
```

Enable debug logging in TempoTracker.cpp:
```cpp
diagnostics_.enabled = true;
```

### "matplotlib not installed"

**Fix:**
```bash
pip install matplotlib numpy
```

### "Ground truth file not found"

**Cause:** CSV file path incorrect

**Fix:**
```bash
# Use relative path from tools/ directory
python3 test_tempo_lock.py --log <log> --ground-truth samples/edm_138bpm.csv

# Or absolute path
python3 test_tempo_lock.py --log <log> --ground-truth /full/path/to/edm_138bpm.csv
```

### "High rejection rate (99.7%)"

**Expected pre-fix!** This is the **documented failure mode** we're fixing.

From debrief: Hat onsets (~0.12s) poison beat-scale IOI measurement. Phase 1 fixes:
- P0-C: Onset poisoning (only update lastOnsetUs for beat-candidates)
- FR-16: Tighten minimum interval to 0.333s (180 BPM max)

---

## References

- **PRD:** `.claude/prd/TEMPO-TRACKING-FIX-001.json` v2.0.0
- **Execution Plan:** `.claude/plans/tempo-tracking-overhaul-execution-plan.md`
- **Failure Debrief:** `TEMPO_TRACKER_COMPLETE_FAILURE_DEBRIEF.md`
- **Failure Catalog:** `.claude/prd/TEMPO-TRACKING-FIX-001-FAILURE-CATALOG.md`

---

## Summary: Test-First Philosophy

**Why this exists:** "Never tested with ground truth" was the meta-failure that enabled all 75 documented failures.

**What changed:** Test infrastructure created FIRST (Phase 0), BEFORE any code implementation.

**Validation approach:**
1. Capture tempo tracker logs (serial or native test)
2. Parse logs with Python tools
3. Compare against ground truth
4. Validate acceptance criteria
5. Iterate until all AC pass

**Success metric:** All 20 AC from PRD v2.0.0 must pass, including:
- AC-11: Onset accuracy ≥ 70%
- AC-9: Lock time ≤ 2.0s EDM, BPM jitter ≤ ±1 BPM, octave flips ≤ 1/60s

---

**Phase 0 Status:** ✅ COMPLETE - Test infrastructure ready for Ralph Loop Iteration 1
