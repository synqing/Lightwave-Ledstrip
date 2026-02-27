"""Binary frame parser for LightwaveOS benchmark data."""

import struct
from typing import Self

from pydantic import BaseModel, Field, field_validator


class BenchmarkFrame(BaseModel):
    """Parsed benchmark frame from binary data.

    32-byte compact frame structure transmitted over WebSocket.
    """

    magic: int = Field(description="Magic number (0x004D4241)")
    timestamp_ms: int = Field(description="Milliseconds since boot")
    avg_total_us: float = Field(description="Average total processing time (µs)")
    avg_goertzel_us: float = Field(description="Average Goertzel computation time (µs)")
    cpu_load_percent: float = Field(description="CPU load percentage")
    peak_total_us: int = Field(description="Peak total processing time (µs)")
    peak_goertzel_us: int = Field(description="Peak Goertzel time (µs)")
    hop_count: int = Field(description="FFT hop count")
    goertzel_count: int = Field(description="Number of Goertzel filters")
    flags: int = Field(description="Status flags bitfield")
    reserved: int = Field(default=0, description="Reserved byte")

    @field_validator("magic")
    @classmethod
    def validate_magic(cls, v: int) -> int:
        """Validate magic number matches expected value."""
        expected = 0x004D4241
        if v != expected:
            msg = f"Invalid magic number: 0x{v:08X}, expected 0x{expected:08X}"
            raise ValueError(msg)
        return v

    @classmethod
    def from_bytes(cls, data: bytes) -> Self:
        """Parse benchmark frame from 32-byte binary data.

        Frame format (little-endian):
        - Offset 0:  uint32 magic (0x004D4241)
        - Offset 4:  uint32 timestamp_ms
        - Offset 8:  float32 avgTotalUs
        - Offset 12: float32 avgGoertzelUs
        - Offset 16: float32 cpuLoadPercent
        - Offset 20: uint16 peakTotalUs
        - Offset 22: uint16 peakGoertzelUs
        - Offset 24: uint32 hopCount
        - Offset 28: uint16 goertzelCount
        - Offset 30: uint8 flags
        - Offset 31: uint8 reserved

        Args:
            data: 32-byte binary frame

        Returns:
            Parsed BenchmarkFrame instance

        Raises:
            ValueError: If data length is not 32 bytes or magic is invalid
        """
        if len(data) != 32:
            msg = f"Invalid frame length: {len(data)} bytes, expected 32"
            raise ValueError(msg)

        # Unpack binary data (little-endian)
        # Format: I=uint32, f=float32, H=uint16, B=uint8
        unpacked = struct.unpack("<IIfffHHIHBB", data)

        return cls(
            magic=unpacked[0],
            timestamp_ms=unpacked[1],
            avg_total_us=unpacked[2],
            avg_goertzel_us=unpacked[3],
            cpu_load_percent=unpacked[4],
            peak_total_us=unpacked[5],
            peak_goertzel_us=unpacked[6],
            hop_count=unpacked[7],
            goertzel_count=unpacked[8],
            flags=unpacked[9],
            reserved=unpacked[10],
        )

    def to_dict(self) -> dict[str, float | int]:
        """Convert frame to dictionary for storage/analysis.

        Returns:
            Dictionary with all frame fields
        """
        return {
            "magic": self.magic,
            "timestamp_ms": self.timestamp_ms,
            "avg_total_us": self.avg_total_us,
            "avg_goertzel_us": self.avg_goertzel_us,
            "cpu_load_percent": self.cpu_load_percent,
            "peak_total_us": self.peak_total_us,
            "peak_goertzel_us": self.peak_goertzel_us,
            "hop_count": self.hop_count,
            "goertzel_count": self.goertzel_count,
            "flags": self.flags,
            "reserved": self.reserved,
        }

    @property
    def is_valid(self) -> bool:
        """Check if frame has valid magic number."""
        return self.magic == 0x004D4241

    @property
    def goertzel_overhead_percent(self) -> float:
        """Calculate Goertzel overhead as percentage of total time."""
        if self.avg_total_us == 0:
            return 0.0
        return (self.avg_goertzel_us / self.avg_total_us) * 100.0

    @property
    def flag_buffer_overflow(self) -> bool:
        """Check if buffer overflow flag is set (bit 0)."""
        return bool(self.flags & 0x01)

    @property
    def flag_timing_warning(self) -> bool:
        """Check if timing warning flag is set (bit 1)."""
        return bool(self.flags & 0x02)

    @property
    def flag_audio_active(self) -> bool:
        """Check if audio active flag is set (bit 2)."""
        return bool(self.flags & 0x04)
