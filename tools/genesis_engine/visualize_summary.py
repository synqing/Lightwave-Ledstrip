#!/usr/bin/env python3
"""Quick visualization summary of Genesis Map data"""

import json
import matplotlib.pyplot as plt
import numpy as np

def plot_genesis_summary(json_file):
    with open(json_file) as f:
        data = json.load(f)
    
    fig, axes = plt.subplots(3, 1, figsize=(12, 8))
    
    # Extract frequency data
    bass = [(p['time_ms']/1000, p['intensity']) for p in data['layers']['frequency']['bass'][:1000]]
    mids = [(p['time_ms']/1000, p['intensity']) for p in data['layers']['frequency']['mids'][:1000]]
    highs = [(p['time_ms']/1000, p['intensity']) for p in data['layers']['frequency']['highs'][:1000]]
    
    # Plot frequency bands
    ax = axes[0]
    ax.plot(*zip(*bass), 'r-', alpha=0.7, label='Bass (20-250Hz)')
    ax.plot(*zip(*mids), 'g-', alpha=0.7, label='Mids (250-4kHz)')
    ax.plot(*zip(*highs), 'b-', alpha=0.7, label='Highs (4-20kHz)')
    ax.set_ylabel('Intensity')
    ax.set_title(f'{data["metadata"]["filename"]} - Frequency Analysis')
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, 60)  # First 60 seconds
    
    # Plot beat grid
    ax = axes[1]
    beats = np.array(data['layers']['rhythm']['beat_grid_ms'][:200]) / 1000
    ax.vlines(beats, 0, 1, 'r', alpha=0.5)
    ax.set_ylabel('Beats')
    ax.set_title(f'Beat Grid (BPM: {data["global_metrics"]["bpm"]:.0f})')
    ax.set_xlim(0, 60)
    
    # Plot dynamics
    if 'dynamics' in data['layers']:
        ax = axes[2]
        rms = [(p['time_ms']/1000, p['level']) for p in data['layers']['dynamics']['rms_curve'][:500]]
        if rms:
            ax.plot(*zip(*rms), 'purple', alpha=0.7)
        ax.set_ylabel('RMS Level')
        ax.set_xlabel('Time (seconds)')
        ax.set_title('Dynamic Range')
        ax.set_xlim(0, 60)
    
    plt.tight_layout()
    plt.savefig('genesis_analysis_summary.png', dpi=150, bbox_inches='tight')
    print("✅ Saved visualization to genesis_analysis_summary.png")
    
    # Print summary stats
    print(f"\n📊 Analysis Summary:")
    print(f"  File: {data['metadata']['filename']}")
    print(f"  Duration: {data['global_metrics']['duration_ms']/1000:.1f}s")
    print(f"  BPM: {data['global_metrics']['bpm']:.0f}")
    print(f"  Beats: {len(data['layers']['rhythm']['beat_grid_ms'])}")
    
    if 'spectral' in data['layers']:
        print(f"\n🌈 Spectral Features:")
        print(f"  Avg Brightness: {data['layers']['spectral']['centroid_avg']:.0f} Hz")
        print(f"  Brightness Variation: ±{data['layers']['spectral']['centroid_std']:.0f} Hz")

if __name__ == "__main__":
    import sys
    plot_genesis_summary(sys.argv[1] if len(sys.argv) > 1 else "Ahmed_Spins_Anchor_Point_advanced.json")