#!/usr/bin/env python3
"""
Tempo Tracker Visualization Tools

Creates multi-panel plots showing tempo tracker internal state over time:
- Density buffer evolution (BPM votes)
- Confidence curve
- Interval scatter (detected vs expected)
- Phase alignment

Usage:
    python3 visualize_tempo_data.py <log_file> [--output <png_file>]

Log Format Expected:
    Structured tempo tracker debug output with:
    - DENSITY: bpm=120.0 density=0.85 conf=0.67
    - INTERVAL: detected=0.435s expected=0.500s
    - PHASE: phase=0.75 beat_tick=0

Output:
    Multi-panel PNG showing temporal evolution of tempo tracking state
"""

import argparse
import re
from pathlib import Path
from typing import List, Dict, Tuple
import sys
from collections import defaultdict


def parse_tempo_log(log_path: Path) -> Dict:
    """
    Parse tempo tracker debug log to extract time-series data.

    Returns:
        Dictionary with lists of:
        - times: timestamps
        - bpm_values: detected BPM over time
        - confidence: confidence over time
        - density_buffers: BPM density distribution snapshots
        - intervals: (time, detected_interval, expected_interval) tuples
        - phase: (time, phase01, beat_tick) tuples
    """
    data = {
        'times': [],
        'bpm_values': [],
        'confidence': [],
        'density_snapshots': [],  # List of (time, bpm_bins, densities)
        'intervals': [],
        'phase': []
    }

    # Regex patterns
    # "t=1.234s bpm=120.5 conf=0.67"
    tempo_pattern = re.compile(r't=([\d.]+)s\s+bpm=([\d.]+)\s+conf=([\d.]+)')

    # "DENSITY: bpm=120.0 density=0.85"
    density_pattern = re.compile(r'DENSITY:\s+bpm=([\d.]+)\s+density=([\d.]+)')

    # "INTERVAL: detected=0.435s expected=0.500s"
    interval_pattern = re.compile(r'INTERVAL:\s+detected=([\d.]+)s\s+expected=([\d.]+)s')

    # "PHASE: t=1.234s phase=0.75 beat_tick=1"
    phase_pattern = re.compile(r'PHASE:\s+t=([\d.]+)s\s+phase=([\d.]+)\s+beat_tick=(\d+)')

    current_time = 0.0
    density_buffer = defaultdict(float)  # BPM -> density

    try:
        with open(log_path, 'r') as f:
            for line in f:
                # Tempo/confidence update
                match = tempo_pattern.search(line)
                if match:
                    t = float(match.group(1))
                    bpm = float(match.group(2))
                    conf = float(match.group(3))
                    data['times'].append(t)
                    data['bpm_values'].append(bpm)
                    data['confidence'].append(conf)
                    current_time = t
                    continue

                # Density buffer entry
                match = density_pattern.search(line)
                if match:
                    bpm = float(match.group(1))
                    density = float(match.group(2))
                    density_buffer[bpm] = density
                    continue

                # Snapshot marker (end of density buffer dump)
                if 'DENSITY_END' in line or 'TEMPO_UPDATE' in line:
                    if density_buffer:
                        data['density_snapshots'].append(
                            (current_time, dict(density_buffer))
                        )
                        density_buffer.clear()
                    continue

                # Interval comparison
                match = interval_pattern.search(line)
                if match:
                    detected = float(match.group(1))
                    expected = float(match.group(2))
                    data['intervals'].append((current_time, detected, expected))
                    continue

                # Phase update
                match = phase_pattern.search(line)
                if match:
                    t = float(match.group(1))
                    phase = float(match.group(2))
                    beat_tick = int(match.group(3))
                    data['phase'].append((t, phase, beat_tick))
                    continue

    except FileNotFoundError:
        print(f"Error: Log file not found: {log_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error parsing log file: {e}")
        sys.exit(1)

    return data


