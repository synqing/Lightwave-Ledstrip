#!/usr/bin/env python3
"""
Standalone test for analyzer modules.
Tests that analyzers can be imported and instantiated without full package dependencies.
"""

import sys
from pathlib import Path

# Add package to path
sys.path.insert(0, str(Path(__file__).parent))

from lwos_test_audio.analyzers.motion_analyzer import MotionAnalyzer, MotionResult
from lwos_test_audio.analyzers.smoothness_analyzer import SmoothnessAnalyzer, SmoothnessResult
from lwos_test_audio.analyzers.latency_analyzer import LatencyAnalyzer, LatencyResult, AudioEvent
from lwos_test_audio.models import EffectValidationSample, AudioMetrics, PerformanceMetrics, EffectType

print("✓ All analyzer modules imported successfully")

# Test instantiation
motion = MotionAnalyzer()
smoothness = SmoothnessAnalyzer()
latency = LatencyAnalyzer()

print("✓ All analyzers instantiated successfully")
print(f"  - MotionAnalyzer: max_reversals={motion.max_reversals}")
print(f"  - SmoothnessAnalyzer: jerk_threshold={smoothness.jerk_threshold}")
print(f"  - LatencyAnalyzer: latency_threshold_ms={latency.latency_threshold_ms}")

# Create dummy samples for testing
dummy_audio = AudioMetrics(
    bass_energy=0.5, mid_energy=0.6, treble_energy=0.4,
    total_energy=0.5, peak_frequency=440.0,
    spectral_centroid=1000.0, spectral_spread=500.0,
    zero_crossing_rate=0.3
)

dummy_perf = PerformanceMetrics(
    frame_time_us=15000, render_time_us=12000, fps=60.0,
    heap_free=100000, heap_fragmentation=0.1, cpu_usage=35.0
)

# Create test samples with varying phase_delta (no jog-dial)
samples = []
for i in range(100):
    sample = EffectValidationSample(
        timestamp=i * 16,  # 16ms intervals
        effect_id=1,
        effect_type=EffectType.AUDIO_REACTIVE,
        audio_metrics=dummy_audio,
        performance_metrics=dummy_perf,
        effect_data=b'\x00' * 56,
        phase=i * 0.01 % 1.0,
        phase_delta=0.01,  # Constant delta (smooth motion)
        energy=0.5 + 0.1 * (i % 10) / 10.0,
        energy_delta=0.01
    )
    samples.append(sample)

print(f"\n✓ Created {len(samples)} test samples")

# Test motion analyzer
print("\nTesting MotionAnalyzer...")
motion_result = motion.analyze(samples)
print(f"  Result: {'PASS' if motion_result.passed else 'FAIL'}")
print(f"  Reversals: {motion_result.reversal_count}")
print(f"  Details: {motion_result.details}")

# Test smoothness analyzer
print("\nTesting SmoothnessAnalyzer...")
smoothness_result = smoothness.analyze(samples)
print(f"  Result: {'PASS' if smoothness_result.passed else 'FAIL'}")
print(f"  Avg Jerk: {smoothness_result.avg_abs_jerk:.6f}")
print(f"  Details: {smoothness_result.details}")

# Test latency analyzer
print("\nTesting LatencyAnalyzer...")
latency_result = latency.analyze(samples)
print(f"  Result: {'PASS' if latency_result.passed else 'FAIL'}")
print(f"  Events: {latency_result.events_detected}")
print(f"  Details: {latency_result.details}")

print("\n✓ All analyzer tests completed successfully!")
