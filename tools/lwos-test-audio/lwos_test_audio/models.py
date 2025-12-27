"""
Data models for LightwaveOS audio effect validation.

Defines the schema for validation samples, metrics, and database storage.
"""

from dataclasses import dataclass, field
from datetime import datetime
from enum import IntEnum
from typing import Optional


class EffectType(IntEnum):
    """Effect type enumeration matching LightwaveOS EffectType."""

    STANDARD = 0
    AUDIO_REACTIVE = 1
    LIGHT_GUIDE_PLATE = 2
    PHYSICS_SIMULATION = 3


@dataclass
class AudioMetrics:
    """Audio processing metrics from the device."""

    bass_energy: float
    mid_energy: float
    treble_energy: float
    total_energy: float
    peak_frequency: float
    spectral_centroid: float
    spectral_spread: float
    zero_crossing_rate: float

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> "AudioMetrics":
        """Parse audio metrics from binary frame (32 bytes, 8 floats)."""
        import struct

        values = struct.unpack_from("<8f", data, offset)
        return cls(
            bass_energy=values[0],
            mid_energy=values[1],
            treble_energy=values[2],
            total_energy=values[3],
            peak_frequency=values[4],
            spectral_centroid=values[5],
            spectral_spread=values[6],
            zero_crossing_rate=values[7],
        )

    def to_dict(self) -> dict[str, float]:
        """Convert to dictionary for JSON serialization."""
        return {
            "bass_energy": self.bass_energy,
            "mid_energy": self.mid_energy,
            "treble_energy": self.treble_energy,
            "total_energy": self.total_energy,
            "peak_frequency": self.peak_frequency,
            "spectral_centroid": self.spectral_centroid,
            "spectral_spread": self.spectral_spread,
            "zero_crossing_rate": self.zero_crossing_rate,
        }


@dataclass
class PerformanceMetrics:
    """Performance metrics from the device."""

    frame_time_us: int
    render_time_us: int
    fps: float
    heap_free: int
    heap_fragmentation: float
    cpu_usage: float

    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> "PerformanceMetrics":
        """Parse performance metrics from binary frame (24 bytes)."""
        import struct

        # uint32, uint32, float, uint32, float, float
        values = struct.unpack_from("<IIfIfI", data, offset)
        return cls(
            frame_time_us=values[0],
            render_time_us=values[1],
            fps=values[2],
            heap_free=values[3],
            heap_fragmentation=values[4],
            cpu_usage=values[5],
        )

    def to_dict(self) -> dict[str, float | int]:
        """Convert to dictionary for JSON serialization."""
        return {
            "frame_time_us": self.frame_time_us,
            "render_time_us": self.render_time_us,
            "fps": self.fps,
            "heap_free": self.heap_free,
            "heap_fragmentation": self.heap_fragmentation,
            "cpu_usage": self.cpu_usage,
        }


@dataclass
class EffectValidationSample:
    """
    Single validation sample frame from LightwaveOS.

    Binary frame format (128 bytes):
    - Timestamp (uint64, 8 bytes)
    - Effect ID (uint32, 4 bytes)
    - Effect Type (uint8, 1 byte)
    - Reserved (3 bytes padding)
    - Audio Metrics (8 floats, 32 bytes)
    - Performance Metrics (24 bytes)
    - Effect-specific data (56 bytes)
    """

    timestamp: int
    effect_id: int
    effect_type: EffectType
    audio_metrics: AudioMetrics
    performance_metrics: PerformanceMetrics
    effect_data: bytes
    session_id: Optional[int] = None
    captured_at: datetime = field(default_factory=datetime.now)

    # Derived motion metrics (computed from effect_data or audio_metrics)
    phase: float = 0.0  # Accumulated phase/position (normalized 0..1, wraps)
    phase_delta: float = 0.0  # Frame-to-frame phase change (-1..1)
    energy: float = 0.0  # Visual energy level (0..1)
    energy_delta: float = 0.0  # Frame-to-frame energy change

    @classmethod
    def from_binary_frame(cls, data: bytes) -> "EffectValidationSample":
        """
        Parse a 128-byte validation frame.

        Frame structure:
        [0-7]     uint64   timestamp (micros since boot)
        [8-11]    uint32   effect_id
        [12]      uint8    effect_type
        [13-15]   padding  reserved
        [16-47]   floats   audio_metrics (8 * 4 bytes)
        [48-71]   mixed    performance_metrics (24 bytes)
        [72-127]  bytes    effect_data (56 bytes)
        """
        import struct

        if len(data) != 128:
            raise ValueError(f"Expected 128 bytes, got {len(data)}")

        # Parse header (16 bytes)
        timestamp, effect_id, effect_type_raw = struct.unpack_from("<QIB3x", data, 0)

        # Parse audio metrics (32 bytes)
        audio_metrics = AudioMetrics.from_bytes(data, offset=16)

        # Parse performance metrics (24 bytes)
        performance_metrics = PerformanceMetrics.from_bytes(data, offset=48)

        # Extract effect-specific data (56 bytes)
        effect_data = data[72:128]

        return cls(
            timestamp=timestamp,
            effect_id=effect_id,
            effect_type=EffectType(effect_type_raw),
            audio_metrics=audio_metrics,
            performance_metrics=performance_metrics,
            effect_data=effect_data,
        )

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "timestamp": self.timestamp,
            "effect_id": self.effect_id,
            "effect_type": self.effect_type.name,
            "audio_metrics": self.audio_metrics.to_dict(),
            "performance_metrics": self.performance_metrics.to_dict(),
            "effect_data_hex": self.effect_data.hex(),
            "captured_at": self.captured_at.isoformat(),
            "session_id": self.session_id,
        }


@dataclass
class ValidationSession:
    """Metadata for a validation recording session."""

    session_id: int
    device_host: str
    start_time: datetime
    end_time: Optional[datetime] = None
    effect_name: Optional[str] = None
    effect_id: Optional[int] = None
    sample_count: int = 0
    notes: str = ""

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "session_id": self.session_id,
            "device_host": self.device_host,
            "start_time": self.start_time.isoformat(),
            "end_time": self.end_time.isoformat() if self.end_time else None,
            "effect_name": self.effect_name,
            "effect_id": self.effect_id,
            "sample_count": self.sample_count,
            "notes": self.notes,
        }


# SQLite schema for sqlite-utils
VALIDATION_SAMPLE_SCHEMA = {
    "id": int,
    "session_id": int,
    "timestamp": int,
    "effect_id": int,
    "effect_type": str,
    "captured_at": str,
    # Audio metrics
    "bass_energy": float,
    "mid_energy": float,
    "treble_energy": float,
    "total_energy": float,
    "peak_frequency": float,
    "spectral_centroid": float,
    "spectral_spread": float,
    "zero_crossing_rate": float,
    # Performance metrics
    "frame_time_us": int,
    "render_time_us": int,
    "fps": float,
    "heap_free": int,
    "heap_fragmentation": float,
    "cpu_usage": float,
    # Effect data
    "effect_data": bytes,
}

SESSION_SCHEMA = {
    "session_id": int,
    "device_host": str,
    "start_time": str,
    "end_time": str,
    "effect_name": str,
    "effect_id": int,
    "sample_count": int,
    "notes": str,
}
