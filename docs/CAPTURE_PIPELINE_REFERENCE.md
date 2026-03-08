# LED Capture Pipeline Reference

> Canonical reference for the LightwaveOS LED frame capture, visualisation, and
> analysis pipeline. Covers every byte, every function, and every CLI flag from
> ESP32 firmware serial output through Python capture, frame parsing, waterfall
> rendering, GIF/MP4 export, and analyser dashboard generation.

**Source files:**

| Component | File |
|-----------|------|
| Firmware capture | `firmware-v3/src/main.cpp` (lines 141--1825) |
| FPS sweep tool | `tools/led_capture/fps_sweep.py` |
| Python capture & waterfall | `tools/led_capture/led_capture.py` (731 lines) |
| Beat-brightness analyser | `tools/led_capture/analyze_beats.py` (~1750 lines) |

---

## 1. Architecture Overview

```
+------------------+          USB CDC Serial (115200 baud)
|   ESP32-S3       |  ──────────────────────────────────────>  +-----------------+
|                  |  Binary frames:                           |  Python Host    |
|  RendererActor   |    v1: 977B, v2: 1009B                    |                 |
|    (Core 1)      |    v3/meta: 49B, v4/slim: 529B            |  led_capture.py |
|                  |  at 1--60 FPS (configurable)              |  or             |
|  captureProducer |                                           |  analyze_beats  |
|  Task (Core 0)   |  <── "capture stream b 30\n" (start)      |  .py            |
|  + esp_timer     |  <── "capture stop\n"       (stop)        +---------+-------+
+------------------+                                                     |
                                                                         v
                                                              +----------+---------+
                                                              |  Frame Buffer      |
                                                              |  (NumPy circular)  |
                                                              +----------+---------+
                                                                         |
                          +------------------+-------------------+--------+-------+
                          |                  |                   |                |
                          v                  v                   v                v
                   +------+-----+    +-------+------+    +------+------+  +------+------+
                   | Waterfall  |    | GIF Export   |    | MP4 Export  |  | Analyser    |
                   | PNG        |    | (strip-view) |    | (strip-view)|  | Dashboard   |
                   | (320-wide) |    | (640x32)     |    | (640x32)    |  | PNG         |
                   +------------+    +--------------+    +-------------+  +-------------+
```

### Entry Points

| Entry point | Description |
|-------------|-------------|
| `python led_capture.py --serial PORT --duration N --output FILE` | Capture frames, export waterfall/GIF/MP4 |
| `python led_capture.py --ws URL --duration N --output FILE` | WebSocket capture (WiFi, mostly legacy) |
| `python analyze_beats.py --serial PORT --duration N` | Capture + compute audio-reactive quality scores |
| `python analyze_beats.py --load session.npz` | Analyse previously saved capture data |
| Serial command `capture stream b 15` | Start firmware-side binary frame streaming |
| Serial command `capture stop` | Stop streaming |

---

## 2. Firmware Side: `capture stream` Command

### 2.1 Command Parsing

The serial command handler lives in `loop()` in `main.cpp`. Commands arrive over
USB CDC at 115200 baud. Lines are buffered into `serialCmdBuffer` until
`\n`/`\r` (or processed immediately for single-char hotkeys).

Any line beginning with `capture` is routed to the capture sub-handler
(line ~1645). The sub-command is extracted by stripping the `capture` prefix:

```cpp
String subcmd = inputLower.substring(7); // after "capture"
subcmd.trim();
```

### 2.2 `capture stream` Variants

**Syntax:** `capture stream [tap] [fps]`

| Parameter | Values | Default | Description |
|-----------|--------|---------|-------------|
| `tap` | `a`, `b`, `c` | `b` | Capture tap point in the render pipeline |
| `fps` | `1`--`60` | `15` | Target streaming rate |

**Tap points:**

| Tap | Enum | Description |
|-----|------|-------------|
| `a` | `TAP_A_PRE_CORRECTION` | Raw effect output before colour correction |
| `b` | `TAP_B_POST_CORRECTION` | After colour correction (default, matches visible output) |
| `c` | `TAP_C_PRE_WS2812` | Final pixel data before RMT transmission |

**What happens on `capture stream b 15`:**

1. `renderer->setCaptureMode(true, tapMask)` enables capture for the specified tap.
2. Global state variables are set:
   - `s_captureStreamTap` = `TAP_B_POST_CORRECTION`
   - `s_captureStreamTapMask` = `0x02`
   - `s_captureStreamIntervalUs` = `1000000 / 30` = `33333`
   - `s_captureStreamLastPushUs` = `0`
   - `s_captureStreamLastFrameIdx` = `UINT32_MAX`
   - `s_captureStreamActive` = `true`
3. Firmware prints: `[CAPTURE] Streaming tap B at 30 FPS (33333 us interval) [async+timer]`

### 2.3 Frame Transmission Architecture

Capture runs on a **dedicated FreeRTOS task** (`captureProducerTask`) pinned to
Core 0, paced by an `esp_timer` with microsecond resolution. This decouples
capture from the Arduino `loop()` task and eliminates tick quantization.

**Architecture:**

```
esp_timer (periodic, µs resolution)
    │  fires at target interval
    ▼
captureTimerCallback()          ◀── runs in esp_timer task (high priority)
    │  xTaskNotifyGive()             does NO work, just wakes the producer
    ▼
captureProducerTask             ◀── FreeRTOS task, Core 0, priority 2
    │  ulTaskNotifyTake()            blocks until notified
    │  getCapturedFrame()            copies 320 CRGBs from renderer snapshot
    │  duplicate check               skips if frameIndex unchanged
    │  assemble header+RGB+trailer   into s_captureFrameBuf
    │  Serial.write(buf, pos)        blocking bulk write via HWCDC
    ▼
USB CDC host (Python)
```

