#!/usr/bin/env python3
"""
FPS Sweep Test for LED Capture Pipeline

Sweeps through multiple FPS targets to find the "clean max" — the highest
FPS where frame drops stay low and show times stay healthy.

Usage:
    python tools/led_capture/fps_sweep.py \
        --serial /dev/cu.usbmodem21401 \
        --fps-targets 15,20,25,30,40,50,60 \
        --duration 30

    # Custom settle time and output file
    python tools/led_capture/fps_sweep.py \
        --serial /dev/cu.usbmodem21401 \
        --duration 20 --settle 5 \
        --output /tmp/fps_sweep_report.txt

    # Different tap point
    python tools/led_capture/fps_sweep.py \
        --serial /dev/cu.usbmodem21401 --tap a
"""

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

# Import capture and analysis functions from the sibling module.
sys.path.insert(0, str(Path(__file__).parent))
from analyze_beats import capture_with_metadata, analyze_correlation


def find_knee(runs: list) -> int | None:
    """Find highest clean FPS before quality degrades.

    Walks the sorted list of successful runs and looks for the point where
    either the drop rate jumps by more than 5x compared to the previous
    target, or the p99 show time exceeds 10 ms.  Returns the last clean
    FPS target, or None if no runs passed.
    """
    clean_runs = [r for r in runs if r['status'] == 'PASS']
    if not clean_runs:
        return None

    for i in range(1, len(clean_runs)):
        prev, curr = clean_runs[i - 1], clean_runs[i]
        drop_jump = curr['drop_rate'] / max(0.001, prev['drop_rate'])
        show_exceeded = curr['show_time_p99_us'] > 10000  # >10 ms
        if drop_jump > 5.0 or show_exceeded:
            return prev['fps_target']

    # All targets were clean — return the highest.
    return clean_runs[-1]['fps_target']


def format_table(runs: list, port: str, fmt: str = 'v2') -> str:
    """Build the ASCII summary table from completed runs."""
    now = datetime.now().strftime('%Y-%m-%d %H:%M')
    fmt_sizes = {'v1': 977, 'v2': 1009, 'meta': 49, 'slim': 529}
    fmt_size = fmt_sizes.get(fmt, '?')
    lines = []
    lines.append(f"FPS Sweep Results -- {port} -- {now} -- format: {fmt} ({fmt_size}B)")
    lines.append('=' * 80)
    lines.append(
        ' Target | Actual | Frames | Drops | Drop%  '
        '| Show p99 | Heap D  | Status'
    )
    lines.append(
        '--------|--------|--------|-------|--------'
        '|----------|---------|-------'
    )

    for r in runs:
        if r['status'] == 'FAIL':
            reason = r.get('reason', 'unknown')
            lines.append(
                f"  {r['fps_target']:>3}   |   --   |   --   |  --   |   --   "
                f"|    --    |   --    |  FAIL ({reason})"
            )
            continue

        actual = r.get('actual_fps', 0)
        n_frames = r.get('n_frames', 0)
        drops = r.get('frame_drops', 0)
        drop_pct = r.get('drop_rate', 0) * 100.0
        show_p99 = r.get('show_time_p99_us', 0) / 1000.0  # us -> ms
        heap_trend = r.get('heap_trend', 0)

        # Format heap trend as bytes/frame with sign.
        if heap_trend == 0:
            heap_str = '   0B/f'
        else:
            heap_str = f'{heap_trend:+.0f}B/f'

        lines.append(
            f"  {r['fps_target']:>3}   | {actual:>5.1f}  | {n_frames:>6} "
            f"| {drops:>5} | {drop_pct:>5.2f}% "
            f"| {show_p99:>6.1f}ms | {heap_str:>7} |  {r['status']}"
        )

    lines.append('=' * 80)

    # Knee detection and recommendation.
    knee = find_knee(runs)
    if knee is not None:
        recommended = int(knee * 0.8)
        # Snap recommendation to the nearest tested target that is <= 80%.
        tested = sorted(r['fps_target'] for r in runs if r['status'] == 'PASS')
        snapped = [t for t in tested if t <= recommended]
        if snapped:
            recommended = snapped[-1]
        else:
            recommended = tested[0] if tested else knee

        lines.append(
            f"Clean max: {knee} FPS | "
            f"Recommended: {recommended} FPS (80% safety margin)"
        )
    else:
        lines.append("Clean max: NONE -- all targets failed or degraded")

    return '\n'.join(lines)


