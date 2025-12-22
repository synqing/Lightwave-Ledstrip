#!/usr/bin/env python3
"""
Visualization Tools for Colour Pipeline Testbed

Generates side-by-side comparisons, difference heatmaps, and time-series plots.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from typing import List, Optional
from frame_parser import Frame
from analyse import AnalysisResults

# ============================================================================
# Strip Rendering
# ============================================================================

def render_strip(rgb_data: np.ndarray, title: str = "", width: int = 8, height: int = 1) -> np.ndarray:
    """
    Render LED strip as an image.
    
    Args:
        rgb_data: RGB data (160, 3) or (320, 3)
        title: Optional title
        width: Image width in inches
        height: Image height in inches
        
    Returns:
        Image array for display
    """
    # Normalize to 0-1 range
    img = rgb_data.astype(np.float32) / 255.0
    
    # Reshape to 2D if needed (for visualization, we can stretch vertically)
    if len(img.shape) == 2:
        # (N, 3) -> (1, N, 3) for horizontal strip
        img = img[np.newaxis, :, :]
    
    return img

def plot_side_by_side(
    baseline_frame: Frame,
    candidate_frame: Frame,
    output_path: Optional[str] = None,
    title: str = "Side-by-Side Comparison"
) -> None:
    """
    Plot side-by-side comparison of baseline vs candidate frames.
    
    Args:
        baseline_frame: Baseline frame
        candidate_frame: Candidate frame
        output_path: Optional path to save PNG
        title: Plot title
    """
    fig, axes = plt.subplots(2, 2, figsize=(16, 4))
    
    # Strip 1 comparison
    ax = axes[0, 0]
    ax.imshow(render_strip(baseline_frame.strip1), aspect='auto', interpolation='nearest')
    ax.set_title("Baseline - Strip 1")
    ax.set_xlabel("LED Index")
    ax.axis('off')
    
    ax = axes[0, 1]
    ax.imshow(render_strip(candidate_frame.strip1), aspect='auto', interpolation='nearest')
    ax.set_title("Candidate - Strip 1")
    ax.set_xlabel("LED Index")
    ax.axis('off')
    
    # Strip 2 comparison
    ax = axes[1, 0]
    ax.imshow(render_strip(baseline_frame.strip2), aspect='auto', interpolation='nearest')
    ax.set_title("Baseline - Strip 2")
    ax.set_xlabel("LED Index")
    ax.axis('off')
    
    ax = axes[1, 1]
    ax.imshow(render_strip(candidate_frame.strip2), aspect='auto', interpolation='nearest')
    ax.set_title("Candidate - Strip 2")
    ax.set_xlabel("LED Index")
    ax.axis('off')
    
    fig.suptitle(title, fontsize=14)
    plt.tight_layout()
    
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Saved: {output_path}")
    else:
        plt.show()
    
    plt.close()

# ============================================================================
# Difference Heatmaps
# ============================================================================

def plot_difference_heatmap(
    baseline_frame: Frame,
    candidate_frame: Frame,
    output_path: Optional[str] = None,
    title: str = "Difference Heatmap"
) -> None:
    """
    Plot difference heatmap showing per-pixel differences.
    
    Args:
        baseline_frame: Baseline frame
        candidate_frame: Candidate frame
        output_path: Optional path to save PNG
        title: Plot title
    """
    # Calculate per-channel differences
    diff = candidate_frame.rgb_data.astype(np.int16) - baseline_frame.rgb_data.astype(np.int16)
    diff_abs = np.abs(diff)
    
    # Calculate total difference (L2 norm per pixel)
    diff_total = np.sqrt(np.sum(diff ** 2, axis=1))
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 4))
    
    # R channel difference
    ax = axes[0, 0]
    im = ax.imshow(diff[:, 0].reshape(1, 320), aspect='auto', cmap='RdBu_r', vmin=-50, vmax=50)
    ax.set_title("R Channel Difference")
    ax.set_xlabel("LED Index")
    plt.colorbar(im, ax=ax)
    
    # G channel difference
    ax = axes[0, 1]
    im = ax.imshow(diff[:, 1].reshape(1, 320), aspect='auto', cmap='RdBu_r', vmin=-50, vmax=50)
    ax.set_title("G Channel Difference")
    ax.set_xlabel("LED Index")
    plt.colorbar(im, ax=ax)
    
    # B channel difference
    ax = axes[1, 0]
    im = ax.imshow(diff[:, 2].reshape(1, 320), aspect='auto', cmap='RdBu_r', vmin=-50, vmax=50)
    ax.set_title("B Channel Difference")
    ax.set_xlabel("LED Index")
    plt.colorbar(im, ax=ax)
    
    # Total difference (L2)
    ax = axes[1, 1]
    im = ax.imshow(diff_total.reshape(1, 320), aspect='auto', cmap='hot', vmin=0, vmax=100)
    ax.set_title("Total Difference (L2)")
    ax.set_xlabel("LED Index")
    plt.colorbar(im, ax=ax)
    
    fig.suptitle(title, fontsize=14)
    plt.tight_layout()
    
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Saved: {output_path}")
    else:
        plt.show()
    
    plt.close()

# ============================================================================
# Time-Series Plots
# ============================================================================

def plot_time_series(
    baseline_frames: List[Frame],
    candidate_frames: List[Frame],
    output_path: Optional[str] = None,
    title: str = "Time-Series Comparison"
) -> None:
    """
    Plot time-series of edge amplitude ratios and luma.
    
    Args:
        baseline_frames: List of baseline frames
        candidate_frames: List of candidate frames
        output_path: Optional path to save PNG
        title: Plot title
    """
    from analyse import calculate_luma
    
    # Calculate ratios over time
    baseline_ratios = []
    candidate_ratios = []
    baseline_luma = []
    candidate_luma = []
    
    for frame in baseline_frames:
        luma1 = np.mean(calculate_luma(frame.strip1))
        luma2 = np.mean(calculate_luma(frame.strip2))
        ratio = luma1 / luma2 if luma2 > 0 else 1.0
        baseline_ratios.append(ratio)
        baseline_luma.append((luma1 + luma2) / 2.0)
    
    for frame in candidate_frames:
        luma1 = np.mean(calculate_luma(frame.strip1))
        luma2 = np.mean(calculate_luma(frame.strip2))
        ratio = luma1 / luma2 if luma2 > 0 else 1.0
        candidate_ratios.append(ratio)
        candidate_luma.append((luma1 + luma2) / 2.0)
    
    fig, axes = plt.subplots(2, 1, figsize=(12, 6))
    
    # Ratio plot
    ax = axes[0]
    frames = np.arange(len(baseline_ratios))
    ax.plot(frames, baseline_ratios, 'b-', label='Baseline I₁/I₂', linewidth=2)
    ax.plot(frames, candidate_ratios, 'r--', label='Candidate I₁/I₂', linewidth=2)
    ax.set_xlabel("Frame Index")
    ax.set_ylabel("I₁/I₂ Ratio")
    ax.set_title("Edge Amplitude Ratio Over Time")
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Luma plot
    ax = axes[1]
    ax.plot(frames, baseline_luma, 'b-', label='Baseline Mean Luma', linewidth=2)
    ax.plot(frames, candidate_luma, 'r--', label='Candidate Mean Luma', linewidth=2)
    ax.set_xlabel("Frame Index")
    ax.set_ylabel("Mean Luma (BT.601)")
    ax.set_title("Mean Luminance Over Time")
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    fig.suptitle(title, fontsize=14)
    plt.tight_layout()
    
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Saved: {output_path}")
    else:
        plt.show()
    
    plt.close()

# ============================================================================
# Transfer Curve Plots
# ============================================================================

def plot_transfer_curves(
    results: AnalysisResults,
    output_path: Optional[str] = None,
    title: str = "Channel Transfer Curves"
) -> None:
    """
    Plot channel transfer curves (input→output mapping).
    
    Args:
        results: AnalysisResults containing transfer curves
        output_path: Optional path to save PNG
        title: Plot title
    """
    fig, axes = plt.subplots(1, 3, figsize=(15, 4))
    
    colors = {'r': 'red', 'g': 'green', 'b': 'blue'}
    
    for idx, (channel, curve) in enumerate(results.transfer_curves.items()):
        ax = axes[idx]
        
        # Plot empirical curve
        ax.plot(curve.input_values, curve.output_values, 
                color=colors[channel], linewidth=2, label='Empirical')
        
        # Plot identity line (no change)
        ax.plot([0, 255], [0, 255], 'k--', alpha=0.5, label='Identity')
        
        # Plot fitted gamma curve
        if curve.gamma_estimate != 1.0:
            gamma_curve = 255 * ((curve.input_values / 255.0) ** curve.gamma_estimate)
            ax.plot(curve.input_values, gamma_curve, 
                   'g--', alpha=0.7, label=f'Gamma {curve.gamma_estimate:.2f}')
        
        ax.set_xlabel("Input Value")
        ax.set_ylabel("Output Value")
        ax.set_title(f"{channel.upper()} Channel (γ={curve.gamma_estimate:.2f})")
        ax.legend()
        ax.grid(True, alpha=0.3)
        ax.set_xlim(0, 255)
        ax.set_ylim(0, 255)
    
    fig.suptitle(title, fontsize=14)
    plt.tight_layout()
    
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Saved: {output_path}")
    else:
        plt.show()
    
    plt.close()

# ============================================================================
# Main (for testing)
# ============================================================================

if __name__ == "__main__":
    import sys
    from frame_parser import parse_serial_dump
    from analyse import compare_baseline_candidate
    
    if len(sys.argv) < 3:
        print("Usage: visualize.py <baseline_dump> <candidate_dump> [output_dir]")
        sys.exit(1)
    
    baseline_file = sys.argv[1]
    candidate_file = sys.argv[2]
    output_dir = sys.argv[3] if len(sys.argv) > 3 else "."
    
    # Parse frames
    baseline_frames = parse_serial_dump(baseline_file)
    candidate_frames = parse_serial_dump(candidate_file)
    
    if not baseline_frames or not candidate_frames:
        print("Error: Could not parse frames")
        sys.exit(1)
    
    # Generate visualizations
    import os
    os.makedirs(output_dir, exist_ok=True)
    
    # Side-by-side
    plot_side_by_side(
        baseline_frames[0], candidate_frames[0],
        output_path=os.path.join(output_dir, "side_by_side.png")
    )
    
    # Difference heatmap
    plot_difference_heatmap(
        baseline_frames[0], candidate_frames[0],
        output_path=os.path.join(output_dir, "difference_heatmap.png")
    )
    
    # Time-series
    plot_time_series(
        baseline_frames, candidate_frames,
        output_path=os.path.join(output_dir, "time_series.png")
    )
    
    # Transfer curves
    tap_id = baseline_frames[0].metadata.tap_id
    results = compare_baseline_candidate(baseline_frames, candidate_frames, tap_id)
    plot_transfer_curves(
        results,
        output_path=os.path.join(output_dir, "transfer_curves.png")
    )
    
    print(f"\nVisualizations saved to: {output_dir}")

