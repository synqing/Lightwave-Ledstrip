"""
Analyzers for LightwaveOS Audio Effect Validation.

This package provides specialized analyzers for detecting common audio-reactive
effect issues:
- Motion jog-dial behavior (bidirectional oscillation)
- Smoothness problems (jitter, stuttering)
- Audio-to-visual latency
"""

from .latency_analyzer import (
    AudioEvent,
    LatencyAnalyzer,
    LatencyMeasurement,
    LatencyResult,
)
from .motion_analyzer import MotionAnalyzer, MotionResult
from .smoothness_analyzer import SmoothnessAnalyzer, SmoothnessResult

__all__ = [
    # Motion analyzer
    "MotionAnalyzer",
    "MotionResult",
    # Smoothness analyzer
    "SmoothnessAnalyzer",
    "SmoothnessResult",
    # Latency analyzer
    "LatencyAnalyzer",
    "LatencyResult",
    "LatencyMeasurement",
    "AudioEvent",
]
