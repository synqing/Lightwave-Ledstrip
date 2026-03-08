#!/usr/bin/env python3
"""
Beat-Brightness Correlation Analyser

Captures LED frames with v2 metrics from K1 devices and computes
objective audio-reactive quality scores.

Usage:
    # Single device analysis (30-second capture)
    python analyze_beats.py --serial /dev/cu.usbmodem212401 --duration 30

    # Side-by-side comparison (two devices)
    python analyze_beats.py \
        --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
        --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
        --duration 30

    # Save raw data for later analysis
    python analyze_beats.py --serial /dev/cu.usbmodem212401 --duration 30 --save session.npz

    # Analyse previously saved data
    python analyze_beats.py --load session.npz

    # Generate dashboard PNG alongside text report
    python analyze_beats.py --serial /dev/cu.usbmodem212401 --duration 30 --dashboard report.png

    # Run regression gate (exit 1 on failure, for CI)
    python analyze_beats.py --serial /dev/cu.usbmodem212401 --duration 30 --gate

    # Gate with custom thresholds
    python analyze_beats.py --load session.npz --gate --gate-drop-rate 0.005 --gate-show-time 8000

    # Comparison with temporal alignment (cross-correlate RMS to sync)
    python analyze_beats.py \
        --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
        --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
        --duration 30 --align

    # Comparison with regression gate on both devices
    python analyze_beats.py \
        --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
        --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
        --duration 30 --gate

    # Three-way comparison (three physical devices)
    python analyze_beats.py \
        --devices "/dev/cu.usbmodem212401:Main HEAD,/dev/cu.usbmodem21401:Milestone,/dev/cu.usbmodem101:Baseline" \
        --duration 30

    # Three-way comparison (individual args)
    python analyze_beats.py \
        --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
        --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
        --serial-c /dev/cu.usbmodem101 --label-c "Baseline" \
        --duration 30

    # Sequential capture (one device, multiple firmware states)
    python analyze_beats.py \
        --devices "/dev/cu.usbmodem212401:Main HEAD,/dev/cu.usbmodem212401:Milestone,/dev/cu.usbmodem212401:Baseline" \
        --sequential --duration 30

Output:
    A text report with:
    - Beat responsiveness score (0-1): how reliably beats cause brightness spikes
    - RMS-brightness correlation: overall audio-reactive coupling strength
    - Beat timing and tempo estimation
    - Frame health (drops, show time, heap stability)
    - Optional regression gate pass/fail table (with --gate)
"""

import argparse
import math
import sys
import time
import threading
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# Import shared constants and parsers from led_capture
from led_capture import (
    SERIAL_MAGIC, SERIAL_V1_FRAME_SIZE, SERIAL_V2_FRAME_SIZE,
    NUM_LEDS, parse_serial_frame,
)

# Version marker for saved sessions — bump when the .npz schema changes.
_SESSION_FORMAT_VERSION = 2

# Module-level verbosity flags, set from CLI args in main().
_verbose = False
_quiet = False


def _safe_fmt(value, fmt: str, fallback: str = 'N/A') -> str:
    """Format a numeric value, returning *fallback* for NaN/inf/None."""
    if value is None:
        return fallback
    try:
        if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
            return fallback
        return f'{value:{fmt}}'
    except (ValueError, TypeError):
        return fallback


def _safe_corrcoef(a: np.ndarray, b: np.ndarray) -> float:
    """Pearson correlation that returns 0.0 on constant/degenerate input."""
    if a.std() < 1e-6 or b.std() < 1e-6:
        return 0.0
    try:
        r = float(np.corrcoef(a, b)[0, 1])
        if math.isnan(r) or math.isinf(r):
            return 0.0
        return r
    except (FloatingPointError, ValueError):
        return 0.0


def _has_v2_metadata(metadata: list) -> bool:
    """Return True if the metadata list contains v2 trailer fields."""
    if not metadata:
        return False
    # Check the first few entries for a v2-specific key.
    for m in metadata[:5]:
        if 'rms' in m or 'bands' in m or 'beat' in m:
            return True
    return False


# ---------------------------------------------------------------------------
# Capture with Metadata
# ---------------------------------------------------------------------------

def capture_with_metadata(port: str, duration: float, fps: int = 15,
                          tap: str = 'b', stop_event: threading.Event = None):
    """Capture frames + full v2 metadata via serial streaming.

    Returns (frames, timestamps, metadata_list) or None.
    frames: (N, 320, 3) uint8, timestamps: (N,) float64, metadata: list[dict]
    """
    try:
        import serial
    except ImportError:
        print("ERROR: 'pyserial' required. pip install pyserial", file=sys.stderr)
        return None

    if stop_event is None:
        stop_event = threading.Event()

    tap_letter = tap.lower()
    if not _quiet:
        print(f"[Capture] Opening {port} (tap {tap_letter.upper()}, {fps} FPS)...")

    try:
        ser = serial.Serial(port, 115200, timeout=0.1, dsrdtr=False, rtscts=False)
    except Exception as e:
        print(f"[Capture] Failed to open {port}: {e}", file=sys.stderr)
        return None

    time.sleep(3)  # Board reset stabilisation (USB CDC DTR/RTS toggle)
    ser.reset_input_buffer()

    ser.write(f'capture stream {tap_letter} {fps}\n'.encode())
    time.sleep(0.3)

    frames = []
    timestamps = []
    metadata_list = []
    recv_buf = bytearray()
    start_time = time.time()
    first_frame_warned = False

    try:
        while not stop_event.is_set():
            elapsed = time.time() - start_time
            if elapsed >= duration:
                break

            # Timeout detection: warn if no frames after 5 seconds.
            if not first_frame_warned and len(frames) == 0 and elapsed > 5.0:
                first_frame_warned = True
                print(f"[Capture] WARNING: No frames received after 5s. "
                      f"Check firmware is running and port {port} is correct.",
                      file=sys.stderr)

            try:
                waiting = ser.in_waiting
            except Exception as e:
                # Device disconnected mid-capture.
                print(f"[Capture] Serial port error: {e}", file=sys.stderr)
                break

            if waiting > 0:
                try:
                    recv_buf.extend(ser.read(waiting))
                except Exception as e:
                    print(f"[Capture] Serial read error: {e}", file=sys.stderr)
                    break
            else:
                time.sleep(0.005)
                continue

            # Scan for complete frames in buffer
            while True:
                magic_idx = recv_buf.find(bytes([SERIAL_MAGIC]))
                if magic_idx < 0:
                    recv_buf.clear()
                    break

                if magic_idx > 0:
                    recv_buf = recv_buf[magic_idx:]

                if len(recv_buf) < 2:
                    break
                version = recv_buf[1]
                frame_size = SERIAL_V2_FRAME_SIZE if version >= 2 else SERIAL_V1_FRAME_SIZE

                if len(recv_buf) < frame_size:
                    break

                frame_data = bytes(recv_buf[:frame_size])
                recv_buf = recv_buf[frame_size:]

                result = parse_serial_frame(frame_data)
                if result is not None:
                    frame, meta = result
                    frames.append(frame)
                    timestamps.append(time.time())
                    metadata_list.append(meta)

                    count = len(frames)
                    if _verbose and count % 10 == 0:
                        m = meta
                        print(f"  [v] frame={m.get('frame_idx', '?')} "
                              f"beat={m.get('beat', '?')} "
                              f"rms={m.get('rms', '?'):.4f} "
                              f"bright=--" if count < 2 else '', end='')
                        # Brightness requires full array; skip in per-frame log.
                        print()
                    if not _quiet and count % 50 == 0:
                        fps_actual = count / (time.time() - start_time)
                        print(f"[Capture] {count} frames ({elapsed:.1f}s, {fps_actual:.1f} FPS)")
    except Exception as e:
        # Catch-all for unexpected serial errors (device yanked, etc.).
        print(f"[Capture] Unexpected error: {e}", file=sys.stderr)
    finally:
        try:
            ser.write(b'capture stop\n')
            time.sleep(0.2)
            ser.close()
        except Exception:
            pass  # Port may already be dead.

    total_time = time.time() - start_time
    actual_fps = len(frames) / max(0.1, total_time)
    beats = sum(1 for m in metadata_list if m.get('beat'))
    if not _quiet:
        print(f"[Capture] Done: {len(frames)} frames in {total_time:.1f}s "
              f"({actual_fps:.1f} FPS, {beats} beats)")

    if not frames:
        return None

    return np.array(frames), np.array(timestamps), metadata_list


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

def compute_brightness(frames: np.ndarray, top_k: int = 32) -> np.ndarray:
    """Perceived brightness per frame using the top-K brightest LEDs.

    Centre-origin effects only light ~20-40 LEDs out of 320. Taking the
    mean or even p90 across ALL LEDs dilutes the signal because the lit
    region is typically smaller than 10% of the strip. Instead, we sort
    per-LED luminance and average the top K values — directly measuring
    the brightness of the active region regardless of how many LEDs are
    dark.

    K=32 (~10% of 320) covers the typical active region of centre-origin
    effects without being so small that single-pixel noise dominates.

    Returns (N,) float in [0, 1].
    """
    # Per-LED luminance: 0.299R + 0.587G + 0.114B (ITU-R BT.601)
    luminance = (frames[:, :, 0] * 0.299
                 + frames[:, :, 1] * 0.587
                 + frames[:, :, 2] * 0.114) / 255.0  # (N, 320)
    # np.partition is O(n) vs O(n log n) for full sort — only need top K
    k = min(top_k, luminance.shape[1])
    partitioned = np.partition(luminance, -k, axis=1)[:, -k:]
    return partitioned.mean(axis=1)


def compute_spatial_spread(frames: np.ndarray) -> np.ndarray:
    """Fraction of the strip that is actively lit per frame.

    For each frame, counts LEDs with luminance > 5% of that frame's
    maximum luminance, then normalises by total LED count. A value of
    0.1 means ~32 LEDs are lit; 1.0 means the entire strip is active.

    Useful for detecting effects that "bloom" outward on beats — the
    spread value jumps when the lit region expands from the centre.

    Returns (N,) float in [0, 1].
    """
    luminance = (frames[:, :, 0] * 0.299
                 + frames[:, :, 1] * 0.587
                 + frames[:, :, 2] * 0.114) / 255.0  # (N, 320)
    # Per-frame maximum luminance (avoid division by zero)
    frame_max = luminance.max(axis=1, keepdims=True)
    frame_max = np.maximum(frame_max, 1e-6)
    # Count LEDs above 5% of that frame's peak
    active = (luminance > frame_max * 0.05).sum(axis=1)
    return active.astype(np.float64) / luminance.shape[1]


def compute_hue_velocity(frames: np.ndarray, top_k: int = 32) -> np.ndarray:
    """Angular velocity of the circular mean hue across the top-K brightest LEDs.

    For each frame, selects the top-K brightest LEDs (by luminance), converts
    their RGB to HSV, computes the circular mean hue using atan2(sin, cos),
    then computes the per-frame angular velocity (wrapped difference).

    Returns (N,) float in [0, pi].  The first element is 0.0 (no prior frame).
    """
    import colorsys

    n = frames.shape[0]
    n_leds = frames.shape[1]
    k = min(top_k, n_leds)

    # Per-LED luminance for top-K selection
    luminance = (frames[:, :, 0] * 0.299
                 + frames[:, :, 1] * 0.587
                 + frames[:, :, 2] * 0.114)  # (N, n_leds) uint-weighted

    # Compute circular mean hue per frame
    mean_hues = np.zeros(n, dtype=np.float64)
    for t in range(n):
        # Indices of top-K brightest LEDs in this frame
        top_indices = np.argpartition(luminance[t], -k)[-k:]
        sin_sum = 0.0
        cos_sum = 0.0
        for idx in top_indices:
            r_f = frames[t, idx, 0] / 255.0
            g_f = frames[t, idx, 1] / 255.0
            b_f = frames[t, idx, 2] / 255.0
            h, _s, _v = colorsys.rgb_to_hsv(r_f, g_f, b_f)
            angle = h * 2.0 * np.pi  # hue [0,1] -> [0, 2pi]
            sin_sum += np.sin(angle)
            cos_sum += np.cos(angle)
        mean_hues[t] = np.arctan2(sin_sum / k, cos_sum / k)  # in [-pi, pi]

    # Angular velocity: |angle_diff(hue[t], hue[t-1])|
    velocity = np.zeros(n, dtype=np.float64)
    for t in range(1, n):
        diff = mean_hues[t] - mean_hues[t - 1]
        # Wrap to [-pi, pi]
        diff = (diff + np.pi) % (2.0 * np.pi) - np.pi
        velocity[t] = abs(diff)

    return velocity


def compute_temporal_gradient_energy(frames: np.ndarray) -> np.ndarray:
    """Total pixel-level motion between consecutive frames.

    Computes the mean absolute difference across all LEDs and colour channels
    between consecutive frames, normalised by LED count.

    Returns (N,) float.  The first element is 0.0 (no prior frame).
    """
    n = frames.shape[0]
    n_leds = frames.shape[1]
    tge = np.zeros(n, dtype=np.float64)
    # Cast to int16 to avoid uint8 underflow in subtraction
    frames_i = frames.astype(np.int16)
    for t in range(1, n):
        tge[t] = np.sum(np.abs(frames_i[t] - frames_i[t - 1])) / n_leds
    return tge


