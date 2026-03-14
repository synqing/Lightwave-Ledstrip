"""
K1 device serial interface for EdgeMixer evaluation.

Handles serial communication with a K1 device over USB CDC at 115200 baud.
Provides EdgeMixer configuration and binary frame capture.

Frame protocol (serial capture v2):
    Magic:   0xFD (1 byte)
    Header:  17 bytes total
        magic(1) version(1) tap(1) effect(1) palette(1) brightness(1)
        speed(1) frameIndex(u32le) timestampUs(u32le) payloadLen(u16le)
    Payload: 960 bytes RGB (320 LEDs x 3) for v2
    Trailer: 32 bytes metrics
        show_time_us(u16le) rms(u16le/65535) bands[8](u8/255)
        beat(u8) onset(u8) flux(u16le/65535) heap_free(u32le)
        show_skips(u16le) bpm(u16le/100) beat_confidence(u16le/65535)
    Total:   1009 bytes per v2 frame

JSON command protocol:
    Send:    {"type":"setEdgeMixer","mode":X,...}\\n
    Send:    {"type":"getEdgeMixer"}\\n
    Receive: {"type":"getEdgeMixer","requestId":"...","success":true,"data":{...}}
"""

from __future__ import annotations

import json
import logging
import struct
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

import numpy as np

try:
    import serial
except ImportError:
    raise ImportError(
        "pyserial is required.  Install with: pip install pyserial"
    )

from .profiles import Profile

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Protocol constants (mirrored from led_capture.py)
# ---------------------------------------------------------------------------

NUM_LEDS = 320
RGB_BYTES = NUM_LEDS * 3  # 960

SERIAL_MAGIC = 0xFD
SERIAL_HEADER_SIZE = 17
SERIAL_V1_FRAME_SIZE = 977   # 17 + 960
SERIAL_V2_FRAME_SIZE = 1009  # 17 + 960 + 32
SERIAL_V2_TRAILER_SIZE = 32


# ---------------------------------------------------------------------------
# Frame metadata
# ---------------------------------------------------------------------------

@dataclass
class FrameMetadata:
    """Parsed metadata from a serial capture frame header + trailer."""

    version: int
    tap: int
    effect: int
    palette: int
    brightness: int
    speed: int
    frame_idx: int
    timestamp_us: int

    # v2 trailer fields (None if v1 frame)
    show_time_us: Optional[int] = None
    rms: Optional[float] = None
    bands: Optional[List[float]] = None
    beat: Optional[bool] = None
    onset: Optional[bool] = None
    flux: Optional[float] = None
    heap_free: Optional[int] = None
    show_skips: Optional[int] = None
    bpm: Optional[float] = None
    beat_confidence: Optional[float] = None


# ---------------------------------------------------------------------------
# Frame parsing
# ---------------------------------------------------------------------------

