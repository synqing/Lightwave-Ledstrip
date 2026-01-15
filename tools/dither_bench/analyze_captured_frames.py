#!/usr/bin/env python3
"""
Frame Capture Analyzer for DitherBench

Analyzes captured frames from hardware vs simulated pipelines.
Generates comparative visualizations and metrics.

Usage:
    python analyze_captured_frames.py \
        --hardware hardware_captures/effect_42/ \
        --simulation simulation_runs/effect_42_sim/ \
        --output analysis_results/
"""

import argparse
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from typing import Dict, List, Tuple
import json


def load_hardware_frame(frame_path: Path) -> np.ndarray:
    """Load hardware-captured frame (.npz format)."""
    data = np.load(frame_path)
    return data['rgb']


def load_simulation_frames(sim_dir: Path, pipeline: str = 'lwos') -> Dict[int, np.ndarray]:
    """
    Load simulation frames for specified pipeline.
    
    Args:
        sim_dir: Directory containing simulation results
        pipeline: Pipeline name ('lwos', 'sensorybridge', 'emotiscope')
    
    Returns:
        Dictionary mapping frame_idx -> rgb array
    """
    frames = {}
    
    # Look for pipeline-specific frame files
    for npz_file in sim_dir.glob(f"{pipeline}_frame_*.npz"):
        frame_idx = int(npz_file.stem.split('_')[-1])
        data = np.load(npz_file)
        frames[frame_idx] = data['rgb']
    
    return frames


def compute_metrics(hw_frame: np.ndarray, sim_frame: np.ndarray) -> Dict[str, float]:
    """
    Compute comparison metrics between hardware and simulation.
    
    Returns:
        Dictionary with MAE, RMSE, max_diff, correlation
    """
    if hw_frame.shape != sim_frame.shape:
        raise ValueError(f"Shape mismatch: hw {hw_frame.shape} vs sim {sim_frame.shape}")
    
    # Convert to float for accurate computation
    hw = hw_frame.astype(np.float32)
    sim = sim_frame.astype(np.float32)
    
    # Mean Absolute Error
    mae = np.mean(np.abs(hw - sim))
    
    # Root Mean Square Error
    rmse = np.sqrt(np.mean((hw - sim) ** 2))
    
    # Maximum difference
    max_diff = np.max(np.abs(hw - sim))
    
    # Correlation (flatten for correlation)
    hw_flat = hw.flatten()
    sim_flat = sim.flatten()
    correlation = np.corrcoef(hw_flat, sim_flat)[0, 1]
    
    return {
        'mae': float(mae),
        'rmse': float(rmse),
        'max_diff': float(max_diff),
        'correlation': float(correlation),
    }