def compute_active_pixel_delta(frames: np.ndarray) -> np.ndarray:
    """Frame-to-frame change in the number of active (lit) LEDs.

    Uses the same luminance threshold as ``compute_spatial_spread()`` to
    count active pixels, then takes the absolute difference between frames.

    Returns (N,) float.  The first element is 0.0 (no prior frame).
    """
    spread = compute_spatial_spread(frames)
    n = len(spread)
    apd = np.zeros(n, dtype=np.float64)
    for t in range(1, n):
        apd[t] = abs(spread[t] - spread[t - 1])
    return apd


def analyze_correlation(frames: np.ndarray, timestamps: np.ndarray,
                        metadata: list, label: str = "Device") -> dict:
    """Compute beat-brightness correlation metrics.

    Returns a dict with scalar metrics and private time-series arrays
    (prefixed with '_') for dashboard rendering.

    If metadata contains only v1 fields (no v2 trailer), a partial
    metrics dict is returned with ``v1_only`` set to True and all
    audio-reactive scores zeroed.  Frame-health and brightness metrics
    are still computed from the RGB payload.
    """
    n = len(metadata)
    if n < 10:
        if not _quiet:
            print(f"[{label}] Too few frames ({n}) for meaningful analysis.")
        return {'label': label, 'n_frames': n, 'error': 'insufficient_frames'}

    # --- Detect v1-only metadata (no audio fields) ---
    v2 = _has_v2_metadata(metadata)
    if not v2 and not _quiet:
        print(f"[{label}] WARNING: v1 frames detected -- audio metrics "
              f"will be zeroed. Use firmware with v2 capture for full analysis.")

    # --- Extract time series ---
    brightness = compute_brightness(frames)
    spatial_spread = compute_spatial_spread(frames)
    beats = np.array([m.get('beat', False) for m in metadata], dtype=bool)
    rms = np.array([m.get('rms', 0.0) for m in metadata], dtype=np.float64)
    bands = np.array([m.get('bands', [0.0] * 8) for m in metadata], dtype=np.float64)
    frame_indices = np.array([m.get('frame_idx', i) for i, m in enumerate(metadata)])
    show_times = np.array([m.get('show_time_us', 0) for m in metadata], dtype=np.float64)
    heap = np.array([m.get('heap_free', 0) for m in metadata], dtype=np.float64)
    onset = np.array([m.get('onset', False) for m in metadata], dtype=bool)
    flux = np.array([m.get('flux', 0.0) for m in metadata], dtype=np.float64)
    fw_bpm = np.array([m.get('bpm', 0.0) for m in metadata], dtype=np.float64)
    fw_conf = np.array([m.get('beat_confidence', 0.0) for m in metadata], dtype=np.float64)

    # Multi-dimensional coupling time-series
    hue_velocity = compute_hue_velocity(frames)
    tge = compute_temporal_gradient_energy(frames)
    apd = compute_active_pixel_delta(frames)

    duration = timestamps[-1] - timestamps[0]
    actual_fps = n / max(0.1, duration)

    # --- Beat statistics ---
    n_beats = int(beats.sum())
    n_onsets = int(onset.sum())
    beat_hz = n_beats / max(0.1, duration)
    # Prefer firmware-reported BPM (always accurate) over counting subsampled
    # beat ticks. At 15 FPS capture / 120 FPS render, ~87% of single-frame
    # beat pulses are missed, making tick-counted BPM wildly inaccurate.
    fw_bpm_valid = fw_bpm[fw_bpm > 0.0]
    if len(fw_bpm_valid) > 0:
        bpm = float(np.median(fw_bpm_valid))
        bpm_source = 'firmware'
    else:
        bpm = beat_hz * 60
        bpm_source = 'tick-count'
    fw_conf_mean = float(fw_conf.mean()) if len(fw_conf) > 0 else 0.0

    # Inter-beat intervals for tempo stability
    beat_idxs = np.where(beats)[0]
    if len(beat_idxs) >= 3:
        ibi_frames = np.diff(beat_idxs)
        ibi_seconds = ibi_frames / actual_fps
        tempo_stability = 1.0 - min(1.0, float(np.std(ibi_seconds) / max(0.01, np.mean(ibi_seconds))))
    else:
        tempo_stability = 0.0

    # --- RMS-Brightness Pearson correlation (NaN-safe) ---
    rms_corr = _safe_corrcoef(rms, brightness)

    # --- Bass-Brightness correlation ---
    # Many effects respond to bands[0] (sub-bass) directly, not the beat flag.
    bass = bands[:, 0] if bands.shape[1] > 0 else np.zeros(n)
    bass_corr = _safe_corrcoef(bass, brightness)

    # Best audio correlation: whichever band correlates most with brightness
    best_band_corr = 0.0
    best_band_idx = 0
    if brightness.std() > 1e-6:
        for bi in range(min(8, bands.shape[1])):
            band_data = bands[:, bi]
            c = _safe_corrcoef(band_data, brightness)
            if abs(c) > abs(best_band_corr):
                best_band_corr = c
                best_band_idx = bi

    # --- Multi-dimensional coupling correlations ---
    hue_vel_rms_corr = _safe_corrcoef(hue_velocity, rms)
    hue_vel_bass_corr = _safe_corrcoef(hue_velocity, bass)
    tge_rms_corr = _safe_corrcoef(tge, rms)
    apd_rms_corr = _safe_corrcoef(apd, rms)

    # Composite audio-visual score: weighted average of key correlations
    composite_audio_visual_score = (
        max(0, rms_corr) * 0.3
        + max(0, hue_vel_rms_corr) * 0.2
        + max(0, tge_rms_corr) * 0.3
        + max(0, apd_rms_corr) * 0.1
        + max(0, bass_corr) * 0.1
    )

    # --- Beat trigger analysis ---
    if n_beats >= 3 and n > 5:
        # Brightness change per frame
        dB = np.diff(brightness)  # (N-1,)
        beat_mask = beats[1:]     # align with diffs

        # Fraction of beats that cause a brightness increase (> 0.1% of range)
        threshold = 0.001
        beat_increases = dB[beat_mask]
        nonbeat_increases = dB[~beat_mask]

        beat_trigger_ratio = float(
            (beat_increases > threshold).mean()) if len(beat_increases) > 0 else 0.0
        nonbeat_spike_rate = float(
            (nonbeat_increases > threshold).mean()) if len(nonbeat_increases) > 0 else 0.0

        # Responsiveness: how much MORE likely is a spike on a beat?
        responsiveness = max(0.0, beat_trigger_ratio - nonbeat_spike_rate)

        # Mean brightness on-beat vs off-beat
        mean_bright_beat = float(brightness[beats].mean()) if beats.sum() > 0 else 0.0
        mean_bright_nobeat = float(brightness[~beats].mean()) if (~beats).sum() > 0 else 0.0

        # Contrast ratio: guard against all-black frames (both values near 0).
        if mean_bright_nobeat < 1e-6 and mean_bright_beat < 1e-6:
            # All frames effectively black -- no meaningful contrast.
            contrast_ratio = 1.0
        elif mean_bright_nobeat < 1e-6:
            # Off-beat is black but on-beat is not -- cap at 100x.
            contrast_ratio = 100.0
        else:
            contrast_ratio = mean_bright_beat / mean_bright_nobeat

        # Beat-aligned response magnitude:
        # For each beat, measure peak brightness in a 3-frame window after
        # minus the baseline brightness in 2 frames before.
        beat_responses = []
        for bi in beat_idxs:
            pre_start = max(0, bi - 2)
            post_end = min(n, bi + 4)
            pre_level = float(brightness[pre_start:bi].mean()) if bi > 0 else float(brightness[0])
            post_peak = float(brightness[bi:post_end].max())
            beat_responses.append(post_peak - pre_level)
        mean_beat_response = float(np.mean(beat_responses)) if beat_responses else 0.0

        # Spatial spread on-beat vs off-beat (bloom detection)
        mean_spread_beat = float(spatial_spread[beats].mean()) if beats.sum() > 0 else 0.0
        mean_spread_nobeat = float(spatial_spread[~beats].mean()) if (~beats).sum() > 0 else 0.0
    else:
        beat_trigger_ratio = 0.0
        nonbeat_spike_rate = 0.0
        responsiveness = 0.0
        mean_bright_beat = float(brightness.mean())
        mean_bright_nobeat = float(brightness.mean())
        contrast_ratio = 1.0
        mean_beat_response = 0.0
        mean_spread_beat = float(spatial_spread.mean())
        mean_spread_nobeat = float(spatial_spread.mean())

    # --- Beat flag diagnostics ---
    beat_pct = n_beats / n if n > 0 else 0
    if beat_pct > 0.9:
        beat_warning = 'saturated'  # >90% frames have beat=true
    elif beat_pct < 0.02 and duration > 5:
        beat_warning = 'absent'     # <2% beats in >5s capture
    else:
        beat_warning = None

    # --- Frame health ---
    # The renderer runs at ~120 FPS but we capture at ~15 FPS, so
    # consecutive frame_idx values have an expected gap of ~8.
    # A genuine "drop" is when the gap is much larger than expected
    # (firmware failed to send a frame on its interval).
    if len(frame_indices) >= 3:
        idx_diffs = np.diff(frame_indices.astype(np.int64))
        idx_diffs_pos = idx_diffs[idx_diffs > 0]
        if len(idx_diffs_pos) > 0:
            median_gap = float(np.median(idx_diffs_pos))
            # A drop is when the gap is >2x the median (missed a streaming slot)
            drop_threshold = max(2, median_gap * 2)
            genuine_drops = idx_diffs_pos[idx_diffs_pos > drop_threshold]
            frame_drops = int(len(genuine_drops))
            total_missing = int(genuine_drops.sum() / max(1, median_gap) - len(genuine_drops)) if frame_drops > 0 else 0
            subsampling_ratio = median_gap
        else:
            frame_drops = 0
            total_missing = 0
            subsampling_ratio = 1.0
    else:
        frame_drops = 0
        total_missing = 0
        subsampling_ratio = 1.0

    show_valid = show_times[(show_times > 0) & (show_times < 50000)]  # <50ms sane
    # ESP32-S3 has max ~8.3 MB with PSRAM; anything above 16 MB is corrupt
    heap_valid = heap[(heap > 0) & (heap < 16_000_000)]

    skip_vals = np.array([m.get('show_skips', 0) for m in metadata])
    skip_sane = skip_vals[skip_vals < 65535]  # u16 max = corrupt if near boundary
    show_skips_total = int(skip_sane.max() - skip_sane.min()) if len(skip_sane) > 1 else 0

    # Heap trend (bytes per frame, positive = growing).
    # Guard against degenerate polyfit (constant heap, insufficient data).
    heap_trend = 0.0
    if len(heap_valid) > 10:
        try:
            heap_trend = float(np.polyfit(range(len(heap_valid)), heap_valid, 1)[0])
            if math.isnan(heap_trend) or math.isinf(heap_trend):
                heap_trend = 0.0
        except (np.linalg.LinAlgError, FloatingPointError, ValueError):
            heap_trend = 0.0

    result = {
        'label': label,
        'n_frames': n,
        'duration': duration,
        'actual_fps': actual_fps,
        'v1_only': not v2,
        # Beat stats
        'n_beats': n_beats,
        'n_onsets': n_onsets,
        'beat_hz': beat_hz,
        'bpm': bpm,
        'bpm_source': bpm_source,
        'fw_beat_confidence': fw_conf_mean,
        'tempo_stability': tempo_stability,
        'beat_warning': beat_warning,
        # Correlation
        'rms_brightness_corr': rms_corr,
        'bass_brightness_corr': bass_corr,
        'best_band_corr': best_band_corr,
        'best_band_idx': best_band_idx,
        'beat_trigger_ratio': beat_trigger_ratio,
        'nonbeat_spike_rate': nonbeat_spike_rate,
        'responsiveness': responsiveness,
        'mean_bright_beat': mean_bright_beat,
        'mean_bright_nobeat': mean_bright_nobeat,
        'contrast_ratio': contrast_ratio,
        'mean_beat_response': mean_beat_response,
        'mean_brightness': float(brightness.mean()),
        'mean_spatial_spread': float(spatial_spread.mean()),
        'mean_spread_beat': mean_spread_beat,
        'mean_spread_nobeat': mean_spread_nobeat,
        # Multi-dimensional coupling
        'hue_vel_rms_corr': hue_vel_rms_corr,
        'hue_vel_bass_corr': hue_vel_bass_corr,
        'tge_rms_corr': tge_rms_corr,
        'apd_rms_corr': apd_rms_corr,
        'composite_audio_visual_score': composite_audio_visual_score,
        # Frame health
        'frame_drops': frame_drops,
        'total_missing_frames': total_missing,
        'drop_rate': frame_drops / max(1, n),
        'show_time_median_us': float(np.median(show_valid)) if len(show_valid) > 0 else 0,
        'show_time_p99_us': float(np.percentile(show_valid, 99)) if len(show_valid) > 0 else 0,
        'heap_min': int(heap_valid.min()) if len(heap_valid) > 0 else 0,
        'heap_max': int(heap_valid.max()) if len(heap_valid) > 0 else 0,
        'heap_trend': heap_trend,
        'show_skips_total': show_skips_total,
        'subsampling_ratio': subsampling_ratio,
        # Private time-series for dashboard (not printed in text report)
        '_brightness': brightness,
        '_spatial_spread': spatial_spread,
        '_rms': rms,
        '_beats': beats,
        '_onset': onset,
        '_bands': bands,
        '_flux': flux,
        '_show_times': show_times,
        '_heap': heap,
        '_timestamps': timestamps,
        '_hue_velocity': hue_velocity,
        '_tge': tge,
    }

    if not v2:
        result['v1_warning'] = ('v1 frames lack audio metrics; '
                                'beat/RMS/band correlations are zeroed.')

    return result


