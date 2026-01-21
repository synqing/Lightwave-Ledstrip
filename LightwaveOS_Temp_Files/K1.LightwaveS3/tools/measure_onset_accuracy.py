#!/usr/bin/env python3
"""
Tempo Tracker Onset Accuracy Measurement

Compares detected onsets from tempo tracker against hand-labeled ground truth
beat timestamps to measure onset quality. Critical for diagnosing the "broken
onset detector" identified in failure debrief.

Usage:
    python3 measure_onset_accuracy.py <log_file> <ground_truth_file> [--tolerance 0.050]

Log Format Expected:
    Lines containing onset timestamps, e.g.:
    "ONSET: t=1.234s strength=0.87" or "Onset detected at 1.234s"

Ground Truth Format (CSV):
    beat_time_sec
    0.435
    0.870
    1.305
    ...

Output:
    - Onset accuracy: % of onsets within tolerance of labeled beats
    - Classification: on-beat %, subdivision %, transient %
    - False negative rate: % of labeled beats without nearby onset
    - Recommended: AC-11 requires ≥70% onset accuracy

Theory:
    From debrief - only ~10-20% of onsets are on actual beats:
    - 40-60% are subdivisions (half-beats, quarter-beats)
    - 20-30% are transients (noise, artifacts)

    This tool measures the breakdown to guide tuning.
"""

import argparse
import re
import csv
from pathlib import Path
from typing import List, Tuple, Dict
import sys


def parse_onset_log(log_path: Path) -> List[float]:
    """
    Parse tempo tracker debug log to extract onset timestamps.

    Returns:
        List of onset times in seconds
    """
    onsets = []

    # Regex patterns for different log formats
    # Format 1: "ONSET: t=1.234s strength=0.87"
    pattern1 = re.compile(r'ONSET:\s+t=([\d.]+)s')
    # Format 2: "Onset detected at 1.234s"
    pattern2 = re.compile(r'Onset detected at\s+([\d.]+)s')
    # Format 3: Timestamp format "t=1.234s onset=1"
    pattern3 = re.compile(r't=([\d.]+)s\s+onset=1')

    try:
        with open(log_path, 'r') as f:
            for line in f:
                for pattern in [pattern1, pattern2, pattern3]:
                    match = pattern.search(line)
                    if match:
                        t = float(match.group(1))
                        onsets.append(t)
                        break

    except FileNotFoundError:
        print(f"Error: Log file not found: {log_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error parsing log file: {e}")
        sys.exit(1)

    return sorted(onsets)


