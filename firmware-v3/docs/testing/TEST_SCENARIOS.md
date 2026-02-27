# Audio Effect Validation Test Scenarios

**Document Version**: 1.0
**Date**: 2025-12-28
**Status**: Production
**Author**: LightwaveOS Engineering

---

## Overview

This document defines six standardized test scenarios for validating audio-reactive LED effects in LightwaveOS. Each scenario includes detailed procedures, expected outcomes, pass/fail criteria, and diagnostic guidance.

---

## Test Environment Setup

### Hardware Requirements

- ESP32-S3-DevKitC-1 running LightwaveOS v2
- SPH0645 I2S MEMS microphone (properly positioned)
- 320 WS2812 LEDs (dual 160-LED strips in LGP configuration)
- Audio source (speaker or direct line-in)
- Test computer with Python 3.10+

### Software Requirements

```bash
# Build firmware with validation
pio run -e esp32dev_validation -t upload

# Install Python tools
cd tools/benchmark
uv pip install -e .
```

### Pre-Test Checklist

- [ ] Firmware built with `FEATURE_EFFECT_VALIDATION=1`
- [ ] Audio input calibrated (RMS ~0.2-0.3 at normal volume)
- [ ] Brightness set to 50% (128) for consistent measurements
- [ ] LGP_SMOOTH preset active
- [ ] WebSocket connection established
- [ ] Data collection verified (at least 10 samples received)

---

## Scenario 1: Jog-Dial Motion Test

### Purpose

Detect unwanted bidirectional motion artifacts where effects appear to oscillate back and forth ("jog-dial" effect) instead of propagating smoothly outward from the center.

### Target Effects

- LGPInterferenceScannerEffect (Effect ID: 16)
- LGPWaveCollisionEffect (Effect ID: 17)

### Test Procedure

**Duration:** 120 seconds per effect

**Step 1: Configure Audio Source**
```bash
# Play bass-heavy music with strong transients
# Recommended: Electronic/EDM with 120-140 BPM
# Alternative: Test tone with 60Hz + 120Hz at -12dB
```

**Step 2: Start Data Collection**
```bash
lwos-bench collect \
    --run-name "jog-dial-scanner-$(date +%Y%m%d)" \
    --duration 120 \
    --host lightwaveos.local
```

**Step 3: Select Target Effect**
```bash
# Via API
curl -X POST http://lightwaveos.local/api/v1/effects/set \
    -H "Content-Type: application/json" \
    -d '{"effectId": 16}'

# Or via serial: 'e', then select effect 16
```

**Step 4: Monitor Real-Time**
```bash
# Watch for reversal events in dashboard
lwos-bench serve --port 8050
# Navigate to: http://localhost:8050/reversals
```

**Step 5: Analyze Results**
```bash
lwos-bench analyze --run-id <run_id> --metric reversals
```

### Expected Outcomes

| Metric | Expected | Acceptable | Fail |
|--------|----------|------------|------|
| Direction reversals | 0 | < 5 | >= 5 |
| Reversal rate | 0/min | < 2.5/min | >= 2.5/min |
| Consecutive reversals | 0 | 0 | >= 1 |
| Reversal magnitude | N/A | < 0.05 | >= 0.05 |

### Pass/Fail Criteria

**PASS:** Fewer than 5 direction reversals in 120 seconds
**FAIL:** 5 or more direction reversals in 120 seconds

### Diagnostic Guidance

**If test fails:**

1. **Check phase_delta sign changes:**
   ```python
   # In analysis script
   reversals = (phase_delta[:-1] * phase_delta[1:]) < 0
   print(f"Reversal indices: {np.where(reversals)[0]}")
   ```

2. **Correlate with audio transients:**
   - Reversals should NOT correlate with beat events
   - If they do, energyDelta may still be coupled to speed

