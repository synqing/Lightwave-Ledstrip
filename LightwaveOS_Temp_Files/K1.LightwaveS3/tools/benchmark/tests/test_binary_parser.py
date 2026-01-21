"""Tests for binary frame parser."""

import struct

import pytest

from lwos_benchmark.parsers.binary import BenchmarkFrame


def create_test_frame(
    magic: int = 0x004D4241,
    timestamp_ms: int = 12345,
    avg_total_us: float = 150.5,
    avg_goertzel_us: float = 75.2,
    cpu_load_percent: float = 45.8,
    peak_total_us: int = 200,
    peak_goertzel_us: int = 100,
    hop_count: int = 512,
    goertzel_count: int = 8,
    flags: int = 0x04,
    reserved: int = 0,
) -> bytes:
    """Create a test binary frame.

    Args:
        All frame fields with defaults

    Returns:
        32-byte binary frame
    """
    return struct.pack(
        "<IIfffHHIHBB",
        magic,
        timestamp_ms,
        avg_total_us,
        avg_goertzel_us,
        cpu_load_percent,
        peak_total_us,
        peak_goertzel_us,
        hop_count,
        goertzel_count,
        flags,
        reserved,
    )


def test_parse_valid_frame() -> None:
    """Test parsing a valid benchmark frame."""
    data = create_test_frame()
    frame = BenchmarkFrame.from_bytes(data)

    assert frame.magic == 0x004D4241
    assert frame.timestamp_ms == 12345
    assert frame.avg_total_us == pytest.approx(150.5)
    assert frame.avg_goertzel_us == pytest.approx(75.2)
    assert frame.cpu_load_percent == pytest.approx(45.8)
    assert frame.peak_total_us == 200
    assert frame.peak_goertzel_us == 100
    assert frame.hop_count == 512
    assert frame.goertzel_count == 8
    assert frame.flags == 0x04
    assert frame.reserved == 0


def test_invalid_frame_length() -> None:
    """Test that invalid frame length raises ValueError."""
    data = b"too short"

    with pytest.raises(ValueError, match="Invalid frame length"):
        BenchmarkFrame.from_bytes(data)


def test_invalid_magic_number() -> None:
    """Test that invalid magic number raises ValueError."""
    data = create_test_frame(magic=0xDEADBEEF)

    with pytest.raises(ValueError, match="Invalid magic number"):
        BenchmarkFrame.from_bytes(data)


def test_frame_validation() -> None:
    """Test frame validation property."""
    valid_data = create_test_frame()
    valid_frame = BenchmarkFrame.from_bytes(valid_data)

    assert valid_frame.is_valid is True


def test_goertzel_overhead_calculation() -> None:
    """Test Goertzel overhead percentage calculation."""
    data = create_test_frame(avg_total_us=100.0, avg_goertzel_us=25.0)
    frame = BenchmarkFrame.from_bytes(data)

    assert frame.goertzel_overhead_percent == pytest.approx(25.0)


def test_goertzel_overhead_zero_total() -> None:
    """Test Goertzel overhead when total time is zero."""
    data = create_test_frame(avg_total_us=0.0, avg_goertzel_us=0.0)
    frame = BenchmarkFrame.from_bytes(data)

    assert frame.goertzel_overhead_percent == 0.0


def test_flag_parsing() -> None:
    """Test status flag bit parsing."""
    # Test buffer overflow flag (bit 0)
    data = create_test_frame(flags=0x01)
    frame = BenchmarkFrame.from_bytes(data)
    assert frame.flag_buffer_overflow is True
    assert frame.flag_timing_warning is False
    assert frame.flag_audio_active is False

    # Test timing warning flag (bit 1)
    data = create_test_frame(flags=0x02)
    frame = BenchmarkFrame.from_bytes(data)
    assert frame.flag_buffer_overflow is False
    assert frame.flag_timing_warning is True
    assert frame.flag_audio_active is False

    # Test audio active flag (bit 2)
    data = create_test_frame(flags=0x04)
    frame = BenchmarkFrame.from_bytes(data)
    assert frame.flag_buffer_overflow is False
    assert frame.flag_timing_warning is False
    assert frame.flag_audio_active is True

    # Test multiple flags
    data = create_test_frame(flags=0x07)
    frame = BenchmarkFrame.from_bytes(data)
    assert frame.flag_buffer_overflow is True
    assert frame.flag_timing_warning is True
    assert frame.flag_audio_active is True


def test_to_dict() -> None:
    """Test conversion to dictionary."""
    data = create_test_frame()
    frame = BenchmarkFrame.from_bytes(data)

    d = frame.to_dict()

    assert d["magic"] == 0x004D4241
    assert d["timestamp_ms"] == 12345
    assert d["avg_total_us"] == pytest.approx(150.5)
    assert d["hop_count"] == 512
    assert d["flags"] == 0x04
