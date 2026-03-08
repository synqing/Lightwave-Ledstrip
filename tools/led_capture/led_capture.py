#!/usr/bin/env python3
"""
LightwaveOS LED Frame Capture & Waterfall Visualiser

Captures LED frame data from one or two K1 devices via WebSocket or serial,
renders waterfall timeline images, and exports as PNG/GIF/MP4.

Usage:
    # Single device via WebSocket
    python led_capture.py --ws ws://192.168.4.1/ws --duration 30 --output waterfall.png

    # Single device via serial
    python led_capture.py --serial /dev/cu.usbmodem212401 --duration 30 --output waterfall.png

    # Side-by-side comparison (two devices)
    python led_capture.py \
        --ws ws://192.168.4.1/ws \
        --compare-serial /dev/cu.usbmodem21401 \
        --duration 30 --output comparison.png

    # Export as animated GIF
    python led_capture.py --ws ws://192.168.4.1/ws --duration 10 --output capture.gif

    # Export as MP4 video
    python led_capture.py --ws ws://192.168.4.1/ws --duration 10 --output capture.mp4

Protocol references:
    WebSocket: 966-byte binary frames (0xFE magic, v1, 2x160 strips, RGB sequential)
    Serial:    976-byte binary frames (0xFD magic, 16-byte header + 960 RGB payload)
"""

import argparse
import asyncio
import json
import struct
import sys
import time
import threading
from pathlib import Path
from typing import Optional

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

NUM_LEDS = 320
LEDS_PER_STRIP = 160
NUM_STRIPS = 2
RGB_BYTES = NUM_LEDS * 3  # 960

# WebSocket frame format (v1)
WS_MAGIC = 0xFE
WS_FRAME_VERSION = 1
WS_HEADER_SIZE = 4  # magic + version + num_strips + leds_per_strip
WS_STRIP_HEADER_SIZE = 1  # strip ID byte
WS_FRAME_SIZE = 966  # 4 + (1 + 480) + (1 + 480)

# Serial capture format
# Header: magic(1) + version(1) + tap(1) + effect(1) + palette(1) + brightness(1)
#         + speed(1) + frameIndex(4) + timestampUs(4) + payloadLen(2) = 17 bytes
SERIAL_MAGIC = 0xFD
SERIAL_HEADER_SIZE = 17
SERIAL_V1_FRAME_SIZE = 977   # 17 + 960
SERIAL_V2_TRAILER_SIZE = 32
SERIAL_V2_FRAME_SIZE = 1009  # 17 + 960 + 32

# Visualisation
WATERFALL_PIXEL_HEIGHT = 3  # vertical pixels per frame row
GAP_WIDTH = 4  # pixel gap between side-by-side strips
LABEL_HEIGHT = 24  # pixels for device label at top


# ---------------------------------------------------------------------------
# Frame Parsing
# ---------------------------------------------------------------------------

def parse_ws_frame(data: bytes) -> Optional[np.ndarray]:
    """Parse a WebSocket LED frame (966 bytes) into (320, 3) uint8 array."""
    if len(data) < WS_FRAME_SIZE:
        return None
    if data[0] != WS_MAGIC:
        return None
    if data[1] != WS_FRAME_VERSION:
        return None

    # Strip 0: bytes 5..484 (480 bytes = 160 LEDs x 3)
    strip0_start = WS_HEADER_SIZE + WS_STRIP_HEADER_SIZE
    strip0_end = strip0_start + LEDS_PER_STRIP * 3

    # Strip 1: bytes 486..965 (480 bytes = 160 LEDs x 3)
    strip1_start = strip0_end + WS_STRIP_HEADER_SIZE
    strip1_end = strip1_start + LEDS_PER_STRIP * 3

    strip0 = np.frombuffer(data[strip0_start:strip0_end], dtype=np.uint8).reshape(LEDS_PER_STRIP, 3)
    strip1 = np.frombuffer(data[strip1_start:strip1_end], dtype=np.uint8).reshape(LEDS_PER_STRIP, 3)

    return np.vstack([strip0, strip1])  # (320, 3)