3. **Verify slew limiter:**
   - Check `speed_scale_smooth` vs `speed_scale_raw`
   - Smooth should lag behind raw by 3-5 seconds

4. **Common causes:**
   - energyDelta still in speed calculation (line 111-112)
   - Slew rate too high (should be 0.25-0.30 units/sec)
   - Time constants too fast (should be 250ms rise, 400ms fall)

---

## Scenario 2: Smoothness Test

### Purpose

Validate that phase/position changes are smooth and free from perceptible jitter or discontinuities.

### Target Effects

All audio-reactive LGP effects:
- LGPInterferenceScannerEffect
- LGPWaveCollisionEffect
- AudioBloomEffect

### Test Procedure

**Duration:** 60 seconds per effect

**Step 1: Configure Audio Source**
```bash
# Play sustained tone or pad sound
# Recommended: 440Hz sine wave at -18dB (constant level)
# Alternative: Ambient music with minimal transients
```

**Step 2: Start Data Collection**
```bash
lwos-bench collect \
    --run-name "smoothness-test-$(date +%Y%m%d)" \
    --duration 60 \
    --host lightwaveos.local
```

**Step 3: Compute Jerk Metric**
```python
# Post-processing analysis
import numpy as np

# Load phase_delta samples
deltas = load_samples(run_id)['phase_delta']

# Compute jerk (second derivative of position)
jerk = np.diff(deltas, n=2)
avg_abs_jerk = np.mean(np.abs(jerk))

print(f"Average absolute jerk: {avg_abs_jerk:.6f}")
```

### Expected Outcomes

| Metric | Excellent | Acceptable | Fail |
|--------|-----------|------------|------|
| avg_abs_jerk | < 0.005 | < 0.01 | >= 0.01 |
| max_abs_jerk | < 0.02 | < 0.05 | >= 0.05 |
| jerk_std_dev | < 0.003 | < 0.008 | >= 0.008 |
| smoothness_score | > 0.95 | > 0.85 | <= 0.85 |

### Pass/Fail Criteria

**PASS:** Average absolute jerk < 0.01 phase units per frame squared
**FAIL:** Average absolute jerk >= 0.01 phase units per frame squared

### Jerk Calculation

```
Jerk is the third derivative of position:

    jerk[t] = delta[t] - 2*delta[t-1] + delta[t-2]

where delta[t] = phase[t] - phase[t-1]

For frame rate of 120 FPS:
- jerk units: phase_units / frame^3
- Target: < 0.01 corresponds to < 0.17 phase_units/sec^3
```

### Diagnostic Guidance

**If test fails:**

1. **Check for quantization:**
   - Phase should have > 16-bit resolution
   - Verify no integer truncation in accumulator

2. **Verify smoothing:**
   - smoothValue() must use correct alpha
   - Alpha should be dt-corrected

3. **Check for discontinuities:**
   ```python
   discontinuities = np.where(np.abs(jerk) > 3 * np.std(jerk))[0]
   print(f"Discontinuity frames: {discontinuities}")
   ```

4. **Common causes:**
   - dt spike causing large phase jump
   - Effect restart resetting phase
   - Integer overflow in phase accumulator

---

## Scenario 3: Frequency Response Test

### Purpose

Verify that all 8 frequency bands respond appropriately to their target frequencies and that per-band gain normalization is working correctly.

### Target Bands

| Band | Center Freq | Target Gain |
|------|-------------|-------------|
| 0 | 60 Hz | 0.8x |
| 1 | 120 Hz | 0.85x |
| 2 | 250 Hz | 1.0x |
| 3 | 500 Hz | 1.2x |
| 4 | 1 kHz | 1.5x |
| 5 | 2 kHz | 1.8x |
| 6 | 4 kHz | 2.0x |
| 7 | 7.8 kHz | 2.2x |

### Test Procedure

**Duration:** ~90 seconds total (10 seconds per frequency + transitions)