**Operational limits (measured, v2 1009-byte frames):**

| Mode | FPS | Notes |
|------|-----|-------|
| Reference | 30 | Exact target tracking, zero drops |
| Stress | 40 | 97% of target, clean |
| Lab-only | 50--60 | 3--4% drops, show time rises |

The remaining ceiling at ~47 FPS is most likely dominated by blocking USB CDC
writes (~14ms per 1009-byte frame) plus residual capture/assembly overhead.
The HWCDC TX ring buffer is enlarged to 4096 bytes at boot
(`Serial.setTxBufferSize(4096)`) to reduce write blocking.

A sync fallback path in `loop()` remains for robustness — it runs only if
the async task fails to create.

### 2.4 Binary Frame Format v2

Total frame size: **1009 bytes** (17-byte header + 960-byte RGB payload + 32-byte metrics trailer).

> **Note:** The Python parser documents SERIAL_HEADER_SIZE as 17 and
> SERIAL_V2_FRAME_SIZE as 1009. The firmware writes 17 header bytes
> (including 1 magic + 1 version + 15 metadata fields), matching the
> Python parser's expectations exactly.

#### Header (17 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | `u8` | `magic` | Always `0xFD` |
| 1 | 1 | `u8` | `version` | `1` = v1 (no trailer), `2` = v2 (with trailer) |
| 2 | 1 | `u8` | `tap` | Tap point enum: `0`=A, `1`=B, `2`=C |
| 3 | 1 | `u8` | `effectId` | Current effect ID (low byte only) |
| 4 | 1 | `u8` | `paletteId` | Current palette index |
| 5 | 1 | `u8` | `brightness` | Global brightness (0--255) |
| 6 | 1 | `u8` | `speed` | Global speed (0--255) |
| 7 | 4 | `u32 LE` | `frameIndex` | Monotonic frame counter from RendererActor |
| 11 | 4 | `u32 LE` | `timestampUs` | `esp_timer_get_time()` at capture moment |
| 15 | 2 | `u16 LE` | `payloadLen` | Always `960` (320 LEDs x 3 bytes RGB) |

#### RGB Payload (960 bytes)

| Offset | Size | Description |
|--------|------|-------------|
| 17 | 960 | 320 LEDs x 3 bytes (R, G, B per LED). Strip 0 = LEDs 0--159, Strip 1 = LEDs 160--319. Written as raw `CRGB` array via `Serial.write((uint8_t*)frame, 960)`. |

#### Metrics Trailer (32 bytes, v2 only)

| Offset (from trailer start) | Absolute offset | Size | Type | Field | Encoding | Python accessor |
|-----|------|------|------|-------|----------|-----------------|
| 0 | 977 | 2 | `u16 LE` | `show_time_us` | Microseconds, capped at 65535 | `metadata['show_time_us']` |
| 2 | 979 | 2 | `u16 LE` | `rms` | `float * 65535` | `metadata['rms']` (divided by 65535.0) |
| 4 | 981 | 8 | `u8[8]` | `bands[0..7]` | `float * 255` per band | `metadata['bands']` (list of 8 floats, divided by 255.0) |
| 12 | 989 | 1 | `u8` | `beat` | `1` if beat tick active, else `0` | `metadata['beat']` (bool) |
| 13 | 990 | 1 | `u8` | `onset` | `1` if snare or hihat trigger, else `0` | `metadata['onset']` (bool) |
| 14 | 991 | 2 | `u16 LE` | `flux` | `float * 65535` | `metadata['flux']` (divided by 65535.0) |
| 16 | 993 | 4 | `u32 LE` | `heap_free` | `esp_get_free_heap_size()` raw bytes | `metadata['heap_free']` |
| 20 | 997 | 2 | `u16 LE` | `show_skips` | Cumulative skip counter, capped at 65535 | `metadata['show_skips']` |
| 22 | 999 | 2 | `u16 LE` | `bpm` | `BPM * 100` (0.01 resolution) | `metadata['bpm']` (divided by 100.0) |
| 24 | 1001 | 2 | `u16 LE` | `beat_confidence` | `float * 65535` | `metadata['beat_confidence']` (divided by 65535.0) |
| 26 | 1003 | 6 | `u8[6]` | `reserved` | Zero-padded, reserved for future use | Not parsed |

**Beat source logic:** The firmware uses `cbf.tempoBeatTick || cbf.es_beat_tick`
for the beat flag, combining both PipelineCore and ESV11 backend beat detectors.

**BPM source logic:** Prefers ESV11 BPM (`cbf.es_bpm`) if non-zero and not the
default 120.0; otherwise falls back to PipelineCore (`cbf.tempoBpm`).

**Confidence source logic:** Prefers ESV11 confidence (`cbf.es_tempo_confidence`)
if non-zero; otherwise falls back to PipelineCore (`cbf.tempoConfidence`).

### 2.5 v1 Frame Format

Total frame size: **977 bytes** (17-byte header + 960-byte RGB payload, no trailer).
Identical header to v2 but with `version = 1`. Used with `capture format v1` or
single-shot `capture dump`.

### 2.6 Other Capture Serial Commands

| Command | Description |
|---------|-------------|
| `capture stop` | Sets `s_captureStreamActive = false`. Firmware prints `[CAPTURE] Streaming stopped`. |
| `capture fps N` | Changes streaming rate without restarting. `N` must be 1--60. Updates `s_captureStreamIntervalMs = 1000 / N`. |
| `capture tap a\|b\|c` | Changes the tap point without restarting. Also calls `renderer->setCaptureMode(true, newMask)` if capture is already enabled. |
| `capture format v1\|v2` | Switches between 977-byte (v1) and 1009-byte (v2) frame format. Default is v2. |
| `capture status` | Prints whether capture is enabled, streaming state, version, interval, and last frame metadata. |
| `capture on [a][b][c]` | Enables capture mode (non-streaming). Tap mask defaults to `0x07` (all taps). |
| `capture off` | Disables capture mode entirely. |
| `capture dump a\|b\|c` | Sends a single v1 frame for the specified tap. Forces a one-shot capture if none is cached. |

