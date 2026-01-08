#!/usr/bin/env python3
"""
Tempo Tracker Interval Distribution Analyzer

Parses tempo tracker debug logs to extract inter-onset intervals (IOI),
generates distribution histograms, and classifies intervals as beat-range
vs rejected (too fast/slow).

Usage:
    python3 analyze_intervals.py <log_file> [--output <png_file>]

Log Format Expected:
    Lines containing "ONSET" with interval data in seconds, e.g.:
    "ONSET: dt=0.435s accepted=1" or "ONSET: dt=0.120s rejected=1"

Output:
    - PNG histogram showing interval distribution
    - CSV file with interval data
    - Summary statistics (beat-range %, rejected %, mean interval)
"""

import argparse
import re
import csv
from pathlib import Path
from typing import List, Tuple, Dict
import sys

# Beat-range constants (from PRD FR-16)
MIN_BEAT_INTERVAL = 60.0 / 180.0  # 0.333s (180 BPM max)
MAX_BEAT_INTERVAL = 60.0 / 60.0   # 1.0s (60 BPM min)


def parse_log_file(log_path: Path) -> Tuple[List[float], List[float]]:
    """
    Parse tempo tracker debug log to extract intervals.

    Returns:
        Tuple of (accepted_intervals, rejected_intervals) in seconds
    """
    accepted = []
    rejected = []

    # Regex patterns for different log formats
    # Format 1: "ONSET: dt=0.435s accepted=1"
    pattern1 = re.compile(r'ONSET:\s+dt=([\d.]+)s\s+accepted=(\d+)')
    # Format 2: "Interval: 0.435s (accepted)"
    pattern2 = re.compile(r'Interval:\s+([\d.]+)s\s+\((\w+)\)')
    # Format 3: Just interval values
    pattern3 = re.compile(r'IOI:\s+([\d.]+)s')

    try:
        with open(log_path, 'r') as f:
            for line in f:
                # Try pattern 1
                match = pattern1.search(line)
                if match:
                    dt = float(match.group(1))
                    is_accepted = int(match.group(2))
                    if is_accepted:
                        accepted.append(dt)
                    else:
                        rejected.append(dt)
                    continue

                # Try pattern 2
                match = pattern2.search(line)
                if match:
                    dt = float(match.group(1))
                    status = match.group(2).lower()
                    if 'accept' in status:
                        accepted.append(dt)
                    else:
                        rejected.append(dt)
                    continue

                # Try pattern 3 (classify based on range)
                match = pattern3.search(line)
                if match:
                    dt = float(match.group(1))
                    if MIN_BEAT_INTERVAL <= dt <= MAX_BEAT_INTERVAL:
                        accepted.append(dt)
                    else:
                        rejected.append(dt)
                    continue

    except FileNotFoundError:
        print(f"Error: Log file not found: {log_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error parsing log file: {e}")
        sys.exit(1)

    return accepted, rejected


def analyze_intervals(accepted: List[float], rejected: List[float]) -> Dict:
    """Compute statistics on interval distribution."""
    total = len(accepted) + len(rejected)

    if total == 0:
        return {
            'total': 0,
            'accepted_count': 0,
            'rejected_count': 0,
            'accepted_pct': 0.0,
            'rejected_pct': 0.0,
            'mean_interval': 0.0,
            'mean_bpm': 0.0,
            'too_fast_count': 0,
            'too_slow_count': 0
        }

    too_fast = sum(1 for dt in rejected if dt < MIN_BEAT_INTERVAL)
    too_slow = sum(1 for dt in rejected if dt > MAX_BEAT_INTERVAL)

    mean_interval = sum(accepted) / len(accepted) if accepted else 0.0
    mean_bpm = 60.0 / mean_interval if mean_interval > 0 else 0.0

    return {
        'total': total,
        'accepted_count': len(accepted),
        'rejected_count': len(rejected),
        'accepted_pct': 100.0 * len(accepted) / total,
        'rejected_pct': 100.0 * len(rejected) / total,
        'mean_interval': mean_interval,
        'mean_bpm': mean_bpm,
        'too_fast_count': too_fast,
        'too_slow_count': too_slow
    }


def write_csv(accepted: List[float], rejected: List[float], csv_path: Path):
    """Write interval data to CSV file."""
    with open(csv_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['interval_sec', 'bpm', 'status'])

        for dt in accepted:
            bpm = 60.0 / dt if dt > 0 else 0.0
            writer.writerow([f'{dt:.3f}', f'{bpm:.1f}', 'accepted'])

        for dt in rejected:
            bpm = 60.0 / dt if dt > 0 else 0.0
            status = 'too_fast' if dt < MIN_BEAT_INTERVAL else 'too_slow'
            writer.writerow([f'{dt:.3f}', f'{bpm:.1f}', status])

    print(f"CSV data written to: {csv_path}")


