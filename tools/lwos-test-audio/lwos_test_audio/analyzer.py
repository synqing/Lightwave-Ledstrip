"""
Analysis engine for LightwaveOS validation data.

Provides statistical analysis, signal processing, and anomaly detection
for captured validation samples.
"""

import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import numpy as np
from scipy import signal, stats
from sqlite_utils import Database

logger = logging.getLogger(__name__)


@dataclass
class AnalysisResult:
    """Results from validation analysis."""

    session_id: int
    effect_id: int
    effect_name: Optional[str]
    sample_count: int

    # Performance statistics
    fps_mean: float
    fps_std: float
    fps_min: float
    fps_max: float
    frame_time_mean_us: float
    frame_time_std_us: float

    # Audio metrics statistics
    bass_energy_mean: float
    bass_energy_std: float
    mid_energy_mean: float
    mid_energy_std: float
    treble_energy_mean: float
    treble_energy_std: float
    total_energy_mean: float
    total_energy_std: float

    # Correlation analysis
    audio_visual_correlation: float  # Correlation between audio energy and render time
    frequency_response_smoothness: float  # How smoothly effect responds to frequency changes

    # Anomaly detection
    frame_drops: int  # Frames below 60 FPS
    render_spikes: int  # Render times > 2 std devs above mean
    audio_saturation_events: int  # Audio metrics at max values

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "session_id": self.session_id,
            "effect_id": self.effect_id,
            "effect_name": self.effect_name,
            "sample_count": self.sample_count,
            "performance": {
                "fps_mean": self.fps_mean,
                "fps_std": self.fps_std,
                "fps_min": self.fps_min,
                "fps_max": self.fps_max,
                "frame_time_mean_us": self.frame_time_mean_us,
                "frame_time_std_us": self.frame_time_std_us,
            },
            "audio_metrics": {
                "bass_energy_mean": self.bass_energy_mean,
                "bass_energy_std": self.bass_energy_std,
                "mid_energy_mean": self.mid_energy_mean,
                "mid_energy_std": self.mid_energy_std,
                "treble_energy_mean": self.treble_energy_mean,
                "treble_energy_std": self.treble_energy_std,
                "total_energy_mean": self.total_energy_mean,
                "total_energy_std": self.total_energy_std,
            },
            "correlations": {
                "audio_visual_correlation": self.audio_visual_correlation,
                "frequency_response_smoothness": self.frequency_response_smoothness,
            },
            "anomalies": {
                "frame_drops": self.frame_drops,
                "render_spikes": self.render_spikes,
                "audio_saturation_events": self.audio_saturation_events,
            },
        }