### 2.7 Audio Snapshot: `copyCachedAudioFrame()`

The metrics trailer reads audio state from `ControlBusFrame cbf` via
`renderer->copyCachedAudioFrame(cbf)`. This is a thread-safe snapshot of the
ControlBus written by the AudioActor on Core 0 and read by Core 1 code (and now
by the Core 0 `loop()` capture handler). The snapshot includes fields such as
`tempoBeatTick`, `es_beat_tick`, `snareTrigger`, `hihatTrigger`, `tempoBpm`,
`es_bpm`, `tempoConfidence`, and `es_tempo_confidence`.

The `BandsDebugSnapshot` (for RMS, bands, and flux) comes from
`renderer->getBandsDebugSnapshot(bandsSnap)`, which reads the most recent
audio-reactive state cached in the RendererActor.

---

## 3. Python Capture: `led_capture.py`

### 3.1 CLI Argument Parsing

```
python led_capture.py --serial PORT [OPTIONS]
python led_capture.py --ws URL [OPTIONS]
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--ws URL` | string | (mutually exclusive with `--serial`) | WebSocket URL, e.g. `ws://192.168.4.1/ws` |
| `--serial PORT` | string | (mutually exclusive with `--ws`) | Serial port, e.g. `/dev/cu.usbmodem212401` |
| `--compare-ws URL` | string | None | Second device WebSocket URL for comparison |
| `--compare-serial PORT` | string | None | Second device serial port for comparison |
| `--duration` | float | `30.0` | Capture duration in seconds |
| `--serial-fps` | int | `5` | Serial streaming FPS (sent to firmware) |
| `--tap` | `a`/`b`/`c` | `b` | Capture tap point |
| `--output` / `-o` | string | `waterfall.png` | Output file (.png, .gif, or .mp4) |
| `--pixel-height` | int | `3` | Vertical pixels per frame row in waterfall |
| `--label` | string | auto | Label for primary device |
| `--compare-label` | string | auto | Label for comparison device |

### 3.2 Serial Connection Setup

```python
ser = serial.Serial(port, 115200, timeout=0.1, dsrdtr=False, rtscts=False)
time.sleep(3)  # Board reset stabilisation (USB CDC DTR/RTS toggle)
ser.reset_input_buffer()
```

- **Baud rate:** 115200 (matches ESP32 `Serial.begin(115200)`)
- **DTR/RTS:** Both disabled (`dsrdtr=False, rtscts=False`). Opening the USB CDC
  port on ESP32-S3 toggles DTR/RTS which resets the board. The 3-second sleep
  allows the board to complete its boot sequence.
- **Timeout:** 0.1 seconds on reads.

After stabilisation, the capture command is sent:

```python
ser.write(f'capture stream {tap_letter} {fps}\n'.encode())
time.sleep(0.3)  # Allow firmware to process command
```

### 3.3 Frame Sync Algorithm

The parser scans a `bytearray` receive buffer (`recv_buf`) for frame boundaries:

1. **Find magic byte:** `recv_buf.find(bytes([0xFD]))`. If not found, clear the
   entire buffer.
2. **Discard pre-magic bytes:** If magic is not at index 0, slice off preceding
   bytes: `recv_buf = recv_buf[magic_idx:]`.
3. **Determine frame size from version byte:** If `recv_buf[1] >= 2`, frame size
   is `SERIAL_V2_FRAME_SIZE` (1009); otherwise `SERIAL_V1_FRAME_SIZE` (977).
4. **Check completeness:** If the buffer has fewer bytes than the expected frame
   size, break and wait for more data.
5. **Extract and parse:** Slice `frame_data = bytes(recv_buf[:frame_size])`,
   advance the buffer, and pass to `parse_serial_frame()`.
6. **Repeat** until no more complete frames are found in the buffer.

There is **no CRC or checksum**. Frame integrity relies on magic byte matching
and payload length validation. Corrupt frames that pass these checks are flagged
via the `suspect` metadata key.

### 3.4 `parse_serial_frame()`

**Signature:** `parse_serial_frame(data: bytes) -> Optional[tuple]`

**Returns:** `(frame_array, metadata)` or `None`.

**Parsing logic (step by step):**

1. Check `len(data) >= SERIAL_V1_FRAME_SIZE` (977). If not, return `None`.
2. Check `data[0] == 0xFD`. If not, return `None`.
3. Check `data[1] in (1, 2)`. Unrecognised versions are rejected.
4. Extract header fields:
   - `version = data[1]`
   - `tap = data[2]`
   - `effect_id = data[3]`
   - `palette_id = data[4]`
   - `brightness = data[5]`
   - `speed = data[6]`
   - `frame_idx = struct.unpack_from('<I', data, 7)[0]`
   - `timestamp_us = struct.unpack_from('<I', data, 11)[0]`
   - `payload_len = struct.unpack_from('<H', data, 15)[0]`
5. Validate `payload_len == 960`. If not, return `None`.
6. Extract RGB payload as `np.ndarray` shape `(320, 3)` dtype `uint8`.
7. **Suspect frame detection:** If fewer than 10% of byte values in the payload
   are unique, mark `suspect = True` (likely corruption or stuck buffer).
8. Build metadata dict with header fields + `suspect` flag.
9. If `version >= 2` and enough data for trailer (32 bytes):
   - Parse all trailer fields (see byte offset table in Section 2.4)
   - BPM field: raw u16 divided by 100.0; zero means no BPM reported.
   - Confidence field: raw u16 divided by 65535.0.
