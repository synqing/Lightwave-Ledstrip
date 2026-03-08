#!/usr/bin/env python3
"""
Capture Test Suite Runner

Runs standardised capture profiles with pass/fail gates.
See docs/CAPTURE_TEST_SUITES.md for the full framework.

Usage:
    # Reference suite (default — everyday truth check)
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401

    # Stress suite (before merge / milestone)
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401 --suite stress

    # Multiple suites
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401 --suite reference,stress

    # Isolation suite (A/B/C tap sweep)
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401 --suite isolation

    # Soak suite (long-run stability)
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401 --suite soak

    # Custom output directory
    python tools/led_capture/capture_suite.py --serial /dev/cu.usbmodem21401 --output /tmp/suite_run
"""

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

# Import capture and analysis functions from sibling modules.
sys.path.insert(0, str(Path(__file__).parent))
from analyze_beats import capture_with_metadata, analyze_correlation


# ==================== Suite Definitions ====================

SUITES = {
    'reference': {
        'description': 'Visual regression baseline (everyday truth check)',
        'runs': [
            {'tap': 'b', 'fps': 30, 'duration': 20, 'fmt': 'v2', 'label': 'Reference'},
        ],
    },
    'stress': {
        'description': 'Capture-path headroom and sharper temporal detail',
        'runs': [
            {'tap': 'b', 'fps': 40, 'duration': 15, 'fmt': 'v2', 'label': 'Stress'},
        ],
    },
    'isolation': {
        'description': 'Tap A/B/C sweep to find where render path changes happen',
        'runs': [
            {'tap': 'a', 'fps': 30, 'duration': 10, 'fmt': 'v2', 'label': 'Isolation-A'},
            {'tap': 'b', 'fps': 30, 'duration': 10, 'fmt': 'v2', 'label': 'Isolation-B'},
            {'tap': 'c', 'fps': 30, 'duration': 10, 'fmt': 'v2', 'label': 'Isolation-C'},
        ],
    },
    'soak': {
        'description': 'Long-run stability: leaks, skips, drift',
        'runs': [
            {'tap': 'b', 'fps': 30, 'duration': 120, 'fmt': 'v2', 'label': 'Soak'},
        ],
    },
}


# ==================== Pass/Fail Gates ====================

def apply_gates(metrics: dict, suite_name: str, run_def: dict) -> tuple[str, list[str]]:
    """Apply pass/fail gates for a suite run.

    Returns (verdict, reasons) where verdict is PASS/WARN/FAIL.
    """
    reasons = []
    target_fps = run_def['fps']
    actual_fps = metrics.get('actual_fps', 0)
    n_frames = metrics.get('n_frames', 0)
    drops = metrics.get('frame_drops', 0)
    drop_rate = metrics.get('drop_rate', 0)
    show_p99 = metrics.get('show_time_p99_us', 0) / 1000.0  # us -> ms
    heap_trend = metrics.get('heap_trend', 0)

    if suite_name == 'reference':
        # Strict gates: must be boringly clean at 30 FPS.
        if abs(actual_fps - target_fps) > 0.5:
            reasons.append(f'FPS off target: {actual_fps:.1f} vs {target_fps}')
        if drops > 0:
            reasons.append(f'{drops} frame drops')
        if show_p99 > 8.0:
            reasons.append(f'show p99 {show_p99:.1f}ms > 8ms')
        if abs(heap_trend) > 5:
            reasons.append(f'heap trend {heap_trend:+.0f}B/f')

    elif suite_name == 'stress':
        # Near-target, small tolerance for drops.
        if actual_fps < target_fps - 2:
            reasons.append(f'FPS low: {actual_fps:.1f} vs {target_fps}')
        if drop_rate > 0.005:
            reasons.append(f'drop rate {drop_rate*100:.2f}% > 0.5%')
        if show_p99 > 8.0:
            reasons.append(f'show p99 {show_p99:.1f}ms > 8ms')
        if abs(heap_trend) > 20:
            reasons.append(f'heap trend {heap_trend:+.0f}B/f')

    elif suite_name == 'soak':
        # Long-run stability.
        if drops > 0:
            reasons.append(f'{drops} frame drops in soak')
        if heap_trend > 5:
            reasons.append(f'heap ratchet: {heap_trend:+.0f}B/f')
        if show_p99 > 8.0:
            reasons.append(f'show p99 {show_p99:.1f}ms > 8ms')
        if abs(actual_fps - target_fps) > 1.0:
            reasons.append(f'FPS drift: {actual_fps:.1f} vs {target_fps}')

    elif suite_name == 'isolation':
        # No automated gate — comparison is manual/analytical.
        pass

    if not reasons:
        return 'PASS', []

    # Distinguish WARN vs FAIL based on severity.
    has_fail = any('drops' in r or 'FPS' in r for r in reasons)
    return ('FAIL' if has_fail else 'WARN'), reasons


# ==================== Run Logic ====================