**Step 1: Generate Frequency Sweep**
```python
# Generate test tones (or use audio file)
frequencies = [60, 120, 250, 500, 1000, 2000, 4000, 7800]
duration_per_tone = 10  # seconds
amplitude = 0.5  # -6dB

# Play each tone sequentially with 0.5s gaps
```

**Step 2: Record Band Responses**
```bash
lwos-bench collect \
    --run-name "freq-response-$(date +%Y%m%d)" \
    --duration 90 \
    --host lightwaveos.local
```

**Step 3: Analyze Per-Band Response**
```python
# Extract band energy during each frequency
for i, freq in enumerate(frequencies):
    band_data = samples_during_frequency(freq)
    band_energy = band_data[f'band_{i}'].mean()
    print(f"Band {i} ({freq}Hz): {band_energy:.4f}")
```

### Expected Outcomes

| Metric | Expected | Acceptable | Fail |
|--------|----------|------------|------|
| Primary band response | > 0.6 | > 0.4 | <= 0.4 |
| Adjacent band leakage | < 0.3 | < 0.5 | >= 0.5 |
| Dead bands | 0 | 0 | >= 1 |
| Gain ratio (band7/band0) | ~2.75 | 2.0-3.5 | outside range |

### Pass/Fail Criteria

**PASS:** All 8 bands show measurable response (> 0.4) to their target frequency
**FAIL:** Any band fails to respond or shows excessive crosstalk

### Verification Matrix

```
Stimulus:   60   120  250  500  1k   2k   4k   7.8k
           ----  ---- ---- ---- ---- ---- ---- ----
Band 0:    [++]  [+]  [ ]  [ ]  [ ]  [ ]  [ ]  [ ]
Band 1:    [+]   [++] [+]  [ ]  [ ]  [ ]  [ ]  [ ]
Band 2:    [ ]   [+]  [++] [+]  [ ]  [ ]  [ ]  [ ]
Band 3:    [ ]   [ ]  [+]  [++] [+]  [ ]  [ ]  [ ]
Band 4:    [ ]   [ ]  [ ]  [+]  [++] [+]  [ ]  [ ]
Band 5:    [ ]   [ ]  [ ]  [ ]  [+]  [++] [+]  [ ]
Band 6:    [ ]   [ ]  [ ]  [ ]  [ ]  [+]  [++] [+]
Band 7:    [ ]   [ ]  [ ]  [ ]  [ ]  [ ]  [+]  [++]

Legend: [++] Primary response, [+] Adjacent spillover, [ ] No response
```

### Diagnostic Guidance

**If test fails:**

1. **Dead band:**
   - Check Goertzel coefficient for that frequency
   - Verify per-band gain applied correctly
   - Check noise floor gate not too aggressive

2. **Excessive crosstalk:**
   - Goertzel Q-factor may need adjustment
   - Check for harmonic content in test signal

3. **Wrong gain balance:**
   - Verify perBandGains array matches expected values
   - Check gain applied after Goertzel, before smoothing

---

## Scenario 4: Transient Response Test

### Purpose

Measure the audio-to-visual latency and verify that transient sounds produce appropriate visual responses within acceptable timing bounds.

### Target Latency

Maximum acceptable latency: **50ms** (3 LED frames at 60 FPS)

### Test Procedure

**Duration:** 60 seconds

**Step 1: Generate Transient Test Signal**
```python
# Create impulse train (4 clicks per second)
# Each click: 10ms burst of white noise at -6dB
# Separation: 250ms (4 Hz rate)

# Alternative: Use metronome click track at 120 BPM
```

**Step 2: Configure Latency Measurement**
```bash
# Enable high-resolution timing
lwos-bench collect \
    --run-name "transient-test-$(date +%Y%m%d)" \
    --duration 60 \
    --high-res-timing \
    --host lightwaveos.local
```