10. Return `(frame.copy(), metadata)`.

### 3.5 Frame Buffer

`FrameBuffer` is a thread-safe circular buffer backed by NumPy arrays:

- **Constructor:** `FrameBuffer(max_frames=7200)` pre-allocates
  `(7200, 320, 3)` uint8 for frames and `(7200,)` float64 for timestamps.
- **`append(frame, timestamp)`:** Writes under a `threading.Lock`. Advances
  `write_idx` modulo `max_frames`. Caps `count` at `max_frames`.
- **`get_all()`:** Returns `(frames, timestamps)` in chronological order. If the
  buffer has wrapped, uses `np.roll` to reorder.
- **`frame_count`:** Property returning current count under lock.

The buffer is sized at `int(duration * 20) + 100` for generous headroom above
typical capture rates.

### 3.6 `capture_serial()` (in `led_capture.py`)

The main serial capture function for `led_capture.py`:

1. Opens serial port with settings described in 3.2.
2. Sends `capture stream {tap} {fps}\n`.
3. Enters read loop:
   - Reads available bytes with `ser.in_waiting`, appends to `recv_buf`.
   - If no data, sleeps 5ms.
   - Scans for complete frames (see 3.3).
   - Appends parsed frames to `FrameBuffer` and metadata to `metadata_log`.
   - Prints progress every 50 frames.
4. On duration expiry, sends `capture stop\n`, closes port.
5. Reports total frames, actual FPS, and beat count.

### 3.7 Progress Reporting

- Every 50 frames: prints frame count, elapsed time, actual FPS.
- On completion: prints total frames, duration, FPS, and beat count.
- `analyze_beats.py` has additional verbosity: with `--verbose`, prints
  per-frame beat/RMS every 10 frames. With `--quiet`, suppresses most output.

---

## 4. Waterfall Rendering

### 4.1 How LED Frames Become Waterfall Images

`render_waterfall(frames, label, pixel_height)`:

1. Input: `frames` is shape `(N, 320, 3)` uint8.
2. **Width** = 320 pixels (1 pixel per LED, no scaling).
3. **Height** = `N * pixel_height` (default `pixel_height = 3`).
4. Each frame row is stretched vertically by `np.repeat(frames, pixel_height, axis=0)`,
   producing shape `(N * pixel_height, 320, 3)`.
5. The array is converted directly to an RGB PIL Image via
   `Image.fromarray(stretched, mode='RGB')`. Colour mapping is identity --
   the LED RGB values are used as pixel RGB values directly.
6. If a `label` is provided, a 24-pixel header is prepended with the label text
   on a dark grey `(20, 20, 20)` background, rendered in Menlo 11pt (or default
   font fallback).

**Time axis:** Time flows downward. Frame 0 is at the top; the most recent frame
is at the bottom.

### 4.2 Dual-Device Comparison Mode

`render_comparison(frames_a, frames_b, label_a, label_b, pixel_height)`:

1. Pads the shorter buffer with black frames to align frame counts.
2. Renders each as a waterfall (via `render_waterfall`).
3. Combines side-by-side with a 4-pixel gap (`GAP_WIDTH = 4`) on a dark grey
   `(40, 40, 40)` background.
4. Total width = `320 + 4 + 320 = 644` pixels.

---

## 5. GIF Export Pipeline

### 5.1 Strip-View Frame Rendering

`render_strip_frame(frame, led_w=4, led_h=12, gap=2, strip_gap=8, bg_color=(15, 15, 15))`:

Each GIF/MP4 frame is a **strip-view** image (NOT a waterfall row). It renders
the physical LED layout:

1. Splits the 320-LED frame into Strip 0 (LEDs 0--159) and Strip 1 (LEDs 160--319).
2. Image width = `160 * led_w` = **640 pixels**.
3. Image height = `2 * led_h + strip_gap` = `2 * 12 + 8` = **32 pixels**.
4. Strip 0 occupies the top 12-pixel row; Strip 1 occupies the bottom 12-pixel
   row; 8-pixel gap between them.
5. Each LED is a `led_w x led_h` (4x12) coloured rectangle on a dark background.

Output: `(32, 640, 3)` uint8 NumPy array.

### 5.2 Frame Duration Calculation

`_compute_frame_durations(timestamps, fallback_fps=5)`:

1. Computes inter-frame deltas in milliseconds: `np.diff(timestamps) * 1000.0`.
2. Clamps to `[20ms, 2000ms]` to reject serial hiccup outliers. The 20ms floor
   is the GIF minimum (browser-enforced).
3. First frame duration uses the median interval.
4. Returns a list of per-frame durations in milliseconds.

If no timestamps are provided, falls back to uniform `1000 / fallback_fps` ms.

### 5.3 GIF Encoding

`export_gif(frames, path, timestamps, fallback_fps=5, led_w=4, led_h=12)`:

1. Computes durations via `_compute_frame_durations()`.
2. For each of the `N` frames, calls `render_strip_frame()` to produce a
   640x32 strip-view image.
3. Writes all images to GIF via `imageio.v3.imwrite(path, images, duration=durations, loop=0)`.
4. Each GIF frame is one capture frame rendered as the strip-view. This is NOT a
   waterfall -- it is an animation of the LED strip appearance over time.

**Output dimensions:** 640 x 32 pixels.

### 5.4 MP4 Export

`export_mp4(frames, path, timestamps, fallback_fps=5, led_w=4, led_h=12)`:

1. Uses `imageio.v3.imopen` with `pyav` plugin and `libx264` codec.
2. Container rate is fixed at 30 FPS.
3. For each source frame, computes how many times to duplicate it at 30 FPS to
   fill its real duration: `n_repeats = max(1, round(duration_ms / 33.3))`.
4. Writes duplicated strip-view frames to the video stream.