def load_ground_truth(gt_path: Path) -> List[float]:
    """
    Load ground truth beat timestamps from CSV file.

    CSV format:
        beat_time_sec
        0.435
        0.870
        ...

    Returns:
        List of beat times in seconds
    """
    beats = []

    try:
        with open(gt_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                if 'beat_time_sec' in row:
                    beats.append(float(row['beat_time_sec']))
                elif 'time' in row:
                    beats.append(float(row['time']))
                else:
                    # Assume first column is time
                    beats.append(float(next(iter(row.values()))))

    except FileNotFoundError:
        print(f"Error: Ground truth file not found: {gt_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error loading ground truth: {e}")
        sys.exit(1)

    return sorted(beats)


def classify_onsets(onsets: List[float], beats: List[float],
                    tolerance: float) -> Dict:
    """
    Classify each onset as: on-beat, subdivision, or transient.

    Args:
        onsets: Detected onset times
        beats: Ground truth beat times
        tolerance: Time window for matching (seconds, default 0.050 = 50ms)

    Returns:
        Dictionary with classification results
    """
    on_beat = []
    subdivision = []
    transient = []

    # Track which beats have been matched (for false negative calc)
    matched_beats = set()

    for onset in onsets:
        # Find nearest beat
        nearest_beat_idx = None
        min_dist = float('inf')

        for i, beat in enumerate(beats):
            dist = abs(onset - beat)
            if dist < min_dist:
                min_dist = dist
                nearest_beat_idx = i

        if nearest_beat_idx is None:
            transient.append(onset)
            continue

        nearest_beat = beats[nearest_beat_idx]

        # Classify based on distance and timing
        if min_dist <= tolerance:
            # Within tolerance of a beat
            on_beat.append(onset)
            matched_beats.add(nearest_beat_idx)

        elif nearest_beat_idx > 0:
            # Check if it's a subdivision (between two beats)
            prev_beat = beats[nearest_beat_idx - 1]
            next_beat = nearest_beat

            interval = next_beat - prev_beat

            # Check for half-beat (subdivision)
            half_beat_pos = prev_beat + interval / 2.0
            if abs(onset - half_beat_pos) <= tolerance * 2:
                subdivision.append(onset)
                continue

            # Check for quarter-beat
            quarter_beat_pos1 = prev_beat + interval / 4.0
            quarter_beat_pos2 = prev_beat + 3.0 * interval / 4.0

            if (abs(onset - quarter_beat_pos1) <= tolerance * 2 or
                abs(onset - quarter_beat_pos2) <= tolerance * 2):
                subdivision.append(onset)
                continue

            # Not aligned with beat or subdivision
            transient.append(onset)
        else:
            transient.append(onset)

    # Calculate false negatives (beats without nearby onset)
    false_negatives = len(beats) - len(matched_beats)

    return {
        'total_onsets': len(onsets),
        'on_beat': on_beat,
        'subdivision': subdivision,
        'transient': transient,
        'on_beat_count': len(on_beat),
        'subdivision_count': len(subdivision),
        'transient_count': len(transient),
        'on_beat_pct': 100.0 * len(on_beat) / len(onsets) if onsets else 0.0,
        'subdivision_pct': 100.0 * len(subdivision) / len(onsets) if onsets else 0.0,
        'transient_pct': 100.0 * len(transient) / len(onsets) if onsets else 0.0,
        'total_beats': len(beats),
        'matched_beats': len(matched_beats),
        'false_negative_count': false_negatives,
        'false_negative_pct': 100.0 * false_negatives / len(beats) if beats else 0.0
    }


def print_summary(results: Dict, tolerance: float):
    """Print analysis summary to console."""
    print("\n" + "="*70)
    print("ONSET ACCURACY ANALYSIS")
    print("="*70)
    print(f"Tolerance window:    ±{tolerance*1000:.0f}ms")
    print(f"\nTotal onsets:        {results['total_onsets']}")
    print(f"Total beats (GT):    {results['total_beats']}")
    print(f"\n--- Onset Classification ---")
    print(f"On-beat:             {results['on_beat_count']:4d} ({results['on_beat_pct']:5.1f}%)")
    print(f"Subdivision:         {results['subdivision_count']:4d} ({results['subdivision_pct']:5.1f}%)")
    print(f"Transient:           {results['transient_count']:4d} ({results['transient_pct']:5.1f}%)")
    print(f"\n--- Detection Quality ---")
    print(f"Matched beats:       {results['matched_beats']}/{results['total_beats']}")
    print(f"False negatives:     {results['false_negative_count']} ({results['false_negative_pct']:.1f}%)")

    print("\n--- Acceptance Criteria Check ---")
    if results['on_beat_pct'] >= 70.0:
        print(f"✓ AC-11 PASS: Onset accuracy {results['on_beat_pct']:.1f}% ≥ 70%")
    else:
        print(f"✗ AC-11 FAIL: Onset accuracy {results['on_beat_pct']:.1f}% < 70%")

    if results['subdivision_pct'] <= 20.0:
        print(f"✓ AC-11 PASS: Subdivision rate {results['subdivision_pct']:.1f}% ≤ 20%")
    else:
        print(f"✗ AC-11 FAIL: Subdivision rate {results['subdivision_pct']:.1f}% > 20%")

    if results['transient_pct'] <= 10.0:
        print(f"✓ AC-11 PASS: Transient rate {results['transient_pct']:.1f}% ≤ 10%")
    else:
        print(f"✗ AC-11 FAIL: Transient rate {results['transient_pct']:.1f}% > 10%")

    print("="*70 + "\n")

    # Diagnostic guidance
    if results['on_beat_pct'] < 70.0:
        print("DIAGNOSTIC RECOMMENDATIONS:")
        print("- Onset accuracy below target (70%)")
        print("- Check novelty threshold tuning (baseline alpha, onset threshold)")
        print("- Consider increasing refractory period if high subdivision rate")
        print("- Review spectral flux computation (may need different frequency weighting)")
        print()


def write_results_csv(results: Dict, onsets: List[float], beats: List[float],
                      output_path: Path, tolerance: float):
    """Write detailed results to CSV for further analysis."""
    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['onset_time_sec', 'classification', 'nearest_beat_sec', 'error_ms'])

        # Write on-beat onsets
        for onset in results['on_beat']:
            nearest = min(beats, key=lambda b: abs(b - onset))
            error = (onset - nearest) * 1000
            writer.writerow([f'{onset:.3f}', 'on_beat', f'{nearest:.3f}', f'{error:.1f}'])

        # Write subdivision onsets
        for onset in results['subdivision']:
            nearest = min(beats, key=lambda b: abs(b - onset))
            error = (onset - nearest) * 1000
            writer.writerow([f'{onset:.3f}', 'subdivision', f'{nearest:.3f}', f'{error:.1f}'])

        # Write transient onsets
        for onset in results['transient']:
            nearest = min(beats, key=lambda b: abs(b - onset))
            error = (onset - nearest) * 1000
            writer.writerow([f'{onset:.3f}', 'transient', f'{nearest:.3f}', f'{error:.1f}'])

    print(f"Detailed results written to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Measure tempo tracker onset accuracy vs ground truth'
    )
    parser.add_argument('log_file', type=str,
                       help='Path to tempo tracker debug log file')
    parser.add_argument('ground_truth', type=str,
                       help='Path to ground truth beat timestamps CSV')
    parser.add_argument('--tolerance', type=float, default=0.050,
                       help='Time tolerance for onset matching (seconds, default: 0.050 = 50ms)')
    parser.add_argument('--output', type=str,
                       help='Output CSV file path (default: <log_file>_accuracy.csv)')

    args = parser.parse_args()

    log_path = Path(args.log_file)
    gt_path = Path(args.ground_truth)
    output_path = Path(args.output) if args.output else log_path.with_name(
        log_path.stem + '_accuracy.csv')

    print(f"Loading ground truth: {gt_path}")
    beats = load_ground_truth(gt_path)
    print(f"  Loaded {len(beats)} beat timestamps")

    print(f"Parsing onset log: {log_path}")
    onsets = parse_onset_log(log_path)
    print(f"  Found {len(onsets)} onset events")

    if not onsets:
        print("Error: No onsets found in log file.")
        sys.exit(1)

    if not beats:
        print("Error: No beats found in ground truth file.")
        sys.exit(1)

    results = classify_onsets(onsets, beats, args.tolerance)
    print_summary(results, args.tolerance)

    write_results_csv(results, onsets, beats, output_path, args.tolerance)

    # Exit with non-zero if accuracy below threshold
    if results['on_beat_pct'] < 70.0:
        sys.exit(2)


if __name__ == '__main__':
    main()
