"""
Unit tests for lwos_test_audio.models module.
"""

import struct
from datetime import datetime

import pytest

from lwos_test_audio.models import (
    AudioMetrics,
    EffectType,
    EffectValidationSample,
    PerformanceMetrics,
    ValidationSession,
)


class TestAudioMetrics:
    """Test AudioMetrics data model."""

    def test_from_bytes(self):
        """Test parsing audio metrics from binary data."""
        # Create test data (8 floats)
        test_values = [0.1, 0.2, 0.3, 0.4, 500.0, 1000.0, 200.0, 0.05]
        data = struct.pack("<8f", *test_values)

        metrics = AudioMetrics.from_bytes(data)

        assert metrics.bass_energy == pytest.approx(0.1)
        assert metrics.mid_energy == pytest.approx(0.2)
        assert metrics.treble_energy == pytest.approx(0.3)
        assert metrics.total_energy == pytest.approx(0.4)
        assert metrics.peak_frequency == pytest.approx(500.0)
        assert metrics.spectral_centroid == pytest.approx(1000.0)
        assert metrics.spectral_spread == pytest.approx(200.0)
        assert metrics.zero_crossing_rate == pytest.approx(0.05)

    def test_to_dict(self):
        """Test conversion to dictionary."""
        metrics = AudioMetrics(
            bass_energy=0.5,
            mid_energy=0.3,
            treble_energy=0.2,
            total_energy=1.0,
            peak_frequency=440.0,
            spectral_centroid=800.0,
            spectral_spread=150.0,
            zero_crossing_rate=0.1,
        )

        result = metrics.to_dict()

        assert result["bass_energy"] == 0.5
        assert result["mid_energy"] == 0.3
        assert result["total_energy"] == 1.0
        assert len(result) == 8


class TestPerformanceMetrics:
    """Test PerformanceMetrics data model."""

    def test_from_bytes(self):
        """Test parsing performance metrics from binary data."""
        # uint32, uint32, float, uint32, float, uint32
        data = struct.pack("<IIfIfI", 8333, 5000, 120.0, 200000, 0.15, 75)

        metrics = PerformanceMetrics.from_bytes(data)

        assert metrics.frame_time_us == 8333
        assert metrics.render_time_us == 5000
        assert metrics.fps == pytest.approx(120.0)
        assert metrics.heap_free == 200000
        assert metrics.heap_fragmentation == pytest.approx(0.15)
        assert metrics.cpu_usage == pytest.approx(75.0)

    def test_to_dict(self):
        """Test conversion to dictionary."""
        metrics = PerformanceMetrics(
            frame_time_us=8333,
            render_time_us=5000,
            fps=120.0,
            heap_free=200000,
            heap_fragmentation=0.15,
            cpu_usage=75.0,
        )

        result = metrics.to_dict()

        assert result["fps"] == 120.0
        assert result["heap_free"] == 200000
        assert len(result) == 6


class TestEffectValidationSample:
    """Test EffectValidationSample data model."""

    def test_from_binary_frame(self):
        """Test parsing a complete 128-byte validation frame."""
        # Build a test frame
        frame = bytearray(128)

        # Header (16 bytes)
        timestamp = 1234567890
        effect_id = 42
        effect_type = EffectType.AUDIO_REACTIVE
        struct.pack_into("<QIB3x", frame, 0, timestamp, effect_id, effect_type)

        # Audio metrics (32 bytes)
        audio_values = [0.1, 0.2, 0.3, 0.4, 500.0, 1000.0, 200.0, 0.05]
        struct.pack_into("<8f", frame, 16, *audio_values)

        # Performance metrics (24 bytes)
        struct.pack_into("<IIfIfI", frame, 48, 8333, 5000, 120.0, 200000, 0.15, 75)

        # Effect data (56 bytes) - just fill with pattern
        for i in range(72, 128):
            frame[i] = i % 256

        # Parse the frame
        sample = EffectValidationSample.from_binary_frame(bytes(frame))

        assert sample.timestamp == timestamp
        assert sample.effect_id == effect_id
        assert sample.effect_type == EffectType.AUDIO_REACTIVE
        assert sample.audio_metrics.bass_energy == pytest.approx(0.1)
        assert sample.performance_metrics.fps == pytest.approx(120.0)
        assert len(sample.effect_data) == 56

    def test_from_binary_frame_invalid_size(self):
        """Test that parsing rejects frames of incorrect size."""
        with pytest.raises(ValueError, match="Expected 128 bytes"):
            EffectValidationSample.from_binary_frame(b"too short")

    def test_to_dict(self):
        """Test conversion to dictionary."""
        audio = AudioMetrics(0.1, 0.2, 0.3, 0.4, 500.0, 1000.0, 200.0, 0.05)
        perf = PerformanceMetrics(8333, 5000, 120.0, 200000, 0.15, 75.0)
        effect_data = b"\x00" * 56

        sample = EffectValidationSample(
            timestamp=1234567890,
            effect_id=42,
            effect_type=EffectType.AUDIO_REACTIVE,
            audio_metrics=audio,
            performance_metrics=perf,
            effect_data=effect_data,
        )

        result = sample.to_dict()

        assert result["timestamp"] == 1234567890
        assert result["effect_id"] == 42
        assert result["effect_type"] == "AUDIO_REACTIVE"
        assert "audio_metrics" in result
        assert "performance_metrics" in result
        assert "effect_data_hex" in result


class TestValidationSession:
    """Test ValidationSession data model."""

    def test_to_dict(self):
        """Test conversion to dictionary."""
        start_time = datetime(2025, 1, 1, 12, 0, 0)
        end_time = datetime(2025, 1, 1, 12, 1, 0)

        session = ValidationSession(
            session_id=1,
            device_host="lightwaveos.local",
            start_time=start_time,
            end_time=end_time,
            effect_name="Audio Pulse",
            effect_id=10,
            sample_count=1000,
            notes="Test session",
        )

        result = session.to_dict()

        assert result["session_id"] == 1
        assert result["device_host"] == "lightwaveos.local"
        assert result["effect_name"] == "Audio Pulse"
        assert result["sample_count"] == 1000
        assert "start_time" in result
        assert "end_time" in result