def plot_histogram(accepted: List[float], rejected: List[float],
                   stats: Dict, output_path: Path):
    """Generate histogram plot (requires matplotlib)."""
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("Warning: matplotlib not installed. Skipping histogram generation.")
        print("Install with: pip install matplotlib")
        return

    # Create figure
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

    # Plot 1: Interval histogram
    bins = np.linspace(0, 1.2, 60)

    if accepted:
        ax1.hist(accepted, bins=bins, alpha=0.7, color='green',
                 label=f'Accepted ({len(accepted)})')
    if rejected:
        ax1.hist(rejected, bins=bins, alpha=0.7, color='red',
                 label=f'Rejected ({len(rejected)})')

    # Mark beat-range boundaries
    ax1.axvline(MIN_BEAT_INTERVAL, color='blue', linestyle='--',
                label=f'Min beat ({MIN_BEAT_INTERVAL:.3f}s = 180 BPM)')
    ax1.axvline(MAX_BEAT_INTERVAL, color='orange', linestyle='--',
                label=f'Max beat ({MAX_BEAT_INTERVAL:.3f}s = 60 BPM)')

    ax1.set_xlabel('Interval (seconds)')
    ax1.set_ylabel('Count')
    ax1.set_title('Inter-Onset Interval Distribution')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: BPM histogram
    bpm_bins = np.linspace(0, 300, 60)

    if accepted:
        accepted_bpm = [60.0/dt for dt in accepted if dt > 0]
        ax2.hist(accepted_bpm, bins=bpm_bins, alpha=0.7, color='green',
                 label=f'Accepted')
    if rejected:
        rejected_bpm = [60.0/dt for dt in rejected if dt > 0]
        ax2.hist(rejected_bpm, bins=bpm_bins, alpha=0.7, color='red',
                 label=f'Rejected')

    # Mark BPM range boundaries
    ax2.axvline(60, color='orange', linestyle='--', label='Min BPM (60)')
    ax2.axvline(180, color='blue', linestyle='--', label='Max BPM (180)')

    ax2.set_xlabel('BPM')
    ax2.set_ylabel('Count')
    ax2.set_title('BPM Distribution')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    # Add summary text
    summary_text = f"""
Total intervals: {stats['total']}
Accepted: {stats['accepted_count']} ({stats['accepted_pct']:.1f}%)
Rejected: {stats['rejected_count']} ({stats['rejected_pct']:.1f}%)
  - Too fast (<{MIN_BEAT_INTERVAL:.3f}s): {stats['too_fast_count']}
  - Too slow (>{MAX_BEAT_INTERVAL:.3f}s): {stats['too_slow_count']}
Mean interval: {stats['mean_interval']:.3f}s ({stats['mean_bpm']:.1f} BPM)
"""

    fig.text(0.02, 0.02, summary_text, fontsize=9, family='monospace',
             verticalalignment='bottom')

    plt.tight_layout(rect=[0, 0.15, 1, 1])
    plt.savefig(output_path, dpi=150)
    print(f"Histogram saved to: {output_path}")
    plt.close()


def print_summary(stats: Dict):
    """Print summary statistics to console."""
    print("\n" + "="*60)
    print("INTERVAL DISTRIBUTION ANALYSIS")
    print("="*60)
    print(f"Total intervals:     {stats['total']}")
    print(f"Accepted:            {stats['accepted_count']} ({stats['accepted_pct']:.1f}%)")
    print(f"Rejected:            {stats['rejected_count']} ({stats['rejected_pct']:.1f}%)")
    print(f"  - Too fast (<{MIN_BEAT_INTERVAL:.3f}s):  {stats['too_fast_count']}")
    print(f"  - Too slow (>{MAX_BEAT_INTERVAL:.3f}s): {stats['too_slow_count']}")
    print(f"\nMean interval:       {stats['mean_interval']:.3f}s")
    print(f"Mean BPM:            {stats['mean_bpm']:.1f}")
    print("="*60 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze tempo tracker interval distribution from debug logs'
    )
    parser.add_argument('log_file', type=str,
                       help='Path to tempo tracker debug log file')
    parser.add_argument('--output', type=str,
                       help='Output PNG file path (default: <log_file>_histogram.png)')
    parser.add_argument('--csv', type=str,
                       help='Output CSV file path (default: <log_file>_intervals.csv)')

    args = parser.parse_args()

    log_path = Path(args.log_file)
    output_path = Path(args.output) if args.output else log_path.with_name(
        log_path.stem + '_histogram.png')
    csv_path = Path(args.csv) if args.csv else log_path.with_name(
        log_path.stem + '_intervals.csv')

    print(f"Parsing log file: {log_path}")
    accepted, rejected = parse_log_file(log_path)

    if not accepted and not rejected:
        print("Warning: No intervals found in log file.")
        print("Expected log lines with format:")
        print("  'ONSET: dt=0.435s accepted=1' or 'Interval: 0.435s (accepted)'")
        sys.exit(1)

    stats = analyze_intervals(accepted, rejected)
    print_summary(stats)

    write_csv(accepted, rejected, csv_path)
    plot_histogram(accepted, rejected, stats, output_path)

    # Exit with non-zero if rejection rate is too high
    if stats['rejected_pct'] > 50.0:
        print(f"WARNING: High rejection rate ({stats['rejected_pct']:.1f}%)")
        sys.exit(2)


if __name__ == '__main__':
    main()
