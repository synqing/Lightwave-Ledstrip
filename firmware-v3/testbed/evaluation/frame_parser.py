"""Binary serial v2 frame parser for LightwaveOS capture stream.

Decodes 1008-byte binary frames from the ESP32-S3 serial capture stream
into structured numpy arrays for offline quality evaluation.

Protocol v2 format (1008 bytes total):
    Header:     17 bytes (sync, version, tap, effect/palette/brightness/speed,
                          frameIndex u32le, timestampUs u32le, rgbLen u16le)
    RGB:        960 bytes (320 LEDs × 3 channels, uint8)
    Metrics:    31 bytes (showUs, rms, bands[8], beat, onset, flux,
                          heapFree, showSkips, bpm, tempoConf, pad[2])

Protocol v4 format (528 bytes total):
    Header:     17 bytes
    RGB:        480 bytes (160 LEDs × 3 channels, every other LED)
    Metrics:    31 bytes
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Iterator, Optional, Sequence

import numpy as np

# --- Constants ---

SYNC_BYTE = 0xFD
HEADER_SIZE = 17
METRICS_SIZE = 31  # actual used bytes in the 32-byte trailer
TRAILER_PAD = 1    # remaining pad to 32
V2_RGB_LEN = 960
V4_RGB_LEN = 480
V2_FRAME_SIZE = HEADER_SIZE + V2_RGB_LEN + METRICS_SIZE + TRAILER_PAD  # 1009
V4_FRAME_SIZE = HEADER_SIZE + V4_RGB_LEN + METRICS_SIZE + TRAILER_PAD  # 529
NUM_LEDS = 320
NUM_BANDS = 8


@dataclass(slots=True)
class FrameMetrics:
    """Per-frame metrics embedded in the capture stream."""

    show_us: int              # LED driver show() duration, microseconds
    rms: float                # Audio RMS energy [0, 1]
    bands: np.ndarray         # 8-band frequency spectrum, each [0, 1]
    beat: bool                # Beat detected this frame
    onset: bool               # Onset (snare/hihat) detected this frame
    flux: float               # Spectral flux [0, 1]
    heap_free: int            # ESP32 free heap bytes
    show_skips: int           # LED driver safety-guard skips
    bpm: float                # Detected BPM (×100 encoding → float)
    tempo_confidence: float   # Tempo tracker confidence [0, 1]


@dataclass(slots=True)
class CaptureFrame:
    """One decoded frame from the binary serial v2 capture stream."""

    version: int
    tap: int
    effect_id: int
    palette_id: int
    brightness: int
    speed: int
    frame_index: int
    timestamp_us: int
    rgb: np.ndarray           # shape (320, 3), dtype uint8
    metrics: FrameMetrics


@dataclass
class CaptureSequence:
    """A sequence of captured frames for offline evaluation."""

    frames: list[CaptureFrame] = field(default_factory=list)

    @property
    def num_frames(self) -> int:
        return len(self.frames)

    @property
    def duration_s(self) -> float:
        """Estimated duration in seconds from timestamp deltas."""
        if len(self.frames) < 2:
            return 0.0
        dt = self.frames[-1].timestamp_us - self.frames[0].timestamp_us
        # Handle uint32 wrap
        if dt < 0:
            dt += 2**32
        return dt / 1e6

    @property
    def fps(self) -> float:
        dur = self.duration_s
        if dur <= 0:
            return 0.0
        return (self.num_frames - 1) / dur

    def rgb_array(self) -> np.ndarray:
        """Stack all frames into (N, 320, 3) uint8 array."""
        return np.stack([f.rgb for f in self.frames], axis=0)

    def rgb_float(self) -> np.ndarray:
        """Stack all frames into (N, 320, 3) float32 [0, 1] array."""
        return self.rgb_array().astype(np.float32) / 255.0

    def rms_array(self) -> np.ndarray:
        """Audio RMS for each frame, shape (N,)."""
        return np.array([f.metrics.rms for f in self.frames], dtype=np.float32)

    def beat_array(self) -> np.ndarray:
        """Beat flags for each frame, shape (N,), dtype bool."""
        return np.array([f.metrics.beat for f in self.frames], dtype=bool)

    def onset_array(self) -> np.ndarray:
        """Onset flags for each frame, shape (N,), dtype bool."""
        return np.array([f.metrics.onset for f in self.frames], dtype=bool)

    def flux_array(self) -> np.ndarray:
        """Spectral flux for each frame, shape (N,)."""
        return np.array([f.metrics.flux for f in self.frames], dtype=np.float32)

    def bands_array(self) -> np.ndarray:
        """Frequency bands for each frame, shape (N, 8)."""
        return np.stack([f.metrics.bands for f in self.frames], axis=0)

    def bpm_array(self) -> np.ndarray:
        """BPM for each frame, shape (N,)."""
        return np.array([f.metrics.bpm for f in self.frames], dtype=np.float32)

    def timestamp_array(self) -> np.ndarray:
        """Timestamps in seconds (relative to first frame), shape (N,)."""
        if not self.frames:
            return np.array([], dtype=np.float64)
        t0 = self.frames[0].timestamp_us
        ts = np.array([f.timestamp_us for f in self.frames], dtype=np.int64)
        # Handle uint32 wrap — detect backwards jumps > 2^31
        diffs = np.diff(ts)
        wrap_mask = diffs < -(2**31)
        if np.any(wrap_mask):
            corrections = np.cumsum(wrap_mask.astype(np.int64)) * (2**32)
            ts[1:] += corrections
        return (ts - ts[0]).astype(np.float64) / 1e6


def _parse_header(buf: bytes) -> dict:
    """Parse 17-byte header, return dict of fields."""
    if len(buf) < HEADER_SIZE:
        raise ValueError(f"Header too short: {len(buf)} < {HEADER_SIZE}")
    if buf[0] != SYNC_BYTE:
        raise ValueError(f"Bad sync byte: 0x{buf[0]:02X} != 0xFD")

    version = buf[1]
    tap = buf[2]
    effect_id = buf[3]
    palette_id = buf[4]
    brightness = buf[5]
    speed = buf[6]
    frame_index = struct.unpack_from("<I", buf, 7)[0]
    timestamp_us = struct.unpack_from("<I", buf, 11)[0]
    rgb_len = struct.unpack_from("<H", buf, 15)[0]

    return {
        "version": version,
        "tap": tap,
        "effect_id": effect_id,
        "palette_id": palette_id,
        "brightness": brightness,
        "speed": speed,
        "frame_index": frame_index,
        "timestamp_us": timestamp_us,
        "rgb_len": rgb_len,
    }


def _parse_metrics(buf: bytes) -> FrameMetrics:
    """Parse 31-byte metrics trailer."""
    if len(buf) < METRICS_SIZE:
        raise ValueError(f"Metrics too short: {len(buf)} < {METRICS_SIZE}")

    pos = 0
    show_us = struct.unpack_from("<H", buf, pos)[0]; pos += 2
    rms_u16 = struct.unpack_from("<H", buf, pos)[0]; pos += 2
    rms = rms_u16 / 65535.0

    bands = np.zeros(NUM_BANDS, dtype=np.float32)
    for i in range(NUM_BANDS):
        bands[i] = buf[pos] / 255.0
        pos += 1

    beat = bool(buf[pos]); pos += 1
    onset = bool(buf[pos]); pos += 1

    flux_u16 = struct.unpack_from("<H", buf, pos)[0]; pos += 2
    flux = flux_u16 / 65535.0

    heap_free = struct.unpack_from("<I", buf, pos)[0]; pos += 4

    show_skips = struct.unpack_from("<H", buf, pos)[0]; pos += 2

    bpm_u16 = struct.unpack_from("<H", buf, pos)[0]; pos += 2
    bpm = bpm_u16 / 100.0

    conf_u16 = struct.unpack_from("<H", buf, pos)[0]; pos += 2
    tempo_confidence = conf_u16 / 65535.0

    return FrameMetrics(
        show_us=show_us,
        rms=rms,
        bands=bands,
        beat=beat,
        onset=onset,
        flux=flux,
        heap_free=heap_free,
        show_skips=show_skips,
        bpm=bpm,
        tempo_confidence=tempo_confidence,
    )


def _parse_rgb_v2(buf: bytes) -> np.ndarray:
    """Parse 960-byte RGB payload → (320, 3) uint8."""
    if len(buf) < V2_RGB_LEN:
        raise ValueError(f"RGB payload too short: {len(buf)} < {V2_RGB_LEN}")
    return np.frombuffer(buf[:V2_RGB_LEN], dtype=np.uint8).reshape(NUM_LEDS, 3).copy()


def _parse_rgb_v4(buf: bytes) -> np.ndarray:
    """Parse 480-byte half-resolution RGB → (320, 3) uint8 via nearest-neighbour."""
    if len(buf) < V4_RGB_LEN:
        raise ValueError(f"RGB payload too short: {len(buf)} < {V4_RGB_LEN}")
    half = np.frombuffer(buf[:V4_RGB_LEN], dtype=np.uint8).reshape(160, 3)
    # Duplicate each pixel for full 320 resolution
    return np.repeat(half, 2, axis=0).copy()


def parse_frame(buf: bytes) -> CaptureFrame:
    """Parse a single binary frame from a complete buffer.

    Args:
        buf: Complete frame bytes (1008 for v2, 528 for v4).

    Returns:
        Decoded CaptureFrame.
    """
    header = _parse_header(buf[:HEADER_SIZE])
    rgb_len = header["rgb_len"]

    rgb_start = HEADER_SIZE
    rgb_end = rgb_start + rgb_len
    metrics_start = rgb_end

    if header["version"] == 4:
        rgb = _parse_rgb_v4(buf[rgb_start:rgb_end])
    elif rgb_len > 0:
        rgb = _parse_rgb_v2(buf[rgb_start:rgb_end])
    else:
        rgb = np.zeros((NUM_LEDS, 3), dtype=np.uint8)

    metrics = _parse_metrics(buf[metrics_start:metrics_start + METRICS_SIZE + TRAILER_PAD])

    return CaptureFrame(
        version=header["version"],
        tap=header["tap"],
        effect_id=header["effect_id"],
        palette_id=header["palette_id"],
        brightness=header["brightness"],
        speed=header["speed"],
        frame_index=header["frame_index"],
        timestamp_us=header["timestamp_us"],
        rgb=rgb,
        metrics=metrics,
    )


def iter_frames(stream: BinaryIO, version: int = 2) -> Iterator[CaptureFrame]:
    """Iterate over frames from a binary stream, syncing on 0xFD.

    Args:
        stream: Binary-mode file or serial port.
        version: Expected protocol version (2 or 4).

    Yields:
        Parsed CaptureFrame objects.
    """
    frame_size = V2_FRAME_SIZE if version == 2 else V4_FRAME_SIZE

    buf = bytearray()
    while True:
        chunk = stream.read(4096)
        if not chunk:
            break
        buf.extend(chunk)

        while len(buf) >= frame_size:
            # Find sync byte
            try:
                sync_idx = buf.index(SYNC_BYTE)
            except ValueError:
                buf.clear()
                break

            if sync_idx > 0:
                del buf[:sync_idx]

            if len(buf) < frame_size:
                break

            frame_bytes = bytes(buf[:frame_size])
            del buf[:frame_size]

            try:
                yield parse_frame(frame_bytes)
            except (ValueError, struct.error):
                # Bad frame — skip one byte and re-sync
                continue


def load_capture(path: str | Path) -> CaptureSequence:
    """Load a binary capture file into a CaptureSequence.

    Args:
        path: Path to binary capture file (.bin).

    Returns:
        CaptureSequence with all parsed frames.
    """
    path = Path(path)
    seq = CaptureSequence()

    with open(path, "rb") as f:
        # Detect version from first sync byte
        header = f.read(HEADER_SIZE)
        if len(header) < HEADER_SIZE:
            return seq
        f.seek(0)

        version = header[1]
        for frame in iter_frames(f, version=version):
            seq.frames.append(frame)

    return seq


def save_capture(seq: CaptureSequence, path: str | Path) -> None:
    """Save a CaptureSequence to a binary file.

    This re-encodes frames into the v2 binary format for archival.

    Args:
        seq: Sequence to save.
        path: Output file path (.bin).
    """
    path = Path(path)

    with open(path, "wb") as f:
        for frame in seq.frames:
            buf = bytearray()

            # Header (17 bytes)
            rgb_len = V2_RGB_LEN
            buf.append(SYNC_BYTE)
            buf.append(frame.version)
            buf.append(frame.tap)
            buf.append(frame.effect_id)
            buf.append(frame.palette_id)
            buf.append(frame.brightness)
            buf.append(frame.speed)
            buf.extend(struct.pack("<I", frame.frame_index))
            buf.extend(struct.pack("<I", frame.timestamp_us))
            buf.extend(struct.pack("<H", rgb_len))

            # RGB (960 bytes)
            buf.extend(frame.rgb.tobytes())

            # Metrics trailer (32 bytes)
            m = frame.metrics
            buf.extend(struct.pack("<H", m.show_us))
            buf.extend(struct.pack("<H", int(m.rms * 65535)))
            for i in range(NUM_BANDS):
                buf.append(int(np.clip(m.bands[i], 0, 1) * 255))
            buf.append(1 if m.beat else 0)
            buf.append(1 if m.onset else 0)
            buf.extend(struct.pack("<H", int(m.flux * 65535)))
            buf.extend(struct.pack("<I", m.heap_free))
            buf.extend(struct.pack("<H", m.show_skips))
            buf.extend(struct.pack("<H", int(m.bpm * 100)))
            buf.extend(struct.pack("<H", int(m.tempo_confidence * 65535)))
            buf.extend(b"\x00" * 6)  # pad to 32 (26 data + 6 pad)

            f.write(buf)


__all__ = [
    "CaptureFrame",
    "CaptureSequence",
    "FrameMetrics",
    "parse_frame",
    "iter_frames",
    "load_capture",
    "save_capture",
]