**Step 3: Detect Transients in Audio and Visual**
```python
# Find audio transient times (from energy_delta peaks)
audio_transients = find_peaks(samples['energy_delta'], height=0.3)

# Find visual response times (from brightness_delta peaks)
visual_responses = find_peaks(samples['brightness_delta'], height=0.2)

# Compute latency for each transient
latencies = []
for audio_t in audio_transients:
    # Find nearest visual response after audio
    visual_after = visual_responses[visual_responses > audio_t]
    if len(visual_after) > 0:
        latency_ms = (visual_after[0] - audio_t) * frame_period_ms
        latencies.append(latency_ms)

avg_latency = np.mean(latencies)
p95_latency = np.percentile(latencies, 95)
```

### Expected Outcomes

| Metric | Excellent | Acceptable | Fail |
|--------|-----------|------------|------|
| Average latency | < 20ms | < 35ms | >= 50ms |
| P95 latency | < 30ms | < 50ms | >= 75ms |
| Max latency | < 40ms | < 75ms | >= 100ms |
| Detection rate | > 98% | > 90% | < 90% |

### Pass/Fail Criteria

**PASS:** P95 latency < 50ms AND detection rate > 90%
**FAIL:** P95 latency >= 50ms OR detection rate < 90%

### Latency Budget

```
Audio capture (I2S DMA):     ~5.8ms  (512 samples @ 88.2kHz)
Goertzel analysis:           ~0.5ms
ControlBus smoothing:        ~0.1ms
Effect render:               ~0.5ms
FastLED show:                ~9.6ms  (320 LEDs)
                            --------
Total pipeline:             ~16.5ms  (theoretical minimum)

Attack smoothing:            +10-20ms (bandAttack = 0.15)
                            --------
Expected total:              ~25-35ms
```

### Diagnostic Guidance

**If test fails:**

1. **High average latency:**
   - Check attack time constants (bandAttack should be 0.15)
   - Verify no extra smoothing in effect code
   - Check for WebSocket buffering delays

2. **High P95 latency (outliers):**
   - Look for frame drops during high CPU load
   - Check WiFi/WebSocket reliability
   - Verify no garbage collection pauses

3. **Low detection rate:**
   - Lower the visual response threshold
   - Check if noise floor is gating transients
   - Verify transient audio is loud enough

---

## Scenario 5: Silence Baseline Test

### Purpose

Verify that effects produce no visible output during silence, confirming that noise floor gating is working correctly.

### Test Procedure

**Duration:** 60 seconds

**Step 1: Ensure Audio Silence**
```bash
# Mute all audio sources
# Cover/mute microphone if possible
# Or play digital silence (all zeros)
```

**Step 2: Collect Baseline Data**
```bash
lwos-bench collect \
    --run-name "silence-baseline-$(date +%Y%m%d)" \
    --duration 60 \
    --host lightwaveos.local
```

**Step 3: Analyze Visual Noise**
```python
# Compute visual activity metrics
band_max = np.max([samples[f'band_{i}'] for i in range(8)], axis=0)
visual_events = np.sum(band_max > 0.1)  # Above 10% threshold
event_rate = visual_events / 60.0  # Events per second

print(f"Visual events: {visual_events}")
print(f"Event rate: {event_rate:.2f}/sec")
```

### Expected Outcomes

| Metric | Expected | Acceptable | Fail |
|--------|----------|------------|------|
| Visual events (60s) | 0 | < 10 | >= 10 |
| Event rate | 0/sec | < 0.17/sec | >= 0.17/sec |
| Max band value | 0 | < 0.05 | >= 0.1 |
| RMS during silence | 0 | < 0.01 | >= 0.02 |

### Pass/Fail Criteria

**PASS:** Fewer than 10 visual events in 60 seconds (< 0.17/sec)
**FAIL:** 10 or more visual events in 60 seconds

### Noise Sources to Consider