def plot_tempo_analysis(data: Dict, output_path: Path, ground_truth_bpm: float = None):
    """Generate multi-panel visualization."""
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("Warning: matplotlib not installed. Skipping visualization.")
        print("Install with: pip install matplotlib numpy")
        return

    fig = plt.figure(figsize=(14, 10))
    gs = fig.add_gridspec(4, 2, hspace=0.3, wspace=0.3)

    # Panel 1: BPM over time
    ax1 = fig.add_subplot(gs[0, :])
    if data['times'] and data['bpm_values']:
        ax1.plot(data['times'], data['bpm_values'], 'b-', linewidth=2, label='Detected BPM')
        if ground_truth_bpm:
            ax1.axhline(ground_truth_bpm, color='green', linestyle='--',
                       label=f'Ground Truth ({ground_truth_bpm:.1f} BPM)')
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('BPM')
        ax1.set_title('Tempo Tracking Over Time')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

    # Panel 2: Confidence over time
    ax2 = fig.add_subplot(gs[1, :])
    if data['times'] and data['confidence']:
        ax2.plot(data['times'], data['confidence'], 'r-', linewidth=2)
        ax2.axhline(0.5, color='orange', linestyle='--', alpha=0.5, label='Lock threshold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Confidence')
        ax2.set_title('Confidence Over Time')
        ax2.set_ylim(0, 1.0)
        ax2.legend()
        ax2.grid(True, alpha=0.3)

    # Panel 3: Interval scatter (detected vs expected)
    ax3 = fig.add_subplot(gs[2, 0])
    if data['intervals']:
        times, detected, expected = zip(*data['intervals'])
        ax3.scatter(expected, detected, alpha=0.5, s=20)
        # Perfect agreement line
        max_val = max(max(detected), max(expected))
        ax3.plot([0, max_val], [0, max_val], 'k--', alpha=0.3)
        ax3.set_xlabel('Expected Interval (s)')
        ax3.set_ylabel('Detected Interval (s)')
        ax3.set_title('Interval Accuracy')
        ax3.grid(True, alpha=0.3)

    # Panel 4: Phase alignment
    ax4 = fig.add_subplot(gs[2, 1])
    if data['phase']:
        times, phases, beat_ticks = zip(*data['phase'])
        ax4.plot(times, phases, 'b-', linewidth=1, alpha=0.7)
        # Mark beat ticks
        beat_times = [t for t, p, b in data['phase'] if b == 1]
        if beat_times:
            ax4.scatter(beat_times, [1.0] * len(beat_times), color='red',
                       marker='v', s=100, label='Beat Tick', zorder=5)
        ax4.set_xlabel('Time (s)')
        ax4.set_ylabel('Phase (0-1)')
        ax4.set_title('Phase Progression')
        ax4.set_ylim(-0.1, 1.1)
        ax4.legend()
        ax4.grid(True, alpha=0.3)

    # Panel 5: Density buffer heatmap (if snapshots available)
    ax5 = fig.add_subplot(gs[3, :])
    if data['density_snapshots']:
        # Build 2D array: time × BPM
        times = [t for t, _ in data['density_snapshots']]
        all_bpms = set()
        for _, density_dict in data['density_snapshots']:
            all_bpms.update(density_dict.keys())
        all_bpms = sorted(all_bpms)

        # Create density matrix
        density_matrix = np.zeros((len(all_bpms), len(times)))
        for time_idx, (t, density_dict) in enumerate(data['density_snapshots']):
            for bpm_idx, bpm in enumerate(all_bpms):
                density_matrix[bpm_idx, time_idx] = density_dict.get(bpm, 0.0)

        # Plot heatmap
        im = ax5.imshow(density_matrix, aspect='auto', origin='lower',
                       extent=[times[0], times[-1], all_bpms[0], all_bpms[-1]],
                       cmap='hot', interpolation='nearest')
        ax5.set_xlabel('Time (s)')
        ax5.set_ylabel('BPM')
        ax5.set_title('Density Buffer Evolution (Heatmap)')
        plt.colorbar(im, ax=ax5, label='Density')

        if ground_truth_bpm:
            ax5.axhline(ground_truth_bpm, color='cyan', linestyle='--',
                       linewidth=2, label='Ground Truth')
            ax5.legend()
    else:
        ax5.text(0.5, 0.5, 'No density buffer snapshots in log',
                ha='center', va='center', transform=ax5.transAxes)
        ax5.set_title('Density Buffer Evolution (Not Available)')

    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Visualization saved to: {output_path}")
    plt.close()


def print_summary(data: Dict):
    """Print summary statistics."""
    print("\n" + "="*60)
    print("TEMPO TRACKER VISUALIZATION SUMMARY")
    print("="*60)
    print(f"Time samples:        {len(data['times'])}")
    print(f"BPM values:          {len(data['bpm_values'])}")
    print(f"Confidence values:   {len(data['confidence'])}")
    print(f"Density snapshots:   {len(data['density_snapshots'])}")
    print(f"Interval pairs:      {len(data['intervals'])}")
    print(f"Phase samples:       {len(data['phase'])}")

    if data['bpm_values']:
        print(f"\nBPM range:           {min(data['bpm_values']):.1f} - {max(data['bpm_values']):.1f}")
        print(f"Mean BPM:            {sum(data['bpm_values'])/len(data['bpm_values']):.1f}")

    if data['confidence']:
        print(f"Confidence range:    {min(data['confidence']):.2f} - {max(data['confidence']):.2f}")
        print(f"Mean confidence:     {sum(data['confidence'])/len(data['confidence']):.2f}")

        # Lock time (first time confidence > 0.5)
        lock_times = [t for t, c in zip(data['times'], data['confidence']) if c >= 0.5]
        if lock_times:
            print(f"Lock time:           {lock_times[0]:.2f}s")

    print("="*60 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description='Visualize tempo tracker internal state over time'
    )
    parser.add_argument('log_file', type=str,
                       help='Path to tempo tracker debug log file')
    parser.add_argument('--output', type=str,
                       help='Output PNG file path (default: <log_file>_viz.png)')
    parser.add_argument('--ground-truth-bpm', type=float,
                       help='Ground truth BPM for reference line')

    args = parser.parse_args()

    log_path = Path(args.log_file)
    output_path = Path(args.output) if args.output else log_path.with_name(
        log_path.stem + '_viz.png')

    print(f"Parsing log file: {log_path}")
    data = parse_tempo_log(log_path)

    if not data['times']:
        print("Warning: No tempo data found in log file.")
        print("Expected log lines with format: 't=1.234s bpm=120.5 conf=0.67'")
        sys.exit(1)

    print_summary(data)
    plot_tempo_analysis(data, output_path, args.ground_truth_bpm)


if __name__ == '__main__':
    main()