def visualize_comparison(hw_frame: np.ndarray, 
                        sim_frame: np.ndarray,
                        output_path: Path,
                        title_suffix: str = ""):
    """
    Create side-by-side comparison visualization.
    
    Args:
        hw_frame: Hardware captured frame (N, 3)
        sim_frame: Simulated frame (N, 3)
        output_path: Output image path
        title_suffix: Additional title text
    """
    fig, axes = plt.subplots(2, 3, figsize=(15, 8))
    
    # Show only first strip (160 LEDs) for clarity
    hw_strip = hw_frame[:160, :]
    sim_strip = sim_frame[:160, :]
    diff = hw_strip.astype(np.int16) - sim_strip.astype(np.int16)
    
    # Hardware RGB
    axes[0, 0].imshow(hw_strip.T, aspect='auto', interpolation='nearest')
    axes[0, 0].set_title(f'Hardware (Strip 1){title_suffix}')
    axes[0, 0].set_ylabel('Channel (R/G/B)')
    axes[0, 0].set_xlabel('LED Index')
    
    # Simulation RGB
    axes[0, 1].imshow(sim_strip.T, aspect='auto', interpolation='nearest')
    axes[0, 1].set_title(f'Simulation (Strip 1){title_suffix}')
    axes[0, 1].set_xlabel('LED Index')
    
    # Difference
    im = axes[0, 2].imshow(diff.T, aspect='auto', interpolation='nearest', 
                          cmap='RdBu', vmin=-20, vmax=20)
    axes[0, 2].set_title(f'Difference (HW - Sim){title_suffix}')
    axes[0, 2].set_xlabel('LED Index')
    plt.colorbar(im, ax=axes[0, 2])
    
    # Per-channel histograms
    channels = ['Red', 'Green', 'Blue']
    for ch_idx, ch_name in enumerate(channels):
        axes[1, ch_idx].hist(hw_strip[:, ch_idx], bins=50, alpha=0.5, 
                            label='Hardware', color='blue')
        axes[1, ch_idx].hist(sim_strip[:, ch_idx], bins=50, alpha=0.5, 
                            label='Simulation', color='orange')
        axes[1, ch_idx].set_title(f'{ch_name} Distribution')
        axes[1, ch_idx].set_xlabel('Value')
        axes[1, ch_idx].set_ylabel('Count')
        axes[1, ch_idx].legend()
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def analyze_sequence(hw_dir: Path, 
                     sim_dir: Path,
                     output_dir: Path,
                     pipeline: str = 'lwos',
                     tap_name: str = 'TAP_B_POST_CORRECTION',
                     max_frames: int = 100):
    """
    Analyze full sequence of captured frames.
    
    Args:
        hw_dir: Hardware capture directory
        sim_dir: Simulation output directory
        output_dir: Analysis output directory
        pipeline: Pipeline to compare ('lwos', 'sensorybridge', 'emotiscope')
        tap_name: Tap point to analyze
        max_frames: Maximum number of frames to analyze
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Analyzing {pipeline} pipeline vs hardware ({tap_name})...")
    
    # Find hardware frames
    hw_frames = sorted(hw_dir.glob(f"frame_*_{tap_name}.npz"))[:max_frames]
    
    if len(hw_frames) == 0:
        print(f"⚠️  No hardware frames found in {hw_dir}")
        return
    
    # Load simulation frames
    sim_frames = load_simulation_frames(sim_dir, pipeline)
    
    if len(sim_frames) == 0:
        print(f"⚠️  No simulation frames found for pipeline '{pipeline}' in {sim_dir}")
        return
    
    print(f"Found {len(hw_frames)} hardware frames, {len(sim_frames)} simulation frames")
    
    # Collect metrics for all frames
    all_metrics = []
    
    for hw_path in hw_frames:
        # Extract frame index from filename
        frame_idx = int(hw_path.stem.split('_')[1])
        
        # Load hardware frame
        hw_frame = load_hardware_frame(hw_path)
        
        # Find matching simulation frame
        if frame_idx not in sim_frames:
            # Try to find closest frame
            closest_idx = min(sim_frames.keys(), key=lambda x: abs(x - frame_idx))
            sim_frame = sim_frames[closest_idx]
            print(f"  Frame {frame_idx}: using sim frame {closest_idx} (closest match)")
        else:
            sim_frame = sim_frames[frame_idx]
        
        # Compute metrics
        metrics = compute_metrics(hw_frame, sim_frame)
        metrics['frame_idx'] = frame_idx
        all_metrics.append(metrics)
        
        # Generate visualization for first 10 frames
        if len(all_metrics) <= 10:
            viz_path = output_dir / f"comparison_frame_{frame_idx:06d}.png"
            visualize_comparison(hw_frame, sim_frame, viz_path, 
                               title_suffix=f" (Frame {frame_idx})")
            print(f"  ✓ Frame {frame_idx}: MAE={metrics['mae']:.2f}, saved {viz_path.name}")
    
    # Save metrics summary
    metrics_path = output_dir / 'metrics_summary.json'
    with open(metrics_path, 'w') as f:
        json.dump(all_metrics, f, indent=2)
    
    print(f"\n✓ Metrics summary saved to: {metrics_path}")
    
    # Compute aggregate statistics
    mae_values = [m['mae'] for m in all_metrics]
    rmse_values = [m['rmse'] for m in all_metrics]
    
    print(f"\n=== Aggregate Statistics ===")
    print(f"  MAE:  mean={np.mean(mae_values):.2f}, std={np.std(mae_values):.2f}, "
          f"min={np.min(mae_values):.2f}, max={np.max(mae_values):.2f}")
    print(f"  RMSE: mean={np.mean(rmse_values):.2f}, std={np.std(rmse_values):.2f}, "
          f"min={np.min(rmse_values):.2f}, max={np.max(rmse_values):.2f}")
    
    # Plot metrics over time
    fig, axes = plt.subplots(2, 1, figsize=(10, 6))
    
    frame_indices = [m['frame_idx'] for m in all_metrics]
    axes[0].plot(frame_indices, mae_values, 'o-', label='MAE')
    axes[0].set_ylabel('MAE')
    axes[0].set_title(f'{pipeline.upper()} vs Hardware - Error Metrics')
    axes[0].grid(True)
    axes[0].legend()
    
    axes[1].plot(frame_indices, rmse_values, 'o-', color='orange', label='RMSE')
    axes[1].set_xlabel('Frame Index')
    axes[1].set_ylabel('RMSE')
    axes[1].grid(True)
    axes[1].legend()
    
    plt.tight_layout()
    metrics_plot_path = output_dir / 'metrics_over_time.png'
    plt.savefig(metrics_plot_path, dpi=150)
    plt.close()
    
    print(f"✓ Metrics plot saved to: {metrics_plot_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze captured frames from hardware vs simulation"
    )
    
    parser.add_argument(
        '--hardware',
        type=Path,
        required=True,
        help='Hardware capture directory'
    )
    
    parser.add_argument(
        '--simulation',
        type=Path,
        required=True,
        help='Simulation output directory'
    )
    
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('analysis_results'),
        help='Output directory for analysis results'
    )
    
    parser.add_argument(
        '--pipeline',
        type=str,
        default='lwos',
        choices=['lwos', 'sensorybridge', 'emotiscope'],
        help='Pipeline to compare against hardware'
    )
    
    parser.add_argument(
        '--tap',
        type=str,
        default='TAP_B_POST_CORRECTION',
        choices=['TAP_A_PRE_CORRECTION', 'TAP_B_POST_CORRECTION', 'TAP_C_PRE_WS2812'],
        help='Tap point to analyze'
    )
    
    parser.add_argument(
        '--max-frames',
        type=int,
        default=100,
        help='Maximum number of frames to analyze'
    )
    
    args = parser.parse_args()
    
    # Run analysis
    analyze_sequence(
        args.hardware,
        args.simulation,
        args.output,
        args.pipeline,
        args.tap,
        args.max_frames
    )
    
    print(f"\n✓ Analysis complete!")
    print(f"  Results saved to: {args.output}")


if __name__ == "__main__":
    main()
