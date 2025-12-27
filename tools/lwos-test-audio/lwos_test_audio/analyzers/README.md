# LightwaveOS Audio Effect Analyzers

This package provides specialized analyzers for detecting common audio-reactive effect issues in LightwaveOS.

## Overview

The analyzers validate audio-reactive effects by analyzing time series data from WebSocket binary frames. Each analyzer focuses on a specific quality metric:

- **MotionAnalyzer**: Detects jog-dial behavior (bidirectional oscillation)
- **SmoothnessAnalyzer**: Measures motion smoothness using jerk analysis
- **LatencyAnalyzer**: Measures audio-to-visual latency

## Analyzers

### MotionAnalyzer

**Purpose**: Detect jog-dial behavior where effects oscillate back and forth instead of moving smoothly.

**Algorithm**:
1. Extract `phase_delta` sequence from samples
2. Count sign reversals (positive → negative or vice versa)
3. Filter out near-zero deltas to avoid noise
4. Compare reversal count to threshold

**Pass Criteria** (default):
- Less than 5 reversals in 120 seconds
- Configurable via `max_reversals` parameter

**Usage**:
```python
from lwos_test_audio.analyzers import MotionAnalyzer

analyzer = MotionAnalyzer(max_reversals=5, epsilon=0.001)
result = analyzer.analyze(samples)

print(f"Passed: {result.passed}")
print(f"Reversals: {result.reversal_count}")
print(f"Details: {result.details}")
```

**Result Fields**:
- `passed`: bool - True if test passed
- `reversal_count`: int - Number of sign changes detected
- `reversal_rate`: float - Reversals per second
- `reversal_indices`: list[int] - Sample indices where reversals occurred
- `duration_seconds`: float - Total duration analyzed
- `details`: str - Human-readable description

### SmoothnessAnalyzer

**Purpose**: Detect jitter and stuttering by measuring jerk (rate of change of acceleration).

**Algorithm**:
1. Extract `phase_delta` sequence (velocity)
2. Compute acceleration: `diff(phase_delta)`
3. Compute jerk: `diff(acceleration)`
4. Calculate average absolute jerk
5. Compute consistency score (% frames with expected delta)

**Pass Criteria** (default):
- Average absolute jerk < 0.01
- Consistency score > 70%

**Usage**:
```python
from lwos_test_audio.analyzers import SmoothnessAnalyzer

analyzer = SmoothnessAnalyzer(
    jerk_threshold=0.01,
    consistency_threshold=70.0,
    delta_tolerance=0.05
)
result = analyzer.analyze(samples)

print(f"Passed: {result.passed}")
print(f"Avg Jerk: {result.avg_abs_jerk:.6f}")
print(f"Consistency: {result.consistency_score:.1f}%")
```

**Result Fields**:
- `passed`: bool - True if test passed
- `avg_abs_jerk`: float - Average absolute jerk magnitude
- `max_jerk`: float - Maximum jerk observed
- `consistency_score`: float - Percentage of consistent frames (0-100)
- `jerk_threshold`: float - Threshold used for pass/fail
- `duration_seconds`: float - Total duration analyzed
- `details`: str - Human-readable description

**Helper Methods**:
- `get_jerk_series(samples)` - Returns full jerk time series for plotting
- `get_consistency_series(samples)` - Returns per-frame consistency flags

### LatencyAnalyzer

**Purpose**: Measure audio-to-visual latency by detecting transient response times.

**Algorithm**:
1. Detect audio transients (energy spikes > threshold)
2. For each transient, scan forward in time
3. Find first significant `energy_delta` increase (visual response)
4. Measure time difference

**Pass Criteria** (default):
- Maximum latency < 50ms for any event
- At least 80% of events have detectable responses

**Usage**:
```python
from lwos_test_audio.analyzers import LatencyAnalyzer

analyzer = LatencyAnalyzer(
    latency_threshold_ms=50.0,
    audio_event_threshold=0.3,
    visual_response_threshold=0.1
)
result = analyzer.analyze(samples)

print(f"Passed: {result.passed}")
print(f"Avg Latency: {result.avg_latency_ms:.1f}ms")
print(f"Response Rate: {result.response_rate:.1f}%")
```

**Result Fields**:
- `passed`: bool - True if test passed
- `measurements`: list[LatencyMeasurement] - Individual measurements
- `avg_latency_ms`: Optional[float] - Average latency
- `max_latency_ms`: Optional[float] - Maximum latency
- `min_latency_ms`: Optional[float] - Minimum latency
- `latency_threshold_ms`: float - Threshold used for pass/fail
- `events_detected`: int - Number of audio events found
- `events_responded`: int - Number with visual response
- `response_rate`: float - Percentage with response (0-100)
- `details`: str - Human-readable description

