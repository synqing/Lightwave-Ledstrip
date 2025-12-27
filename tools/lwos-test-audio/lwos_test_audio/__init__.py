"""
LightwaveOS Audio Effect Testing Framework

Provides tools for capturing, analyzing, and validating audio-reactive LED effects
from LightwaveOS devices via WebSocket binary frames.
"""

__version__ = "0.1.0"

from lwos_test_audio.models import (
    EffectValidationSample,
    AudioMetrics,
    PerformanceMetrics,
    ValidationSession,
)
from lwos_test_audio.collector import ValidationCollector
from lwos_test_audio.analyzer import ValidationAnalyzer

__all__ = [
    "__version__",
    "EffectValidationSample",
    "AudioMetrics",
    "PerformanceMetrics",
    "ValidationSession",
    "ValidationCollector",
    "ValidationAnalyzer",
]