# ---------------------------------------------------------------------------
# Text Report
# ---------------------------------------------------------------------------

def _rating(value: float, thresholds: list) -> str:
    """Map a value to a rating label using (threshold, label) pairs (descending)."""
    for thresh, label in thresholds:
        if value >= thresh:
            return label
    return thresholds[-1][1]


def format_report(m: dict) -> str:
    """Format analysis metrics as a human-readable text report.

    All numeric values are fetched via ``.get()`` with safe defaults;
    NaN/inf values are printed as 'N/A'.
    """
    if m.get('error'):
        return f"\n  [{m.get('label', '?')}] Analysis failed: {m['error']}\n"

    lines = []
    lines.append('')
    lines.append('=' * 60)
    lines.append(f"  Beat-Brightness Correlation: {m.get('label', '?')}")
    lines.append('=' * 60)
    lines.append('')

    # v1-only warning banner
    if m.get('v1_warning'):
        lines.append(f"  NOTE: {m['v1_warning']}")
        lines.append('')

    lines.append(f"  Capture:  {m.get('n_frames', 0)} frames, "
                 f"{_safe_fmt(m.get('actual_fps'), '.1f')} FPS, "
                 f"{_safe_fmt(m.get('duration'), '.1f')}s")
    bpm_src = m.get('bpm_source', 'tick-count')
    bpm_str = _safe_fmt(m.get('bpm'), '.0f')
    if bpm_src == 'firmware':
        fw_conf = m.get('fw_beat_confidence', 0.0)
        lines.append(f"  BPM:      {bpm_str}  (firmware-reported, "
                     f"confidence {_safe_fmt(fw_conf * 100, '.0f')}%)")
        lines.append(f"  Ticks:    {m.get('n_beats', 0)} captured "
                     f"(~{100 * m.get('n_beats', 0) / max(1, int(m.get('bpm', 120) / 60 * m.get('duration', 30))):.0f}% "
                     f"of expected at {_safe_fmt(m.get('actual_fps'), '.0f')} FPS)")
    else:
        lines.append(f"  Beats:    {m.get('n_beats', 0)} detected "
                     f"({_safe_fmt(m.get('beat_hz'), '.1f')} Hz = "
                     f"{bpm_str} BPM) [tick-count, may be subsampled]")
    if m.get('beat_warning') == 'saturated':
        lines.append(f"  WARNING:  Beat flag saturated "
                     f"({m.get('n_beats', 0)}/{m.get('n_frames', 0)} frames)")
        lines.append(f"            Beat detection may be always-on or threshold too low.")
    elif m.get('beat_warning') == 'absent' and bpm_src != 'firmware':
        lines.append(f"  WARNING:  No beats detected. Audio input silent or detector offline?")
    lines.append(f"  Onsets:   {m.get('n_onsets', 0)} (snare/hihat)")
    tempo_stab = m.get('tempo_stability', 0)
    if isinstance(tempo_stab, (int, float)) and tempo_stab > 0:
        lines.append(f"  Tempo:    {_safe_fmt(tempo_stab * 100, '.0f')}% stable")
    lines.append('')

    # Score card
    resp = m.get('responsiveness', 0.0)
    resp_rating = _rating(resp, [(0.5, 'EXCELLENT'), (0.3, 'good'), (0.1, 'weak'), (0.0, 'none')])

    rms_corr = m.get('rms_brightness_corr', 0.0)
    corr_rating = _rating(rms_corr, [(0.7, 'STRONG'), (0.4, 'moderate'), (0.1, 'weak'), (-1.0, 'none')])

    lines.append('  Audio-Reactive Quality')
    lines.append(f"  {'~' * 44}")
    band_names = ['Sub', 'Bass', 'Low-Mid', 'Mid', 'Upper-Mid', 'Presence', 'Brilliance', 'Air']
    best_bi = m.get('best_band_idx', 0)
    best_bn = band_names[best_bi] if best_bi < len(band_names) else f'band{best_bi}'

    bass_corr = m.get('bass_brightness_corr', 0.0)
    bass_rating = _rating(abs(bass_corr), [(0.7, 'STRONG'), (0.4, 'moderate'), (0.1, 'weak'), (0.0, 'none')])

    lines.append(f"  Responsiveness score:    {_safe_fmt(resp, '+.3f')}  ({resp_rating})")
    lines.append(f"  RMS-brightness corr:     {_safe_fmt(rms_corr, '+.3f')}  ({corr_rating})")
    lines.append(f"  Bass-brightness corr:    {_safe_fmt(bass_corr, '+.3f')}  ({bass_rating})")
    lines.append(f"  Best band correlation:   {_safe_fmt(m.get('best_band_corr', 0), '+.3f')}  ({best_bn})")
    lines.append(f"  Beat trigger ratio:      {_safe_fmt(m.get('beat_trigger_ratio', 0), '.0%')}  "
                 f"(beats causing brightness spike)")
    lines.append(f"  Non-beat spike rate:     {_safe_fmt(m.get('nonbeat_spike_rate', 0), '.0%')}  "
                 f"(background noise)")
    lines.append(f"  Mean beat response:      {_safe_fmt(m.get('mean_beat_response', 0), '+.4f')}")
    lines.append('')
    lines.append(f"  Brightness on beat:      {_safe_fmt(m.get('mean_bright_beat', 0), '.4f')}")
    lines.append(f"  Brightness off beat:     {_safe_fmt(m.get('mean_bright_nobeat', 0), '.4f')}")
    lines.append(f"  Contrast ratio:          {_safe_fmt(m.get('contrast_ratio', 0), '.2f')}x")
    lines.append(f"  Mean brightness:         {_safe_fmt(m.get('mean_brightness', 0), '.4f')}")
    spread = m.get('mean_spatial_spread', 0)
    spread_line = f"  Spatial spread:          {_safe_fmt(spread, '.1%')} of strip active"
    if m.get('n_beats', 0) >= 3:
        spread_line += (f"  (beat: {_safe_fmt(m.get('mean_spread_beat', 0), '.1%')}"
                        f" / off: {_safe_fmt(m.get('mean_spread_nobeat', 0), '.1%')})")
    lines.append(spread_line)
    lines.append('')

    # Multi-Dimensional Coupling
    lines.append('  --- Multi-Dimensional Coupling ---')
    lines.append(f"  Hue velocity \u2194 RMS:    {_safe_fmt(m.get('hue_vel_rms_corr', 0), '+.3f')}")
    lines.append(f"  Hue velocity \u2194 bass:   {_safe_fmt(m.get('hue_vel_bass_corr', 0), '+.3f')}")
    lines.append(f"  TGE \u2194 RMS:             {_safe_fmt(m.get('tge_rms_corr', 0), '+.3f')}")
    lines.append(f"  Active pixel \u0394 \u2194 RMS:  {_safe_fmt(m.get('apd_rms_corr', 0), '+.3f')}")
    composite = m.get('composite_audio_visual_score', 0)
    composite_rating = _rating(composite, [
        (0.5, 'EXCELLENT'), (0.3, 'good'), (0.15, 'moderate'), (0.0, 'weak')])
    lines.append(f"  Composite A/V score:    {_safe_fmt(composite, '+.3f')}  ({composite_rating})")
    lines.append('')

    # Health
    lines.append('  Frame Health')
    lines.append(f"  {'~' * 44}")
    sub = m.get('subsampling_ratio', 1)
    lines.append(f"  Subsampling:      1:{_safe_fmt(sub, '.0f')}  (renderer FPS / capture FPS)")
    lines.append(f"  Frame drops:      {m.get('frame_drops', 0)}  "
                 f"({m.get('total_missing_frames', 0)} missed streaming slots)")
    lines.append(f"  Drop rate:        {_safe_fmt(m.get('drop_rate', 0), '.4f')}")
    lines.append(f"  Show skips:       {m.get('show_skips_total', 0)}")

    show_med = m.get('show_time_median_us', 0)
    if isinstance(show_med, (int, float)) and show_med > 0:
        lines.append(f"  Show time:        median {show_med / 1000:.1f}ms, "
                     f"p99 {m.get('show_time_p99_us', 0) / 1000:.1f}ms")

    heap_min = m.get('heap_min', 0)
    if isinstance(heap_min, (int, float)) and heap_min > 0:
        ht = m.get('heap_trend', 0)
        trend = 'rising' if ht > 10 else ('falling' if ht < -10 else 'stable')
        lines.append(f"  Heap:             {heap_min:,} - {m.get('heap_max', 0):,} bytes ({trend})")

    lines.append('')
    return '\n'.join(lines)


def format_comparison(ma: dict, mb: dict) -> str:
    """Format a side-by-side comparison table of two devices.

    Uses ``.get()`` with defaults throughout so missing keys never crash.
    """
    if ma.get('error') or mb.get('error'):
        return "  Cannot compare: one or both analyses failed.\n"

    lines = []
    lines.append('')
    lines.append('=' * 62)
    lines.append(f"  COMPARISON: {ma.get('label', '?')}  vs  {mb.get('label', '?')}")
    lines.append('=' * 62)
    lines.append('')

    hdr_a = ma.get('label', '?')[:12]
    hdr_b = mb.get('label', '?')[:12]
    lines.append(f"  {'Metric':<30s}  {hdr_a:>10s}       {hdr_b:>10s}")
    lines.append(f"  {'~' * 58}")

    def row(label, key, fmt='.3f', higher_better=True):
        va = ma.get(key, 0)
        vb = mb.get(key, 0)
        # Guard against NaN/inf from either side.
        if isinstance(va, float) and (math.isnan(va) or math.isinf(va)):
            va = 0.0
        if isinstance(vb, float) and (math.isnan(vb) or math.isinf(vb)):
            vb = 0.0

        if fmt.endswith('%'):
            sa = _safe_fmt(va, '.0%')
            sb = _safe_fmt(vb, '.0%')
            diff = va - vb
        elif fmt == ',d':
            sa = f"{int(va):,}"
            sb = f"{int(vb):,}"
            diff = va - vb
        elif fmt == 'd':
            sa = str(int(va))
            sb = str(int(vb))
            diff = va - vb
        else:
            sa = _safe_fmt(va, fmt)
            sb = _safe_fmt(vb, fmt)
            diff = va - vb

        if abs(diff) < 0.005:
            arrow = '  =  '
        elif (diff > 0) == higher_better:
            arrow = ' <<< '
        else:
            arrow = ' >>> '

        lines.append(f"  {label:<30s}  {sa:>10s}{arrow}{sb:>10s}")

    row('Responsiveness score', 'responsiveness', '+.3f', True)
    row('RMS-brightness corr', 'rms_brightness_corr', '+.3f', True)
    row('Bass-brightness corr', 'bass_brightness_corr', '+.3f', True)
    row('Best band correlation', 'best_band_corr', '+.3f', True)
    row('Beat trigger ratio', 'beat_trigger_ratio', '.0%', True)
    row('Non-beat spike rate', 'nonbeat_spike_rate', '.0%', False)
    row('Contrast ratio', 'contrast_ratio', '.2f', True)
    row('Mean beat response', 'mean_beat_response', '+.4f', True)
    lines.append(f"  {'~' * 58}")
    row('Frame drops', 'frame_drops', 'd', False)
    row('Show time median (us)', 'show_time_median_us', '.0f', False)
    row('Heap min', 'heap_min', ',d', True)
    row('Tempo stability', 'tempo_stability', '.0%', True)
    lines.append(f"")

    # Composite score: weighted combination of all audio-reactive indicators
    # Use best of RMS/bass correlation to capture effects that respond to
    # specific bands rather than overall energy.
    best_corr_a = max(max(0, ma.get('rms_brightness_corr', 0)),
                      max(0, ma.get('bass_brightness_corr', 0)),
                      max(0, ma.get('best_band_corr', 0)))
    best_corr_b = max(max(0, mb.get('rms_brightness_corr', 0)),
                      max(0, mb.get('bass_brightness_corr', 0)),
                      max(0, mb.get('best_band_corr', 0)))

    w_resp, w_corr, w_contrast = 0.4, 0.4, 0.2
    score_a = (ma.get('responsiveness', 0) * w_resp
               + best_corr_a * w_corr
               + min(1, ma.get('contrast_ratio', 1) / 3) * w_contrast)
    score_b = (mb.get('responsiveness', 0) * w_resp
               + best_corr_b * w_corr
               + min(1, mb.get('contrast_ratio', 1) / 3) * w_contrast)

    if abs(score_a - score_b) < 0.02:
        verdict = "TIE -- no significant difference"
    elif score_a > score_b:
        verdict = (f"{ma.get('label', '?')} wins  "
                   f"(composite {score_a:.3f} vs {score_b:.3f})")
    else:
        verdict = (f"{mb.get('label', '?')} wins  "
                   f"(composite {score_b:.3f} vs {score_a:.3f})")

    lines.append(f"  Verdict: {verdict}")
    lines.append(f"")
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Regression Gate
# ---------------------------------------------------------------------------