### 5.5 Comparison Mode Export Limitations

- **GIF comparison:** Falls back to a static PNG of the side-by-side waterfall.
  Animated comparison GIFs are not implemented.
- **MP4 comparison:** Falls back to a PNG export of the waterfall. Not implemented.

---

## 6. Analyser: `analyze_beats.py`

### 6.1 CLI Argument Parsing

The analyser supports single-device, dual-device, three-way, and N-way
comparisons with parallel or sequential capture.

**Core arguments:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--serial PORT` | string | required | Primary device serial port |
| `--label` | string | auto | Label for primary device |
| `--compare-serial PORT` | string | None | Second device serial port |
| `--compare-label` | string | None | Label for second device |
| `--serial-c PORT` | string | None | Third device serial port |
| `--label-c` | string | None | Label for third device |
| `--devices` | string | None | N-way spec: `"port1:label1,port2:label2,..."` |
| `--sequential` | flag | False | Single-port sequential capture (reflash between) |
| `--duration` | float | `30.0` | Capture duration per device (seconds) |
| `--fps` | int | `15` | Streaming FPS |
| `--tap` | `a`/`b`/`c` | `b` | Capture tap point |
| `--save FILE` | string | None | Save raw data as `.npz` |
| `--load FILE` | string | None | Load previously saved `.npz` instead of capturing |
| `--dashboard FILE` | string | None | Generate dashboard PNG |
| `--gate` | flag | False | Run regression gate (exit 1 on failure) |
| `--gate-drop-rate` | float | `0.01` | Custom gate: max frame drop rate |
| `--gate-show-time` | float | `10000` | Custom gate: max show time p99 (microseconds) |
| `--align` | flag | False | Temporal alignment via RMS cross-correlation |
| `--verbose` | flag | False | Per-frame debug output |
| `--quiet` | flag | False | Suppress most output |

### 6.2 Data Acquisition

`capture_with_metadata(port, duration, fps, tap, stop_event)`:

Identical frame sync algorithm to `led_capture.py`'s `capture_serial()` but
returns `(np.array(frames), np.array(timestamps), metadata_list)` directly
instead of using a `FrameBuffer`. This function is used by the analyser for all
serial captures.

Includes:
- 5-second timeout warning if no frames received.
- Robust error handling for disconnected devices.
- `capture stop` sent in `finally` block.

### 6.3 Compute Functions

#### `compute_brightness(frames, top_k=32)`

**Purpose:** Perceived brightness of the active region per frame.

**Algorithm:**
1. Compute per-LED luminance using ITU-R BT.601:
   `L = (0.299*R + 0.587*G + 0.114*B) / 255.0` -- shape `(N, 320)`.
2. Use `np.partition` (O(n) average) to find the top-K brightest LEDs per frame.
   K=32 (~10% of 320 LEDs) covers the typical active region of centre-origin
   effects without being diluted by the 90% of dark LEDs.
3. Return the mean of the top-K values per frame -- shape `(N,)` in `[0, 1]`.

#### `compute_spatial_spread(frames)`

**Purpose:** Fraction of strip that is actively lit per frame.

**Algorithm:**
1. Compute per-LED luminance (same BT.601 formula).
2. Find per-frame maximum luminance.
3. Count LEDs with luminance > 5% of that frame's peak.
4. Normalise by total LED count (320).
5. Return shape `(N,)` in `[0, 1]`. A value of 0.1 means ~32 LEDs are lit.

#### `compute_hue_velocity(frames, top_k=32)`

**Purpose:** Angular velocity of the dominant hue across the brightest LEDs.

**Algorithm:**
1. For each frame, find the top-K brightest LEDs by luminance.
2. Convert each LED's RGB to HSV using `colorsys.rgb_to_hsv()`.
3. Compute the circular mean hue using `atan2(sum(sin(h)), sum(cos(h)))`.
4. Compute per-frame angular velocity: `|hue[t] - hue[t-1]|`, wrapped to
   `[-pi, pi]` via `(diff + pi) % (2*pi) - pi`.
5. Return shape `(N,)` in `[0, pi]`. First element is 0.0.

#### `compute_temporal_gradient_energy(frames)`

**Purpose:** Total pixel-level motion between consecutive frames.

**Algorithm:**
1. Cast frames to `int16` to avoid uint8 underflow.
2. Compute `sum(|frame[t] - frame[t-1]|)` across all LEDs and channels.
3. Normalise by LED count (320).
4. Return shape `(N,)` float. First element is 0.0.

#### `compute_active_pixel_delta(frames)`

**Purpose:** Frame-to-frame change in the count of active (lit) LEDs.

**Algorithm:**
1. Compute spatial spread via `compute_spatial_spread()`.
2. Return `|spread[t] - spread[t-1]|` per frame.
3. Return shape `(N,)` float. First element is 0.0.

### 6.4 `analyze_correlation()`

**Signature:** `analyze_correlation(frames, timestamps, metadata, label) -> dict`

**Minimum requirement:** At least 10 frames. Returns an error dict otherwise.

**Processing steps:**

1. **v2 detection:** Checks first 5 metadata entries for `rms`/`bands`/`beat`
   keys. If absent, flags `v1_only = True` and zeroes all audio metrics.

2. **Time-series extraction:**
   - `brightness` = `compute_brightness(frames)` -- top-K luminance
   - `spatial_spread` = `compute_spatial_spread(frames)`
   - `beats` = boolean array from `metadata['beat']`
   - `rms` = float array from `metadata['rms']`
   - `bands` = `(N, 8)` float array from `metadata['bands']`
   - `show_times`, `heap`, `onset`, `flux` = from metadata
   - `fw_bpm`, `fw_conf` = firmware-reported BPM and confidence
   - `hue_velocity` = `compute_hue_velocity(frames)`
   - `tge` = `compute_temporal_gradient_energy(frames)`
   - `apd` = `compute_active_pixel_delta(frames)`

3. **BPM source selection:** Prefers firmware-reported BPM (median of non-zero
   values) because at 15 FPS capture / 120 FPS render, ~87% of single-frame beat
   pulses are missed, making tick-counted BPM wildly inaccurate.

4. **Tempo stability:** Computed from inter-beat intervals:
   `1.0 - std(IBI) / mean(IBI)`, requires at least 3 beats.

5. **Correlation computations** (all use `_safe_corrcoef` which returns 0.0 for
   constant/degenerate input):
   - `rms_corr` = Pearson(RMS, brightness)
   - `bass_corr` = Pearson(bands[0], brightness)
   - `best_band_corr` = max |Pearson(bands[i], brightness)| across all 8 bands
   - `hue_vel_rms_corr` = Pearson(hue_velocity, RMS)
   - `hue_vel_bass_corr` = Pearson(hue_velocity, bass)
   - `tge_rms_corr` = Pearson(TGE, RMS)
   - `apd_rms_corr` = Pearson(APD, RMS)

6. **Composite audio-visual score:**
   ```
   score = max(0, rms_corr)      * 0.3
         + max(0, hue_vel_rms)   * 0.2
         + max(0, tge_rms)       * 0.3
         + max(0, apd_rms)       * 0.1
         + max(0, bass_corr)     * 0.1
   ```

7. **Beat trigger analysis** (requires >= 3 beats):
   - `dB = diff(brightness)` -- frame-to-frame brightness change
   - `beat_trigger_ratio` = fraction of beat frames where `dB > 0.001`
   - `nonbeat_spike_rate` = fraction of non-beat frames where `dB > 0.001`
   - `responsiveness` = `beat_trigger_ratio - nonbeat_spike_rate` (clamped >= 0)
   - `contrast_ratio` = mean brightness on-beat / mean brightness off-beat
   - `mean_beat_response` = average of (peak brightness in 3-frame window after
     beat minus baseline in 2-frame window before beat)
   - Spatial bloom detection: `mean_spread_beat` vs `mean_spread_nobeat`

8. **Beat diagnostics:** Warns if >90% frames have beat=true (`saturated`) or
   <2% in >5s capture (`absent`).

9. **Frame health:**
   - `frame_drops`: gaps > 2x the median inter-frame index gap
   - `total_missing`: estimated missed streaming slots
   - `subsampling_ratio`: median gap between consecutive `frame_idx` values
   - `show_time_median_us`, `show_time_p99_us`: from trailer data
   - `heap_min`, `heap_max`, `heap_trend` (linear fit slope, bytes per frame)
   - `show_skips_total`: max - min of show_skips counter

### 6.5 Result Dict Structure

The `analyze_correlation()` return dict contains these keys:

| Key | Type | Description |
|-----|------|-------------|
| `label` | str | Device label |
| `n_frames` | int | Total captured frames |
| `duration` | float | Capture duration (seconds) |
| `actual_fps` | float | Measured FPS |
| `v1_only` | bool | True if no v2 trailer data |
| `n_beats` | int | Beat tick count |
| `n_onsets` | int | Onset (snare/hihat) count |
| `beat_hz` | float | Beats per second |
| `bpm` | float | Beats per minute |
| `bpm_source` | str | `'firmware'` or `'tick-count'` |
| `fw_beat_confidence` | float | Mean firmware confidence |
| `tempo_stability` | float | 0--1, higher = more stable tempo |
| `beat_warning` | str/None | `'saturated'`, `'absent'`, or `None` |
| `rms_brightness_corr` | float | Pearson(RMS, brightness) |
| `bass_brightness_corr` | float | Pearson(bass, brightness) |
| `best_band_corr` | float | Best single-band correlation |
| `best_band_idx` | int | Index of best-correlating band |
| `beat_trigger_ratio` | float | Fraction of beats causing spike |
| `nonbeat_spike_rate` | float | Background spike rate |
| `responsiveness` | float | Net beat responsiveness score |
| `mean_bright_beat` | float | Mean brightness on beat frames |
| `mean_bright_nobeat` | float | Mean brightness off beat frames |
| `contrast_ratio` | float | On-beat / off-beat brightness ratio |
| `mean_beat_response` | float | Average beat response magnitude |
| `mean_brightness` | float | Overall mean brightness |
| `mean_spatial_spread` | float | Mean fraction of strip active |
| `mean_spread_beat` | float | Spatial spread on beats |
| `mean_spread_nobeat` | float | Spatial spread off beats |
| `hue_vel_rms_corr` | float | Pearson(hue velocity, RMS) |
| `hue_vel_bass_corr` | float | Pearson(hue velocity, bass) |
| `tge_rms_corr` | float | Pearson(TGE, RMS) |
| `apd_rms_corr` | float | Pearson(APD, RMS) |
| `composite_audio_visual_score` | float | Weighted composite score |
| `frame_drops` | int | Detected frame drops |
| `total_missing_frames` | int | Estimated missing streaming slots |
| `drop_rate` | float | Frame drops / total frames |
| `show_time_median_us` | float | Median show time (microseconds) |
| `show_time_p99_us` | float | 99th percentile show time |
| `heap_min` | int | Minimum heap free bytes |
| `heap_max` | int | Maximum heap free bytes |
| `heap_trend` | float | Heap linear trend (bytes/frame) |
| `show_skips_total` | int | Total show skip events |
| `subsampling_ratio` | float | Renderer FPS / capture FPS ratio |
| `_brightness` | ndarray | Time-series (N,) -- private, for dashboard |
| `_spatial_spread` | ndarray | Time-series (N,) -- private |
| `_rms` | ndarray | Time-series (N,) -- private |
| `_beats` | ndarray(bool) | Time-series (N,) -- private |
| `_onset` | ndarray(bool) | Time-series (N,) -- private |
| `_bands` | ndarray | Time-series (N, 8) -- private |
| `_flux` | ndarray | Time-series (N,) -- private |
| `_show_times` | ndarray | Time-series (N,) -- private |
| `_heap` | ndarray | Time-series (N,) -- private |
| `_timestamps` | ndarray | Time-series (N,) -- private |
| `_hue_velocity` | ndarray | Time-series (N,) -- private |
| `_tge` | ndarray | Time-series (N,) -- private |

### 6.6 `format_report()`

Generates a human-readable text report from the metrics dict. Sections:

1. **Capture summary:** frame count, FPS, duration.
2. **BPM:** firmware-reported BPM with confidence, or tick-count BPM with
   subsampling warning. Includes expected beat capture percentage.
3. **Beat warnings:** saturated or absent.
4. **Audio-reactive quality scorecard:**
   - Responsiveness with rating (EXCELLENT / good / weak / none)
   - RMS-brightness correlation with rating (STRONG / moderate / weak / none)
   - Bass-brightness correlation
   - Best band correlation with band name (Sub / Bass / Low-Mid / Mid /
     Upper-Mid / Presence / Brilliance / Air)
   - Beat trigger ratio, non-beat spike rate, mean beat response
   - Brightness on/off beat, contrast ratio
   - Spatial spread (overall and beat vs off-beat)
5. **Multi-dimensional coupling:** hue velocity, TGE, APD correlations and
   composite A/V score.
6. **Frame health:** subsampling ratio, drops, show time, heap, show skips.

All numeric values are fetched via `.get()` with safe defaults. NaN/inf values
print as `N/A` via `_safe_fmt()`.

### 6.7 `render_dashboard()`

**Signature:** `render_dashboard(metrics, width=900) -> Image.Image`

**Returns:** A PIL Image or `None` if fewer than 2 frames.

**Layout:**

```
+-------------------------------------------+  width = 900px
| Header: label, frame count, FPS, BPM      |  50px
+-------------------------------------------+
| Score summary: 4 columns                   |  80px
|  Responsiveness | RMS Corr | Beat Trig | C |
+-------------------------------------------+
| Sparkline: Brightness     [beat markers]   |  44px + 6px gap
| Sparkline: RMS Audio      [beat markers]   |  44px + 6px gap
| Heatmap:   Bands [8]      [beat markers]   |  44px + 6px gap
| Sparkline: Show Time                       |  44px + 6px gap
| Sparkline: Heap (KB)                       |  44px + 6px gap
| Sparkline: Hue Velocity   [beat markers]   |  44px + 6px gap
| Sparkline: TGE            [beat markers]   |  44px + 6px gap
+-------------------------------------------+
Total height = 50 + 80 + 7 * (44 + 6) + 16 = ~496px
```

**Score summary block:** 4 columns of (label, value, rating). Values are
colour-coded: green (>0.3), yellow (>0.1), red (<=0.1).

**`_draw_sparkline(draw, x0, y0, w, h, data, colour, vmin, vmax, beat_markers)`:**

1. Fills background rectangle with `(30, 30, 30)`.
2. Draws beat markers as vertical red lines `(80, 30, 30)` behind the sparkline.
3. Maps each data point to a pixel position: `px = x0 + i * w / n`,
   `py = y0 + h - ((val - vmin) / (vmax - vmin)) * (h - 4) - 2`.
4. Draws connected line segments in the specified colour.
5. Adds min/max labels in grey 9pt font to the right of the sparkline area.

**`_draw_heatmap(draw, x0, y0, w, h, data_2d, beat_markers)`:**

1. Fills background rectangle.
2. For each time step and each of the 8 bands, draws a coloured rectangle using
   a blue-to-orange colourmap: `R = min(255, v*400)`, `G = min(255, v*200)`,
   `B = max(0, 80 - v*80)`.
3. Low frequencies at bottom, high at top.
4. Beat markers as white vertical lines.

**Sparkline rows and data sources:**

| Row | Data | Colour | vmin | vmax |
|-----|------|--------|------|------|
| Brightness | `_brightness` | `(100, 200, 255)` cyan | 0 | 1 |
| RMS Audio | `_rms` | `(100, 255, 100)` green | 0 | 1 |
| Bands [8] | `_bands` | blue-to-orange heatmap | 0 | 1 |
| Show Time | `_show_times / 1000` (ms) | `(255, 200, 100)` amber | 0 | max(10, peak) |
| Heap (KB) | `_heap / 1024` | `(200, 130, 255)` purple | 95% of min | 102% of max |
| Hue Velocity | `_hue_velocity` | `(255, 150, 200)` pink | 0 | max(0.1, peak) |
| TGE | `_tge` | `(255, 130, 80)` orange | 0 | max(1.0, peak) |

**Comparison dashboard:** `render_comparison_dashboard()` stacks two dashboards
vertically with a 4-pixel grey divider. `render_multi_dashboard()` extends this
to N devices.

### 6.8 Regression Gate

`run_regression_gate(metrics, thresholds)` evaluates 5 health checks:

| Check | Metric key | Default threshold | Direction | Display |
|-------|-----------|-------------------|-----------|---------|
| Frame drop rate | `drop_rate` | < 0.01 (1%) | `lt` | decimal |
| Show time p99 | `show_time_p99_us` | < 10000 (10ms) | `lt` | milliseconds |
| Heap minimum | `heap_min` | > 100000 (100 KB) | `gt` | human bytes |
| Heap trend | `heap_trend` | > -100 B/frame | `gt` | B/frame |
| Show skips | `show_skips_total` | < 10 | `lt` | integer |

Each check returns a `GateResult` with status `PASS`, `FAIL`, or `SKIP`
(skipped when the firmware doesn't report that metric).

With `--gate`, the script exits with code 1 if any check fails. Custom
thresholds can be set via `--gate-drop-rate` and `--gate-show-time`.

---

## 7. Data Types and Constants

### 7.1 Python Constants (`led_capture.py`)

| Constant | Value | Description |
|----------|-------|-------------|
| `NUM_LEDS` | `320` | Total LEDs across both strips |
| `LEDS_PER_STRIP` | `160` | LEDs per physical strip |
| `NUM_STRIPS` | `2` | Number of physical strips |
| `RGB_BYTES` | `960` | `320 * 3` bytes for full RGB payload |
| `WS_MAGIC` | `0xFE` | WebSocket frame magic byte |
| `WS_FRAME_SIZE` | `966` | WebSocket frame total size |
| `SERIAL_MAGIC` | `0xFD` | Serial frame magic byte |
| `SERIAL_HEADER_SIZE` | `17` | Serial frame header size |
| `SERIAL_V1_FRAME_SIZE` | `977` | `17 + 960` |
| `SERIAL_V2_TRAILER_SIZE` | `32` | v2 metrics trailer size |
| `SERIAL_V2_FRAME_SIZE` | `1009` | `17 + 960 + 32` |
| `WATERFALL_PIXEL_HEIGHT` | `3` | Default vertical pixels per frame row |
| `GAP_WIDTH` | `4` | Pixel gap in side-by-side comparison |
| `LABEL_HEIGHT` | `24` | Pixel height for device label |

### 7.2 Firmware Constants (`main.cpp`)

| Variable | Value | Description |
|----------|-------|-------------|
| `s_captureStreamVersion` | `2` | Default frame format version |
| `s_captureStreamIntervalMs` | `66` | Default ~15 FPS interval |
| `s_captureStreamTapMask` | `0x02` | Default: tap B |
| `LOOP_SCRATCH_LED_COUNT` | `320` | Size of capture scratch buffer |

### 7.3 Analyser Constants (`analyze_beats.py`)

| Constant | Value | Description |
|----------|-------|-------------|
| `_SESSION_FORMAT_VERSION` | `2` | Version marker for `.npz` session files |
| `DEFAULT_GATE_THRESHOLDS['drop_rate']` | `(0.01, 'lt')` | < 1% frame drops |
| `DEFAULT_GATE_THRESHOLDS['show_time']` | `(10000.0, 'lt')` | < 10ms p99 |
| `DEFAULT_GATE_THRESHOLDS['heap_min']` | `(100000, 'gt')` | > 100 KB free |
| `DEFAULT_GATE_THRESHOLDS['heap_trend']` | `(-100.0, 'gt')` | No fast leak |
| `DEFAULT_GATE_THRESHOLDS['show_skips']` | `(10, 'lt')` | < 10 skip events |

---

## 8. Quick Reference

### 8.1 Common CLI Invocations

```bash
# Basic waterfall capture (30 seconds, PNG)
python tools/led_capture/led_capture.py \
    --serial /dev/cu.usbmodem21401 \
    --duration 30 \
    --output waterfall.png