def _parse_serial_frame(data: bytes) -> Optional[Tuple[np.ndarray, FrameMetadata]]:
    """Parse a single serial capture frame from raw bytes.

    Returns (led_array, metadata) where led_array is (320, 3) uint8,
    or None if the frame is malformed.
    """
    if len(data) < SERIAL_HEADER_SIZE:
        return None
    if data[0] != SERIAL_MAGIC:
        return None

    version = data[1]
    if version not in (1, 2):
        # We only handle v1/v2 full-RGB frames for evaluation captures.
        return None

    tap = data[2]
    effect_id = data[3]
    palette_id = data[4]
    brightness = data[5]
    speed = data[6]
    frame_idx = struct.unpack_from('<I', data, 7)[0]
    timestamp_us = struct.unpack_from('<I', data, 11)[0]
    payload_len = struct.unpack_from('<H', data, 15)[0]

    if payload_len != RGB_BYTES:
        return None

    # Determine expected frame size.
    frame_size = SERIAL_V2_FRAME_SIZE if version >= 2 else SERIAL_V1_FRAME_SIZE
    if len(data) < frame_size:
        return None

    # Extract RGB payload.
    payload = data[SERIAL_HEADER_SIZE:SERIAL_HEADER_SIZE + RGB_BYTES]
    frame = np.frombuffer(payload, dtype=np.uint8).reshape(NUM_LEDS, 3).copy()

    meta = FrameMetadata(
        version=version,
        tap=tap,
        effect=effect_id,
        palette=palette_id,
        brightness=brightness,
        speed=speed,
        frame_idx=frame_idx,
        timestamp_us=timestamp_us,
    )

    # Parse v2 trailer if present.
    trailer_offset = SERIAL_HEADER_SIZE + RGB_BYTES
    if version >= 2 and len(data) >= trailer_offset + SERIAL_V2_TRAILER_SIZE:
        t = data[trailer_offset:]
        meta.show_time_us = struct.unpack_from('<H', t, 0)[0]
        meta.rms = struct.unpack_from('<H', t, 2)[0] / 65535.0
        meta.bands = [t[4 + i] / 255.0 for i in range(8)]
        meta.beat = bool(t[12])
        meta.onset = bool(t[13])
        meta.flux = struct.unpack_from('<H', t, 14)[0] / 65535.0
        meta.heap_free = struct.unpack_from('<I', t, 16)[0]
        meta.show_skips = struct.unpack_from('<H', t, 20)[0]
        bpm_raw = struct.unpack_from('<H', t, 22)[0]
        meta.bpm = bpm_raw / 100.0 if bpm_raw > 0 else 0.0
        conf_raw = struct.unpack_from('<H', t, 24)[0]
        meta.beat_confidence = conf_raw / 65535.0

    return frame, meta


def _expected_frame_size(version: int) -> int:
    """Return the expected byte length for a given frame version."""
    if version >= 2:
        return SERIAL_V2_FRAME_SIZE
    return SERIAL_V1_FRAME_SIZE


# ---------------------------------------------------------------------------
# K1Device
# ---------------------------------------------------------------------------