def run_sweep(port: str, fps_targets: list[int], duration: float,
              settle: float, tap: str, fmt: str = 'v2') -> list[dict]:
    """Execute the FPS sweep, returning a list of run result dicts."""
    n = len(fps_targets)
    # Estimate total time: each run = duration + ~3s board reset + settle.
    boot_time = 3.0
    est_total = n * (duration + boot_time + settle)
    est_min = est_total / 60.0

    fmt_sizes = {'v1': 977, 'v2': 1009, 'meta': 49, 'slim': 529}
    fmt_size = fmt_sizes.get(fmt, '?')

    print(f"FPS Sweep: {n} targets {fps_targets}")
    print(f"Duration per target: {duration}s | Settle: {settle}s | Tap: {tap} | Format: {fmt} ({fmt_size}B)")
    print(f"Estimated total time: {est_total:.0f}s ({est_min:.1f} min)")
    print(f"Serial port: {port}")

    runs = []
    sweep_start = time.monotonic()

    for idx, fps in enumerate(fps_targets):
        run_start = time.monotonic()
        elapsed_total = run_start - sweep_start

        print(f"\n{'=' * 60}")
        print(
            f"[{idx + 1}/{n}] Testing {fps} FPS "
            f"({duration}s capture)... "
            f"[elapsed: {elapsed_total:.0f}s]"
        )
        print('=' * 60)

        result = capture_with_metadata(port, duration, fps, tap, fmt=fmt)

        if result is None:
            print(f"  FAILED -- no frames captured at {fps} FPS")
            runs.append({
                'fps_target': fps,
                'status': 'FAIL',
                'reason': 'no_capture',
            })
            if settle > 0 and idx < n - 1:
                print(f"  Settling {settle}s before next target...")
                time.sleep(settle)
            continue

        frames, timestamps, metadata = result
        run_elapsed = time.monotonic() - run_start
        print(f"  Captured {len(frames)} frames in {run_elapsed:.1f}s, analysing...")

        metrics = analyze_correlation(frames, timestamps, metadata, f"FPS={fps}")

        run = {
            'fps_target': fps,
            'actual_fps': metrics.get('actual_fps', 0),
            'n_frames': metrics.get('n_frames', 0),
            'frame_drops': metrics.get('frame_drops', 0),
            'drop_rate': metrics.get('drop_rate', 0),
            'show_time_p99_us': metrics.get('show_time_p99_us', 0),
            'heap_min': metrics.get('heap_min', 0),
            'heap_trend': metrics.get('heap_trend', 0),
            'composite_score': metrics.get('composite_audio_visual_score', 0),
            'status': 'PASS',
        }
        runs.append(run)

        # Brief summary for this target.
        drop_pct = run['drop_rate'] * 100.0
        show_ms = run['show_time_p99_us'] / 1000.0
        print(
            f"  Result: {run['actual_fps']:.1f} actual FPS, "
            f"{run['frame_drops']} drops ({drop_pct:.2f}%), "
            f"show p99 {show_ms:.1f}ms"
        )

        # Settle between runs (skip after the last one).
        if settle > 0 and idx < n - 1:
            print(f"  Settling {settle}s before next target...")
            time.sleep(settle)

    total_elapsed = time.monotonic() - sweep_start
    print(f"\nSweep complete in {total_elapsed:.0f}s ({total_elapsed / 60.0:.1f} min)")

    return runs


def main():
    parser = argparse.ArgumentParser(
        description='FPS sweep test for the LED capture pipeline'
    )
    parser.add_argument(
        '--serial', required=True,
        help='Serial port for the device (e.g. /dev/cu.usbmodem21401)'
    )
    parser.add_argument(
        '--fps-targets', default='15,20,25,30,40,50,60',
        help='Comma-separated FPS targets to sweep (default: 15,20,25,30,40,50,60)'
    )
    parser.add_argument(
        '--duration', type=float, default=30,
        help='Capture duration per FPS target in seconds (default: 30)'
    )
    parser.add_argument(
        '--settle', type=float, default=3,
        help='Extra settle time between runs beyond the 3s boot reset (default: 3)'
    )
    parser.add_argument(
        '--output', default=None,
        help='Write report to this file (also prints to stdout)'
    )
    parser.add_argument(
        '--tap', choices=['a', 'b', 'c'], default='b',
        help='Capture tap point (default: b)'
    )
    parser.add_argument(
        '--format', choices=['v1', 'v2', 'meta', 'slim'], default='v2',
        dest='fmt',
        help='Frame format: v2 (1009B), meta (49B, no RGB), slim (529B, half-res) (default: v2)'
    )

    args = parser.parse_args()

    # Parse FPS targets.
    try:
        fps_targets = [int(x.strip()) for x in args.fps_targets.split(',')]
    except ValueError:
        print(f"ERROR: invalid --fps-targets value: {args.fps_targets!r}",
              file=sys.stderr)
        sys.exit(1)

    if not fps_targets:
        print("ERROR: no FPS targets specified", file=sys.stderr)
        sys.exit(1)

    # Run the sweep.
    runs = run_sweep(
        port=args.serial,
        fps_targets=fps_targets,
        duration=args.duration,
        settle=args.settle,
        tap=args.tap,
        fmt=args.fmt,
    )

    # Build and display the summary table.
    report = format_table(runs, args.serial, args.fmt)
    print(f"\n{report}")

    # Optionally write to file.
    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(report + '\n', encoding='utf-8')
        print(f"\nReport written to {out_path}")


if __name__ == '__main__':
    main()