def run_suite(port: str, suite_name: str, output_dir: Path,
              settle: float = 3.0) -> list[dict]:
    """Run all captures in a suite and return results with verdicts."""
    suite = SUITES[suite_name]
    runs = suite['runs']
    results = []

    print(f"\n{'='*60}")
    print(f"Suite: {suite_name.upper()} — {suite['description']}")
    print(f"Runs: {len(runs)} | Port: {port}")
    print(f"{'='*60}")

    for idx, run_def in enumerate(runs):
        label = run_def['label']
        tap = run_def['tap']
        fps = run_def['fps']
        dur = run_def['duration']
        fmt = run_def['fmt']

        print(f"\n--- [{idx+1}/{len(runs)}] {label}: tap {tap.upper()}, "
              f"{fps} FPS, {dur}s, {fmt} ---")

        result = capture_with_metadata(port, dur, fps, tap, fmt=fmt)

        if result is None:
            print(f"  FAILED — no frames captured")
            results.append({
                'label': label,
                'suite': suite_name,
                'verdict': 'FAIL',
                'reasons': ['no frames captured'],
                'metrics': {},
            })
            if settle > 0 and idx < len(runs) - 1:
                time.sleep(settle)
            continue

        frames, timestamps, metadata = result
        print(f"  Captured {len(frames)} frames, analysing...")

        metrics = analyze_correlation(frames, timestamps, metadata, label)
        verdict, reasons = apply_gates(metrics, suite_name, run_def)

        actual = metrics.get('actual_fps', 0)
        drops = metrics.get('frame_drops', 0)
        drop_pct = metrics.get('drop_rate', 0) * 100
        show_ms = metrics.get('show_time_p99_us', 0) / 1000.0
        heap_t = metrics.get('heap_trend', 0)

        status_sym = '✓' if verdict == 'PASS' else '⚠' if verdict == 'WARN' else '✗'
        print(f"  {status_sym} {verdict}: {actual:.1f} FPS, "
              f"{drops} drops ({drop_pct:.2f}%), "
              f"show p99 {show_ms:.1f}ms, heap {heap_t:+.0f}B/f")
        if reasons:
            for r in reasons:
                print(f"    → {r}")

        results.append({
            'label': label,
            'suite': suite_name,
            'verdict': verdict,
            'reasons': reasons,
            'metrics': metrics,
            'run_def': run_def,
        })

        if settle > 0 and idx < len(runs) - 1:
            print(f"  Settling {settle}s...")
            time.sleep(settle)

    return results


def format_summary(all_results: list[dict], port: str) -> str:
    """Build a summary report from all suite results."""
    now = datetime.now().strftime('%Y-%m-%d %H:%M')
    lines = [
        f"Capture Suite Report — {port} — {now}",
        '=' * 70,
        f" {'Label':<16} | {'FPS':>5} | {'Drops':>5} | {'Show p99':>8} "
        f"| {'Heap':>7} | Verdict",
        f" {'-'*16}-+-{'-'*5}-+-{'-'*5}-+-{'-'*8}-+-{'-'*7}-+--------",
    ]

    for r in all_results:
        m = r.get('metrics', {})
        if not m:
            lines.append(f" {r['label']:<16} |   --  |   --  |    --    "
                         f"|    --   | {r['verdict']}")
            continue

        actual = m.get('actual_fps', 0)
        drops = m.get('frame_drops', 0)
        show_ms = m.get('show_time_p99_us', 0) / 1000.0
        heap_t = m.get('heap_trend', 0)

        lines.append(
            f" {r['label']:<16} | {actual:>5.1f} | {drops:>5} | {show_ms:>6.1f}ms "
            f"| {heap_t:>+5.0f}B/f | {r['verdict']}"
        )
        for reason in r.get('reasons', []):
            lines.append(f"   → {reason}")

    lines.append('=' * 70)

    # Overall verdict.
    verdicts = [r['verdict'] for r in all_results]
    if 'FAIL' in verdicts:
        overall = 'FAIL'
    elif 'WARN' in verdicts:
        overall = 'WARN'
    else:
        overall = 'PASS'
    lines.append(f"Overall: {overall}")

    return '\n'.join(lines)


# ==================== Main ====================

def main():
    parser = argparse.ArgumentParser(
        description='Run standardised capture test suites with pass/fail gates'
    )
    parser.add_argument(
        '--serial', required=True,
        help='Serial port for the device'
    )
    parser.add_argument(
        '--suite', default='reference',
        help='Comma-separated suite names: reference, stress, isolation, soak '
             '(default: reference)'
    )
    parser.add_argument(
        '--settle', type=float, default=3.0,
        help='Settle time between runs in seconds (default: 3)'
    )
    parser.add_argument(
        '--output', default=None,
        help='Output directory for reports (default: /tmp/capture_suite_<timestamp>)'
    )

    args = parser.parse_args()

    # Parse suite names.
    suite_names = [s.strip() for s in args.suite.split(',')]
    for name in suite_names:
        if name not in SUITES:
            print(f"ERROR: unknown suite '{name}'. "
                  f"Available: {', '.join(SUITES.keys())}", file=sys.stderr)
            sys.exit(1)

    # Output directory.
    if args.output:
        out_dir = Path(args.output)
    else:
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        out_dir = Path(f'/tmp/capture_suite_{ts}')
    out_dir.mkdir(parents=True, exist_ok=True)

    # Run all requested suites.
    all_results = []
    total_start = time.monotonic()

    for suite_name in suite_names:
        results = run_suite(args.serial, suite_name, out_dir, args.settle)
        all_results.extend(results)

    total_elapsed = time.monotonic() - total_start

    # Summary report.
    report = format_summary(all_results, args.serial)
    print(f"\n{report}")
    print(f"\nCompleted in {total_elapsed:.0f}s ({total_elapsed/60:.1f} min)")

    # Write report to file.
    report_path = out_dir / 'suite_report.txt'
    report_path.write_text(report + '\n', encoding='utf-8')
    print(f"Report: {report_path}")


if __name__ == '__main__':
    main()
