"""
Smoothness Analyzer for LightwaveOS Audio Effect Validation.

Analyzes visual motion smoothness by computing jerk (rate of change of
acceleration) from phase and phase_delta time series. Also computes
consistency scores to detect frame-to-frame variation issues.
"""

from dataclasses import dataclass
from typing import Sequence

import numpy as np

from ..models import EffectValidationSample


@dataclass
class SmoothnessResult:
    """
    Result of smoothness analysis.

    Attributes:
        passed: True if motion is smooth (avg_abs_jerk < threshold)
        avg_abs_jerk: Average absolute jerk magnitude
        max_jerk: Maximum jerk value observed
        consistency_score: Percentage of frames with expected delta (0-100)
        jerk_threshold: Threshold used for pass/fail
        total_samples: Number of samples analyzed
        duration_seconds: Total duration of analyzed samples
        details: Human-readable description of findings
    """

    passed: bool
    avg_abs_jerk: float
    max_jerk: float
    consistency_score: float
    jerk_threshold: float
    total_samples: int
    duration_seconds: float
    details: str


class SmoothnessAnalyzer:
    """
    Analyzes motion smoothness using jerk (derivative of acceleration).

    Jerk is the third derivative of position:
    - Position: phase
    - Velocity: phase_delta (1st derivative)
    - Acceleration: diff(phase_delta) (2nd derivative)
    - Jerk: diff(diff(phase_delta)) (3rd derivative)

    High jerk indicates abrupt changes in motion, which appear as
    stuttering or jittery visual effects.

    Algorithm:
    1. Extract phase_delta sequence
    2. Compute acceleration as diff(phase_delta)
    3. Compute jerk as diff(acceleration)
    4. Calculate average absolute jerk
    5. Compute consistency score (% frames with consistent delta)
    6. Pass if avg_abs_jerk < threshold

    Pass criteria (default):
    - Average absolute jerk < 0.01
    - Consistency score > 70%
    """

    def __init__(
        self,
        jerk_threshold: float = 0.01,
        consistency_threshold: float = 70.0,
        delta_tolerance: float = 0.05,
    ) -> None:
        """
        Initialize smoothness analyzer.

        Args:
            jerk_threshold: Maximum allowed average absolute jerk (default: 0.01)
            consistency_threshold: Minimum consistency score % (default: 70)
            delta_tolerance: Tolerance for "consistent" delta detection (default: 0.05)
        """
        self.jerk_threshold = jerk_threshold
        self.consistency_threshold = consistency_threshold
        self.delta_tolerance = delta_tolerance

    def analyze(self, samples: Sequence[EffectValidationSample]) -> SmoothnessResult:
        """
        Analyze motion smoothness from phase and phase_delta series.

        Args:
            samples: Sequence of validation samples

        Returns:
            SmoothnessResult with pass/fail status and detailed metrics

        Raises:
            ValueError: If samples list is too short for jerk calculation
        """
        if not samples:
            raise ValueError("Cannot analyze empty sample list")

        if len(samples) < 4:
            raise ValueError(f"Need at least 4 samples for jerk analysis, got {len(samples)}")

        # Extract phase_delta sequence
        phase_deltas = np.array([s.phase_delta for s in samples])

        # Calculate duration
        if len(samples) > 1:
            duration_ms = samples[-1].timestamp - samples[0].timestamp
            duration_seconds = duration_ms / 1000.0
        else:
            duration_seconds = 0.0

        # Compute acceleration (2nd derivative of phase)
        # acceleration[i] = phase_delta[i+1] - phase_delta[i]
        acceleration = np.diff(phase_deltas)

        # Compute jerk (3rd derivative of phase)
        # jerk[i] = acceleration[i+1] - acceleration[i]
        jerk = np.diff(acceleration)

        # Calculate average absolute jerk
        avg_abs_jerk = float(np.mean(np.abs(jerk)))
        max_jerk = float(np.max(np.abs(jerk)))

        # Compute consistency score
        # A frame is "consistent" if its delta is within tolerance of the median delta
        median_delta = np.median(np.abs(phase_deltas))
        consistent_frames = np.abs(np.abs(phase_deltas) - median_delta) < self.delta_tolerance
        consistency_score = float(np.mean(consistent_frames) * 100.0)

        # Determine pass/fail
        jerk_passed = avg_abs_jerk < self.jerk_threshold
        consistency_passed = consistency_score >= self.consistency_threshold
        passed = jerk_passed and consistency_passed

        # Build details message
        if passed:
            details = (
                f"Motion is smooth: avg_jerk={avg_abs_jerk:.6f}, "
                f"max_jerk={max_jerk:.6f}, "
                f"consistency={consistency_score:.1f}%"
            )
        else:
            issues = []
            if not jerk_passed:
                issues.append(
                    f"high jerk ({avg_abs_jerk:.6f} > {self.jerk_threshold})"
                )
            if not consistency_passed:
                issues.append(
                    f"low consistency ({consistency_score:.1f}% < {self.consistency_threshold}%)"
                )
            details = f"Smoothness issues detected: {', '.join(issues)}"

        return SmoothnessResult(
            passed=passed,
            avg_abs_jerk=avg_abs_jerk,
            max_jerk=max_jerk,
            consistency_score=consistency_score,
            jerk_threshold=self.jerk_threshold,
            total_samples=len(samples),
            duration_seconds=duration_seconds,
            details=details,
        )

    def get_jerk_series(self, samples: Sequence[EffectValidationSample]) -> np.ndarray:
        """
        Compute full jerk time series for plotting/debugging.

        Args:
            samples: Sequence of validation samples

        Returns:
            Numpy array of jerk values (length = len(samples) - 3)
        """
        phase_deltas = np.array([s.phase_delta for s in samples])
        acceleration = np.diff(phase_deltas)
        jerk = np.diff(acceleration)
        return jerk

    def get_consistency_series(
        self, samples: Sequence[EffectValidationSample]
    ) -> np.ndarray:
        """
        Compute per-frame consistency flags (True if within tolerance of median).

        Args:
            samples: Sequence of validation samples

        Returns:
            Boolean numpy array indicating consistent frames
        """
        phase_deltas = np.array([s.phase_delta for s in samples])
        median_delta = np.median(np.abs(phase_deltas))
        return np.abs(np.abs(phase_deltas) - median_delta) < self.delta_tolerance
