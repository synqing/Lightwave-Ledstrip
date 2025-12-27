"""
Latency Analyzer for LightwaveOS Audio Effect Validation.

Measures audio-to-visual latency by detecting transient events in the
audio signal (energy spikes) and measuring the time until a corresponding
visual response (energy_delta increase) is observed.
"""

from dataclasses import dataclass
from typing import Optional, Sequence

import numpy as np

from ..models import EffectValidationSample


@dataclass
class AudioEvent:
    """
    Detected audio transient event.

    Attributes:
        sample_index: Index in the sample sequence
        timestamp_ms: Timestamp in milliseconds
        energy_magnitude: Size of the energy spike
    """

    sample_index: int
    timestamp_ms: int
    energy_magnitude: float


@dataclass
class LatencyMeasurement:
    """
    Single latency measurement from audio event to visual response.

    Attributes:
        audio_event: The detected audio transient
        visual_response_index: Sample index where visual response detected
        visual_response_timestamp_ms: Timestamp of visual response
        latency_ms: Measured latency in milliseconds
        passed: True if latency < threshold
    """

    audio_event: AudioEvent
    visual_response_index: Optional[int]
    visual_response_timestamp_ms: Optional[int]
    latency_ms: Optional[float]
    passed: bool


@dataclass
class LatencyResult:
    """
    Result of latency analysis across all detected events.

    Attributes:
        passed: True if all latencies < threshold
        measurements: List of individual latency measurements
        avg_latency_ms: Average latency across all events
        max_latency_ms: Maximum latency observed
        min_latency_ms: Minimum latency observed
        latency_threshold_ms: Threshold used for pass/fail
        events_detected: Number of audio events detected
        events_responded: Number of events with visual response
        response_rate: Percentage of events with response (0-100)
        total_samples: Total number of samples analyzed
        duration_seconds: Total duration of analyzed samples
        details: Human-readable description of findings
    """

    passed: bool
    measurements: list[LatencyMeasurement]
    avg_latency_ms: Optional[float]
    max_latency_ms: Optional[float]
    min_latency_ms: Optional[float]
    latency_threshold_ms: float
    events_detected: int
    events_responded: int
    response_rate: float
    total_samples: int
    duration_seconds: float
    details: str


