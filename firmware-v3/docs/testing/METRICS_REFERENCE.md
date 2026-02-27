# Effect Validation Metrics Reference

**Document Version**: 1.0
**Date**: 2025-12-28
**Status**: Production
**Author**: LightwaveOS Engineering

---

## Overview

This document provides detailed specifications for all metrics captured by the LightwaveOS Audio Effect Validation framework, including field definitions, binary encoding formats, calculation algorithms, and interpretation guidance.

---

## Table of Contents

1. [EffectValidationSample Fields](#effectvalidationsample-fields)
2. [Binary Frame Format Specification](#binary-frame-format-specification)
3. [Reversal Detection Algorithm](#reversal-detection-algorithm)
4. [Jerk Calculation Formula](#jerk-calculation-formula)
5. [Latency Measurement Methodology](#latency-measurement-methodology)
6. [Derived Metrics](#derived-metrics)
7. [Statistical Aggregations](#statistical-aggregations)

---

## EffectValidationSample Fields

The `EffectValidationSample` structure captures a snapshot of effect state at a single point in time. The structure is exactly 128 bytes, aligned for cache efficiency on ESP32.

### Memory Layout

```
Offset  Size   Type      Field                 Description
------  ----   ----      -----                 -----------
0       4      uint32    timestamp_us          Microseconds since boot
4       4      uint32    hop_seq               Audio hop sequence number
8       1      uint8     effect_id             Current effect index (0-255)
9       1      uint8     reversal_count        Direction reversals this frame
10      2      uint16    reserved1             Alignment padding
12      4      float32   phase                 Normalized phase (0.0-1.0)
16      4      float32   phase_delta           Rate of change per frame
20      4      float32   speed_scale_raw       Raw speed before slew limiting
24      4      float32   speed_scale_smooth    Smoothed speed after slew
28      4      float32   dominant_freq_bin     Dominant frequency (0.0-7.0)
32      4      float32   energy_avg            Average energy (0.0-1.0)
36      4      float32   energy_delta          Energy change from previous
40      4      float32   scroll_phase          Bloom scroll phase (0.0-1.0)
44      84     uint8[]   reserved2             Reserved for future use
------
128 bytes total
```

### Field Definitions

#### timestamp_us (offset 0, 4 bytes)

**Type:** uint32_t
**Range:** 0 to 4,294,967,295 (wraps every ~71.6 minutes)
**Unit:** Microseconds

Capture timestamp from `esp_timer_get_time()`. Used for:
- Calculating frame intervals
- Correlating samples with audio hops
- Measuring processing latency

**Example:**
```cpp
sample.timestamp_us = esp_timer_get_time() & 0xFFFFFFFF;
```

#### hop_seq (offset 4, 4 bytes)

**Type:** uint32_t
**Range:** 0 to 4,294,967,295
**Unit:** Monotonic counter

Audio hop sequence number from ControlBusFrame. Increments once per audio hop (~5.8ms at 172 Hz hop rate). Used for:
- Detecting missed audio hops
- Correlating visual state with audio state
- Measuring audio-visual synchronization

**Relationship to audio:**
```
hop_rate = sample_rate / hop_size = 88200 / 512 = 172.27 Hz
hop_period = 5.81 ms
```

#### effect_id (offset 8, 1 byte)

**Type:** uint8_t
**Range:** 0 to 255
**Unit:** Effect index

Identifier of the effect being validated:

| ID | Effect Name |
|----|-------------|
| 16 | LGPInterferenceScannerEffect |
| 17 | LGPWaveCollisionEffect |
| 18 | AudioBloomEffect |
| 19 | AudioWaveformEffect |

#### reversal_count (offset 9, 1 byte)

**Type:** uint8_t
**Range:** 0 to 255
**Unit:** Count per frame

Number of direction reversals detected in this frame. A reversal occurs when `phase_delta` changes sign with significant magnitude.

**Normal value:** 0
**Failure indicator:** > 0

#### phase (offset 12, 4 bytes)

**Type:** float32
**Range:** 0.0 to 1.0 (normalized) or unbounded (raw accumulator)
**Unit:** Phase units (radians / 2*pi for normalized)

Current phase accumulator value. Represents the "position" of the effect animation.

For LGP effects:
- Increasing phase = outward motion from center
- Decreasing phase = inward motion (should not occur)

**Normalization:**
```cpp
float normalized_phase = fmodf(m_phase, 1.0f);
if (normalized_phase < 0) normalized_phase += 1.0f;
```

#### phase_delta (offset 16, 4 bytes)

**Type:** float32
**Range:** Typically -0.1 to 0.1
**Unit:** Phase units per frame

Rate of change of phase between frames:
```
phase_delta = phase[t] - phase[t-1]
```

**Sign convention:**
- Positive: Forward/outward motion
- Negative: Backward/inward motion (indicates reversal)

**Expected values:**
```
At 120 FPS, speed=25, speedScale=1.0:
phase_delta = speedNorm * speedScale * dt * rate_factor
            = 0.5 * 1.0 * 0.00833 * 4.0
            = 0.0167 phase units/frame
```

#### speed_scale_raw (offset 20, 4 bytes)

**Type:** float32
**Range:** 0.0 to 2.0
**Unit:** Multiplier

Speed scale target value before slew limiting:
```cpp
// LGPInterferenceScannerEffect
float speedTarget = 0.6f + 0.8f * m_energyAvgSmooth;
// Range: 0.6 to 1.4
```

#### speed_scale_smooth (offset 24, 4 bytes)

**Type:** float32
**Range:** 0.0 to 2.0
**Unit:** Multiplier

Speed scale value after slew rate limiting:
```cpp
const float maxSlewRate = 0.3f;  // units/second
float slewLimit = maxSlewRate * dt;
float speedDelta = speedTarget - m_speedScaleSmooth;
if (speedDelta > slewLimit) speedDelta = slewLimit;
if (speedDelta < -slewLimit) speedDelta = -slewLimit;
m_speedScaleSmooth += speedDelta;
```

**Key relationship:**
```
|speed_scale_smooth - speed_scale_raw| <= maxSlewRate * dt
```

#### dominant_freq_bin (offset 28, 4 bytes)

**Type:** float32
**Range:** 0.0 to 7.0
**Unit:** Band index (smoothed)

Smoothed dominant frequency band index:

| Value | Approximate Frequency |
|-------|----------------------|
| 0.0 | 60 Hz (sub-bass) |
| 1.0 | 120 Hz (bass) |
| 2.0 | 250 Hz (low-mid) |
| 3.0 | 500 Hz (mid) |
| 4.0 | 1 kHz (upper-mid) |
| 5.0 | 2 kHz (presence) |
| 6.0 | 4 kHz (brilliance) |
| 7.0 | 7.8 kHz (air) |

#### energy_avg (offset 32, 4 bytes)

**Type:** float32
**Range:** 0.0 to 1.0
**Unit:** Normalized energy

Smoothed average energy across frequency bands:
```cpp
m_energyAvg = m_chromaEnergySum / CHROMA_HISTORY;
// Where chromaEnergySum is sum of last 4 normalized chroma energies
```

**Smoothing applied:**
```cpp
m_energyAvgSmooth = smoothValue(m_energyAvgSmooth, m_energyAvg, riseAvg, fallAvg);
// riseAvg = dt / (0.20f + dt)  ~200ms time constant
// fallAvg = dt / (0.50f + dt)  ~500ms time constant
```

#### energy_delta (offset 36, 4 bytes)

**Type:** float32
**Range:** 0.0 to 1.0 (clamped positive)
**Unit:** Normalized energy difference

Change in energy from the moving average, indicating transient activity:
```cpp
m_energyDelta = energyNorm - m_energyAvg;
if (m_energyDelta < 0.0f) m_energyDelta = 0.0f;
```

**Usage:** Drives brightness pulses on transients (not speed modulation)

#### scroll_phase (offset 40, 4 bytes)

**Type:** float32
**Range:** 0.0 to 1.0
**Unit:** Fractional LED position

AudioBloom scroll phase accumulator:
```cpp
float scrollRate = 0.3f + (ctx.speed / 50.0f) * 2.2f;
m_scrollPhase += scrollRate;
uint8_t step = (uint8_t)m_scrollPhase;
m_scrollPhase -= step;  // Keep fractional part
```

---

## Binary Frame Format Specification

### Validation Frame Header

```
Offset  Size  Type    Field          Value/Description
------  ----  ----    -----          -----------------
0       1     uint8   magic[0]       0x54 ('T')
1       1     uint8   magic[1]       0x56 ('V')
2       1     uint8   magic[2]       0x56 ('V')
3       1     uint8   sample_count   Number of samples (0-16)
```

**Full magic number:** 0x4C565654 (little-endian "LVVT" - Lightwave Validation)

### Complete Frame Layout

```
Bytes       Content
---------   -------
0-3         Header (4 bytes)
4-131       Sample 0 (128 bytes)
132-259     Sample 1 (128 bytes)
...
1924-2051   Sample 15 (128 bytes)
---------
Max: 2052 bytes
```

### Frame Validation

```python
def validate_frame(data: bytes) -> bool:
    """Validate a binary validation frame."""
    if len(data) < 4:
        return False

    # Check magic number (first 3 bytes)
    if data[0:3] != b'TVV':
        return False

    # Check sample count
    sample_count = data[3]
    if sample_count > 16:
        return False

    # Check frame length
    expected_length = 4 + (sample_count * 128)
    if len(data) != expected_length:
        return False

    return True
```

### Sample Extraction

```python
import struct

def parse_sample(data: bytes, offset: int) -> dict:
    """Parse a single EffectValidationSample from frame."""
    fmt = '<IIBBHffffffff84x'  # Little-endian, match C struct
    unpacked = struct.unpack_from(fmt, data, offset)

    return {
        'timestamp_us': unpacked[0],
        'hop_seq': unpacked[1],
        'effect_id': unpacked[2],
        'reversal_count': unpacked[3],
        'reserved1': unpacked[4],
        'phase': unpacked[5],
        'phase_delta': unpacked[6],
        'speed_scale_raw': unpacked[7],
        'speed_scale_smooth': unpacked[8],
        'dominant_freq_bin': unpacked[9],
        'energy_avg': unpacked[10],
        'energy_delta': unpacked[11],
        'scroll_phase': unpacked[12],
    }
```

---

## Reversal Detection Algorithm

### Definition

A **reversal** occurs when the effect motion changes direction, typically caused by noisy audio input affecting the phase accumulator.

### Detection Logic

```cpp
/**
 * Detect direction reversal in phase delta
 *
 * Returns true if:
 * 1. Sign of phase_delta changed from previous frame
 * 2. Both deltas have non-zero magnitude
 * 3. Magnitude of change exceeds threshold
 */
static inline bool detectReversal(float prev_delta, float curr_delta) {
    // Ignore near-zero deltas (effect at rest)
    const float MIN_MAGNITUDE = 0.001f;
    if (fabsf(prev_delta) < MIN_MAGNITUDE ||
        fabsf(curr_delta) < MIN_MAGNITUDE) {
        return false;
    }

    // Check for sign change
    bool prev_positive = prev_delta > 0.0f;
    bool curr_positive = curr_delta > 0.0f;

    if (prev_positive == curr_positive) {
        return false;  // Same direction
    }

    // Verify magnitude of reversal
    float magnitude = fabsf(curr_delta - prev_delta);
    const float MIN_REVERSAL_MAGNITUDE = 0.01f;

    return magnitude > MIN_REVERSAL_MAGNITUDE;
}
```

### Counting Reversals

```python
def count_reversals(phase_deltas: np.ndarray,
                    min_magnitude: float = 0.01) -> int:
    """Count direction reversals in phase delta series."""
    reversals = 0

    for i in range(1, len(phase_deltas)):
        prev = phase_deltas[i-1]
        curr = phase_deltas[i]

        # Skip near-zero values
        if abs(prev) < 0.001 or abs(curr) < 0.001:
            continue

        # Check sign change
        if (prev > 0) != (curr > 0):
            # Verify magnitude
            if abs(curr - prev) > min_magnitude:
                reversals += 1

    return reversals
```

### Reversal Rate Calculation

```python
def reversal_rate(phase_deltas: np.ndarray,
                  duration_seconds: float) -> float:
    """Calculate reversals per minute."""
    count = count_reversals(phase_deltas)
    return (count / duration_seconds) * 60.0
```

### Thresholds

| Metric | Pass | Marginal | Fail |
|--------|------|----------|------|
| Reversals per minute | < 2.5 | 2.5-5.0 | > 5.0 |
| Consecutive reversals | 0 | 1 | > 1 |
| Reversal magnitude | < 0.02 | 0.02-0.05 | > 0.05 |

---

## Jerk Calculation Formula

### Definition

**Jerk** is the rate of change of acceleration, or the third derivative of position. High jerk values indicate abrupt changes in motion that appear visually jarring.

### Mathematical Formula

For discrete samples at constant time interval dt:

```
velocity[t]     = (phase[t] - phase[t-1]) / dt
acceleration[t] = (velocity[t] - velocity[t-1]) / dt
jerk[t]         = (acceleration[t] - acceleration[t-1]) / dt
```

Simplified for unit time (dt = 1 frame):

```
jerk[t] = phase_delta[t] - 2 * phase_delta[t-1] + phase_delta[t-2]
```

### Implementation

```cpp
/**
 * Compute jerk from three consecutive phase delta values
 *
 * @param delta0  Phase delta from 2 frames ago
 * @param delta1  Phase delta from 1 frame ago
 * @param delta2  Current phase delta
 * @return Jerk value (phase units / frame^3)
 */
static inline float computeJerk(float delta0, float delta1, float delta2) {
    // Second difference approximates jerk when dt=1
    return delta2 - 2.0f * delta1 + delta0;
}

/**
 * Compute absolute jerk magnitude
 */
static inline float computeJerkMagnitude(float delta0, float delta1, float delta2) {
    float jerk = computeJerk(delta0, delta1, delta2);
    return (jerk >= 0.0f) ? jerk : -jerk;
}
```

### Python Analysis

```python
import numpy as np

def compute_jerk_series(phase_deltas: np.ndarray) -> np.ndarray:
    """Compute jerk for entire series of phase deltas."""
    # Pad beginning with zeros
    padded = np.pad(phase_deltas, (2, 0), mode='constant')

    jerk = np.zeros(len(phase_deltas))
    for i in range(len(phase_deltas)):
        jerk[i] = padded[i+2] - 2*padded[i+1] + padded[i]

    return jerk

def jerk_statistics(phase_deltas: np.ndarray) -> dict:
    """Compute jerk statistics."""
    jerk = compute_jerk_series(phase_deltas)
    abs_jerk = np.abs(jerk)

    return {
        'avg_jerk': np.mean(jerk),
        'avg_abs_jerk': np.mean(abs_jerk),
        'max_abs_jerk': np.max(abs_jerk),
        'jerk_std': np.std(jerk),
        'jerk_percentile_95': np.percentile(abs_jerk, 95),
    }
```

### Interpretation

| avg_abs_jerk | Quality | Visual Perception |
|--------------|---------|-------------------|
| < 0.005 | Excellent | Perfectly smooth, undetectable jitter |
| 0.005 - 0.01 | Good | Very smooth, minor jitter not noticeable |
| 0.01 - 0.02 | Acceptable | Smooth, occasional visible jitter |
| 0.02 - 0.05 | Marginal | Noticeable jitter, somewhat distracting |
| > 0.05 | Poor | Obvious jitter, unacceptable |

### Unit Conversion

At 120 FPS:
```
1 phase_unit/frame^3 = 1 * 120^3 = 1,728,000 phase_units/sec^3

Target avg_abs_jerk < 0.01:
0.01 * 1,728,000 = 17,280 phase_units/sec^3
```

---

## Latency Measurement Methodology

### Definition

**Audio-to-visual latency** is the time delay between an audio transient occurring and the corresponding visual response appearing on the LEDs.

### Measurement Pipeline

```
Audio Transient                    Visual Response
      |                                  |
      v                                  v
[Audio Input] --> [DSP Pipeline] --> [Effect Render] --> [LED Output]
      |               |                    |                  |
      t0             t1                   t2                 t3

Latency Components:
- Audio capture latency: t1 - t0 (~5.8ms I2S DMA buffer)
- DSP processing latency: t2 - t1 (~0.5ms Goertzel + smoothing)
- Render latency: t3 - t2 (~10ms FastLED show)
- Smoothing latency: Variable (attack time constant)
```

### Transient Detection

**Audio transient (in samples):**
```python
def detect_audio_transients(energy_delta: np.ndarray,
                           threshold: float = 0.3) -> np.ndarray:
    """Find frames where audio transient occurred."""
    # Find peaks in energy_delta
    peaks = []
    for i in range(1, len(energy_delta) - 1):
        if (energy_delta[i] > threshold and
            energy_delta[i] > energy_delta[i-1] and
            energy_delta[i] > energy_delta[i+1]):
            peaks.append(i)
    return np.array(peaks)
```

**Visual response (from phase_delta change):**
```python
def detect_visual_responses(phase_delta: np.ndarray,
                           threshold: float = 0.005) -> np.ndarray:
    """Find frames where visual response occurred."""
    # Compute derivative of phase_delta (acceleration)
    accel = np.diff(phase_delta)

    # Find onset points
    onsets = []
    for i in range(1, len(accel)):
        if accel[i] > threshold and accel[i-1] <= threshold:
            onsets.append(i)
    return np.array(onsets)
```

### Latency Calculation

```python
def measure_latency(audio_transients: np.ndarray,
                   visual_responses: np.ndarray,
                   frame_period_ms: float = 8.33) -> np.ndarray:
    """Match audio transients to visual responses and compute latency."""
    latencies = []

    for audio_t in audio_transients:
        # Find first visual response after audio transient
        candidates = visual_responses[visual_responses > audio_t]
        if len(candidates) > 0:
            visual_t = candidates[0]
            latency_frames = visual_t - audio_t
            latency_ms = latency_frames * frame_period_ms
            if latency_ms < 200:  # Ignore if > 200ms (likely mismatch)
                latencies.append(latency_ms)

    return np.array(latencies)

def latency_statistics(latencies: np.ndarray) -> dict:
    """Compute latency statistics."""
    if len(latencies) == 0:
        return {'error': 'No latency measurements'}

    return {
        'avg_latency_ms': np.mean(latencies),
        'median_latency_ms': np.median(latencies),
        'p95_latency_ms': np.percentile(latencies, 95),
        'max_latency_ms': np.max(latencies),
        'min_latency_ms': np.min(latencies),
        'std_latency_ms': np.std(latencies),
        'sample_count': len(latencies),
    }
```

### Expected Latency Budget

| Stage | Time | Cumulative |
|-------|------|------------|
| I2S DMA buffer fill | 5.8 ms | 5.8 ms |
| Goertzel analysis | 0.5 ms | 6.3 ms |
| ControlBus smoothing | 0.1 ms | 6.4 ms |
| Attack smoothing (15%) | ~10 ms | 16.4 ms |
| Effect render | 0.5 ms | 16.9 ms |
| FastLED show | 9.6 ms | 26.5 ms |
| **Typical total** | | **25-35 ms** |

---

## Derived Metrics

### Smoothness Score

A composite metric from 0.0 (very jerky) to 1.0 (perfectly smooth):

```cpp
static inline float computeSmoothness(float delta0, float delta1, float delta2) {
    // Compute mean velocity
    float mean = (delta0 + delta1 + delta2) / 3.0f;

    // Handle near-zero mean
    float abs_mean = fabsf(mean);
    if (abs_mean < 0.0001f) {
        return 1.0f;  // Stationary is considered smooth
    }

    // Compute variance
    float d0 = delta0 - mean;
    float d1 = delta1 - mean;
    float d2 = delta2 - mean;
    float variance = (d0*d0 + d1*d1 + d2*d2) / 3.0f;

    // Standard deviation (approximation)
    float std_dev = sqrtf(variance);

    // Coefficient of variation
    float cv = std_dev / abs_mean;

    // Map to smoothness (CV=0 => 1.0, CV>=1 => 0.0)
    float smoothness = 1.0f - cv;
    return fmaxf(0.0f, fminf(1.0f, smoothness));
}
```

### Speed Consistency

Ratio of slew-limited speed to target speed:

```python
def speed_consistency(speed_raw: np.ndarray,
                     speed_smooth: np.ndarray) -> float:
    """Compute speed consistency metric (0-1)."""
    # Compute absolute difference
    diff = np.abs(speed_smooth - speed_raw)

    # Normalize by expected range
    expected_range = 0.8  # 1.4 - 0.6 for Scanner

    # Consistency = 1 - (avg_diff / expected_range)
    consistency = 1.0 - (np.mean(diff) / expected_range)
    return max(0.0, min(1.0, consistency))
```

### Directional Consistency

Percentage of frames moving in the expected direction:

```python
def directional_consistency(phase_deltas: np.ndarray) -> float:
    """Compute percentage of positive (forward) deltas."""
    non_zero = phase_deltas[np.abs(phase_deltas) > 0.001]
    if len(non_zero) == 0:
        return 1.0  # All zeros is considered consistent

    positive_count = np.sum(non_zero > 0)
    return positive_count / len(non_zero)
```

---

## Statistical Aggregations

### Per-Run Summary

```python
def compute_run_summary(samples: list[dict]) -> dict:
    """Compute summary statistics for a validation run."""
    df = pd.DataFrame(samples)

    return {
        'duration_seconds': (df['timestamp_us'].max() - df['timestamp_us'].min()) / 1e6,
        'sample_count': len(df),
        'frame_rate': len(df) / ((df['timestamp_us'].max() - df['timestamp_us'].min()) / 1e6),

        # Reversal metrics
        'total_reversals': df['reversal_count'].sum(),
        'reversal_rate_per_min': df['reversal_count'].sum() / (len(df) / 172.0) * 60,

        # Motion metrics
        'avg_phase_delta': df['phase_delta'].mean(),
        'std_phase_delta': df['phase_delta'].std(),
        'avg_abs_jerk': compute_jerk_series(df['phase_delta'].values).mean(),

        # Speed metrics
        'avg_speed_raw': df['speed_scale_raw'].mean(),
        'avg_speed_smooth': df['speed_scale_smooth'].mean(),
        'speed_consistency': speed_consistency(
            df['speed_scale_raw'].values,
            df['speed_scale_smooth'].values
        ),

        # Audio metrics
        'avg_energy': df['energy_avg'].mean(),
        'avg_energy_delta': df['energy_delta'].mean(),
        'dominant_band_mode': df['dominant_freq_bin'].mode().iloc[0],

        # Quality scores
        'smoothness_score': compute_smoothness_series(df['phase_delta'].values).mean(),
        'directional_consistency': directional_consistency(df['phase_delta'].values),
    }
```

### Comparative Analysis

```python
def compare_runs(run_a: dict, run_b: dict) -> dict:
    """Compare two validation runs statistically."""
    from scipy import stats

    results = {}

    for metric in ['avg_abs_jerk', 'total_reversals', 'smoothness_score']:
        a_vals = run_a['samples'][metric]
        b_vals = run_b['samples'][metric]

        # t-test
        t_stat, p_value = stats.ttest_ind(a_vals, b_vals)

        # Cohen's d effect size
        pooled_std = np.sqrt((np.std(a_vals)**2 + np.std(b_vals)**2) / 2)
        cohens_d = (np.mean(a_vals) - np.mean(b_vals)) / pooled_std

        results[metric] = {
            'a_mean': np.mean(a_vals),
            'b_mean': np.mean(b_vals),
            'difference': np.mean(b_vals) - np.mean(a_vals),
            'percent_change': (np.mean(b_vals) - np.mean(a_vals)) / np.mean(a_vals) * 100,
            't_statistic': t_stat,
            'p_value': p_value,
            'cohens_d': cohens_d,
            'significant': p_value < 0.05,
        }

    return results
```

---

## Reference Tables

### Effect IDs

| ID | Effect Name | Key Metrics |
|----|-------------|-------------|
| 16 | LGPInterferenceScannerEffect | phase, speed_scale, reversal_count |
| 17 | LGPWaveCollisionEffect | phase, speed_scale, reversal_count |
| 18 | AudioBloomEffect | scroll_phase, energy_avg |
| 19 | AudioWaveformEffect | energy_avg, energy_delta |

### Threshold Reference

| Metric | Excellent | Good | Acceptable | Fail |
|--------|-----------|------|------------|------|
| reversals (120s) | 0 | 1-2 | 3-4 | >= 5 |
| avg_abs_jerk | < 0.005 | < 0.008 | < 0.01 | >= 0.01 |
| smoothness_score | > 0.95 | > 0.90 | > 0.85 | <= 0.85 |
| p95_latency_ms | < 30 | < 40 | < 50 | >= 50 |
| directional_consistency | > 0.99 | > 0.97 | > 0.95 | <= 0.95 |

### Binary Struct Format Codes

| Type | Python struct | Size | Notes |
|------|---------------|------|-------|
| uint8 | B | 1 | Unsigned byte |
| uint16 | H | 2 | Little-endian unsigned short |
| uint32 | I | 4 | Little-endian unsigned int |
| float32 | f | 4 | IEEE 754 single precision |
| padding | x | 1 | Skip byte |

---

## Related Documentation

- [AUDIO_TEST_HARNESS.md](./AUDIO_TEST_HARNESS.md) - Framework overview
- [TEST_SCENARIOS.md](./TEST_SCENARIOS.md) - Test procedures
- [EffectValidationMetrics.h](/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/validation/EffectValidationMetrics.h) - Source implementation

---

*End of Metrics Reference Documentation*