| Source | Frequency | Mitigation |
|--------|-----------|------------|
| Power supply hum | 60Hz, 120Hz | perBandNoiseFloor[0,1] |
| HVAC | 50-200Hz | perBandNoiseFloor[0-2] |
| Computer fan | 1-4kHz | perBandNoiseFloor[4-6] |
| Electrical noise | Broadband | Global noise floor |

### Diagnostic Guidance

**If test fails:**

1. **Check noise floor calibration:**
   ```cpp
   // Current values in AudioTuning.h
   perBandNoiseFloors = {0.0008, 0.0012, 0.0006, 0.0005,
                         0.0008, 0.0010, 0.0012, 0.0006};
   ```

2. **Identify noisy bands:**
   ```python
   for i in range(8):
       band_activity = np.sum(samples[f'band_{i}'] > 0.05)
       print(f"Band {i}: {band_activity} events")
   ```

3. **Increase noise floor if needed:**
   - Multiply offending band's floor by 1.5x
   - Re-run test to verify

4. **Verify usePerBandNoiseFloor:**
   - Should be `true` in LGP_SMOOTH preset
   - Check AudioTuning.h line 289

---

## Scenario 6: Speed Parameter Sweep Test

### Purpose

Verify that the speed parameter (1-50) produces consistent, proportional changes in effect animation rate across the entire range.

### Target Effects

- AudioBloomEffect (scroll rate)
- LGPInterferenceScannerEffect (phase rate)
- LGPWaveCollisionEffect (phase rate)

### Test Procedure

**Duration:** ~5 minutes total

**Step 1: Configure Test**
```python
speed_values = [10, 20, 30, 40, 50]  # Test points
duration_per_speed = 30  # seconds each
```

**Step 2: Run Speed Sweep**
```bash
# For each speed value
for speed in 10 20 30 40 50; do
    # Set speed via API
    curl -X POST http://lightwaveos.local/api/v1/parameters \
        -H "Content-Type: application/json" \
        -d "{\"speed\": $speed}"

    # Collect data
    lwos-bench collect \
        --run-name "speed-sweep-${speed}" \
        --duration 30 \
        --host lightwaveos.local

    sleep 2
done
```

**Step 3: Analyze Speed Proportionality**
```python
# Compute phase rate for each speed setting
phase_rates = {}
for speed in speed_values:
    samples = load_run(f"speed-sweep-{speed}")
    phase_rate = np.mean(np.abs(samples['phase_delta']))
    phase_rates[speed] = phase_rate

# Check linearity
speeds = np.array(list(phase_rates.keys()))
rates = np.array(list(phase_rates.values()))
correlation = np.corrcoef(speeds, rates)[0, 1]

print(f"Speed-rate correlation: {correlation:.4f}")
```

### Expected Outcomes

| Speed | Expected Phase Rate | Tolerance |
|-------|-------------------|-----------|
| 10 | 0.20x baseline | +/-20% |
| 20 | 0.40x baseline | +/-15% |
| 30 | 0.60x baseline | +/-10% |
| 40 | 0.80x baseline | +/-10% |
| 50 | 1.00x baseline | +/-5% |

| Metric | Expected | Acceptable | Fail |
|--------|----------|------------|------|
| Correlation (r) | > 0.98 | > 0.95 | <= 0.95 |
| Rate ratio (50/10) | 5.0 | 4.0-6.0 | outside |
| Monotonicity | Yes | Yes | No |

### Pass/Fail Criteria

**PASS:** Correlation r > 0.95 AND monotonically increasing
**FAIL:** Correlation r <= 0.95 OR non-monotonic behavior

### AudioBloom Scroll Rate Reference

```
ScrollRate = 0.3 + (speed/50) * 2.2

Speed 10:  0.3 + 0.2 * 2.2 = 0.74 LEDs/hop
Speed 25:  0.3 + 0.5 * 2.2 = 1.40 LEDs/hop
Speed 50:  0.3 + 1.0 * 2.2 = 2.50 LEDs/hop

Ratio (50/10) = 2.5 / 0.74 = 3.38x
```