**Auto-Detection**:
The analyzer can auto-detect audio events from `energy_delta` spikes, or you can provide pre-detected events:

```python
from lwos_test_audio.analyzers import AudioEvent

events = [
    AudioEvent(sample_index=10, timestamp_ms=160, energy_magnitude=0.5),
    AudioEvent(sample_index=50, timestamp_ms=800, energy_magnitude=0.7),
]

result = analyzer.analyze(samples, audio_events=events)
```

## Data Models

All analyzers operate on `EffectValidationSample` dataclasses:

```python
@dataclass
class EffectValidationSample:
    timestamp: int              # Milliseconds
    effect_id: int
    effect_type: EffectType
    audio_metrics: AudioMetrics
    performance_metrics: PerformanceMetrics

    # Derived motion metrics (computed by collector)
    phase: float = 0.0          # Position (0..1, wraps)
    phase_delta: float = 0.0    # Velocity (-1..1)
    energy: float = 0.0         # Energy level (0..1)
    energy_delta: float = 0.0   # Energy change
```

## Example: Complete Validation

```python
from lwos_test_audio.analyzers import (
    MotionAnalyzer,
    SmoothnessAnalyzer,
    LatencyAnalyzer,
)
from lwos_test_audio.report import ValidationReport
from lwos_test_audio.models import ValidationSession

# Assume samples were collected via ValidationCollector
# samples: list[EffectValidationSample]

# Create analyzers
motion = MotionAnalyzer()
smoothness = SmoothnessAnalyzer()
latency = LatencyAnalyzer()

# Run analyses
motion_result = motion.analyze(samples)
smoothness_result = smoothness.analyze(samples)
latency_result = latency.analyze(samples)

# Generate report
session = ValidationSession(
    session_id="test-001",
    effect_name="Audio Wave",
    started_at=datetime.now(),
    device_url="ws://lightwaveos.local/ws",
    total_samples=len(samples),
    actual_duration_seconds=120.0,
)

report = ValidationReport(session)
report.print_terminal_report(
    motion_result=motion_result,
    smoothness_result=smoothness_result,
    latency_result=latency_result,
)

# Save JSON summary
summary = report.generate_json_summary(
    motion_result=motion_result,
    smoothness_result=smoothness_result,
    latency_result=latency_result,
    output_path=Path("validation_results.json"),
)

# Save HTML report
report.generate_html_report(
    output_path=Path("validation_report.html"),
    motion_result=motion_result,
    smoothness_result=smoothness_result,
    latency_result=latency_result,
)

# Check overall pass/fail
if summary["overall_pass"]:
    print("✓ Validation PASSED")
else:
    print("✗ Validation FAILED")
```

## Configuration

All analyzers support customizable thresholds:

```python
# Strict motion requirements (no reversals allowed)
motion = MotionAnalyzer(max_reversals=0, epsilon=0.0001)

# Relaxed smoothness (allow more jerk)
smoothness = SmoothnessAnalyzer(
    jerk_threshold=0.05,
    consistency_threshold=50.0
)

# Tight latency requirements
latency = LatencyAnalyzer(
    latency_threshold_ms=30.0,
    min_response_rate=95.0
)
```

## Performance

All analyzers use NumPy for efficient numerical operations:

- **MotionAnalyzer**: O(n) time, O(n) space
- **SmoothnessAnalyzer**: O(n) time, O(n) space
- **LatencyAnalyzer**: O(n × m) time where m = avg events per sample

Typical performance for 120-second capture (7500 samples @ 62.5Hz):
- MotionAnalyzer: ~5ms
- SmoothnessAnalyzer: ~10ms
- LatencyAnalyzer: ~20ms (depends on event count)

## Dependencies

- `numpy>=1.26.0` - Numerical operations
- `rich>=13.7.0` - Terminal output (report.py only)
- `scipy>=1.11.0` - Statistical functions (future use)

## Testing

Run the standalone test:

```bash
cd tools/lwos-test-audio
python3 test_analyzers_standalone.py
```

This creates dummy samples and validates all analyzer functionality without requiring device connection.

## Future Enhancements

Potential additions:

1. **FrequencyAnalyzer**: Validate frequency response accuracy
2. **TemporalConsistencyAnalyzer**: Detect frame drops and timing jitter
3. **ColorAccuracyAnalyzer**: Validate color mapping from audio features
4. **MultiEffectAnalyzer**: Compare multiple effect runs
5. **RegressionAnalyzer**: Compare against baseline captures

## References

- [Audio Benchmark CI Documentation](../../../../docs/ci_cd/AUDIO_BENCHMARK_CI.md)
- [LightwaveOS Audio API](../../../../docs/api/AUDIO_CONTROL_API.md)
- [ControlBus Implementation](../../../../v2/src/audio/contracts/ControlBus.h)