# Default thresholds for the regression gate. Each key maps to a tuple of
# (threshold_value, comparison_direction) where direction is 'lt' (actual must
# be less than threshold) or 'gt' (actual must be greater than threshold).
DEFAULT_GATE_THRESHOLDS = {
    'drop_rate':  (0.01,    'lt'),   # < 1% frame drops
    'show_time':  (10000.0, 'lt'),   # p99 < 10 ms (in microseconds)
    'heap_min':   (100000,  'gt'),   # > 100 KB free
    'heap_trend': (-100.0,  'gt'),   # not leaking faster than 100 B/frame
    'show_skips': (10,      'lt'),   # fewer than 10 skip events
}

# Human-readable names for gate checks (used in the report table).
_GATE_CHECK_NAMES = {
    'drop_rate':  'Frame drop rate',
    'show_time':  'Show time p99',
    'heap_min':   'Heap minimum',
    'heap_trend': 'Heap trend',
    'show_skips': 'Show skips',
}


class GateResult:
    """Result of a single regression gate check."""

    __slots__ = ('name', 'status', 'actual', 'threshold', 'direction',
                 'actual_display', 'threshold_display')

    def __init__(self, name: str, status: str, actual, threshold,
                 direction: str, actual_display: str = '',
                 threshold_display: str = ''):
        self.name = name
        self.status = status              # 'PASS', 'FAIL', or 'SKIP'
        self.actual = actual
        self.threshold = threshold
        self.direction = direction        # 'lt' or 'gt'
        self.actual_display = actual_display
        self.threshold_display = threshold_display


def _human_bytes(b: float) -> str:
    """Format a byte count for human display (e.g. 8.1 MB, 100 KB)."""
    if b >= 1_000_000:
        return f'{b / 1_000_000:.1f} MB'
    elif b >= 1_000:
        return f'{b / 1_000:.0f} KB'
    else:
        return f'{int(b)} B'


def run_regression_gate(metrics: dict,
                        thresholds: dict = None) -> list:
    """Evaluate pass/fail for each regression gate check.

    Parameters
    ----------
    metrics : dict
        Output from ``analyze_correlation()``.
    thresholds : dict, optional
        Override individual thresholds. Keys match ``DEFAULT_GATE_THRESHOLDS``.

    Returns
    -------
    list[GateResult]
        One result per check, in display order.
    """
    th = dict(DEFAULT_GATE_THRESHOLDS)
    if thresholds:
        for k, v in thresholds.items():
            if k in th:
                th[k] = (v, th[k][1])

    results = []

    # 1. Frame drop rate  (always evaluated)
    val = metrics.get('drop_rate', 0.0)
    limit, direction = th['drop_rate']
    results.append(GateResult(
        name='drop_rate',
        status='PASS' if val < limit else 'FAIL',
        actual=val, threshold=limit, direction=direction,
        actual_display=f'{val:.4f}',
        threshold_display=f'< {limit:.4f}',
    ))

    # 2. Show time p99  (skip if firmware doesn't report show_time)
    val = metrics.get('show_time_p99_us', 0.0)
    limit, direction = th['show_time']
    if val > 0:
        results.append(GateResult(
            name='show_time',
            status='PASS' if val < limit else 'FAIL',
            actual=val, threshold=limit, direction=direction,
            actual_display=f'{val / 1000:.1f}ms',
            threshold_display=f'< {limit / 1000:.1f}ms',
        ))
    else:
        results.append(GateResult(
            name='show_time',
            status='SKIP',
            actual=0, threshold=limit, direction=direction,
            actual_display='n/a',
            threshold_display=f'< {limit / 1000:.1f}ms',
        ))

    # 3. Heap minimum  (skip if firmware doesn't report heap)
    val = metrics.get('heap_min', 0)
    limit, direction = th['heap_min']
    if val > 0:
        results.append(GateResult(
            name='heap_min',
            status='PASS' if val > limit else 'FAIL',
            actual=val, threshold=limit, direction=direction,
            actual_display=_human_bytes(val),
            threshold_display=f'> {_human_bytes(limit)}',
        ))
    else:
        results.append(GateResult(
            name='heap_min',
            status='SKIP',
            actual=0, threshold=limit, direction=direction,
            actual_display='n/a',
            threshold_display=f'> {_human_bytes(limit)}',
        ))

    # 4. Heap trend  (skip if no heap data)
    val = metrics.get('heap_trend', 0.0)
    limit, direction = th['heap_trend']
    heap_min = metrics.get('heap_min', 0)
    if heap_min > 0:
        if val > 10:
            trend_str = f'+{val:.0f} B/frame'
        elif val < -10:
            trend_str = f'{val:.0f} B/frame'
        else:
            trend_str = 'stable'
        results.append(GateResult(
            name='heap_trend',
            status='PASS' if val > limit else 'FAIL',
            actual=val, threshold=limit, direction=direction,
            actual_display=trend_str,
            threshold_display=f'> {limit:.0f} B/frame',
        ))
    else:
        results.append(GateResult(
            name='heap_trend',
            status='SKIP',
            actual=0, threshold=limit, direction=direction,
            actual_display='n/a',
            threshold_display=f'> {limit:.0f} B/frame',
        ))

    # 5. Show skips  (always evaluated)
    val = metrics.get('show_skips_total', 0)
    limit, direction = th['show_skips']
    results.append(GateResult(
        name='show_skips',
        status='PASS' if val < limit else 'FAIL',
        actual=val, threshold=limit, direction=direction,
        actual_display=str(int(val)),
        threshold_display=f'< {int(limit)}',
    ))

    return results


def format_gate_report(results: list, label: str = '') -> str:
    """Format regression gate results as a clear pass/fail table.

    Parameters
    ----------
    results : list[GateResult]
        Output from ``run_regression_gate()``.
    label : str
        Device/build label for the header.

    Returns
    -------
    str
        Formatted multi-line report.
    """
    header = f'Regression Gate: {label}' if label else 'Regression Gate'
    rule = '\u2500' * 50  # thin horizontal line

    lines = ['']
    lines.append(f'  {header}')
    lines.append(f'  {rule}')

    n_pass = 0
    n_fail = 0
    n_skip = 0

    for r in results:
        tag = f'[{r.status}]'
        name = _GATE_CHECK_NAMES.get(r.name, r.name)

        if r.status == 'PASS':
            n_pass += 1
        elif r.status == 'FAIL':
            n_fail += 1
        else:
            n_skip += 1

        lines.append(
            f'  {tag:<6s} {name:<22s} {r.actual_display:<12s} {r.threshold_display}'
        )

    lines.append(f'  {rule}')

    evaluated = n_pass + n_fail
    if n_fail > 0:
        verdict = f'FAIL ({n_pass}/{evaluated} passed, {n_fail} failed)'
    else:
        verdict = f'PASS ({n_pass}/{evaluated})'
    if n_skip > 0:
        verdict += f'  [{n_skip} skipped]'

    lines.append(f'  RESULT: {verdict}')
    lines.append('')
    return '\n'.join(lines)


def gate_has_failures(results: list) -> bool:
    """Return True if any gate check has status FAIL."""
    return any(r.status == 'FAIL' for r in results)


def _compute_composite_score(metrics: dict) -> float:
    """Compute the weighted composite audio-reactive score for a device.

    Matches the formula used in ``format_comparison()``.
    """
    best_corr = max(
        max(0, metrics.get('rms_brightness_corr', 0)),
        max(0, metrics.get('bass_brightness_corr', 0)),
        max(0, metrics.get('best_band_corr', 0)),
    )
    w_resp, w_corr, w_contrast = 0.4, 0.4, 0.2
    return (metrics.get('responsiveness', 0) * w_resp
            + best_corr * w_corr
            + min(1, metrics.get('contrast_ratio', 1) / 3) * w_contrast)


def format_comparison_gate(metrics_a: dict, metrics_b: dict,
                           regression_threshold: float = 0.1) -> str:
    """Comparative regression gate between two devices.

    Flags a REGRESSION WARNING if device A's composite score is more
    than ``regression_threshold`` below device B's. This is advisory
    (not a hard fail).
    """
    if metrics_a.get('error') or metrics_b.get('error'):
        return '  Comparative gate skipped: one or both analyses failed.\n'

    score_a = _compute_composite_score(metrics_a)
    score_b = _compute_composite_score(metrics_b)
    delta = score_a - score_b

    rule = '\u2500' * 50

    lines = ['']
    lines.append('  Comparative Gate')
    lines.append(f'  {rule}')
    lines.append(f'  {metrics_a["label"]:<20s}  composite = {score_a:.3f}')
    lines.append(f'  {metrics_b["label"]:<20s}  composite = {score_b:.3f}')
    lines.append(
        f'  Delta (A - B):         {delta:+.3f}'
        f'  (threshold: {regression_threshold:.3f})'
    )

    if delta < -regression_threshold:
        lines.append(
            f'  WARNING: REGRESSION -- {metrics_a["label"]} scores '
            f'{abs(delta):.3f} below {metrics_b["label"]}'
        )
    else:
        lines.append('  OK -- no regression detected')

    lines.append('')
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Dashboard PNG
# ---------------------------------------------------------------------------

def _load_font(size: int):
    """Load Menlo or fall back to default."""
    try:
        return ImageFont.truetype("/System/Library/Fonts/Menlo.ttc", size)
    except (OSError, IOError):
        return ImageFont.load_default()


def _draw_sparkline(draw, x0, y0, w, h, data, color,
                    vmin=None, vmax=None, beat_markers=None):
    """Draw a sparkline into a rectangle."""
    if len(data) == 0:
        return

    # Background
    draw.rectangle([x0, y0, x0 + w, y0 + h], fill=(30, 30, 30))

    if vmin is None:
        vmin = float(data.min())
    if vmax is None:
        vmax = float(data.max())
    if vmax <= vmin:
        vmax = vmin + 1.0

    n = len(data)

    # Beat markers first (behind sparkline)
    if beat_markers is not None:
        for i in range(min(n, len(beat_markers))):
            if beat_markers[i]:
                bx = x0 + int(i * w / n)
                draw.line([(bx, y0), (bx, y0 + h)], fill=(80, 30, 30), width=1)

    # Sparkline
    points = []
    for i in range(n):
        px = x0 + int(i * w / n)
        py = y0 + h - int((float(data[i]) - vmin) / (vmax - vmin) * (h - 4)) - 2
        py = max(y0, min(y0 + h, py))
        points.append((px, py))

    if len(points) >= 2:
        draw.line(points, fill=color, width=1)

    # Range labels
    font_s = _load_font(9)
    draw.text((x0 + w + 4, y0 - 1), f"{vmax:.2f}", fill=(90, 90, 90), font=font_s)
    draw.text((x0 + w + 4, y0 + h - 11), f"{vmin:.2f}", fill=(90, 90, 90), font=font_s)


def _draw_heatmap(draw, x0, y0, w, h, data_2d, beat_markers=None):
    """Draw an 8-band heatmap (time x bands). data_2d: (N, 8) in [0, 1]."""
    n, nbands = data_2d.shape
    if n == 0:
        return

    draw.rectangle([x0, y0, x0 + w, y0 + h], fill=(30, 30, 30))

    band_h = h / nbands

    for i in range(n):
        px = x0 + int(i * w / n)
        pw = max(1, int(w / n))
        for b in range(nbands):
            v = float(data_2d[i, b])
            # Blue-to-orange colour map
            r = int(min(255, v * 400))
            g = int(min(255, v * 200))
            bl = int(max(0, 80 - v * 80))
            by = y0 + int((nbands - 1 - b) * band_h)  # low freq at bottom
            bh = max(1, int(band_h))
            draw.rectangle([px, by, px + pw, by + bh], fill=(r, g, bl))

    # Beat markers
    if beat_markers is not None:
        for i in range(min(n, len(beat_markers))):
            if beat_markers[i]:
                bx = x0 + int(i * w / n)
                draw.line([(bx, y0), (bx, y0 + h)], fill=(255, 255, 255, 80), width=1)