class LatencyAnalyzer:
    """
    Measures audio-to-visual latency from transient detection.

    Algorithm:
    1. Detect audio transients (energy jumps > threshold)
    2. For each transient, scan forward in time
    3. Find first significant energy_delta increase
    4. Measure time difference
    5. Pass if all latencies < threshold

    Pass criteria (default):
    - Maximum latency < 50ms for any event
    - At least 80% of events have detectable responses
    """

    def __init__(
        self,
        latency_threshold_ms: float = 50.0,
        audio_event_threshold: float = 0.3,
        visual_response_threshold: float = 0.1,
        response_search_window_ms: float = 200.0,
        min_response_rate: float = 80.0,
    ) -> None:
        """
        Initialize latency analyzer.

        Args:
            latency_threshold_ms: Maximum acceptable latency (default: 50ms)
            audio_event_threshold: Minimum energy jump to detect transient (default: 0.3)
            visual_response_threshold: Minimum energy_delta for visual response (default: 0.1)
            response_search_window_ms: How far ahead to search for response (default: 200ms)
            min_response_rate: Minimum % of events with response (default: 80%)
        """
        self.latency_threshold_ms = latency_threshold_ms
        self.audio_event_threshold = audio_event_threshold
        self.visual_response_threshold = visual_response_threshold
        self.response_search_window_ms = response_search_window_ms
        self.min_response_rate = min_response_rate

    def analyze(
        self,
        samples: Sequence[EffectValidationSample],
        audio_events: Optional[list[AudioEvent]] = None,
    ) -> LatencyResult:
        """
        Analyze audio-to-visual latency.

        Args:
            samples: Sequence of validation samples
            audio_events: Pre-detected audio events (optional, will auto-detect if None)

        Returns:
            LatencyResult with pass/fail status and detailed measurements

        Raises:
            ValueError: If samples list is empty or too short
        """
        if not samples:
            raise ValueError("Cannot analyze empty sample list")

        if len(samples) < 10:
            raise ValueError(f"Need at least 10 samples for analysis, got {len(samples)}")

        # Calculate duration
        duration_ms = samples[-1].timestamp - samples[0].timestamp
        duration_seconds = duration_ms / 1000.0

        # Auto-detect audio events if not provided
        if audio_events is None:
            audio_events = self._detect_audio_events(samples)

        if not audio_events:
            # No audio events detected
            return LatencyResult(
                passed=True,
                measurements=[],
                avg_latency_ms=None,
                max_latency_ms=None,
                min_latency_ms=None,
                latency_threshold_ms=self.latency_threshold_ms,
                events_detected=0,
                events_responded=0,
                response_rate=0.0,
                total_samples=len(samples),
                duration_seconds=duration_seconds,
                details="No audio transients detected in capture",
            )

        # Measure latency for each event
        measurements = []
        for event in audio_events:
            measurement = self._measure_event_latency(samples, event)
            measurements.append(measurement)

        # Calculate statistics
        successful_measurements = [
            m for m in measurements if m.latency_ms is not None
        ]

        if successful_measurements:
            latencies = [m.latency_ms for m in successful_measurements]
            avg_latency_ms = float(np.mean(latencies))
            max_latency_ms = float(np.max(latencies))
            min_latency_ms = float(np.min(latencies))
        else:
            avg_latency_ms = None
            max_latency_ms = None
            min_latency_ms = None

        events_detected = len(audio_events)
        events_responded = len(successful_measurements)
        response_rate = (events_responded / events_detected * 100.0) if events_detected > 0 else 0.0

        # Determine pass/fail
        latency_passed = (
            max_latency_ms is None or max_latency_ms <= self.latency_threshold_ms
        )
        response_rate_passed = response_rate >= self.min_response_rate
        passed = latency_passed and response_rate_passed

        # Build details message
        if passed:
            if avg_latency_ms is not None:
                details = (
                    f"Latency acceptable: avg={avg_latency_ms:.1f}ms, "
                    f"max={max_latency_ms:.1f}ms, "
                    f"response_rate={response_rate:.1f}%"
                )
            else:
                details = "No audio events to measure latency"
        else:
            issues = []
            if not latency_passed and max_latency_ms is not None:
                issues.append(
                    f"high latency (max={max_latency_ms:.1f}ms > {self.latency_threshold_ms}ms)"
                )
            if not response_rate_passed:
                issues.append(
                    f"low response rate ({response_rate:.1f}% < {self.min_response_rate}%)"
                )
            details = f"Latency issues detected: {', '.join(issues)}"

        return LatencyResult(
            passed=passed,
            measurements=measurements,
            avg_latency_ms=avg_latency_ms,
            max_latency_ms=max_latency_ms,
            min_latency_ms=min_latency_ms,
            latency_threshold_ms=self.latency_threshold_ms,
            events_detected=events_detected,
            events_responded=events_responded,
            response_rate=response_rate,
            total_samples=len(samples),
            duration_seconds=duration_seconds,
            details=details,
        )

    def _detect_audio_events(
        self, samples: Sequence[EffectValidationSample]
    ) -> list[AudioEvent]:
        """
        Auto-detect audio transient events from energy_delta spikes.

        Args:
            samples: Sequence of validation samples

        Returns:
            List of detected AudioEvent objects
        """
        events = []

        # Extract energy_delta series
        energy_deltas = np.array([s.energy_delta for s in samples])

        # Find peaks (local maxima above threshold)
        for i in range(1, len(energy_deltas) - 1):
            if energy_deltas[i] > self.audio_event_threshold:
                # Check if it's a local maximum
                if (
                    energy_deltas[i] > energy_deltas[i - 1]
                    and energy_deltas[i] >= energy_deltas[i + 1]
                ):
                    events.append(
                        AudioEvent(
                            sample_index=i,
                            timestamp_ms=samples[i].timestamp,
                            energy_magnitude=energy_deltas[i],
                        )
                    )

        return events

    def _measure_event_latency(
        self, samples: Sequence[EffectValidationSample], event: AudioEvent
    ) -> LatencyMeasurement:
        """
        Measure latency from a single audio event to visual response.

        Args:
            samples: Sequence of validation samples
            event: Audio event to measure from

        Returns:
            LatencyMeasurement with timing data
        """
        # Search forward from event for visual response
        search_start_idx = event.sample_index + 1
        search_end_timestamp = event.timestamp_ms + self.response_search_window_ms

        for i in range(search_start_idx, len(samples)):
            sample = samples[i]

            # Stop if we've exceeded the search window
            if sample.timestamp > search_end_timestamp:
                break

            # Check for visual response (energy_delta spike)
            if sample.energy_delta > self.visual_response_threshold:
                latency_ms = sample.timestamp - event.timestamp_ms
                passed = latency_ms <= self.latency_threshold_ms

                return LatencyMeasurement(
                    audio_event=event,
                    visual_response_index=i,
                    visual_response_timestamp_ms=sample.timestamp,
                    latency_ms=latency_ms,
                    passed=passed,
                )

        # No response found within search window
        return LatencyMeasurement(
            audio_event=event,
            visual_response_index=None,
            visual_response_timestamp_ms=None,
            latency_ms=None,
            passed=False,
        )