class K1Device:
    """Serial interface to a K1 device for EdgeMixer evaluation.

    Parameters
    ----------
    port : str
        Serial port path (e.g. ``/dev/cu.usbmodem212401``).
    baud : int
        Baud rate. K1 uses 115200.
    open_settle : float
        Seconds to wait after opening the port, since USB CDC open
        resets the ESP32-S3 board (DTR/RTS toggle).
    """

    def __init__(
        self,
        port: str,
        baud: int = 115200,
        open_settle: float = 3.0,
    ):
        self._port_name = port
        self._baud = baud

        logger.info("Opening serial port %s at %d baud...", port, baud)
        self._ser = serial.Serial(
            port,
            baud,
            timeout=0.1,
            dsrdtr=False,
            rtscts=False,
        )
        # Opening the USB CDC port resets the board. Wait for it to boot.
        time.sleep(open_settle)
        self._ser.reset_input_buffer()
        logger.info("Port %s ready.", port)

    # ------------------------------------------------------------------
    # Effect control
    # ------------------------------------------------------------------

    def set_effect(self, effect_id: int, brightness: int = 255) -> None:
        """Set the active effect and brightness on the device.

        Parameters
        ----------
        effect_id : int
            Effect ID (e.g. 0x0100 for Fire).
        brightness : int
            Brightness 0-255.
        """
        self._send_json({"type": "setBrightness", "brightness": brightness})
        time.sleep(0.1)
        self._send_json({"type": "setEffect", "effectId": effect_id})
        time.sleep(0.5)
        self._ser.reset_input_buffer()
        logger.info("Set effect=0x%04X brightness=%d", effect_id, brightness)

    # ------------------------------------------------------------------
    # EdgeMixer configuration
    # ------------------------------------------------------------------

    def set_edgemixer(
        self,
        profile: Profile,
        settle_time: float = 0.5,
        verify: bool = True,
        max_retries: int = 2,
    ) -> None:
        """Apply an EdgeMixer profile to the device with mandatory verification.

        Sends a ``setEdgeMixer`` JSON command, waits for the pipeline to
        stabilise, then verifies via ``getEdgeMixer`` that the device state
        matches. Raises ``RuntimeError`` if verification fails after retries.

        Parameters
        ----------
        profile : Profile
            The EdgeMixer parameter profile to apply.
        settle_time : float
            Seconds to wait after sending for the actor message queue to
            deliver and the render pipeline to reach steady state.
        verify : bool
            If True (default), call ``getEdgeMixer`` after settle and
            confirm mode/spatial/temporal match the profile. This prevents
            silent failures where commands are dropped or mis-parsed.
        max_retries : int
            Number of SET+verify attempts before raising.
        """
        for attempt in range(1, max_retries + 1):
            cmd = {
                "type": "setEdgeMixer",
                "mode": profile.mode,
                "spread": profile.spread,
                "strength": profile.strength,
                "spatial": profile.spatial,
                "temporal": profile.temporal,
            }
            self._send_json(cmd)
            logger.info(
                "Set EdgeMixer: %s (mode=%d spread=%d strength=%d spatial=%d temporal=%d) [attempt %d]",
                profile.name, profile.mode, profile.spread,
                profile.strength, profile.spatial, profile.temporal, attempt,
            )
            if settle_time > 0:
                time.sleep(settle_time)

            if not verify:
                return

            # Verify the state took effect.
            state = self.get_edgemixer(timeout=2.0)
            if state is None:
                logger.warning("Verification failed: no response from getEdgeMixer (attempt %d/%d)",
                               attempt, max_retries)
                continue

            # Check the fields that matter.
            mismatches = []
            for field in ("mode", "spatial", "temporal"):
                expected = getattr(profile, field)
                actual = state.get(field)
                if actual != expected:
                    mismatches.append(f"{field}: expected={expected} actual={actual}")

            if not mismatches:
                logger.info("Verified EdgeMixer state: mode=%d spatial=%d temporal=%d",
                            state.get("mode"), state.get("spatial"), state.get("temporal"))
                return

            logger.warning("Verification mismatch (attempt %d/%d): %s",
                           attempt, max_retries, "; ".join(mismatches))

        raise RuntimeError(
            f"EdgeMixer set_edgemixer failed after {max_retries} attempts. "
            f"Profile '{profile.name}' did not take effect. "
            f"Last mismatches: {'; '.join(mismatches)}"
        )

    def get_edgemixer(self, timeout: float = 3.0) -> Optional[Dict]:
        """Query the current EdgeMixer configuration from the device.

        Sends a ``getEdgeMixer`` JSON command and parses the response.

        Parameters
        ----------
        timeout : float
            Maximum seconds to wait for the response line.

        Returns
        -------
        dict or None
            The ``data`` payload from the response, e.g.
            ``{"mode":1, "modeName":"analogous", "spread":30, ...}``,
            or None if no valid response was received within the timeout.
        """
        # Drain any pending input first.
        self._ser.reset_input_buffer()

        self._send_json({"type": "getEdgeMixer"})

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self._ser.readline()
            if not line:
                continue

            # The device may emit binary capture frames and log lines.
            # We look for a JSON line containing the getEdgeMixer response.
            try:
                text = line.decode("utf-8", errors="replace").strip()
            except Exception:
                continue

            if '"type":"getEdgeMixer"' not in text and '"type": "getEdgeMixer"' not in text:
                continue

            try:
                obj = json.loads(text)
            except json.JSONDecodeError:
                continue

            if obj.get("type") == "getEdgeMixer" and obj.get("success"):
                return obj.get("data")

        logger.warning("getEdgeMixer timed out after %.1fs", timeout)
        return None

    # ------------------------------------------------------------------
    # Frame capture
    # ------------------------------------------------------------------

    def capture_frames(
        self,
        n_frames: int,
        fps: int = 30,
        tap: str = "c",
        timeout: float = 0.0,
        settle_time: float = 0.0,
    ) -> List[Tuple[np.ndarray, FrameMetadata]]:
        """Capture *n_frames* binary LED frames from the device.

        Sends ``capture stream <tap> <fps>``, collects frames, then sends
        ``capture stop``.

        Parameters
        ----------
        n_frames : int
            Number of complete frames to collect.
        fps : int
            Capture rate requested from the device.
        tap : str
            Capture tap point: ``'a'``=pre-correction, ``'b'``=post-correction,
            ``'c'``=pre-WS2812.
        timeout : float
            Maximum seconds to wait for all frames. If 0, defaults to
            ``(n_frames / fps) * 3 + 5`` -- generous headroom.
        settle_time : float
            Seconds to wait after starting the stream before counting
            frames, allowing the capture pipeline to reach steady state.

        Returns
        -------
        list of (ndarray, FrameMetadata)
            Each entry is a ``(frame, metadata)`` tuple where *frame* is
            a ``(320, 3)`` uint8 numpy array of RGB values. The list may
            contain fewer than *n_frames* entries if the timeout elapsed.
        """
        tap_letter = tap.lower()
        if tap_letter not in ("a", "b", "c"):
            raise ValueError(f"tap must be 'a', 'b', or 'c', got {tap!r}")

        if timeout <= 0:
            timeout = (n_frames / max(1, fps)) * 3.0 + 5.0

        self._ser.reset_input_buffer()

        # Start streaming.
        start_cmd = f"capture stream {tap_letter} {fps}\n"
        self._ser.write(start_cmd.encode())
        logger.info("Started capture: tap=%s fps=%d n_frames=%d timeout=%.1fs",
                     tap_letter, fps, n_frames, timeout)

        # Optional settle period -- discard early frames while the
        # capture pipeline stabilises.
        if settle_time > 0:
            time.sleep(settle_time)
            self._ser.reset_input_buffer()

        results: List[Tuple[np.ndarray, FrameMetadata]] = []
        recv_buf = bytearray()
        deadline = time.monotonic() + timeout

        while len(results) < n_frames and time.monotonic() < deadline:
            waiting = self._ser.in_waiting
            if waiting > 0:
                recv_buf.extend(self._ser.read(waiting))
            else:
                time.sleep(0.005)
                continue

            # Scan for complete frames in the buffer.
            while len(results) < n_frames:
                magic_idx = recv_buf.find(bytes([SERIAL_MAGIC]))
                if magic_idx < 0:
                    recv_buf.clear()
                    break

                # Discard bytes before magic.
                if magic_idx > 0:
                    recv_buf = recv_buf[magic_idx:]

                # Need at least the header to determine version.
                if len(recv_buf) < 2:
                    break

                version = recv_buf[1]
                frame_size = _expected_frame_size(version)

                if len(recv_buf) < frame_size:
                    break  # Wait for more data.

                frame_data = bytes(recv_buf[:frame_size])
                recv_buf = recv_buf[frame_size:]

                result = _parse_serial_frame(frame_data)
                if result is not None:
                    results.append(result)

        # Stop streaming.
        self._ser.write(b"capture stop\n")
        time.sleep(0.2)
        # Drain any remaining capture data.
        self._ser.reset_input_buffer()

        if len(results) < n_frames:
            logger.warning(
                "Captured %d/%d frames (%.1fs timeout).",
                len(results), n_frames, timeout,
            )
        else:
            logger.info("Captured %d frames.", len(results))

        return results

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def close(self) -> None:
        """Close the serial port."""
        if self._ser and self._ser.is_open:
            # Ensure capture is stopped before closing.
            try:
                self._ser.write(b"capture stop\n")
                time.sleep(0.1)
            except Exception:
                pass
            self._ser.close()
            logger.info("Closed port %s.", self._port_name)

    def __enter__(self) -> "K1Device":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __repr__(self) -> str:
        state = "open" if (self._ser and self._ser.is_open) else "closed"
        return f"K1Device({self._port_name!r}, {state})"

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _send_json(self, obj: dict) -> None:
        """Serialise *obj* as compact JSON and send with trailing newline."""
        payload = json.dumps(obj, separators=(",", ":")) + "\n"
        self._ser.write(payload.encode())