def parse_serial_frame(data: bytes) -> Optional[tuple]:
    """Parse a serial capture frame (v1=977 bytes, v2=1009 bytes).

    Returns (frame_array, metadata) or None.

    Known limitation: the serial capture protocol has no CRC or checksum
    field. Frame integrity relies solely on the magic byte and payload
    length validation. Corrupt frames that happen to pass these checks
    are flagged via the ``suspect`` metadata key (see below).
    """
    if len(data) < SERIAL_V1_FRAME_SIZE:
        return None
    if data[0] != SERIAL_MAGIC:
        return None

    version = data[1]
    # Reject frames with unrecognised version bytes — likely corruption.
    if version not in (1, 2):
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

    payload = data[SERIAL_HEADER_SIZE:SERIAL_HEADER_SIZE + RGB_BYTES]
    frame = np.frombuffer(payload, dtype=np.uint8).reshape(NUM_LEDS, 3)

    # Sanity check: if >90% of payload bytes are identical, the frame is
    # likely corrupted (e.g. a stuck serial buffer or memset artefact).
    # Note: np.unique on a bytes object treats it as a single element,
    # so we must operate on the already-parsed uint8 frame array.
    _unique_ratio = len(np.unique(frame)) / max(1, len(payload))
    suspect = _unique_ratio < 0.10  # fewer than 10% unique byte values

    metadata = {
        'version': version,
        'tap': tap,
        'effect': effect_id,
        'palette': palette_id,
        'brightness': brightness,
        'speed': speed,
        'frame_idx': frame_idx,
        'timestamp_us': timestamp_us,
        'suspect': suspect,
    }

    # v2 metrics trailer (32 bytes after RGB payload)
    trailer_offset = SERIAL_HEADER_SIZE + RGB_BYTES
    if version >= 2 and len(data) >= trailer_offset + SERIAL_V2_TRAILER_SIZE:
        t = data[trailer_offset:]
        metadata['show_time_us'] = struct.unpack_from('<H', t, 0)[0]
        metadata['rms'] = struct.unpack_from('<H', t, 2)[0] / 65535.0
        metadata['bands'] = [t[4 + i] / 255.0 for i in range(8)]
        metadata['beat'] = bool(t[12])
        metadata['onset'] = bool(t[13])
        metadata['flux'] = struct.unpack_from('<H', t, 14)[0] / 65535.0
        metadata['heap_free'] = struct.unpack_from('<I', t, 16)[0]
        metadata['show_skips'] = struct.unpack_from('<H', t, 20)[0]
        # v2.1 fields (repurposed padding, backwards-compatible: old firmware writes zeros)
        bpm_raw = struct.unpack_from('<H', t, 22)[0]
        metadata['bpm'] = bpm_raw / 100.0 if bpm_raw > 0 else 0.0
        conf_raw = struct.unpack_from('<H', t, 24)[0]
        metadata['beat_confidence'] = conf_raw / 65535.0

    return frame.copy(), metadata


# ---------------------------------------------------------------------------
# Frame Buffer
# ---------------------------------------------------------------------------

class FrameBuffer:
    """Circular buffer of LED frames backed by a NumPy array."""

    def __init__(self, max_frames: int = 7200):
        self.max_frames = max_frames
        self.frames = np.zeros((max_frames, NUM_LEDS, 3), dtype=np.uint8)
        self.timestamps = np.zeros(max_frames, dtype=np.float64)
        self.count = 0
        self.write_idx = 0
        self.lock = threading.Lock()

    def append(self, frame: np.ndarray, timestamp: Optional[float] = None):
        if timestamp is None:
            timestamp = time.time()
        with self.lock:
            self.frames[self.write_idx] = frame
            self.timestamps[self.write_idx] = timestamp
            self.write_idx = (self.write_idx + 1) % self.max_frames
            self.count = min(self.count + 1, self.max_frames)

    def get_all(self) -> tuple:
        """Return (frames, timestamps) in chronological order."""
        with self.lock:
            if self.count < self.max_frames:
                return self.frames[:self.count].copy(), self.timestamps[:self.count].copy()
            else:
                # Circular wrap — reorder
                idx = self.write_idx
                frames = np.roll(self.frames, -idx, axis=0)
                timestamps = np.roll(self.timestamps, -idx)
                return frames.copy(), timestamps.copy()

    @property
    def frame_count(self) -> int:
        with self.lock:
            return self.count


