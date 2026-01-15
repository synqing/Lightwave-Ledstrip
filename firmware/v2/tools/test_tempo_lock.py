#!/usr/bin/env python3
"""
Tempo Tracker Ground Truth Validation Framework

Main test harness for validating tempo tracking performance against ground truth.
Orchestrates audio playback through tempo tracker and validates against acceptance
criteria from PRD.

Usage:
    # Using pre-captured log (Phase 0 approach)
    python3 test_tempo_lock.py --log tempo_log.txt --ground-truth edm_138bpm.csv

    # Future: Native test integration
    python3 test_tempo_lock.py --audio edm_138bpm.wav --ground-truth edm_138bpm.csv

Log Format Expected:
    Tempo tracker debug output with:
    - "t=1.234s bpm=120.5 conf=0.67" - Tempo updates
    - "ONSET: t=1.234s strength=0.87" - Onset events
    - "INTERVAL: detected=0.435s" - Intervals

Ground Truth CSV:
    beat_time_sec, expected_bpm
    0.435, 138.0
    0.870, 138.0
    ...

Output:
    PASS/FAIL with detailed metrics:
    - Lock time (AC-9: EDM ≤ 2.0s, worst-case ≤ 5.0s)
    - BPM accuracy (AC-9: jitter ≤ ±1 BPM)
    - Confidence stability (AC-9: octave flips ≤ 1/60s)
    - Onset quality (AC-11: accuracy ≥ 70%)
"""

import argparse
import re
import csv
from pathlib import Path
from typing import List, Tuple, Dict, Optional
import sys
import subprocess


class TempoTestResult:
    """Container for tempo tracking test results."""

    def __init__(self):
        self.lock_time: Optional[float] = None
        self.mean_bpm: float = 0.0
        self.bpm_error: float = 0.0
        self.bpm_jitter: float = 0.0
        self.confidence_mean: float = 0.0
        self.octave_flips: int = 0
        self.onset_accuracy: float = 0.0
        self.passed: bool = False
        self.failure_reasons: List[str] = []


def parse_tempo_log_full(log_path: Path) -> Tuple[List[Tuple[float, float, float]], List[float]]:
    """
    Parse full tempo tracker log.

    Returns:
        Tuple of (tempo_data, onset_times)
        tempo_data: List of (time, bpm, confidence) tuples
        onset_times: List of onset timestamps
    """
    tempo_data = []
    onsets = []

    # "t=1.234s bpm=120.5 conf=0.67"
    tempo_pattern = re.compile(r't=([\d.]+)s\s+bpm=([\d.]+)\s+conf=([\d.]+)')
    # "ONSET: t=1.234s"
    onset_pattern = re.compile(r'ONSET:\s+t=([\d.]+)s')

    try:
        with open(log_path, 'r') as f:
            for line in f:
                match = tempo_pattern.search(line)
                if match:
                    t = float(match.group(1))
                    bpm = float(match.group(2))
                    conf = float(match.group(3))
                    tempo_data.append((t, bpm, conf))
                    continue

                match = onset_pattern.search(line)
                if match:
                    t = float(match.group(1))
                    onsets.append(t)

    except Exception as e:
        print(f"Error parsing log: {e}")
        sys.exit(1)

    return tempo_data, onsets