# GIF capture (10 seconds, strip-view animation)
python tools/led_capture/led_capture.py \
    --serial /dev/cu.usbmodem21401 \
    --duration 10 \
    --serial-fps 15 \
    --output capture.gif

# MP4 capture
python tools/led_capture/led_capture.py \
    --serial /dev/cu.usbmodem21401 \
    --duration 10 \
    --output capture.mp4

# Side-by-side waterfall comparison
python tools/led_capture/led_capture.py \
    --serial /dev/cu.usbmodem212401 \
    --compare-serial /dev/cu.usbmodem21401 \
    --duration 30 \
    --output comparison.png

# Single-device beat analysis with dashboard
python tools/led_capture/analyze_beats.py \
    --serial /dev/cu.usbmodem212401 \
    --duration 30 \
    --dashboard /tmp/dash.png

# Dual-device comparison with regression gate
python tools/led_capture/analyze_beats.py \
    --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
    --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
    --duration 30 --gate

# Three-way comparison
python tools/led_capture/analyze_beats.py \
    --devices "/dev/cu.usbmodem212401:Main HEAD,/dev/cu.usbmodem21401:Milestone,/dev/cu.usbmodem101:Baseline" \
    --duration 30

# Save session for later analysis
python tools/led_capture/analyze_beats.py \
    --serial /dev/cu.usbmodem212401 --duration 30 --save session.npz