# ---------------------------------------------------------------------------
# WebSocket Capture
# ---------------------------------------------------------------------------

async def capture_ws(url: str, buffer: FrameBuffer, duration: float, stop_event: asyncio.Event):
    """Capture LED frames from a WebSocket LED stream."""
    try:
        import websockets
    except ImportError:
        print("ERROR: 'websockets' package required. Install: pip install websockets", file=sys.stderr)
        return

    print(f"[WS] Connecting to {url}...")
    try:
        async with websockets.connect(url, ping_interval=20, ping_timeout=10) as ws:
            # Subscribe to LED stream
            subscribe_msg = json.dumps({"type": "ledStream.subscribe"})
            await ws.send(subscribe_msg)

            # Wait for subscription confirmation
            response = await asyncio.wait_for(ws.recv(), timeout=5.0)
            if isinstance(response, str):
                resp_data = json.loads(response)
                if resp_data.get("type") == "ledStream.subscribed":
                    info = resp_data.get("data", {})
                    print(f"[WS] Subscribed: {info.get('frameSize')} bytes/frame, "
                          f"target {info.get('targetFps')} FPS, transport={info.get('transport')}")
                else:
                    print(f"[WS] Unexpected response: {resp_data.get('type')}")

            start_time = time.time()
            frame_count = 0

            while not stop_event.is_set():
                elapsed = time.time() - start_time
                if elapsed >= duration:
                    break

                try:
                    data = await asyncio.wait_for(ws.recv(), timeout=1.0)
                except asyncio.TimeoutError:
                    continue

                if isinstance(data, bytes):
                    frame = parse_ws_frame(data)
                    if frame is not None:
                        buffer.append(frame)
                        frame_count += 1
                        if frame_count % 50 == 0:
                            print(f"[WS] {frame_count} frames captured ({elapsed:.1f}s)")

            # Unsubscribe
            try:
                await ws.send(json.dumps({"type": "ledStream.unsubscribe"}))
            except Exception:
                pass

            print(f"[WS] Capture complete: {frame_count} frames in {time.time() - start_time:.1f}s")

    except Exception as e:
        print(f"[WS] Error: {e}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Serial Capture
# ---------------------------------------------------------------------------

def capture_serial(port: str, buffer: FrameBuffer, duration: float,
                   stop_event: threading.Event, fps: int = 15, tap: str = 'b'):
    """Capture LED frames via serial streaming mode (firmware pushes frames)."""
    try:
        import serial
    except ImportError:
        print("ERROR: 'pyserial' package required. Install: pip install pyserial", file=sys.stderr)
        return

    tap_letter = tap.lower()

    print(f"[Serial] Opening {port} (stream tap {tap_letter.upper()}, {fps} FPS)...")
    try:
        ser = serial.Serial(port, 115200, timeout=0.1, dsrdtr=False, rtscts=False)
        time.sleep(3)  # Wait for board to stabilise after port open
        ser.reset_input_buffer()

        # Start streaming — firmware pushes frames continuously
        ser.write(f'capture stream {tap_letter} {fps}\n'.encode())
        # No sleep — frame parser scans for 0xFD magic, text is skipped.

        start_time = time.time()
        frame_count = 0
        recv_buf = bytearray()
        metadata_log = []

        while not stop_event.is_set():
            elapsed = time.time() - start_time
            if elapsed >= duration:
                break

            # Read available data
            waiting = ser.in_waiting
            if waiting > 0:
                recv_buf.extend(ser.read(waiting))
            else:
                time.sleep(0.005)
                continue

            # Scan for complete frames in buffer
            while True:
                magic_idx = recv_buf.find(bytes([SERIAL_MAGIC]))
                if magic_idx < 0:
                    recv_buf.clear()
                    break

                # Discard bytes before magic
                if magic_idx > 0:
                    recv_buf = recv_buf[magic_idx:]

                # Determine frame size from version byte
                if len(recv_buf) < 2:
                    break
                version = recv_buf[1]
                frame_size = SERIAL_V2_FRAME_SIZE if version >= 2 else SERIAL_V1_FRAME_SIZE

                if len(recv_buf) < frame_size:
                    break  # Wait for more data

                frame_data = bytes(recv_buf[:frame_size])
                recv_buf = recv_buf[frame_size:]

                result = parse_serial_frame(frame_data)
                if result is not None:
                    frame, metadata = result
                    buffer.append(frame)
                    metadata_log.append(metadata)
                    frame_count += 1
                    if frame_count % 50 == 0:
                        fps_actual = frame_count / (time.time() - start_time)
                        print(f"[Serial] {frame_count} frames ({elapsed:.1f}s, {fps_actual:.1f} FPS)")

        # Stop streaming
        ser.write(b'capture stop\n')
        time.sleep(0.2)
        ser.close()

        actual_fps = frame_count / max(0.1, time.time() - start_time)
        beats = sum(1 for m in metadata_log if m.get('beat'))
        print(f"[Serial] Capture complete: {frame_count} frames in {time.time() - start_time:.1f}s "
              f"({actual_fps:.1f} FPS, {beats} beats detected)")

    except Exception as e:
        print(f"[Serial] Error: {e}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Waterfall Rendering
# ---------------------------------------------------------------------------

def render_waterfall(frames: np.ndarray, label: str = "",
                     pixel_height: int = WATERFALL_PIXEL_HEIGHT) -> Image.Image:
    """Render a (N, 320, 3) frame array as a vertical waterfall image.

    Each frame becomes a horizontal pixel row, stretched vertically by pixel_height.
    Time flows downward.
    """
    n_frames = frames.shape[0]
    width = NUM_LEDS
    height = n_frames * pixel_height

    # Stretch each frame row vertically
    stretched = np.repeat(frames, pixel_height, axis=0)  # (N*pixel_height, 320, 3)

    img = Image.fromarray(stretched, mode='RGB')

    # Add label if provided
    if label:
        total_height = LABEL_HEIGHT + height
        labelled = Image.new('RGB', (width, total_height), (20, 20, 20))

        draw = ImageDraw.Draw(labelled)
        try:
            font = ImageFont.truetype("/System/Library/Fonts/Menlo.ttc", 11)
        except (OSError, IOError):
            font = ImageFont.load_default()
        draw.text((4, 4), label, fill=(200, 200, 200), font=font)

        labelled.paste(img, (0, LABEL_HEIGHT))
        return labelled

    return img


def render_comparison(frames_a: np.ndarray, frames_b: np.ndarray,
                      label_a: str = "Device A", label_b: str = "Device B",
                      pixel_height: int = WATERFALL_PIXEL_HEIGHT) -> Image.Image:
    """Render two frame buffers side-by-side with a gap between them."""
    # Align frame counts (pad shorter with black)
    max_frames = max(frames_a.shape[0], frames_b.shape[0])

    if frames_a.shape[0] < max_frames:
        pad = np.zeros((max_frames - frames_a.shape[0], NUM_LEDS, 3), dtype=np.uint8)
        frames_a = np.vstack([frames_a, pad])
    if frames_b.shape[0] < max_frames:
        pad = np.zeros((max_frames - frames_b.shape[0], NUM_LEDS, 3), dtype=np.uint8)
        frames_b = np.vstack([frames_b, pad])

    img_a = render_waterfall(frames_a, label_a, pixel_height)
    img_b = render_waterfall(frames_b, label_b, pixel_height)

    # Combine side-by-side with gap
    total_width = img_a.width + GAP_WIDTH + img_b.width
    total_height = max(img_a.height, img_b.height)

    combined = Image.new('RGB', (total_width, total_height), (40, 40, 40))
    combined.paste(img_a, (0, 0))
    combined.paste(img_b, (img_a.width + GAP_WIDTH, 0))

    return combined


# ---------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------

def export_png(image: Image.Image, path: str):
    """Save waterfall image as PNG."""
    image.save(path, 'PNG')
    print(f"[Export] PNG saved: {path} ({image.width}x{image.height})")


def render_strip_frame(frame: np.ndarray, led_w: int = 4, led_h: int = 12,
                       gap: int = 2, strip_gap: int = 8,
                       bg_color: tuple = (15, 15, 15)) -> np.ndarray:
    """Render a single (320, 3) frame as a strip-view image.

    Shows two horizontal strips stacked vertically, each LED as a coloured
    rectangle — resembling the physical LGP layout.

    Returns an RGB numpy array.
    """
    # Strip 0 = LEDs 0..159, Strip 1 = LEDs 160..319
    strip0 = frame[:LEDS_PER_STRIP]   # (160, 3)
    strip1 = frame[LEDS_PER_STRIP:]   # (160, 3)

    img_w = LEDS_PER_STRIP * led_w
    img_h = 2 * led_h + strip_gap
    img = np.full((img_h, img_w, 3), bg_color, dtype=np.uint8)

    for i in range(LEDS_PER_STRIP):
        x = i * led_w
        # Strip 0 (top row)
        img[0:led_h, x:x + led_w] = strip0[i]
        # Strip 1 (bottom row)
        img[led_h + strip_gap:img_h, x:x + led_w] = strip1[i]

    return img


def _compute_frame_durations(timestamps: np.ndarray, fallback_fps: int = 5) -> list:
    """Compute per-frame durations in milliseconds from real capture timestamps.

    GIF minimum frame duration is 20ms (browser enforced). Clamp to that floor.
    Uses median interval as the duration for the first frame (no prior reference).
    """
    n = len(timestamps)
    fallback_ms = int(1000 / fallback_fps)

    if n < 2:
        return [fallback_ms] * n

    # Inter-frame deltas in ms
    deltas_ms = np.diff(timestamps) * 1000.0

    # Clamp to [20ms, 2000ms] — reject outliers from serial hiccups
    deltas_ms = np.clip(deltas_ms, 20, 2000)

    median_ms = float(np.median(deltas_ms))
    durations = [max(20, int(round(median_ms)))]  # first frame uses median
    for d in deltas_ms:
        durations.append(max(20, int(round(d))))

    return durations


def export_gif(frames: np.ndarray, path: str, timestamps: np.ndarray = None,
               fallback_fps: int = 5, led_w: int = 4, led_h: int = 12, **kwargs):
    """Export frames as animated GIF with real-time playback timing.

    If timestamps are provided, each GIF frame gets its actual inter-frame
    duration so playback matches the real capture timing (including jitter).
    Falls back to uniform interval at fallback_fps if no timestamps given.
    """
    try:
        import imageio.v3 as iio
    except ImportError:
        print("ERROR: 'imageio' package required. Install: pip install imageio", file=sys.stderr)
        return

    # Compute durations
    if timestamps is not None and len(timestamps) == frames.shape[0]:
        durations = _compute_frame_durations(timestamps, fallback_fps)
        median_fps = 1000.0 / np.median(durations)
        print(f"[Export] Generating GIF ({frames.shape[0]} frames, "
              f"real-time durations, median {median_fps:.1f} FPS)...")
    else:
        dur_ms = int(1000 / fallback_fps)
        durations = [dur_ms] * frames.shape[0]
        print(f"[Export] Generating GIF ({frames.shape[0]} frames at {fallback_fps} FPS)...")

    images = []
    for i in range(frames.shape[0]):
        img = render_strip_frame(frames[i], led_w=led_w, led_h=led_h)
        images.append(img)

    iio.imwrite(path, images, duration=durations, loop=0)
    w, h = images[0].shape[1], images[0].shape[0]
    total_dur = sum(durations) / 1000.0
    print(f"[Export] GIF saved: {path} ({len(images)} frames, {w}x{h}px, {total_dur:.1f}s playback)")


def export_mp4(frames: np.ndarray, path: str, timestamps: np.ndarray = None,
               fallback_fps: int = 5, led_w: int = 4, led_h: int = 12, **kwargs):
    """Export frames as MP4 video with real-time playback timing.

    MP4 uses a fixed FPS container, so we duplicate frames to approximate
    real capture timing at 30 FPS output resolution.
    """
    try:
        import imageio.v3 as iio
    except ImportError:
        print("ERROR: 'imageio[ffmpeg]' required. Install: pip install imageio[ffmpeg]", file=sys.stderr)
        return

    output_fps = 30  # MP4 container rate

    if timestamps is not None and len(timestamps) == frames.shape[0]:
        durations = _compute_frame_durations(timestamps, fallback_fps)
        print(f"[Export] Generating MP4 ({frames.shape[0]} source frames, "
              f"real-time timing at {output_fps} FPS container)...")
    else:
        dur_ms = int(1000 / fallback_fps)
        durations = [dur_ms] * frames.shape[0]
        print(f"[Export] Generating MP4 ({frames.shape[0]} frames at {fallback_fps} FPS)...")

    with iio.imopen(path, "w", plugin="pyav") as writer:
        writer.init_video_stream("libx264", fps=output_fps)

        frame_interval_ms = 1000.0 / output_fps

        for i in range(frames.shape[0]):
            img = render_strip_frame(frames[i], led_w=led_w, led_h=led_h)
            # Duplicate frame to fill its real duration at the output FPS
            n_repeats = max(1, int(round(durations[i] / frame_interval_ms)))
            for _ in range(n_repeats):
                writer.write_frame(img)

    total_dur = sum(durations) / 1000.0
    print(f"[Export] MP4 saved: {path} ({total_dur:.1f}s playback)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='LightwaveOS LED Frame Capture & Waterfall Visualiser',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    # Primary device
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--ws', metavar='URL', help='WebSocket URL (e.g. ws://192.168.4.1/ws)')
    group.add_argument('--serial', metavar='PORT', help='Serial port (e.g. /dev/cu.usbmodem212401)')

    # Comparison device (optional)
    parser.add_argument('--compare-ws', metavar='URL', help='Second device WebSocket URL')
    parser.add_argument('--compare-serial', metavar='PORT', help='Second device serial port')

    # Capture settings
    parser.add_argument('--duration', type=float, default=30.0, help='Capture duration in seconds (default: 30)')
    parser.add_argument('--serial-fps', type=int, default=5, help='Serial capture rate in FPS (default: 5)')
    parser.add_argument('--tap', choices=['a', 'b', 'c'], default='b',
                        help='Serial capture tap point: a=pre-correction, b=post-correction, c=pre-WS2812 (default: b)')

    # Output
    parser.add_argument('--output', '-o', default='waterfall.png',
                        help='Output file path (.png, .gif, or .mp4)')
    parser.add_argument('--pixel-height', type=int, default=WATERFALL_PIXEL_HEIGHT,
                        help=f'Vertical pixels per frame row (default: {WATERFALL_PIXEL_HEIGHT})')

    # Labels
    parser.add_argument('--label', default=None, help='Label for primary device')
    parser.add_argument('--compare-label', default=None, help='Label for comparison device')

    args = parser.parse_args()

    # Determine output format
    output_path = Path(args.output)
    output_ext = output_path.suffix.lower()
    if output_ext not in ('.png', '.gif', '.mp4'):
        print(f"ERROR: Unsupported output format '{output_ext}'. Use .png, .gif, or .mp4", file=sys.stderr)
        sys.exit(1)

    # Create buffers
    max_frames = int(args.duration * max(20, getattr(args, 'fps', 20) * 1.5)) + 100
    buf_a = FrameBuffer(max_frames)
    buf_b = FrameBuffer(max_frames) if (args.compare_ws or args.compare_serial) else None

    is_comparison = buf_b is not None

    # Default labels
    label_a = args.label or (args.ws if args.ws else args.serial)
    label_b = args.compare_label or (args.compare_ws or args.compare_serial or "Device B")

    print(f"Capture: {args.duration}s, output: {args.output}")
    if is_comparison:
        print(f"Comparison mode: [{label_a}] vs [{label_b}]")

    # --- Run captures ---

    threads = []
    stop_serial = threading.Event()

    # Primary device — serial
    if args.serial:
        t = threading.Thread(target=capture_serial,
                             args=(args.serial, buf_a, args.duration, stop_serial,
                                   args.serial_fps, args.tap),
                             daemon=True)
        threads.append(t)

    # Comparison device — serial
    if args.compare_serial:
        t = threading.Thread(target=capture_serial,
                             args=(args.compare_serial, buf_b, args.duration, stop_serial,
                                   args.serial_fps, args.tap),
                             daemon=True)
        threads.append(t)

    # Start serial threads
    for t in threads:
        t.start()

    # Run async WebSocket captures
    async def run_ws_captures():
        stop_ws = asyncio.Event()
        tasks = []

        if args.ws:
            tasks.append(capture_ws(args.ws, buf_a, args.duration, stop_ws))
        if args.compare_ws:
            tasks.append(capture_ws(args.compare_ws, buf_b, args.duration, stop_ws))

        if tasks:
            await asyncio.gather(*tasks)

    if args.ws or args.compare_ws:
        asyncio.run(run_ws_captures())

    # Wait for serial threads to complete (they exit via their own duration check)
    for t in threads:
        t.join(timeout=args.duration + 15)  # Extra headroom for 3s stabilisation wait
    stop_serial.set()  # Signal any stragglers
    for t in threads:
        t.join(timeout=5)

    # --- Check results ---
    print(f"\nCapture results:")
    print(f"  Device A: {buf_a.frame_count} frames")
    if buf_b:
        print(f"  Device B: {buf_b.frame_count} frames")

    if buf_a.frame_count == 0:
        print("ERROR: No frames captured from primary device.", file=sys.stderr)
        sys.exit(1)

    # --- Render & Export ---
    frames_a, ts_a = buf_a.get_all()

    if is_comparison and buf_b.frame_count > 0:
        frames_b, ts_b = buf_b.get_all()

        if output_ext == '.png':
            img = render_comparison(frames_a, frames_b, label_a, label_b, args.pixel_height)
            export_png(img, str(output_path))
        elif output_ext == '.gif':
            # For GIF comparison, render combined frames
            img = render_comparison(frames_a, frames_b, label_a, label_b, args.pixel_height)
            export_png(img, str(output_path))  # fallback to PNG for comparison GIF
            print("Note: GIF export for comparison mode saves as static PNG. Use .mp4 for animation.")
        elif output_ext == '.mp4':
            img = render_comparison(frames_a, frames_b, label_a, label_b, args.pixel_height)
            export_png(img, str(output_path.with_suffix('.png')))
            print("Note: MP4 comparison export not yet implemented. Saved as PNG.")
    else:
        if output_ext == '.png':
            img = render_waterfall(frames_a, label_a, args.pixel_height)
            export_png(img, str(output_path))
        elif output_ext == '.gif':
            export_gif(frames_a, str(output_path), timestamps=ts_a,
                       fallback_fps=args.serial_fps)
        elif output_ext == '.mp4':
            export_mp4(frames_a, str(output_path), timestamps=ts_a,
                       fallback_fps=args.serial_fps)

    print("Done.")


if __name__ == '__main__':
    main()