def load_ground_truth_full(gt_path: Path) -> Tuple[List[float], float]:
    """
    Load ground truth beat timestamps and expected BPM.

    Returns:
        Tuple of (beat_times, expected_bpm)
    """
    beats = []
    expected_bpm = None

    try:
        with open(gt_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                beat_time = float(row.get('beat_time_sec', row.get('time', 0)))
                beats.append(beat_time)

                if expected_bpm is None and 'expected_bpm' in row:
                    expected_bpm = float(row['expected_bpm'])

    except Exception as e:
        print(f"Error loading ground truth: {e}")
        sys.exit(1)

    # If expected_bpm not in file, calculate from first two beats
    if expected_bpm is None and len(beats) >= 2:
        interval = beats[1] - beats[0]
        expected_bpm = 60.0 / interval

    return beats, expected_bpm


def calculate_lock_time(tempo_data: List[Tuple[float, float, float]],
                       lock_threshold: float = 0.5) -> Optional[float]:
    """Calculate time when confidence first exceeds lock threshold."""
    for t, bpm, conf in tempo_data:
        if conf >= lock_threshold:
            return t
    return None


def calculate_bpm_jitter(tempo_data: List[Tuple[float, float, float]],
                        lock_time: float) -> float:
    """Calculate BPM jitter (standard deviation) after lock."""
    locked_bpms = [bpm for t, bpm, conf in tempo_data if t >= lock_time]

    if len(locked_bpms) < 2:
        return 0.0

    mean_bpm = sum(locked_bpms) / len(locked_bpms)
    variance = sum((bpm - mean_bpm) ** 2 for bpm in locked_bpms) / len(locked_bpms)
    return variance ** 0.5


def count_octave_flips(tempo_data: List[Tuple[float, float, float]],
                      lock_time: float) -> int:
    """Count number of octave flips (2× or 0.5× jumps) after lock."""
    locked_data = [(t, bpm) for t, bpm, conf in tempo_data if t >= lock_time]

    if len(locked_data) < 2:
        return 0

    flips = 0
    prev_bpm = locked_data[0][1]

    for t, bpm in locked_data[1:]:
        ratio = bpm / prev_bpm if prev_bpm > 0 else 1.0

        # Check for octave jumps (within 10% tolerance)
        if 1.8 <= ratio <= 2.2 or 0.45 <= ratio <= 0.55:
            flips += 1

        prev_bpm = bpm

    return flips


def run_test(log_path: Path, gt_path: Path, genre: str = 'EDM') -> TempoTestResult:
    """
    Run complete tempo tracking validation test.

    Args:
        log_path: Path to tempo tracker log file
        gt_path: Path to ground truth CSV
        genre: Music genre ('EDM' for 2.0s target, 'complex' for 5.0s)

    Returns:
        TempoTestResult with all metrics and pass/fail status
    """
    result = TempoTestResult()

    print(f"\nLoading test data...")
    tempo_data, onsets = parse_tempo_log_full(log_path)
    beats, expected_bpm = load_ground_truth_full(gt_path)

    print(f"  Tempo samples: {len(tempo_data)}")
    print(f"  Onset events: {len(onsets)}")
    print(f"  Ground truth beats: {len(beats)}")
    print(f"  Expected BPM: {expected_bpm:.1f}")

    if not tempo_data:
        result.failure_reasons.append("No tempo data in log")
        return result

    # Calculate lock time (AC-9)
    result.lock_time = calculate_lock_time(tempo_data)
    if result.lock_time is None:
        result.failure_reasons.append("Never achieved lock (confidence < 0.5)")
        return result

    print(f"\nLock achieved at: {result.lock_time:.2f}s")

    # Lock time threshold depends on genre
    max_lock_time = 2.0 if genre == 'EDM' else 5.0

    if result.lock_time > max_lock_time:
        result.failure_reasons.append(
            f"Lock time {result.lock_time:.2f}s > {max_lock_time}s ({genre} target)"
        )

    # Calculate mean BPM after lock
    locked_bpms = [bpm for t, bpm, conf in tempo_data if t >= result.lock_time]
    if locked_bpms:
        result.mean_bpm = sum(locked_bpms) / len(locked_bpms)
        result.bpm_error = abs(result.mean_bpm - expected_bpm)

        print(f"Mean BPM (locked): {result.mean_bpm:.1f}")
        print(f"BPM error: {result.bpm_error:.1f}")

        # AC-9: BPM jitter ≤ ±1 BPM
        result.bpm_jitter = calculate_bpm_jitter(tempo_data, result.lock_time)
        print(f"BPM jitter (std dev): {result.bpm_jitter:.2f}")

        if result.bpm_jitter > 1.0:
            result.failure_reasons.append(f"BPM jitter {result.bpm_jitter:.2f} > 1.0 BPM")

    # AC-9: Octave flips ≤ 1/60s
    locked_duration = tempo_data[-1][0] - result.lock_time if tempo_data else 0.0
    result.octave_flips = count_octave_flips(tempo_data, result.lock_time)

    if locked_duration > 0:
        flip_rate = result.octave_flips / locked_duration * 60.0
        print(f"Octave flips: {result.octave_flips} in {locked_duration:.1f}s ({flip_rate:.2f}/60s)")

        if flip_rate > 1.0:
            result.failure_reasons.append(
                f"Octave flip rate {flip_rate:.2f}/60s > 1.0/60s"
            )

    # Mean confidence
    confidences = [conf for t, bpm, conf in tempo_data if t >= result.lock_time]
    if confidences:
        result.confidence_mean = sum(confidences) / len(confidences)
        print(f"Mean confidence (locked): {result.confidence_mean:.2f}")

    # AC-11: Onset accuracy (requires calling measure_onset_accuracy.py)
    # For Phase 0, we'll note this requires separate tool invocation
    print(f"\nNote: Onset accuracy (AC-11) requires separate validation:")
    print(f"  python3 measure_onset_accuracy.py {log_path} {gt_path}")

    # Determine pass/fail
    result.passed = len(result.failure_reasons) == 0

    return result


def print_test_report(result: TempoTestResult, test_name: str):
    """Print formatted test report."""
    print("\n" + "="*70)
    print(f"TEMPO TRACKING TEST REPORT: {test_name}")
    print("="*70)

    print("\n--- Metrics ---")
    if result.lock_time:
        print(f"Lock time:           {result.lock_time:.2f}s")
    else:
        print(f"Lock time:           FAILED (never locked)")

    print(f"Mean BPM:            {result.mean_bpm:.1f}")
    print(f"BPM error:           {result.bpm_error:.1f}")
    print(f"BPM jitter:          {result.bpm_jitter:.2f}")
    print(f"Octave flips:        {result.octave_flips}")
    print(f"Confidence (mean):   {result.confidence_mean:.2f}")

    print("\n--- Acceptance Criteria ---")

    if result.passed:
        print("✓ ALL ACCEPTANCE CRITERIA PASSED")
    else:
        print("✗ TEST FAILED")
        for reason in result.failure_reasons:
            print(f"  - {reason}")

    print("="*70 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Tempo tracking ground truth validation framework'
    )
    parser.add_argument('--log', type=str, required=True,
                       help='Path to tempo tracker debug log file')
    parser.add_argument('--ground-truth', type=str, required=True,
                       help='Path to ground truth CSV file')
    parser.add_argument('--genre', type=str, default='EDM',
                       choices=['EDM', 'complex'],
                       help='Music genre (affects lock time threshold)')
    parser.add_argument('--test-name', type=str,
                       help='Test name for report (default: log filename)')

    args = parser.parse_args()

    log_path = Path(args.log)
    gt_path = Path(args.ground_truth)
    test_name = args.test_name or log_path.stem

    print("="*70)
    print("TEMPO TRACKER GROUND TRUTH VALIDATION")
    print("="*70)
    print(f"Log file:     {log_path}")
    print(f"Ground truth: {gt_path}")
    print(f"Genre:        {args.genre}")

    result = run_test(log_path, gt_path, args.genre)
    print_test_report(result, test_name)

    # Exit code: 0 = pass, 1 = fail
    sys.exit(0 if result.passed else 1)


if __name__ == '__main__':
    main()
