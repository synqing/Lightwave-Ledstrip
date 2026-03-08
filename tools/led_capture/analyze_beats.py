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

    # Comparison with regression gate on both devices
    python analyze_beats.py \
        --serial /dev/cu.usbmodem212401 --label "Main HEAD" \
        --compare-serial /dev/cu.usbmodem21401 --compare-label "Milestone" \
        --duration 30 --gate

Output:
    A text report with:
    - Beat responsiveness score (0-1): how reliably beats cause brightness spikes
    - RMS-brightness correlation: overall audio-reactive coupling strength
    - Beat timing and tempo estimation
    - Frame health (drops, show time, heap stability)
    - Optional regression gate pass/fail table (with --gate)
"""

import argparse
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
                    if count % 50 == 0:
                        fps_actual = count / (time.time() - start_time)
                        print(f"[Capture] {count} frames ({elapsed:.1f}s, {fps_actual:.1f} FPS)")
    finally:
        ser.write(b'capture stop\n')
        time.sleep(0.2)
        ser.close()

    total_time = time.time() - start_time
    actual_fps = len(frames) / max(0.1, total_time)
    beats = sum(1 for m in metadata_list if m.get('beat'))
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


def analyze_correlation(frames: np.ndarray, timestamps: np.ndarray,
                        metadata: list, label: str = "Device") -> dict:
    """Compute beat-brightness correlation metrics.

    Returns a dict with scalar metrics and private time-series arrays
    (prefixed with '_') for dashboard rendering.
    """
    n = len(metadata)
    if n < 10:
        print(f"[{label}] Too few frames ({n}) for meaningful analysis.")
        return {'label': label, 'n_frames': n, 'error': 'insufficient_frames'}

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

    duration = timestamps[-1] - timestamps[0]
    actual_fps = n / max(0.1, duration)

    # --- Beat statistics ---
    n_beats = int(beats.sum())
    n_onsets = int(onset.sum())
    beat_hz = n_beats / max(0.1, duration)
    bpm = beat_hz * 60

    # Inter-beat intervals for tempo stability
    beat_idxs = np.where(beats)[0]
    if len(beat_idxs) >= 3:
        ibi_frames = np.diff(beat_idxs)
        ibi_seconds = ibi_frames / actual_fps
        tempo_stability = 1.0 - min(1.0, float(np.std(ibi_seconds) / max(0.01, np.mean(ibi_seconds))))
    else:
        tempo_stability = 0.0

    # --- RMS-Brightness Pearson correlation ---
    if rms.std() > 1e-6 and brightness.std() > 1e-6:
        rms_corr = float(np.corrcoef(rms, brightness)[0, 1])
    else:
        rms_corr = 0.0

    # --- Bass-Brightness correlation ---
    # Many effects respond to bands[0] (sub-bass) directly, not the beat flag.
    bass = bands[:, 0] if bands.shape[1] > 0 else np.zeros(n)
    if bass.std() > 1e-6 and brightness.std() > 1e-6:
        bass_corr = float(np.corrcoef(bass, brightness)[0, 1])
    else:
        bass_corr = 0.0

    # Best audio correlation: whichever band correlates most with brightness
    best_band_corr = 0.0
    best_band_idx = 0
    if brightness.std() > 1e-6:
        for bi in range(min(8, bands.shape[1])):
            band_data = bands[:, bi]
            if band_data.std() > 1e-6:
                c = float(np.corrcoef(band_data, brightness)[0, 1])
                if abs(c) > abs(best_band_corr):
                    best_band_corr = c
                    best_band_idx = bi

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
        mean_bright_beat = float(brightness[beats].mean())
        mean_bright_nobeat = float(brightness[~beats].mean()) if (~beats).sum() > 0 else 0.0
        contrast_ratio = mean_bright_beat / max(0.001, mean_bright_nobeat)

        # Beat-aligned response magnitude:
        # For each beat, measure peak brightness in a 3-frame window after
        # minus the baseline brightness in 2 frames before.
        beat_responses = []
        for bi in beat_idxs:
            pre_start = max(0, bi - 2)
            post_end = min(n, bi + 4)
            pre_level = float(brightness[pre_start:bi].mean()) if bi > 0 else brightness[0]
            post_peak = float(brightness[bi:post_end].max())
            beat_responses.append(post_peak - pre_level)
        mean_beat_response = float(np.mean(beat_responses))

        # Spatial spread on-beat vs off-beat (bloom detection)
        mean_spread_beat = float(spatial_spread[beats].mean())
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

    show_valid = show_times[show_times > 0]
    heap_valid = heap[heap > 0]

    skip_vals = [m.get('show_skips', 0) for m in metadata]
    show_skips_total = max(skip_vals) - min(skip_vals) if skip_vals else 0

    # Heap trend (bytes per frame, positive = growing)
    if len(heap_valid) > 10:
        heap_trend = float(np.polyfit(range(len(heap_valid)), heap_valid, 1)[0])
    else:
        heap_trend = 0.0

    return {
        'label': label,
        'n_frames': n,
        'duration': duration,
        'actual_fps': actual_fps,
        # Beat stats
        'n_beats': n_beats,
        'n_onsets': n_onsets,
        'beat_hz': beat_hz,
        'bpm': bpm,
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
    }


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
    """Format analysis metrics as a human-readable text report."""
    if m.get('error'):
        return f"\n  [{m['label']}] Analysis failed: {m['error']}\n"

    lines = []
    lines.append(f"")
    lines.append(f"{'=' * 60}")
    lines.append(f"  Beat-Brightness Correlation: {m['label']}")
    lines.append(f"{'=' * 60}")
    lines.append(f"")
    lines.append(f"  Capture:  {m['n_frames']} frames, {m['actual_fps']:.1f} FPS, {m['duration']:.1f}s")
    lines.append(f"  Beats:    {m['n_beats']} detected ({m['beat_hz']:.1f} Hz = {m['bpm']:.0f} BPM)")
    if m.get('beat_warning') == 'saturated':
        lines.append(f"  WARNING:  Beat flag saturated ({m['n_beats']}/{m['n_frames']} frames)")
        lines.append(f"            Beat detection may be always-on or threshold too low.")
    elif m.get('beat_warning') == 'absent':
        lines.append(f"  WARNING:  No beats detected. Audio input silent or detector offline?")
    lines.append(f"  Onsets:   {m['n_onsets']} (snare/hihat)")
    if m['tempo_stability'] > 0:
        stab_pct = m['tempo_stability'] * 100
        lines.append(f"  Tempo:    {stab_pct:.0f}% stable")
    lines.append(f"")

    # Score card
    resp = m['responsiveness']
    resp_rating = _rating(resp, [(0.5, 'EXCELLENT'), (0.3, 'good'), (0.1, 'weak'), (0.0, 'none')])

    rms_corr = m['rms_brightness_corr']
    corr_rating = _rating(rms_corr, [(0.7, 'STRONG'), (0.4, 'moderate'), (0.1, 'weak'), (-1.0, 'none')])

    lines.append(f"  Audio-Reactive Quality")
    lines.append(f"  {'~' * 44}")
    band_names = ['Sub', 'Bass', 'Low-Mid', 'Mid', 'Upper-Mid', 'Presence', 'Brilliance', 'Air']
    best_bi = m.get('best_band_idx', 0)
    best_bn = band_names[best_bi] if best_bi < len(band_names) else f'band{best_bi}'

    bass_corr = m.get('bass_brightness_corr', 0)
    bass_rating = _rating(abs(bass_corr), [(0.7, 'STRONG'), (0.4, 'moderate'), (0.1, 'weak'), (0.0, 'none')])

    lines.append(f"  Responsiveness score:    {resp:+.3f}  ({resp_rating})")
    lines.append(f"  RMS-brightness corr:     {rms_corr:+.3f}  ({corr_rating})")
    lines.append(f"  Bass-brightness corr:    {bass_corr:+.3f}  ({bass_rating})")
    lines.append(f"  Best band correlation:   {m.get('best_band_corr', 0):+.3f}  ({best_bn})")
    lines.append(f"  Beat trigger ratio:      {m['beat_trigger_ratio']:.0%}  "
                 f"(beats causing brightness spike)")
    lines.append(f"  Non-beat spike rate:     {m['nonbeat_spike_rate']:.0%}  "
                 f"(background noise)")
    lines.append(f"  Mean beat response:      {m['mean_beat_response']:+.4f}")
    lines.append(f"")
    lines.append(f"  Brightness on beat:      {m['mean_bright_beat']:.4f}")
    lines.append(f"  Brightness off beat:     {m['mean_bright_nobeat']:.4f}")
    lines.append(f"  Contrast ratio:          {m['contrast_ratio']:.2f}x")
    lines.append(f"  Mean brightness:         {m['mean_brightness']:.4f}")
    spread = m.get('mean_spatial_spread', 0)
    spread_line = f"  Spatial spread:          {spread:.1%} of strip active"
    if m.get('n_beats', 0) >= 3:
        spread_line += (f"  (beat: {m.get('mean_spread_beat', 0):.1%}"
                        f" / off: {m.get('mean_spread_nobeat', 0):.1%})")
    lines.append(spread_line)
    lines.append(f"")

    # Health
    lines.append(f"  Frame Health")
    lines.append(f"  {'~' * 44}")
    sub = m.get('subsampling_ratio', 1)
    lines.append(f"  Subsampling:      1:{sub:.0f}  (renderer FPS / capture FPS)")
    lines.append(f"  Frame drops:      {m['frame_drops']}  "
                 f"({m['total_missing_frames']} missed streaming slots)")
    lines.append(f"  Drop rate:        {m['drop_rate']:.4f}")
    lines.append(f"  Show skips:       {m['show_skips_total']}")

    if m['show_time_median_us'] > 0:
        lines.append(f"  Show time:        median {m['show_time_median_us'] / 1000:.1f}ms, "
                     f"p99 {m['show_time_p99_us'] / 1000:.1f}ms")

    if m['heap_min'] > 0:
        trend = 'rising' if m['heap_trend'] > 10 else ('falling' if m['heap_trend'] < -10 else 'stable')
        lines.append(f"  Heap:             {m['heap_min']:,} - {m['heap_max']:,} bytes ({trend})")

    lines.append(f"")
    return '\n'.join(lines)


def format_comparison(ma: dict, mb: dict) -> str:
    """Format a side-by-side comparison table of two devices."""
    if ma.get('error') or mb.get('error'):
        return "  Cannot compare: one or both analyses failed.\n"

    lines = []
    lines.append(f"")
    lines.append(f"{'=' * 62}")
    lines.append(f"  COMPARISON: {ma['label']}  vs  {mb['label']}")
    lines.append(f"{'=' * 62}")
    lines.append(f"")

    hdr_a = ma['label'][:12]
    hdr_b = mb['label'][:12]
    lines.append(f"  {'Metric':<30s}  {hdr_a:>10s}       {hdr_b:>10s}")
    lines.append(f"  {'~' * 58}")

    def row(label, key, fmt='.3f', higher_better=True):
        va = ma.get(key, 0)
        vb = mb.get(key, 0)

        if fmt.endswith('%'):
            sa = f"{va:.0%}"
            sb = f"{vb:.0%}"
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
            sa = f"{va:{fmt}}"
            sb = f"{vb:{fmt}}"
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
    best_corr_a = max(max(0, ma['rms_brightness_corr']),
                      max(0, ma.get('bass_brightness_corr', 0)),
                      max(0, ma.get('best_band_corr', 0)))
    best_corr_b = max(max(0, mb['rms_brightness_corr']),
                      max(0, mb.get('bass_brightness_corr', 0)),
                      max(0, mb.get('best_band_corr', 0)))

    w_resp, w_corr, w_contrast = 0.4, 0.4, 0.2
    score_a = (ma['responsiveness'] * w_resp
               + best_corr_a * w_corr
               + min(1, ma['contrast_ratio'] / 3) * w_contrast)
    score_b = (mb['responsiveness'] * w_resp
               + best_corr_b * w_corr
               + min(1, mb['contrast_ratio'] / 3) * w_contrast)

    if abs(score_a - score_b) < 0.02:
        verdict = "TIE — no significant difference"
    elif score_a > score_b:
        verdict = f"{ma['label']} wins  (composite {score_a:.3f} vs {score_b:.3f})"
    else:
        verdict = f"{mb['label']} wins  (composite {score_b:.3f} vs {score_a:.3f})"

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
    rows = ['Brightness', 'RMS Audio', 'Bands [8]', 'Show Time', 'Heap']
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
# Save / Load
# ---------------------------------------------------------------------------

def save_session(path: str, frames_a, ts_a, meta_a,
                 frames_b=None, ts_b=None, meta_b=None,
                 label_a='Device A', label_b='Device B'):
    """Save capture session to compressed .npz file."""
    d = {
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
    print(f"[Save] Session data: {path}")


def load_session(path: str):
    """Load saved session. Returns dict with keys frames_a, timestamps_a, etc."""
    data = np.load(path, allow_pickle=True)
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
    # Labels
    parser.add_argument('--label', default='Device A',
                        help='Label for primary device')
    parser.add_argument('--compare-label', default='Device B',
                        help='Label for comparison device')
    # Capture settings
    parser.add_argument('--duration', type=float, default=30.0,
                        help='Capture duration in seconds (default: 30)')
    parser.add_argument('--fps', type=int, default=15,
                        help='Streaming FPS (default: 15)')
    parser.add_argument('--tap', choices=['a', 'b', 'c'], default='b',
                        help='Capture tap point (default: b)')
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

    args = parser.parse_args()

    if not args.serial and not args.load:
        parser.error("Either --serial or --load is required")

    # ----- Capture or Load -----
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
                    args.compare_serial, args.duration, args.fps, args.tap, stop_b)

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
                print("WARNING: No frames from comparison device", file=sys.stderr)
        else:
            result = capture_with_metadata(
                args.serial, args.duration, args.fps, args.tap)
            if result is None:
                print("ERROR: No frames captured", file=sys.stderr)
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

    # ----- Analysis -----
    metrics_a = analyze_correlation(frames_a, timestamps_a, metadata_a, label_a)
    print(format_report(metrics_a))

    metrics_b_result = None
    if frames_b is not None:
        metrics_b_result = analyze_correlation(frames_b, timestamps_b, metadata_b, label_b)
        print(format_report(metrics_b_result))
        print(format_comparison(metrics_a, metrics_b_result))

    # ----- Dashboard -----
    if args.dashboard:
        if metrics_b_result is not None:
            dashboard = render_comparison_dashboard(metrics_a, metrics_b_result)
        else:
            dashboard = render_dashboard(metrics_a)

        if dashboard:
            dashboard.save(args.dashboard, 'PNG')
            print(f"[Dashboard] Saved: {args.dashboard} ({dashboard.width}x{dashboard.height})")
        else:
            print("[Dashboard] Could not render (insufficient data)")

    # ----- Regression Gate -----
    if args.gate:
        # Build threshold overrides from CLI flags
        gate_overrides = {}
        if args.gate_drop_rate is not None:
            gate_overrides['drop_rate'] = args.gate_drop_rate
        if args.gate_show_time is not None:
            gate_overrides['show_time'] = args.gate_show_time
        if args.gate_heap_min is not None:
            gate_overrides['heap_min'] = args.gate_heap_min
        if args.gate_heap_trend is not None:
            gate_overrides['heap_trend'] = args.gate_heap_trend
        if args.gate_show_skips is not None:
            gate_overrides['show_skips'] = args.gate_show_skips

        any_failure = False

        # Gate for device A
        if not metrics_a.get('error'):
            gate_a = run_regression_gate(metrics_a, gate_overrides or None)
            print(format_gate_report(gate_a, metrics_a.get('label', '')))
            if gate_has_failures(gate_a):
                any_failure = True

        # Gate for device B (if comparison mode)
        if metrics_b_result is not None and not metrics_b_result.get('error'):
            gate_b = run_regression_gate(metrics_b_result, gate_overrides or None)
            print(format_gate_report(gate_b, metrics_b_result.get('label', '')))
            if gate_has_failures(gate_b):
                any_failure = True

            # Comparative gate (advisory, not a hard fail)
            print(format_comparison_gate(metrics_a, metrics_b_result))

        if any_failure:
            sys.exit(1)


if __name__ == '__main__':
    main()