def render_dashboard(metrics: dict, width: int = 900) -> Image.Image:
    """Render a metrics dashboard as a PNG image.

    Shows sparklines for brightness, RMS, spectral bands heatmap,
    show time, and heap — with beat events overlaid as vertical markers.
    """
    brightness = metrics.get('_brightness', np.array([]))
    rms = metrics.get('_rms', np.array([]))
    beats = metrics.get('_beats', np.array([]))
    bands = metrics.get('_bands', np.zeros((0, 8)))
    show_times = metrics.get('_show_times', np.array([]))
    heap = metrics.get('_heap', np.array([]))
    hue_velocity = metrics.get('_hue_velocity', np.array([]))
    tge_data = metrics.get('_tge', np.array([]))

    n = len(brightness)
    if n < 2:
        return None

    # Layout constants
    margin = 16
    label_w = 110
    range_w = 50  # space for min/max labels right of sparkline
    spark_w = width - label_w - range_w - margin * 2
    row_h = 44
    row_gap = 6
    header_h = 50
    score_h = 80  # score summary block
    rows = ['Brightness', 'RMS Audio', 'Bands [8]', 'Show Time', 'Heap',
            'Hue Velocity', 'TGE']
    n_rows = len(rows)
    total_h = header_h + score_h + n_rows * (row_h + row_gap) + margin

    img = Image.new('RGB', (width, total_h), (20, 20, 20))
    draw = ImageDraw.Draw(img)
    font = _load_font(11)
    font_header = _load_font(14)
    font_score = _load_font(12)

    # Header
    draw.text((margin, 10), f"{metrics['label']}", fill=(230, 230, 230), font=font_header)
    draw.text((margin, 28),
              f"{n} frames | {metrics['duration']:.1f}s | {metrics['actual_fps']:.1f} FPS | "
              f"{metrics['n_beats']} beats ({metrics['bpm']:.0f} BPM)",
              fill=(140, 140, 140), font=font)

    # Score summary block
    sy = header_h
    draw.rectangle([margin, sy, width - margin, sy + score_h], fill=(28, 28, 28))

    resp = metrics['responsiveness']
    corr = metrics['rms_brightness_corr']
    trig = metrics['beat_trigger_ratio']
    contr = metrics['contrast_ratio']

    col_w = (width - margin * 2) // 4
    for i, (lbl, val, fmt) in enumerate([
        ('Responsiveness', resp, '+.3f'),
        ('RMS Correlation', corr, '+.3f'),
        ('Beat Trigger', trig, '.0%'),
        ('Contrast', contr, '.2f'),
    ]):
        cx = margin + i * col_w + 10
        draw.text((cx, sy + 8), lbl, fill=(120, 120, 120), font=_load_font(9))
        if fmt == '.0%':
            val_str = f"{val:.0%}"
        else:
            val_str = f"{val:{fmt}}"
        # Colour-code the value
        if isinstance(val, float) and val > 0.3:
            vc = (100, 230, 130)
        elif isinstance(val, float) and val > 0.1:
            vc = (230, 200, 80)
        else:
            vc = (200, 100, 100)
        draw.text((cx, sy + 24), val_str, fill=vc, font=font_header)
        draw.text((cx, sy + 48),
                  _rating(val, [(0.5, 'EXCELLENT'), (0.3, 'good'), (0.1, 'weak'), (0.0, 'none')])
                  if i < 2 else (
                      _rating(val, [(0.7, 'EXCELLENT'), (0.5, 'good'), (0.3, 'weak'), (0.0, 'none')])
                      if i == 2 else
                      _rating(val, [(2.0, 'STRONG'), (1.3, 'moderate'), (1.0, 'flat'), (0.0, 'none')])
                  ),
                  fill=(90, 90, 90), font=_load_font(9))

    # Sparkline rows
    y = header_h + score_h + row_gap
    sx = margin + label_w

    # 1. Brightness
    draw.text((margin, y + row_h // 2 - 6), 'Brightness', fill=(160, 160, 160), font=font)
    _draw_sparkline(draw, sx, y, spark_w, row_h, brightness,
                    (100, 200, 255), vmin=0, vmax=1, beat_markers=beats)
    y += row_h + row_gap

    # 2. RMS
    draw.text((margin, y + row_h // 2 - 6), 'RMS Audio', fill=(160, 160, 160), font=font)
    _draw_sparkline(draw, sx, y, spark_w, row_h, rms,
                    (100, 255, 100), vmin=0, vmax=1, beat_markers=beats)
    y += row_h + row_gap

    # 3. Bands heatmap
    draw.text((margin, y + row_h // 2 - 6), 'Bands [8]', fill=(160, 160, 160), font=font)
    _draw_heatmap(draw, sx, y, spark_w, row_h, bands, beat_markers=beats)
    # Band labels
    font_tiny = _load_font(8)
    band_labels = ['Sub', 'Bass', 'LM', 'Mid', 'UM', 'Pres', 'Bril', 'Air']
    band_h = row_h / 8
    for b, bl in enumerate(band_labels):
        by = y + int((7 - b) * band_h)
        draw.text((sx + spark_w + 4, by), bl, fill=(70, 70, 70), font=font_tiny)
    y += row_h + row_gap

    # 4. Show time (ms)
    draw.text((margin, y + row_h // 2 - 6), 'Show Time', fill=(160, 160, 160), font=font)
    st_ms = show_times / 1000.0
    _draw_sparkline(draw, sx, y, spark_w, row_h, st_ms,
                    (255, 200, 100), vmin=0,
                    vmax=max(10, float(st_ms.max())) if len(st_ms) > 0 else 10)
    y += row_h + row_gap

    # 5. Heap
    draw.text((margin, y + row_h // 2 - 6), 'Heap (KB)', fill=(160, 160, 160), font=font)
    if len(heap) > 0 and heap.max() > 0:
        heap_kb = heap / 1024.0
        _draw_sparkline(draw, sx, y, spark_w, row_h, heap_kb,
                        (200, 130, 255),
                        vmin=float(heap_kb[heap_kb > 0].min()) * 0.95 if (heap_kb > 0).any() else 0,
                        vmax=float(heap_kb.max()) * 1.02)
    y += row_h + row_gap

    # 6. Hue Velocity
    draw.text((margin, y + row_h // 2 - 6), 'Hue Velocity', fill=(160, 160, 160), font=font)
    if len(hue_velocity) > 0:
        _draw_sparkline(draw, sx, y, spark_w, row_h, hue_velocity,
                        (255, 150, 200), vmin=0,
                        vmax=max(0.1, float(hue_velocity.max())),
                        beat_markers=beats)
    y += row_h + row_gap

    # 7. Temporal Gradient Energy
    draw.text((margin, y + row_h // 2 - 6), 'TGE', fill=(160, 160, 160), font=font)
    if len(tge_data) > 0:
        _draw_sparkline(draw, sx, y, spark_w, row_h, tge_data,
                        (255, 130, 80), vmin=0,
                        vmax=max(1.0, float(tge_data.max())),
                        beat_markers=beats)

    return img


def render_comparison_dashboard(ma: dict, mb: dict, width: int = 900) -> Image.Image:
    """Render two dashboards stacked vertically with a divider."""
    da = render_dashboard(ma, width)
    db = render_dashboard(mb, width)

    if da is None or db is None:
        return da or db

    gap = 4
    combined = Image.new('RGB', (width, da.height + gap + db.height), (50, 50, 50))
    combined.paste(da, (0, 0))
    combined.paste(db, (0, da.height + gap))
    return combined


# ---------------------------------------------------------------------------
# N-Way Multi-Device Capture & Comparison
# ---------------------------------------------------------------------------

def capture_multiple_devices(device_specs: list, duration: float,
                             fps: int = 15, tap: str = 'b') -> list:
    """Capture from N devices in parallel.

    Spawns a capture thread per device (same pattern as the existing
    dual-device capture in main()) and collects results.

    Parameters
    ----------
    device_specs : list[tuple[str, str]]
        List of (port, label) tuples for each device.
    duration : float
        Capture duration in seconds.
    fps : int
        Streaming FPS for capture.
    tap : str
        Capture tap point ('a', 'b', or 'c').

    Returns
    -------
    list[tuple[str, ndarray | None, ndarray | None, list | None]]
        List of (label, frames, timestamps, metadata) tuples.
        frames/timestamps/metadata are None if that device failed.
    """
    n = len(device_specs)
    results = [None] * n
    stop_events = [threading.Event() for _ in range(n)]

    def _worker(idx, port, label, stop_evt):
        results[idx] = capture_with_metadata(port, duration, fps, tap, stop_evt)

    threads = []
    for i, (port, label) in enumerate(device_specs):
        t = threading.Thread(
            target=_worker, args=(i, port, label, stop_events[i]),
            daemon=True,
        )
        threads.append(t)
        t.start()

    # Wait for all threads with a generous timeout
    timeout = duration + 20
    for t in threads:
        t.join(timeout=timeout)

    # Signal any stragglers to stop
    for evt in stop_events:
        evt.set()

    # Build output list, warning on partial failures
    output = []
    for i, (port, label) in enumerate(device_specs):
        cap = results[i]
        if cap is None:
            print(f"WARNING: No frames captured from {label} ({port})",
                  file=sys.stderr)
            output.append((label, None, None, None))
        else:
            frames, timestamps, metadata = cap
            output.append((label, frames, timestamps, metadata))

    return output


def capture_sequential_devices(port: str, labels: list, duration: float,
                               fps: int = 15, tap: str = 'b') -> list:
    """Capture from the same port multiple times, prompting between runs.

    Useful for three-way comparison when only one physical device is
    available. The user reflashes firmware between captures.

    Parameters
    ----------
    port : str
        Serial port path.
    labels : list[str]
        Label for each sequential capture run.
    duration : float
        Capture duration in seconds per run.
    fps : int
        Streaming FPS.
    tap : str
        Capture tap point.

    Returns
    -------
    list[tuple[str, ndarray | None, ndarray | None, list | None]]
        Same format as ``capture_multiple_devices()``.
    """
    output = []
    for i, label in enumerate(labels):
        if i > 0:
            print(f"\n{'=' * 60}")
            print(f"  Flash firmware for: {label}")
            print(f"  Then press Enter to begin capture {i + 1}/{len(labels)}...")
            print(f"{'=' * 60}")
            try:
                input()
            except EOFError:
                print("WARNING: stdin closed, skipping remaining captures",
                      file=sys.stderr)
                output.append((label, None, None, None))
                continue

        print(f"\n[Sequential] Capturing {label} ({i + 1}/{len(labels)})...")
        cap = capture_with_metadata(port, duration, fps, tap)
        if cap is None:
            print(f"WARNING: No frames captured for {label}", file=sys.stderr)
            output.append((label, None, None, None))
        else:
            frames, timestamps, metadata = cap
            output.append((label, frames, timestamps, metadata))

    return output


def format_multi_comparison(metrics_list: list) -> str:
    """Format an N-way comparison table with ranking.

    Parameters
    ----------
    metrics_list : list[dict]
        List of metrics dicts from ``analyze_correlation()``.

    Returns
    -------
    str
        Formatted multi-line comparison table.
    """
    # Filter out errored entries for the table body
    valid = [m for m in metrics_list if not m.get('error')]
    if len(valid) < 2:
        return "  Cannot compare: fewer than 2 valid analyses.\n"

    n = len(valid)
    labels = [m['label'] for m in valid]

    # Truncate labels to fit columns
    max_label = 14
    short_labels = [lbl[:max_label] for lbl in labels]

    # Column widths
    metric_col = 28
    val_col = max_label + 2
    total_w = metric_col + val_col * n

    lines = []
    lines.append('')
    lines.append(f"N-Way Comparison ({n} devices)")
    lines.append('\u2550' * total_w)

    # Header row
    hdr = f"  {'Metric':<{metric_col - 2}s}"
    for sl in short_labels:
        hdr += f"  {sl:>{val_col - 2}s}"
    lines.append(hdr)
    lines.append('\u2500' * total_w)

    # Row definitions: (display_name, key, fmt, higher_is_better)
    rows_def = [
        ('Responsiveness',       'responsiveness',        '+.3f',  True),
        ('RMS-brightness corr',  'rms_brightness_corr',   '+.3f',  True),
        ('Bass-brightness corr', 'bass_brightness_corr',  '+.3f',  True),
        ('Best band corr',       'best_band_corr',        '+.3f',  True),
        ('Beat trigger ratio',   'beat_trigger_ratio',    '.0%',   True),
        ('Non-beat spike rate',  'nonbeat_spike_rate',    '.0%',   False),
        ('Contrast ratio',       'contrast_ratio',        '.2f',   True),
        ('Mean beat response',   'mean_beat_response',    '+.4f',  True),
        (None, None, None, None),  # separator
        ('Frame drops',          'frame_drops',           'd',     False),
        ('Show time median (us)','show_time_median_us',   '.0f',   False),
        ('Heap min',             'heap_min',              ',d',    True),
        ('Tempo stability',      'tempo_stability',       '.0%',   True),
    ]

    for row_name, key, fmt, higher_better in rows_def:
        if row_name is None:
            lines.append('\u2500' * total_w)
            continue

        vals = [m.get(key, 0) for m in valid]

        # Format each value
        formatted = []
        for v in vals:
            if fmt == '.0%':
                formatted.append(f"{v:.0%}")
            elif fmt == ',d':
                formatted.append(f"{int(v):,}")
            elif fmt == 'd':
                formatted.append(str(int(v)))
            else:
                formatted.append(f"{v:{fmt}}")

        # Find the best value index
        if higher_better:
            best_idx = int(np.argmax(vals))
        else:
            best_idx = int(np.argmin(vals))

        # Build row with best value marked
        row_str = f"  {row_name:<{metric_col - 2}s}"
        for i, fv in enumerate(formatted):
            marker = ' *' if i == best_idx else '  '
            row_str += f"{marker}{fv:>{val_col - 4}s}  "
        lines.append(row_str)

    lines.append('\u2500' * total_w)

    # Composite scores and ranking
    scores = [_compute_composite_score(m) for m in valid]
    ranked = sorted(range(n), key=lambda i: scores[i], reverse=True)
    ordinals = ['1st', '2nd', '3rd'] + [f'{i+1}th' for i in range(3, n)]

    score_row = f"  {'Composite score':<{metric_col - 2}s}"
    rank_row = f"  {'Rank':<{metric_col - 2}s}"
    for i in range(n):
        score_row += f"  {scores[i]:>{val_col - 2}.3f}"
        rank_pos = next(j for j, ri in enumerate(ranked) if ri == i)
        rank_row += f"  {ordinals[rank_pos]:>{val_col - 2}s}"

    lines.append(score_row)
    lines.append(rank_row)
    lines.append('\u2550' * total_w)

    # Verdict
    winner_idx = ranked[0]
    if len(ranked) >= 2 and abs(scores[ranked[0]] - scores[ranked[1]]) < 0.02:
        lines.append(f"  Verdict: TIE between {labels[ranked[0]]} and "
                     f"{labels[ranked[1]]}")
    else:
        lines.append(f"  Verdict: {labels[winner_idx]} wins  "
                     f"(composite {scores[winner_idx]:.3f})")

    lines.append('')
    return '\n'.join(lines)


def render_multi_dashboard(metrics_list: list,
                           width: int = 900) -> Image.Image:
    """Render N dashboards stacked vertically with dividers.

    Extends the existing ``render_comparison_dashboard`` pattern to
    support an arbitrary number of devices.

    Parameters
    ----------
    metrics_list : list[dict]
        List of metrics dicts from ``analyze_correlation()``.
    width : int
        Image width in pixels.

    Returns
    -------
    Image.Image or None
        Combined dashboard image, or None if no valid data.
    """
    panels = []
    for m in metrics_list:
        if not m.get('error'):
            panel = render_dashboard(m, width)
            if panel is not None:
                panels.append(panel)

    if not panels:
        return None

    if len(panels) == 1:
        return panels[0]

    gap = 4
    total_h = sum(p.height for p in panels) + gap * (len(panels) - 1)
    combined = Image.new('RGB', (width, total_h), (50, 50, 50))

    y_offset = 0
    for i, panel in enumerate(panels):
        combined.paste(panel, (0, y_offset))
        y_offset += panel.height + gap

    return combined


def _parse_devices_arg(value: str) -> list:
    """Parse --devices argument into a list of (port, label) tuples.

    Format: "port1:label1,port2:label2,..."
    If a label is omitted, a default "Device N" label is assigned.
    """
    specs = []
    for i, entry in enumerate(value.split(',')):
        entry = entry.strip()
        if not entry:
            continue
        if ':' in entry:
            port, label = entry.split(':', 1)
            specs.append((port.strip(), label.strip()))
        else:
            specs.append((entry.strip(), f'Device {chr(65 + i)}'))
    return specs


# ---------------------------------------------------------------------------
# Save / Load
# ---------------------------------------------------------------------------

def save_session(path: str, frames_a, ts_a, meta_a,
                 frames_b=None, ts_b=None, meta_b=None,
                 label_a='Device A', label_b='Device B'):
    """Save capture session to compressed .npz file.

    Includes a ``_format_version`` key so future loaders can detect
    schema changes.
    """
    d = {
        '_format_version': _SESSION_FORMAT_VERSION,
        'frames_a': frames_a,
        'timestamps_a': ts_a,
        'metadata_a': np.array(meta_a, dtype=object),
        'label_a': label_a,
    }
    if frames_b is not None:
        d['frames_b'] = frames_b
        d['timestamps_b'] = ts_b
        d['metadata_b'] = np.array(meta_b, dtype=object)
        d['label_b'] = label_b
    np.savez_compressed(path, **d)
    if not _quiet:
        print(f"[Save] Session data: {path}")


def load_session(path: str):
    """Load saved session. Returns dict with keys frames_a, timestamps_a, etc.

    Validates that the file contains the minimum required keys before
    accessing them, and warns on format version mismatches.
    """
    data = np.load(path, allow_pickle=True)

    # Check format version (absent in v0/v1 files -- that is fine).
    file_ver = int(data['_format_version']) if '_format_version' in data else 1
    if file_ver > _SESSION_FORMAT_VERSION:
        print(f"[Load] WARNING: File format version {file_ver} is newer than "
              f"this tool supports ({_SESSION_FORMAT_VERSION}). "
              f"Some fields may be missing.", file=sys.stderr)

    # Validate required keys.
    required = ['frames_a', 'timestamps_a', 'metadata_a']
    missing = [k for k in required if k not in data]
    if missing:
        print(f"[Load] ERROR: Malformed session file '{path}' -- "
              f"missing keys: {', '.join(missing)}", file=sys.stderr)
        sys.exit(1)

    result = {
        'frames_a': data['frames_a'],
        'timestamps_a': data['timestamps_a'],
        'metadata_a': data['metadata_a'].tolist(),
        'label_a': str(data.get('label_a', 'Device A')),
    }
    if 'frames_b' in data:
        result['frames_b'] = data['frames_b']
        result['timestamps_b'] = data['timestamps_b']
        result['metadata_b'] = data['metadata_b'].tolist()
        result['label_b'] = str(data.get('label_b', 'Device B'))
    return result


# ---------------------------------------------------------------------------
# Temporal Alignment
# ---------------------------------------------------------------------------

# Maximum lag to search when cross-correlating two captures (~2 seconds at
# 15 FPS).  Beyond this the captures are too misaligned to salvage.
_MAX_ALIGN_OFFSET = 30


def align_captures(metadata_a: list, metadata_b: list,
                   timestamps_a: np.ndarray, timestamps_b: np.ndarray,
                   target_fps: float = 15.0):
    """Cross-correlate RMS signals to find the temporal offset between two
    captures of the same audio environment.

    Both devices hear the same room audio but their serial capture streams
    start at slightly different wall-clock times (board reset jitter). This
    function finds the lag that maximises normalised cross-correlation of
    the RMS energy envelopes, giving a post-hoc time shift to align the
    two streams frame-by-frame.

    Parameters
    ----------
    metadata_a, metadata_b : list[dict]
        Per-frame v2 metadata from each capture.
    timestamps_a, timestamps_b : ndarray
        Wall-clock timestamps (seconds) for each frame.
    target_fps : float
        Resample rate for the common time base.

    Returns
    -------
    (offset_frames, correlation_score, offset_seconds) or None
        offset_frames : int
            How many frames to shift stream B relative to A.
            Positive means B started *ahead* (B's events appear earlier).
        correlation_score : float
            Peak normalised cross-correlation value in [0, 1].
        offset_seconds : float
            Offset converted to seconds at the target FPS.
        Returns None if alignment is not possible (e.g. silent RMS).
    """
    rms_a = np.array([m.get('rms', 0.0) for m in metadata_a], dtype=np.float64)
    rms_b = np.array([m.get('rms', 0.0) for m in metadata_b], dtype=np.float64)

    # Edge case: one device has no RMS data — alignment impossible.
    if rms_a.max() < 1e-9 or rms_b.max() < 1e-9:
        return None

    # Resample both to a uniform time grid at target_fps.  The raw captures
    # have irregular inter-frame intervals due to serial jitter.
    def _resample(rms, timestamps, fps):
        if len(timestamps) < 2:
            return rms
        t0 = timestamps[0]
        duration = timestamps[-1] - t0
        n_uniform = max(2, int(np.round(duration * fps)))
        t_uniform = np.linspace(0, duration, n_uniform)
        t_rel = timestamps - t0
        return np.interp(t_uniform, t_rel, rms)

    sig_a = _resample(rms_a, timestamps_a, target_fps)
    sig_b = _resample(rms_b, timestamps_b, target_fps)

    # Zero-mean normalisation for Pearson-style cross-correlation.
    sig_a = sig_a - sig_a.mean()
    sig_b = sig_b - sig_b.mean()

    energy_a = np.sqrt(np.sum(sig_a ** 2))
    energy_b = np.sqrt(np.sum(sig_b ** 2))
    if energy_a < 1e-9 or energy_b < 1e-9:
        return None

    # Full cross-correlation.
    xcorr = np.correlate(sig_a, sig_b, mode='full')
    # Normalise by the geometric mean of the two signal energies.
    xcorr /= (energy_a * energy_b)

    # The zero-lag position in the 'full' output is at index len(sig_b) - 1.
    zero_lag = len(sig_b) - 1

    # Restrict the search window to +/- _MAX_ALIGN_OFFSET frames.
    search_lo = max(0, zero_lag - _MAX_ALIGN_OFFSET)
    search_hi = min(len(xcorr), zero_lag + _MAX_ALIGN_OFFSET + 1)
    window = xcorr[search_lo:search_hi]

    peak_idx_in_window = int(np.argmax(window))
    peak_lag = (search_lo + peak_idx_in_window) - zero_lag
    correlation_score = float(window[peak_idx_in_window])
    # Clamp to [0, 1] — negative peaks indicate anti-correlation.
    correlation_score = max(0.0, min(1.0, correlation_score))

    offset_frames = int(peak_lag)
    offset_seconds = offset_frames / target_fps

    return (offset_frames, correlation_score, offset_seconds)


def apply_alignment(frames_a, timestamps_a, metadata_a,
                    frames_b, timestamps_b, metadata_b,
                    offset_frames: int):
    """Trim both capture streams so they overlap after the temporal shift.

    Parameters
    ----------
    frames_a, frames_b : ndarray  (N, 320, 3)
    timestamps_a, timestamps_b : ndarray  (N,)
    metadata_a, metadata_b : list[dict]
    offset_frames : int
        From ``align_captures()``.  Positive means B is ahead of A, so we
        trim the first ``offset_frames`` from B (or the first ``-offset``
        from A if negative).

    Returns
    -------
    (frames_a_aligned, ts_a_aligned, meta_a_aligned,
     frames_b_aligned, ts_b_aligned, meta_b_aligned)
    """
    n_a = len(metadata_a)
    n_b = len(metadata_b)

    if offset_frames >= 0:
        # B is ahead — drop the first `offset_frames` from B.
        start_a = 0
        start_b = offset_frames
    else:
        # A is ahead — drop the first `-offset_frames` from A.
        start_a = -offset_frames
        start_b = 0

    # Common length after trimming the start.
    remaining_a = n_a - start_a
    remaining_b = n_b - start_b
    common_len = min(remaining_a, remaining_b)

    if common_len <= 0:
        # No overlap — return empty arrays.  Caller should check length.
        empty_f = np.zeros((0, frames_a.shape[1], 3), dtype=np.uint8)
        empty_t = np.array([], dtype=np.float64)
        return (empty_f, empty_t, [],
                empty_f.copy(), empty_t.copy(), [])

    end_a = start_a + common_len
    end_b = start_b + common_len

    return (
        frames_a[start_a:end_a],
        timestamps_a[start_a:end_a],
        metadata_a[start_a:end_a],
        frames_b[start_b:end_b],
        timestamps_b[start_b:end_b],
        metadata_b[start_b:end_b],
    )


# ---------------------------------------------------------------------------
# Effect Gallery
# ---------------------------------------------------------------------------

# Predefined gallery profiles — effect IDs to cycle through.
# These are placeholders; customise for your firmware build.
GALLERY_PROFILES = {
    'quick': [0x0100, 0x0200, 0x0300, 0x0400, 0x0500],
    'audio-reactive': [
        0x0100, 0x0200, 0x0500, 0x0800,
        0x0A00, 0x0B00, 0x1100, 0x1200,
    ],
}


def _capture_on_open_port(ser, duration: float, fps: int, tap: str,
                          stop_event: threading.Event):
    """Capture frames from an already-open serial port.

    Like ``capture_with_metadata`` but does NOT open or close the port,
    so the ESP32-S3 is not reset between captures.  The caller is
    responsible for port lifecycle.

    Returns (frames, timestamps, metadata_list) or None.
    """
    tap_letter = tap.lower()
    ser.reset_input_buffer()
    ser.write(f'capture stream {tap_letter} {fps}\n'.encode())
    time.sleep(0.3)

    frames = []
    timestamps = []
    metadata_list = []
    recv_buf = bytearray()
    start_time = time.time()

    try:
        while not stop_event.is_set():
            elapsed = time.time() - start_time
            if elapsed >= duration:
                break

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

                if magic_idx > 0:
                    recv_buf = recv_buf[magic_idx:]

                if len(recv_buf) < 2:
                    break
                version = recv_buf[1]
                frame_size = (SERIAL_V2_FRAME_SIZE if version >= 2
                              else SERIAL_V1_FRAME_SIZE)

                if len(recv_buf) < frame_size:
                    break

                frame_data = bytes(recv_buf[:frame_size])
                recv_buf = recv_buf[frame_size:]

                result = parse_serial_frame(frame_data)
                if result is not None:
                    frame, meta = result
                    frames.append(frame)
                    timestamps.append(time.time())
                    metadata_list.append(meta)

                    count = len(frames)
                    if count % 50 == 0:
                        fps_actual = count / (time.time() - start_time)
                        print(f"  [{count} frames, {elapsed:.1f}s, "
                              f"{fps_actual:.1f} FPS]")
    finally:
        ser.write(b'capture stop\n')
        time.sleep(0.2)

    if not frames:
        return None

    return np.array(frames), np.array(timestamps), metadata_list


def run_effect_gallery(port: str, effects: list, duration_per_effect: float,
                       fps: int = 15, tap: str = 'b',
                       stop_event: threading.Event = None):
    """Cycle through effects, capturing and analysing each one.

    Opens the serial port ONCE, then for each effect ID:
      1. Sends ``effect <hex_id>`` to switch the firmware
      2. Waits 2 seconds for the transition animation to settle
      3. Captures for ``duration_per_effect`` seconds
      4. Runs ``analyze_correlation()`` on the captured data

    Parameters
    ----------
    port : str
        Serial port path.
    effects : list[int]
        Effect IDs (e.g. ``[0x0100, 0x0200]``).
    duration_per_effect : float
        Seconds to capture per effect.
    fps : int
        Streaming FPS for capture.
    tap : str
        Capture tap point ('a', 'b', or 'c').
    stop_event : threading.Event, optional
        External stop signal.

    Returns
    -------
    list[tuple[int, dict, tuple | None]]
        (effect_id, metrics_dict, raw_data_tuple_or_None) per effect.
        raw_data_tuple is (frames, timestamps, metadata) when available.
    """
    try:
        import serial as pyserial
    except ImportError:
        print("ERROR: 'pyserial' required. pip install pyserial",
              file=sys.stderr)
        return []

    if stop_event is None:
        stop_event = threading.Event()

    print(f"[Gallery] Opening {port} (one-time, {len(effects)} effects)...")
    try:
        ser = pyserial.Serial(port, 115200, timeout=0.1,
                              dsrdtr=False, rtscts=False)
    except Exception as e:
        print(f"[Gallery] Failed to open {port}: {e}", file=sys.stderr)
        return []

    # Board reset stabilisation (USB CDC DTR/RTS toggle)
    time.sleep(3)
    ser.reset_input_buffer()

    results = []
    total = len(effects)

    try:
        for idx, eff_id in enumerate(effects):
            if stop_event.is_set():
                break

            hex_str = f'{eff_id:04X}'
            print(f"\n[Gallery] ({idx + 1}/{total}) Switching to effect "
                  f"0x{hex_str}...")

            # Send the effect-switch command
            ser.write(f'effect {hex_str}\n'.encode())
            time.sleep(0.1)
            # Drain any text response from the firmware
            if ser.in_waiting:
                ser.read(ser.in_waiting)

            # Wait for effect transition animation to settle
            print(f"[Gallery] Stabilising (2s)...")
            time.sleep(2)
            if stop_event.is_set():
                break

            # Capture
            print(f"[Gallery] Capturing 0x{hex_str} for "
                  f"{duration_per_effect:.0f}s...")
            cap = _capture_on_open_port(
                ser, duration_per_effect, fps, tap, stop_event)

            if cap is None:
                print(f"[Gallery] WARNING: No frames for 0x{hex_str}")
                metrics = {
                    'label': f'0x{hex_str}',
                    'n_frames': 0,
                    'error': 'no_frames',
                }
                results.append((eff_id, metrics, None))
                continue

            frames, timestamps, metadata = cap
            n = len(frames)
            print(f"[Gallery] 0x{hex_str}: {n} frames captured")

            metrics = analyze_correlation(
                frames, timestamps, metadata, label=f'0x{hex_str}')
            results.append((eff_id, metrics, (frames, timestamps, metadata)))

    finally:
        ser.close()

    print(f"\n[Gallery] Complete: {len(results)}/{total} effects captured.")
    return results


def format_gallery_report(results: list, gate_thresholds: dict = None) -> str:
    """Format a summary comparison table for all gallery effects.

    Parameters
    ----------
    results : list
        Output from ``run_effect_gallery()``.
    gate_thresholds : dict, optional
        If provided, run regression gate per-effect and include verdict.

    Returns
    -------
    str
        Multi-line formatted report.
    """
    if not results:
        return '\n  Effect Gallery Report\n  (no results)\n'

    lines = []
    lines.append('')
    lines.append('Effect Gallery Report')
    lines.append('\u2550' * 72)

    # Header row
    lines.append(
        f'  {"Effect":<8s}  {"Resp.":>6s}  {"RMS Corr":>9s}  '
        f'{"Bass Corr":>9s}  {"Spread":>7s}  {"Drops":>5s}  {"Verdict":<6s}'
    )
    lines.append('  ' + '\u2500' * 66)

    n_pass = 0
    n_warn = 0
    n_fail = 0
    best_eff = None
    best_resp = -1.0

    for eff_id, metrics, _raw in results:
        hex_str = f'0x{eff_id:04X}'

        if metrics.get('error'):
            lines.append(f'  {hex_str:<8s}  {"--":>6s}  {"--":>9s}  '
                         f'{"--":>9s}  {"--":>7s}  {"--":>5s}  SKIP')
            continue

        resp = metrics.get('responsiveness', 0.0)
        rms_c = metrics.get('rms_brightness_corr', 0.0)
        bass_c = metrics.get('bass_brightness_corr', 0.0)
        spread = metrics.get('mean_spatial_spread', 0.0)
        drops = metrics.get('frame_drops', 0)

        # Determine verdict
        if gate_thresholds is not None:
            gate_results = run_regression_gate(metrics, gate_thresholds)
            if gate_has_failures(gate_results):
                verdict = 'FAIL'
                n_fail += 1
            elif resp < 0.05 and abs(rms_c) < 0.1 and abs(bass_c) < 0.1:
                verdict = 'WARN'
                n_warn += 1
            else:
                verdict = 'PASS'
                n_pass += 1
        else:
            # Without gate, use audio-reactivity heuristics only
            if resp < 0.05 and abs(rms_c) < 0.1 and abs(bass_c) < 0.1:
                verdict = 'WARN'
                n_warn += 1
            else:
                verdict = 'PASS'
                n_pass += 1

        if resp > best_resp:
            best_resp = resp
            best_eff = hex_str

        lines.append(
            f'  {hex_str:<8s}  {resp:+.2f}   {rms_c:+.2f}      '
            f'{bass_c:+.2f}      {spread:>5.0%}   {drops:>5d}  {verdict}'
        )

    lines.append('  ' + '\u2500' * 66)

    total = n_pass + n_warn + n_fail
    lines.append(
        f'  Gallery: {total} effects, {n_pass} PASS, {n_warn} WARN, '
        f'{n_fail} FAIL'
    )
    if best_eff is not None:
        lines.append(
            f'  Best audio-reactive: {best_eff} '
            f'(responsiveness {best_resp:+.2f})'
        )

    lines.append('')
    return '\n'.join(lines)


def _build_gate_overrides(args) -> dict:
    """Extract gate threshold overrides from parsed CLI args."""
    overrides = {}
    if args.gate_drop_rate is not None:
        overrides['drop_rate'] = args.gate_drop_rate
    if args.gate_show_time is not None:
        overrides['show_time'] = args.gate_show_time
    if args.gate_heap_min is not None:
        overrides['heap_min'] = args.gate_heap_min
    if args.gate_heap_trend is not None:
        overrides['heap_trend'] = args.gate_heap_trend
    if args.gate_show_skips is not None:
        overrides['show_skips'] = args.gate_show_skips
    return overrides or None


def _parse_gallery_arg(value: str) -> list:
    """Parse --gallery argument: profile name or comma-separated hex IDs."""
    if value in GALLERY_PROFILES:
        return GALLERY_PROFILES[value]

    # Try parsing as comma-separated hex values
    parts = [p.strip() for p in value.split(',')]
    ids = []
    for p in parts:
        try:
            ids.append(int(p, 16))
        except ValueError:
            print(f"ERROR: Invalid effect ID '{p}'. "
                  f"Expected hex (e.g. 0x0100) or profile name "
                  f"({', '.join(GALLERY_PROFILES.keys())})",
                  file=sys.stderr)
            sys.exit(1)
    return ids


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Beat-Brightness Correlation Analyser for LightwaveOS',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # Device A
    parser.add_argument('--serial', metavar='PORT',
                        help='Serial port for primary device')
    # Device B (comparison)
    parser.add_argument('--compare-serial', metavar='PORT',
                        help='Serial port for comparison device')
    # Device C (third device)
    parser.add_argument('--serial-c', metavar='PORT',
                        help='Serial port for a third device')
    # Labels
    parser.add_argument('--label', default='Device A',
                        help='Label for primary device')
    parser.add_argument('--compare-label', default='Device B',
                        help='Label for comparison device')
    parser.add_argument('--label-c', default='Device C',
                        help='Label for the third device')

    # N-way multi-device
    parser.add_argument(
        '--devices', metavar='SPECS',
        help='Comma-separated port:label pairs for N-way comparison. '
             'e.g. "/dev/cu.usbmodem212401:Main HEAD,'
             '/dev/cu.usbmodem21401:Milestone,'
             '/dev/cu.usbmodem101:Baseline". '
             'Takes precedence over --serial/--compare-serial/--serial-c.')
    parser.add_argument(
        '--sequential', action='store_true',
        help='Capture sequentially from --serial, prompting between runs. '
             'Use with --devices to specify labels (port is ignored, '
             '--serial is used). Useful for N-way on a single physical device.')
    # Capture settings
    parser.add_argument('--duration', type=float, default=30.0,
                        help='Capture duration in seconds (default: 30)')
    parser.add_argument('--fps', type=int, default=15,
                        help='Streaming FPS (default: 15)')
    parser.add_argument('--tap', choices=['a', 'b', 'c'], default='b',
                        help='Capture tap point (default: b)')
    parser.add_argument('--retries', type=int, default=1, metavar='N',
                        help='Number of capture retries if 0 frames received (default: 1)')
    # Verbosity
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print per-frame metadata every 10 frames, '
                             'alignment diagnostics, and gate thresholds')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress all output except the final report/gate result')
    # Temporal alignment
    parser.add_argument('--align', action='store_true',
                        help='Cross-correlate RMS to time-align two captures '
                             '(use with --compare-serial or loaded comparison)')
    # Save/load
    parser.add_argument('--save', metavar='FILE',
                        help='Save raw session data (.npz)')
    parser.add_argument('--load', metavar='FILE',
                        help='Load previously saved session instead of live capture')
    # Dashboard
    parser.add_argument('--dashboard', metavar='FILE',
                        help='Save metrics dashboard as PNG')

    # Regression gate
    gate_group = parser.add_argument_group('regression gate')
    gate_group.add_argument('--gate', action='store_true',
                            help='Enable regression gate (exit 1 on failure)')
    gate_group.add_argument('--gate-drop-rate', type=float, default=None,
                            metavar='RATE',
                            help='Max frame drop rate (default: 0.01)')
    gate_group.add_argument('--gate-show-time', type=float, default=None,
                            metavar='US',
                            help='Max show time p99 in microseconds (default: 10000)')
    gate_group.add_argument('--gate-heap-min', type=int, default=None,
                            metavar='BYTES',
                            help='Min heap free bytes (default: 100000)')
    gate_group.add_argument('--gate-heap-trend', type=float, default=None,
                            metavar='BPF',
                            help='Min heap trend in bytes/frame (default: -100)')
    gate_group.add_argument('--gate-show-skips', type=int, default=None,
                            metavar='N',
                            help='Max show skips (default: 10)')

    # Effect gallery
    gallery_group = parser.add_argument_group('effect gallery')
    gallery_group.add_argument(
        '--gallery', metavar='PROFILE',
        help='Run effect gallery: profile name (quick, audio-reactive) '
             'or comma-separated hex IDs (e.g. 0x0100,0x0200,0x0500)')
    gallery_group.add_argument(
        '--gallery-duration', type=float, default=10.0, metavar='SEC',
        help='Seconds to capture per effect (default: 10)')
    gallery_group.add_argument(
        '--gallery-save', metavar='FILE',
        help='Save all per-effect capture data to .npz')

    args = parser.parse_args()

    # Set module-level verbosity flags.
    global _verbose, _quiet
    _verbose = getattr(args, 'verbose', False)
    _quiet = getattr(args, 'quiet', False)
    if _verbose and _quiet:
        parser.error("--verbose and --quiet are mutually exclusive")

    if _verbose and not _quiet:
        # Print the gate thresholds being used so the user can verify.
        print("[Verbose] Gate thresholds (default or overridden):")
        for k, (v, d) in DEFAULT_GATE_THRESHOLDS.items():
            print(f"  {k}: {v} ({d})")

    # ----- Gallery Mode (early return) -----
    if args.gallery:
        if not args.serial:
            parser.error("--gallery requires --serial")
        effects = _parse_gallery_arg(args.gallery)
        gallery_results = run_effect_gallery(
            args.serial, effects, args.gallery_duration,
            args.fps, args.tap)

        if not gallery_results:
            print("ERROR: Gallery produced no results", file=sys.stderr)
            sys.exit(1)

        # Per-effect detailed reports
        for eff_id, metrics, _raw in gallery_results:
            print(format_report(metrics))

        # Build gate overrides if --gate is active
        gate_overrides = _build_gate_overrides(args) if args.gate else None

        # Summary table
        print(format_gallery_report(gallery_results, gate_overrides))

        # Per-effect gate reports when --gate is active
        any_failure = False
        if args.gate:
            for eff_id, metrics, _raw in gallery_results:
                if not metrics.get('error'):
                    gate_r = run_regression_gate(metrics, gate_overrides)
                    print(format_gate_report(gate_r, metrics.get('label', '')))
                    if gate_has_failures(gate_r):
                        any_failure = True

        # Save gallery data if requested
        if args.gallery_save:
            save_dict = {}
            for i, (eff_id, metrics, raw) in enumerate(gallery_results):
                prefix = f'eff_{eff_id:04X}'
                save_dict[f'{prefix}_id'] = eff_id
                if raw is not None:
                    frames, timestamps, metadata = raw
                    save_dict[f'{prefix}_frames'] = frames
                    save_dict[f'{prefix}_timestamps'] = timestamps
                    save_dict[f'{prefix}_metadata'] = np.array(
                        metadata, dtype=object)
            np.savez_compressed(args.gallery_save, **save_dict)
            print(f"[Gallery] Saved: {args.gallery_save}")

        if any_failure:
            sys.exit(1)
        sys.exit(0)

    # ----- Resolve device list -----
    # Build a unified device_specs list: [(port, label), ...].
    # --devices takes precedence; otherwise assemble from individual args.
    device_specs = []

    if args.devices:
        device_specs = _parse_devices_arg(args.devices)
        if not device_specs:
            parser.error("--devices: no valid port:label pairs found")
    elif not args.load:
        if args.serial:
            device_specs.append((args.serial, args.label))
        if args.compare_serial:
            device_specs.append((args.compare_serial, args.compare_label))
        if args.serial_c:
            device_specs.append((args.serial_c, args.label_c))

    if not device_specs and not args.load:
        parser.error("Either --serial, --devices, or --load is required")

    # Determine whether to use the N-way (multi) path.
    # Multi path: --devices given, or 3+ individual ports, or --sequential.
    use_multi = (args.devices is not None
                 or len(device_specs) >= 3
                 or args.sequential)

    # =================================================================
    # N-WAY MULTI-DEVICE PATH
    # =================================================================
    if use_multi and not args.load:
        if args.sequential:
            # Sequential capture: use first port, labels from device_specs
            seq_port = device_specs[0][0] if device_specs else args.serial
            if not seq_port:
                parser.error("--sequential requires --serial or --devices")
            seq_labels = [label for (_port, label) in device_specs]
            if len(seq_labels) < 2:
                parser.error("--sequential requires at least 2 labels "
                             "(use --devices or --serial + --compare-serial)")
            captures = capture_sequential_devices(
                seq_port, seq_labels, args.duration, args.fps, args.tap)
        else:
            captures = capture_multiple_devices(
                device_specs, args.duration, args.fps, args.tap)

        # Analyse each device
        all_metrics = []
        for label, frames, timestamps, metadata in captures:
            if frames is None:
                all_metrics.append({
                    'label': label, 'n_frames': 0,
                    'error': 'capture_failed',
                })
                continue
            m = analyze_correlation(frames, timestamps, metadata, label)
            all_metrics.append(m)
            print(format_report(m))

        valid_metrics = [m for m in all_metrics if not m.get('error')]
        if not valid_metrics:
            print("ERROR: No devices produced valid captures",
                  file=sys.stderr)
            sys.exit(1)

        # Pairwise alignment note (raw data not retained post-analysis)
        if args.align and len(valid_metrics) >= 2:
            print("[Align] NOTE: Pairwise alignment in N-way mode is not "
                  "yet supported. Re-run with 2-device mode for post-hoc "
                  "alignment if needed.")

        # N-way comparison table
        if len(valid_metrics) >= 2:
            print(format_multi_comparison(all_metrics))

        # Dashboard
        if args.dashboard:
            dashboard = render_multi_dashboard(all_metrics)
            if dashboard:
                dashboard.save(args.dashboard, 'PNG')
                print(f"[Dashboard] Saved: {args.dashboard} "
                      f"({dashboard.width}x{dashboard.height})")
            else:
                print("[Dashboard] Could not render (insufficient data)")

        # Regression gate on each device independently
        if args.gate:
            gate_overrides = _build_gate_overrides(args)
            any_failure = False

            for m in all_metrics:
                if not m.get('error'):
                    gate_r = run_regression_gate(m, gate_overrides)
                    print(format_gate_report(gate_r, m.get('label', '')))
                    if gate_has_failures(gate_r):
                        any_failure = True

            if any_failure:
                sys.exit(1)

        sys.exit(0)

    # =================================================================
    # CLASSIC 1- OR 2-DEVICE PATH (backwards compatible)
    # =================================================================
    if not args.serial and not args.load:
        parser.error("Either --serial or --load is required")

    frames_b = None

    if args.load:
        print(f"Loading session from {args.load}...")
        session = load_session(args.load)
        frames_a = session['frames_a']
        timestamps_a = session['timestamps_a']
        metadata_a = session['metadata_a']
        label_a = session.get('label_a', args.label)

        if 'frames_b' in session:
            frames_b = session['frames_b']
            timestamps_b = session['timestamps_b']
            metadata_b = session['metadata_b']
            label_b = session.get('label_b', args.compare_label)
        else:
            label_b = args.compare_label
    else:
        label_a = args.label
        label_b = args.compare_label

        if args.compare_serial:
            # Parallel capture from both devices
            stop_a = threading.Event()
            stop_b = threading.Event()
            result_a = [None]
            result_b = [None]

            def cap_a():
                result_a[0] = capture_with_metadata(
                    args.serial, args.duration, args.fps, args.tap, stop_a)

            def cap_b():
                result_b[0] = capture_with_metadata(
                    args.compare_serial, args.duration, args.fps, args.tap,
                    stop_b)

            ta = threading.Thread(target=cap_a, daemon=True)
            tb = threading.Thread(target=cap_b, daemon=True)
            ta.start()
            tb.start()
            ta.join(timeout=args.duration + 20)
            tb.join(timeout=args.duration + 20)
            stop_a.set()
            stop_b.set()

            if result_a[0] is None:
                print("ERROR: No frames from primary device", file=sys.stderr)
                sys.exit(1)

            frames_a, timestamps_a, metadata_a = result_a[0]

            if result_b[0] is not None:
                frames_b, timestamps_b, metadata_b = result_b[0]
            else:
                print("WARNING: No frames from comparison device",
                      file=sys.stderr)
        else:
            # Single-device capture with retry support.
            max_attempts = max(1, getattr(args, 'retries', 1))
            result = None
            for attempt in range(max_attempts):
                result = capture_with_metadata(
                    args.serial, args.duration, args.fps, args.tap)
                if result is not None:
                    break
                if attempt < max_attempts - 1:
                    print(f"[Capture] WARNING: 0 frames on attempt "
                          f"{attempt + 1}/{max_attempts}, retrying...",
                          file=sys.stderr)
                    time.sleep(1)
            if result is None:
                print("ERROR: No frames captured after "
                      f"{max_attempts} attempt(s)", file=sys.stderr)
                sys.exit(1)
            frames_a, timestamps_a, metadata_a = result

        # Save if requested
        if args.save:
            save_session(
                args.save, frames_a, timestamps_a, metadata_a,
                frames_b if frames_b is not None else None,
                timestamps_b if frames_b is not None else None,
                metadata_b if frames_b is not None else None,
                label_a, label_b,
            )

    # ----- Temporal Alignment -----
    alignment_info = None
    if args.align and frames_b is not None:
        align_result = align_captures(
            metadata_a, metadata_b, timestamps_a, timestamps_b)

        if align_result is None:
            print("[Align] WARNING: Cannot align — one or both devices "
                  "have no RMS data (silent capture?).")
        else:
            offset_frames, corr_score, offset_secs = align_result

            sign = '+' if offset_frames >= 0 else ''
            if not _quiet:
                print(f"[Align] Offset: {sign}{offset_frames} frames "
                      f"({offset_secs:.2f}s), correlation: {corr_score:.2f}")

            if _verbose:
                rms_a_arr = np.array([m.get('rms', 0.0) for m in metadata_a])
                rms_b_arr = np.array([m.get('rms', 0.0) for m in metadata_b])
                print(f"  [v] RMS A: mean={rms_a_arr.mean():.4f} "
                      f"std={rms_a_arr.std():.4f} "
                      f"len={len(rms_a_arr)}")
                print(f"  [v] RMS B: mean={rms_b_arr.mean():.4f} "
                      f"std={rms_b_arr.std():.4f} "
                      f"len={len(rms_b_arr)}")
                print(f"  [v] Search window: +/- {_MAX_ALIGN_OFFSET} frames")

            if corr_score < 0.3:
                print(f"[Align] WARNING: Low correlation ({corr_score:.2f}). "
                      f"Different audio environments?")

            if offset_frames != 0:
                (frames_a, timestamps_a, metadata_a,
                 frames_b, timestamps_b, metadata_b) = apply_alignment(
                    frames_a, timestamps_a, metadata_a,
                    frames_b, timestamps_b, metadata_b,
                    offset_frames)
                print(f"[Align] Trimmed to {len(metadata_a)} overlapping "
                      f"frames.")

                if len(metadata_a) < 10:
                    print("[Align] ERROR: Too few overlapping frames after "
                          "alignment.", file=sys.stderr)
                    sys.exit(1)

            alignment_info = {
                'offset_frames': offset_frames,
                'correlation': corr_score,
                'offset_seconds': offset_secs,
            }

    # ----- Analysis -----
    metrics_a = analyze_correlation(
        frames_a, timestamps_a, metadata_a, label_a)
    print(format_report(metrics_a))

    metrics_b_result = None
    if frames_b is not None:
        metrics_b_result = analyze_correlation(
            frames_b, timestamps_b, metadata_b, label_b)
        print(format_report(metrics_b_result))

        # Inject alignment header if alignment was applied.
        comparison_text = format_comparison(metrics_a, metrics_b_result)
        if alignment_info is not None:
            ai = alignment_info
            sign = '+' if ai['offset_frames'] >= 0 else ''
            align_header = (
                f"  [Aligned] Offset: {sign}{ai['offset_frames']} frames "
                f"({ai['offset_seconds']:.2f}s), "
                f"correlation: {ai['correlation']:.2f}\n"
            )
            clines = comparison_text.split('\n')
            insert_idx = None
            for i, line in enumerate(clines):
                if line.strip().startswith('COMPARISON:'):
                    insert_idx = i + 1
                    break
            if insert_idx is not None:
                clines.insert(insert_idx, align_header)
            comparison_text = '\n'.join(clines)

        print(comparison_text)

    # ----- Dashboard -----
    if args.dashboard:
        if metrics_b_result is not None:
            dashboard = render_comparison_dashboard(
                metrics_a, metrics_b_result)
        else:
            dashboard = render_dashboard(metrics_a)

        if dashboard:
            dashboard.save(args.dashboard, 'PNG')
            print(f"[Dashboard] Saved: {args.dashboard} "
                  f"({dashboard.width}x{dashboard.height})")
        else:
            print("[Dashboard] Could not render (insufficient data)")

    # ----- Regression Gate -----
    if args.gate:
        gate_overrides = _build_gate_overrides(args)
        any_failure = False

        # Gate for device A
        if not metrics_a.get('error'):
            gate_a = run_regression_gate(metrics_a, gate_overrides)
            print(format_gate_report(gate_a, metrics_a.get('label', '')))
            if gate_has_failures(gate_a):
                any_failure = True

        # Gate for device B (if comparison mode)
        if metrics_b_result is not None and not metrics_b_result.get('error'):
            gate_b = run_regression_gate(
                metrics_b_result, gate_overrides)
            print(format_gate_report(
                gate_b, metrics_b_result.get('label', '')))
            if gate_has_failures(gate_b):
                any_failure = True

            # Comparative gate (advisory, not a hard fail)
            print(format_comparison_gate(metrics_a, metrics_b_result))

        if any_failure:
            sys.exit(1)


if __name__ == '__main__':
    main()