# Analyse saved session
python tools/led_capture/analyze_beats.py --load session.npz --dashboard report.png
```

### 8.2 Serial Commands (Firmware)

| Command | Description |
|---------|-------------|
| `capture stream b 15` | Start streaming tap B at 15 FPS (v2 format) |
| `capture stream a 30` | Start streaming tap A at 30 FPS |
| `capture stream c 5` | Start streaming tap C at 5 FPS |
| `capture stop` | Stop streaming |
| `capture fps 30` | Change streaming rate to 30 FPS |
| `capture tap a` | Switch to tap A without restarting |
| `capture format v1` | Switch to v1 format (977 bytes, no metrics) |
| `capture format v2` | Switch to v2 format (1009 bytes, with metrics) |
| `capture status` | Print capture state, streaming info, last frame |
| `capture on b` | Enable capture mode (non-streaming) for tap B |
| `capture off` | Disable capture mode entirely |
| `capture dump b` | Send a single v1 frame for tap B |
| `effect 0x1403` | Set effect by hex ID |

### 8.3 Python Dependencies

```bash
pip install numpy Pillow pyserial websockets imageio[ffmpeg]
```

Or use the project venv at `tools/led_capture/.venv/`.

---

*Last updated: 2026-03-08. Source of truth: the three files listed at the top of
this document. If this reference and the code disagree, the code wins.*