class ValidationAnalyzer:
    """
    Analyzes validation data from SQLite database.

    Provides statistical analysis, signal processing, and anomaly detection
    for audio-reactive effect validation.
    """

    def __init__(self, db_path: Path):
        """
        Initialize the analyzer.

        Args:
            db_path: Path to SQLite database with validation data
        """
        self.db_path = db_path
        self.db = Database(db_path)

    def get_sessions(self) -> list[dict]:
        """Get all validation sessions."""
        return list(self.db["sessions"].rows)

    def get_session_samples(self, session_id: int) -> np.ndarray:
        """
        Load all samples for a session into NumPy structured array.

        Args:
            session_id: Session ID to load

        Returns:
            NumPy structured array with all sample data
        """
        samples = list(self.db["samples"].rows_where("session_id = ?", [session_id]))

        if not samples:
            raise ValueError(f"No samples found for session {session_id}")

        # Convert to NumPy structured array for efficient analysis
        dtype = [
            ("timestamp", "i8"),
            ("effect_id", "i4"),
            ("bass_energy", "f4"),
            ("mid_energy", "f4"),
            ("treble_energy", "f4"),
            ("total_energy", "f4"),
            ("peak_frequency", "f4"),
            ("spectral_centroid", "f4"),
            ("spectral_spread", "f4"),
            ("zero_crossing_rate", "f4"),
            ("frame_time_us", "i4"),
            ("render_time_us", "i4"),
            ("fps", "f4"),
            ("heap_free", "i4"),
            ("heap_fragmentation", "f4"),
            ("cpu_usage", "f4"),
        ]

        data = np.zeros(len(samples), dtype=dtype)

        for i, sample in enumerate(samples):
            data[i]["timestamp"] = sample["timestamp"]
            data[i]["effect_id"] = sample["effect_id"]
            data[i]["bass_energy"] = sample["bass_energy"]
            data[i]["mid_energy"] = sample["mid_energy"]
            data[i]["treble_energy"] = sample["treble_energy"]
            data[i]["total_energy"] = sample["total_energy"]
            data[i]["peak_frequency"] = sample["peak_frequency"]
            data[i]["spectral_centroid"] = sample["spectral_centroid"]
            data[i]["spectral_spread"] = sample["spectral_spread"]
            data[i]["zero_crossing_rate"] = sample["zero_crossing_rate"]
            data[i]["frame_time_us"] = sample["frame_time_us"]
            data[i]["render_time_us"] = sample["render_time_us"]
            data[i]["fps"] = sample["fps"]
            data[i]["heap_free"] = sample["heap_free"]
            data[i]["heap_fragmentation"] = sample["heap_fragmentation"]
            data[i]["cpu_usage"] = sample["cpu_usage"]

        return data

    def analyze_session(self, session_id: int) -> AnalysisResult:
        """
        Perform comprehensive analysis on a validation session.

        Args:
            session_id: Session ID to analyze

        Returns:
            AnalysisResult with statistical analysis and anomaly detection
        """
        # Load session metadata
        session = self.db["sessions"].get(session_id)
        if not session:
            raise ValueError(f"Session {session_id} not found")

        # Load samples
        data = self.get_session_samples(session_id)

        # Performance statistics
        fps_mean = float(np.mean(data["fps"]))
        fps_std = float(np.std(data["fps"]))
        fps_min = float(np.min(data["fps"]))
        fps_max = float(np.max(data["fps"]))
        frame_time_mean = float(np.mean(data["frame_time_us"]))
        frame_time_std = float(np.std(data["frame_time_us"]))

        # Audio metrics statistics
        bass_mean = float(np.mean(data["bass_energy"]))
        bass_std = float(np.std(data["bass_energy"]))
        mid_mean = float(np.mean(data["mid_energy"]))
        mid_std = float(np.std(data["mid_energy"]))
        treble_mean = float(np.mean(data["treble_energy"]))
        treble_std = float(np.std(data["treble_energy"]))
        total_mean = float(np.mean(data["total_energy"]))
        total_std = float(np.std(data["total_energy"]))

        # Correlation analysis
        # Audio-visual correlation: how well does render time track audio energy?
        audio_visual_corr = float(
            np.corrcoef(data["total_energy"], data["render_time_us"])[0, 1]
        )

        # Frequency response smoothness: low-pass filter the peak frequency
        # and measure deviation (smoother = more stable frequency response)
        b, a = signal.butter(3, 0.1)  # Low-pass filter
        filtered_freq = signal.filtfilt(b, a, data["peak_frequency"])
        freq_deviation = np.std(data["peak_frequency"] - filtered_freq)
        freq_response_smoothness = float(1.0 / (1.0 + freq_deviation))

        # Anomaly detection
        frame_drops = int(np.sum(data["fps"] < 60.0))

        render_mean = np.mean(data["render_time_us"])
        render_std = np.std(data["render_time_us"])
        render_threshold = render_mean + 2 * render_std
        render_spikes = int(np.sum(data["render_time_us"] > render_threshold))

        # Audio saturation: check if any audio metrics are pegged at extreme values
        audio_saturation = 0
        for metric in ["bass_energy", "mid_energy", "treble_energy"]:
            metric_max = np.max(data[metric])
            # Assume saturation if metric is at or near 1.0 (normalized energy)
            if metric_max >= 0.99:
                audio_saturation += int(np.sum(data[metric] >= 0.99))

        return AnalysisResult(
            session_id=session_id,
            effect_id=int(data["effect_id"][0]),
            effect_name=session.get("effect_name"),
            sample_count=len(data),
            fps_mean=fps_mean,
            fps_std=fps_std,
            fps_min=fps_min,
            fps_max=fps_max,
            frame_time_mean_us=frame_time_mean,
            frame_time_std_us=frame_time_std,
            bass_energy_mean=bass_mean,
            bass_energy_std=bass_std,
            mid_energy_mean=mid_mean,
            mid_energy_std=mid_std,
            treble_energy_mean=treble_mean,
            treble_energy_std=treble_std,
            total_energy_mean=total_mean,
            total_energy_std=total_std,
            audio_visual_correlation=audio_visual_corr,
            frequency_response_smoothness=freq_response_smoothness,
            frame_drops=frame_drops,
            render_spikes=render_spikes,
            audio_saturation_events=audio_saturation,
        )

    def compare_sessions(self, session_ids: list[int]) -> dict:
        """
        Compare multiple validation sessions.

        Args:
            session_ids: List of session IDs to compare

        Returns:
            Dictionary with comparative statistics
        """
        results = [self.analyze_session(sid) for sid in session_ids]

        return {
            "sessions": [r.to_dict() for r in results],
            "comparison": {
                "fps_range": [min(r.fps_mean for r in results), max(r.fps_mean for r in results)],
                "audio_visual_correlation_range": [
                    min(r.audio_visual_correlation for r in results),
                    max(r.audio_visual_correlation for r in results),
                ],
                "total_frame_drops": sum(r.frame_drops for r in results),
                "total_render_spikes": sum(r.render_spikes for r in results),
            },
        }

    def export_timeseries(self, session_id: int, output_path: Path) -> None:
        """
        Export session data as CSV for external analysis.

        Args:
            session_id: Session ID to export
            output_path: Output CSV file path
        """
        import csv

        data = self.get_session_samples(session_id)

        with open(output_path, "w", newline="") as f:
            writer = csv.writer(f)

            # Write header
            writer.writerow(data.dtype.names)

            # Write rows
            for row in data:
                writer.writerow(row)

        logger.info(f"Exported {len(data)} samples to {output_path}")