### Diagnostic Guidance

**If test fails:**

1. **Non-linear response:**
   - Check speed normalization (should be speed/50.0f)
   - Verify no clamping or quantization

2. **Inverted response:**
   - Check sign of speed term in formula
   - Verify UI sends correct values

3. **Inconsistent rates:**
   - Check for interference from audio modulation
   - Verify speed isolation from audio input

4. **Plateau at high speeds:**
   - Check for implicit rate limiting
   - Verify slew limiter allows high speeds

---

## Test Execution Summary

### Recommended Test Order

1. **Silence Baseline** - Establish clean baseline first
2. **Frequency Response** - Verify audio pipeline
3. **Transient Response** - Check timing
4. **Smoothness** - Verify motion quality
5. **Jog-Dial Motion** - Core fix validation
6. **Speed Parameter Sweep** - Parameter verification

### Test Automation Script

```bash
#!/bin/bash
# run_all_tests.sh

HOST="lightwaveos.local"
DATE=$(date +%Y%m%d)

echo "=== LightwaveOS Effect Validation Suite ==="
echo "Date: $DATE"
echo "Host: $HOST"

# 1. Silence Baseline
echo "=== Test 1: Silence Baseline ==="
lwos-bench collect --run-name "silence-$DATE" --duration 60 --host $HOST

# 2. Frequency Response (requires external audio)
echo "=== Test 2: Frequency Response ==="
echo "Play frequency sweep audio now..."
read -p "Press Enter when ready..."
lwos-bench collect --run-name "freq-response-$DATE" --duration 90 --host $HOST

# 3. Transient Response
echo "=== Test 3: Transient Response ==="
echo "Play click track audio now..."
read -p "Press Enter when ready..."
lwos-bench collect --run-name "transient-$DATE" --duration 60 --host $HOST

# 4. Smoothness
echo "=== Test 4: Smoothness ==="
echo "Play sustained tone audio now..."
read -p "Press Enter when ready..."
lwos-bench collect --run-name "smoothness-$DATE" --duration 60 --host $HOST

# 5. Jog-Dial Motion
echo "=== Test 5: Jog-Dial Motion ==="
echo "Play bass-heavy music now..."
read -p "Press Enter when ready..."
curl -X POST http://$HOST/api/v1/effects/set -d '{"effectId": 16}'
lwos-bench collect --run-name "jogdial-scanner-$DATE" --duration 120 --host $HOST
curl -X POST http://$HOST/api/v1/effects/set -d '{"effectId": 17}'
lwos-bench collect --run-name "jogdial-collision-$DATE" --duration 120 --host $HOST

# 6. Speed Parameter Sweep
echo "=== Test 6: Speed Parameter Sweep ==="
for speed in 10 20 30 40 50; do
    curl -X POST http://$HOST/api/v1/parameters -d "{\"speed\": $speed}"
    sleep 2
    lwos-bench collect --run-name "speed-$speed-$DATE" --duration 30 --host $HOST
done

echo "=== All tests complete ==="
echo "Run: lwos-bench report --date $DATE"
```

### Results Reporting

```bash
# Generate comprehensive report
lwos-bench report \
    --date $(date +%Y%m%d) \
    --output validation_report.html \
    --format html

# Quick pass/fail summary
lwos-bench summary --date $(date +%Y%m%d)
```

---

## Related Documentation

- [AUDIO_TEST_HARNESS.md](./AUDIO_TEST_HARNESS.md) - Framework overview
- [METRICS_REFERENCE.md](./METRICS_REFERENCE.md) - Detailed metric definitions
- [AUDIO_LGP_EXPECTED_OUTCOMES.md](../AUDIO_LGP_EXPECTED_OUTCOMES.md) - Expected behaviors

---

*End of Test Scenarios Documentation*
