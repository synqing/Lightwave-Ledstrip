"""
Motion Analyzer for LightwaveOS Audio Effect Validation.

Detects jog-dial behavior (bidirectional oscillation) by analyzing phase_delta
sign reversals over time. Proper audio-reactive motion should be monotonic
or smoothly varying, not oscillating back and forth.
"""

from dataclasses import dataclass
from typing import Sequence

import numpy as np

from ..models import EffectValidationSample


@dataclass
class MotionResult:
    """
    Result of motion analysis for jog-dial detection.

    Attributes:
        passed: True if motion is smooth (< threshold reversals)
        reversal_count: Number of sign changes in phase_delta
        total_samples: Number of samples analyzed
        reversal_rate: Reversals per second
        reversal_indices: Sample indices where reversals occurred
        duration_seconds: Total duration of analyzed samples
        details: Human-readable description of findings
    """

    passed: bool
    reversal_count: int
    total_samples: int
    reversal_rate: float
    reversal_indices: list[int]
    duration_seconds: float
    details: str


class MotionAnalyzer:
    """
    Analyzes phase_delta time series for jog-dial detection.

    The jog-dial problem occurs when audio-reactive effects oscillate
    bidirectionally instead of moving smoothly. This is detected by
    counting sign reversals in the phase_delta sequence.

    Algorithm:
    1. Extract phase_delta values from all samples
    2. Count transitions from positive to negative or vice versa
    3. Ignore near-zero deltas (< epsilon) to avoid noise
    4. Pass if reversal count is below threshold for duration

    Pass criteria (default):
    - Less than 5 reversals in 120 seconds
    - Configurable via max_reversals parameter
    """

    def __init__(
        self,
        max_reversals: int = 5,
        epsilon: float = 0.001,
        expected_duration_seconds: float = 120.0,
    ) -> None:
        """
        Initialize motion analyzer.

        Args:
            max_reversals: Maximum allowed sign reversals (default: 5)
            epsilon: Minimum absolute phase_delta to consider (default: 0.001)
            expected_duration_seconds: Expected capture duration (default: 120)
        """
        self.max_reversals = max_reversals
        self.epsilon = epsilon
        self.expected_duration_seconds = expected_duration_seconds

    def analyze(self, samples: Sequence[EffectValidationSample]) -> MotionResult:
        """
        Analyze phase_delta series for jog-dial behavior.

        Args:
            samples: Sequence of validation samples with phase_delta values

        Returns:
            MotionResult with pass/fail status and detailed metrics

        Raises:
            ValueError: If samples list is empty or too short
        """
        if not samples:
            raise ValueError("Cannot analyze empty sample list")

        if len(samples) < 10:
            raise ValueError(f"Need at least 10 samples for analysis, got {len(samples)}")

        # Extract phase_delta values
        phase_deltas = np.array([s.phase_delta for s in samples])

        # Calculate duration from timestamps
        if len(samples) > 1:
            duration_ms = samples[-1].timestamp - samples[0].timestamp
            duration_seconds = duration_ms / 1000.0
        else:
            duration_seconds = 0.0

        # Find significant deltas (above epsilon threshold)
        significant_mask = np.abs(phase_deltas) > self.epsilon
        significant_deltas = phase_deltas[significant_mask]
        significant_indices = np.where(significant_mask)[0]

        if len(significant_deltas) < 2:
            # Not enough motion to analyze
            return MotionResult(
                passed=True,
                reversal_count=0,
                total_samples=len(samples),
                reversal_rate=0.0,
                reversal_indices=[],
                duration_seconds=duration_seconds,
                details="Insufficient motion detected (all deltas near zero)",
            )

        # Detect sign changes (jog-dial behavior)
        # A sign change occurs when sign(delta[i]) != sign(delta[i+1])
        signs = np.sign(significant_deltas)
        sign_changes = np.diff(signs) != 0
        reversal_count = int(np.sum(sign_changes))

        # Get indices where reversals occurred (in original sample space)
        reversal_positions = significant_indices[1:][sign_changes]
        reversal_indices = reversal_positions.tolist()

        # Calculate reversal rate
        reversal_rate = (
            reversal_count / duration_seconds if duration_seconds > 0 else 0.0
        )

        # Determine pass/fail
        passed = reversal_count <= self.max_reversals

        # Build details message
        if passed:
            details = (
                f"Motion is smooth with {reversal_count} reversals "
                f"over {duration_seconds:.1f}s ({reversal_rate:.3f} reversals/sec)"
            )
        else:
            details = (
                f"Jog-dial detected: {reversal_count} reversals "
                f"over {duration_seconds:.1f}s ({reversal_rate:.3f} reversals/sec) "
                f"exceeds threshold of {self.max_reversals}"
            )

        return MotionResult(
            passed=passed,
            reversal_count=reversal_count,
            total_samples=len(samples),
            reversal_rate=reversal_rate,
            reversal_indices=reversal_indices,
            duration_seconds=duration_seconds,
            details=details,
        )

    def get_reversal_timestamps(
        self, samples: Sequence[EffectValidationSample], result: MotionResult
    ) -> list[int]:
        """
        Get timestamps (ms) of reversal events.

        Args:
            samples: Original sample sequence
            result: MotionResult from analyze()

        Returns:
            List of timestamps in milliseconds where reversals occurred
        """
        return [samples[idx].timestamp for idx in result.reversal_indices]
