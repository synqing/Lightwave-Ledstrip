#!/usr/bin/env python3
"""
LightwaveOS Dithering Test Suite - Host Simulation
===================================================

Simulates the v2 rendering pipeline dithering stages to compare:
- Bayer 4×4 ordered dithering (spatial)
- FastLED-style temporal dithering (random frame-to-frame noise)
- Combined (both applied)
- No dithering (baseline)

Generates quantitative metrics and visualizations for objective analysis.

Requirements:
    pip install numpy scipy pandas matplotlib plotly bokeh pillow

Usage:
    python dithering_test_suite.py --output ./test_results
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from scipy.stats import entropy
from scipy import signal
import argparse
from pathlib import Path
from typing import Tuple, Dict, List
import json
from datetime import datetime

# =============================================================================
# CONSTANTS
# =============================================================================

LED_COUNT = 320
STRIP_COUNT = 2
LEDS_PER_STRIP = 160

# Bayer 4x4 matrix (from ColorCorrectionEngine.cpp)
BAYER_4X4 = np.array([
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5]
], dtype=np.uint8)

# =============================================================================
# DITHERING IMPLEMENTATIONS (matching firmware)
# =============================================================================

def apply_bayer_dithering(buffer: np.ndarray) -> np.ndarray:
    """
    Apply Bayer 4×4 ordered dithering (spatial).
    
    Args:
        buffer: RGB array of shape (n_leds, 3), dtype uint8
        
    Returns:
        Dithered buffer (new array, input unchanged)
    """
    result = buffer.copy()
    n_leds = result.shape[0]
    
    for i in range(n_leds):
        # Get threshold based on LED position (creates 4x4 pattern)
        threshold = BAYER_4X4[i % 4, (i // 4) % 4]
        
        # Apply ordered dithering: if low nibble exceeds threshold, round up
        for channel in range(3):
            value = result[i, channel]
            if (value & 0x0F) > threshold and value < 255:
                result[i, channel] = value + 1
                
    return result


def apply_temporal_dithering(buffer: np.ndarray, frame_index: int, 
                             rng: np.random.Generator) -> np.ndarray:
    """
    Apply FastLED-style temporal dithering (random +1/0/-1 per frame).
    
    Args:
        buffer: RGB array of shape (n_leds, 3), dtype uint8
        frame_index: Current frame number (affects random seed pattern)
        rng: NumPy random generator for reproducibility
        
    Returns:
        Dithered buffer (new array, input unchanged)
    """
    result = buffer.copy().astype(np.int16)  # Need signed for subtraction
    
    # Generate random adjustments (-1, 0, +1) for each pixel channel
    # FastLED uses a pseudo-random sequence based on frame counter
    adjustments = rng.integers(-1, 2, size=(buffer.shape[0], 3), dtype=np.int8)
    
    result += adjustments
    
    # Clamp to valid range [0, 255]
    result = np.clip(result, 0, 255).astype(np.uint8)
    
    return result


def apply_gamma_correction(buffer: np.ndarray, gamma: float = 2.2) -> np.ndarray:
    """
    Apply gamma correction (matching ColorCorrectionEngine LUT).
    
    Args:
        buffer: RGB array of shape (n_leds, 3), dtype uint8
        gamma: Gamma value (default 2.2)
        
    Returns:
        Gamma-corrected buffer
    """
    # Generate gamma LUT (same as firmware)
    lut = np.zeros(256, dtype=np.uint8)
    for i in range(256):
        normalized = i / 255.0
        gamma_corrected = np.power(normalized, gamma)
        lut[i] = np.clip(gamma_corrected * 255.0 + 0.5, 0, 255).astype(np.uint8)
    
    # Apply LUT
    return lut[buffer]


# =============================================================================
# TEST STIMULI GENERATORS
# =============================================================================

def generate_gradient_ramp(start: int = 0, end: int = 255, 
                           channel: int = 0) -> np.ndarray:
    """
    Generate a smooth linear gradient ramp (test for banding).
    
    Args:
        start: Starting brightness (0-255)
        end: Ending brightness (0-255)
        channel: RGB channel (0=R, 1=G, 2=B)
        
    Returns:
        LED buffer (320, 3)
    """
    buffer = np.zeros((LED_COUNT, 3), dtype=np.uint8)
    ramp = np.linspace(start, end, LED_COUNT).astype(np.uint8)
    buffer[:, channel] = ramp
    return buffer


def generate_near_black_gradient() -> np.ndarray:
    """
    Generate a near-black gradient (test for flicker/shimmer).
    
    Returns:
        LED buffer (320, 3)
    """
    buffer = np.zeros((LED_COUNT, 3), dtype=np.uint8)
    ramp = np.linspace(0, 20, LED_COUNT).astype(np.uint8)
    buffer[:, :] = ramp[:, np.newaxis]  # All channels same
    return buffer


def generate_palette_blend() -> np.ndarray:
    """
    Generate a palette-like blend (RGB hue wheel).
    
    Returns:
        LED buffer (320, 3)
    """
    buffer = np.zeros((LED_COUNT, 3), dtype=np.uint8)
    
    # HSV hue wheel, full saturation, mid brightness
    for i in range(LED_COUNT):
        hue = (i / LED_COUNT) * 360.0
        # Simple HSV to RGB (good enough for testing)
        c = 0.5  # Brightness (V)
        s = 1.0  # Saturation
        h_prime = hue / 60.0
        x = c * (1 - abs((h_prime % 2) - 1))
        
        if 0 <= h_prime < 1:
            r, g, b = c, x, 0
        elif 1 <= h_prime < 2:
            r, g, b = x, c, 0
        elif 2 <= h_prime < 3:
            r, g, b = 0, c, x
        elif 3 <= h_prime < 4:
            r, g, b = 0, x, c
        elif 4 <= h_prime < 5:
            r, g, b = x, 0, c
        else:
            r, g, b = c, 0, x
            
        buffer[i] = [int(r * 255), int(g * 255), int(b * 255)]
    
    return buffer


# =============================================================================
# METRICS
# =============================================================================

def compute_gradient_banding_score(buffer: np.ndarray, channel: int = 0) -> float:
    """
    Measure gradient banding via first-derivative histogram entropy.
    
    Lower entropy = more banding (large steps create histogram spikes).
    Higher entropy = smoother gradient (uniform derivative distribution).
    
    Args:
        buffer: LED buffer (n_leds, 3)
        channel: RGB channel to analyze
        
    Returns:
        Banding score (higher = more banding, 0 = perfectly smooth)
    """
    values = buffer[:, channel].astype(np.int16)
    
    # Compute first derivative (step sizes between adjacent LEDs)
    derivative = np.abs(np.diff(values))
    
    # Histogram of step sizes
    hist, _ = np.histogram(derivative, bins=20, range=(0, 20))
    hist = hist / np.sum(hist)  # Normalize
    
    # Entropy: high entropy = uniform steps (good), low entropy = spiky (bad)
    ent = entropy(hist + 1e-10)  # Add epsilon to avoid log(0)
    
    # Invert so higher score = more banding
    max_entropy = np.log(20)  # Max possible entropy for 20 bins
    banding_score = max_entropy - ent
    
    return banding_score


def compute_temporal_flicker_energy(frames: List[np.ndarray], 
                                    channel: int = 0) -> float:
    """
    Measure temporal flicker via frame-to-frame variance.
    
    Args:
        frames: List of LED buffers (each shape (n_leds, 3))
        channel: RGB channel to analyze
        
    Returns:
        Flicker energy (stddev of per-pixel temporal variance)
    """
    if len(frames) < 2:
        return 0.0
    
    # Stack frames into (n_frames, n_leds) array
    stack = np.array([f[:, channel] for f in frames], dtype=np.float32)
    
    # Compute per-pixel variance across frames
    pixel_variance = np.var(stack, axis=0)
    
    # Return mean variance (average flicker energy)
    return float(np.mean(pixel_variance))


def compute_power_budget(buffer: np.ndarray) -> float:
    """
    Estimate power consumption (sum of all RGB values).
    
    Args:
        buffer: LED buffer (n_leds, 3)
        
    Returns:
        Relative power (arbitrary units)
    """
    return float(np.sum(buffer))


# =============================================================================
# PIPELINE SIMULATOR
# =============================================================================

class DitheringPipeline:
    """Simulate LightwaveOS v2 dithering pipeline with different configs."""
    
    def __init__(self, enable_bayer: bool = True, enable_temporal: bool = True,
                 enable_gamma: bool = True, seed: int = 42):
        self.enable_bayer = enable_bayer
        self.enable_temporal = enable_temporal
        self.enable_gamma = enable_gamma
        self.rng = np.random.default_rng(seed)
        
    def process_frame(self, buffer: np.ndarray, frame_index: int = 0) -> np.ndarray:
        """
        Process a single frame through the pipeline.
        
        Args:
            buffer: Input LED buffer (n_leds, 3), dtype uint8
            frame_index: Frame number (for temporal dithering seed)
            
        Returns:
            Processed buffer
        """
        result = buffer.copy()
        
        # Stage 1: Gamma correction (before dithering, per firmware pipeline)
        if self.enable_gamma:
            result = apply_gamma_correction(result)
        
        # Stage 2: Bayer dithering (spatial)
        if self.enable_bayer:
            result = apply_bayer_dithering(result)
        
        # Stage 3: Temporal dithering (happens in FastLED.show(), after Bayer)
        if self.enable_temporal:
            result = apply_temporal_dithering(result, frame_index, self.rng)
        
        return result
    
    def process_sequence(self, buffer: np.ndarray, n_frames: int = 10) -> List[np.ndarray]:
        """
        Process multiple frames (for temporal analysis).
        
        Args:
            buffer: Input LED buffer (static stimulus)
            n_frames: Number of frames to generate
            
        Returns:
            List of processed frames
        """
        return [self.process_frame(buffer, frame_idx) 
                for frame_idx in range(n_frames)]


# =============================================================================
# TEST RUNNER
# =============================================================================

def run_test_suite(output_dir: Path):
    """Run comprehensive dithering test suite."""
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("=" * 80)
    print("LightwaveOS Dithering Test Suite - Host Simulation")
    print("=" * 80)
    print(f"Output directory: {output_dir}")
    print()
    
    # Define test configurations
    configs = {
        'A_both': {'bayer': True, 'temporal': True},
        'B_temporal_only': {'bayer': False, 'temporal': True},
        'C_bayer_only': {'bayer': True, 'temporal': False},
        'D_none': {'bayer': False, 'temporal': False},
    }
    
    # Generate test stimuli
    stimuli = {
        'gradient_red': generate_gradient_ramp(0, 255, channel=0),
        'gradient_green': generate_gradient_ramp(0, 255, channel=1),
        'gradient_blue': generate_gradient_ramp(0, 255, channel=2),
        'near_black': generate_near_black_gradient(),
        'palette_blend': generate_palette_blend(),
    }
    
    # Results storage
    results = []
    frame_sequences = {}  # For temporal analysis
    
    # Run tests
    print("Running tests...")
    for stimulus_name, stimulus_buffer in stimuli.items():
        print(f"\n  Stimulus: {stimulus_name}")
        
        for config_name, config_params in configs.items():
            print(f"    Config: {config_name} (Bayer={config_params['bayer']}, Temporal={config_params['temporal']})")
            
            pipeline = DitheringPipeline(
                enable_bayer=config_params['bayer'],
                enable_temporal=config_params['temporal'],
                enable_gamma=True,
                seed=42  # Fixed seed for reproducibility
            )
            
            # Generate frame sequence (for temporal metrics)
            n_frames = 10
            frames = pipeline.process_sequence(stimulus_buffer, n_frames)
            
            # Store for visualization
            key = f"{stimulus_name}_{config_name}"
            frame_sequences[key] = frames
            
            # Compute metrics
            # Use first frame for spatial metrics
            frame0 = frames[0]
            
            banding_r = compute_gradient_banding_score(frame0, channel=0)
            banding_g = compute_gradient_banding_score(frame0, channel=1)
            banding_b = compute_gradient_banding_score(frame0, channel=2)
            banding_avg = (banding_r + banding_g + banding_b) / 3.0
            
            flicker_r = compute_temporal_flicker_energy(frames, channel=0)
            flicker_g = compute_temporal_flicker_energy(frames, channel=1)
            flicker_b = compute_temporal_flicker_energy(frames, channel=2)
            flicker_avg = (flicker_r + flicker_g + flicker_b) / 3.0
            
            power = compute_power_budget(frame0)
            
            results.append({
                'stimulus': stimulus_name,
                'config': config_name,
                'bayer_enabled': config_params['bayer'],
                'temporal_enabled': config_params['temporal'],
                'banding_score': banding_avg,
                'banding_r': banding_r,
                'banding_g': banding_g,
                'banding_b': banding_b,
                'flicker_energy': flicker_avg,
                'flicker_r': flicker_r,
                'flicker_g': flicker_g,
                'flicker_b': flicker_b,
                'power_budget': power,
            })
    
    # Convert to DataFrame
    df = pd.DataFrame(results)
    
    # Save results
    csv_path = output_dir / 'metrics.csv'
    df.to_csv(csv_path, index=False)
    print(f"\n✓ Metrics saved to {csv_path}")
    
    # Generate visualizations
    print("\nGenerating visualizations...")
    generate_matplotlib_plots(df, frame_sequences, output_dir)
    generate_plotly_dashboard(df, frame_sequences, output_dir)
    
    # Print summary
    print("\n" + "=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(df.groupby('config')[['banding_score', 'flicker_energy', 'power_budget']].mean())
    
    print(f"\n✓ All results saved to {output_dir}")
    print("\n Done!")


# =============================================================================
# VISUALIZATION
# =============================================================================

def generate_matplotlib_plots(df: pd.DataFrame, frame_sequences: Dict, output_dir: Path):
    """Generate Matplotlib plots (static visualizations)."""
    
    # Plot 1: Banding Score Comparison
    fig, ax = plt.subplots(figsize=(12, 6))
    
    for config in df['config'].unique():
        subset = df[df['config'] == config]
        ax.plot(subset['stimulus'], subset['banding_score'], marker='o', label=config)
    
    ax.set_xlabel('Stimulus')
    ax.set_ylabel('Banding Score (lower = smoother)')
    ax.set_title('Gradient Banding Comparison')
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig(output_dir / 'banding_comparison.png', dpi=150)
    plt.close()
    print(f"  ✓ banding_comparison.png")
    
    # Plot 2: Flicker Energy Comparison
    fig, ax = plt.subplots(figsize=(12, 6))
    
    for config in df['config'].unique():
        subset = df[df['config'] == config]
        ax.plot(subset['stimulus'], subset['flicker_energy'], marker='s', label=config)
    
    ax.set_xlabel('Stimulus')
    ax.set_ylabel('Flicker Energy (lower = more stable)')
    ax.set_title('Temporal Flicker Comparison')
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig(output_dir / 'flicker_comparison.png', dpi=150)
    plt.close()
    print(f"  ✓ flicker_comparison.png")
    
    # Plot 3: Scatter plot (Banding vs Flicker trade-off)
    fig, ax = plt.subplots(figsize=(10, 8))
    
    for config in df['config'].unique():
        subset = df[df['config'] == config]
        ax.scatter(subset['banding_score'], subset['flicker_energy'], 
                  s=100, alpha=0.6, label=config)
    
    ax.set_xlabel('Banding Score')
    ax.set_ylabel('Flicker Energy')
    ax.set_title('Banding vs Flicker Trade-off')
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_dir / 'tradeoff_scatter.png', dpi=150)
    plt.close()
    print(f"  ✓ tradeoff_scatter.png")


def generate_plotly_dashboard(df: pd.DataFrame, frame_sequences: Dict, output_dir: Path):
    """Generate interactive Plotly dashboard."""
    
    # Create subplots
    fig = make_subplots(
        rows=2, cols=2,
        subplot_titles=('Banding Score by Config', 'Flicker Energy by Config',
                       'Power Budget Comparison', 'Banding vs Flicker Trade-off'),
        specs=[[{'type': 'bar'}, {'type': 'bar'}],
               [{'type': 'bar'}, {'type': 'scatter'}]]
    )
    
    # Aggregate by config
    config_agg = df.groupby('config').agg({
        'banding_score': 'mean',
        'flicker_energy': 'mean',
        'power_budget': 'mean'
    }).reset_index()
    
    # Plot 1: Banding Score
    fig.add_trace(
        go.Bar(x=config_agg['config'], y=config_agg['banding_score'], 
               name='Banding Score', marker_color='indianred'),
        row=1, col=1
    )
    
    # Plot 2: Flicker Energy
    fig.add_trace(
        go.Bar(x=config_agg['config'], y=config_agg['flicker_energy'],
               name='Flicker Energy', marker_color='lightsalmon'),
        row=1, col=2
    )
    
    # Plot 3: Power Budget
    fig.add_trace(
        go.Bar(x=config_agg['config'], y=config_agg['power_budget'],
               name='Power Budget', marker_color='lightseagreen'),
        row=2, col=1
    )
    
    # Plot 4: Scatter (Banding vs Flicker)
    for config in df['config'].unique():
        subset = df[df['config'] == config]
        fig.add_trace(
            go.Scatter(x=subset['banding_score'], y=subset['flicker_energy'],
                      mode='markers', name=config, marker=dict(size=10)),
            row=2, col=2
        )
    
    fig.update_xaxes(title_text="Config", row=1, col=1)
    fig.update_xaxes(title_text="Config", row=1, col=2)
    fig.update_xaxes(title_text="Config", row=2, col=1)
    fig.update_xaxes(title_text="Banding Score", row=2, col=2)
    
    fig.update_yaxes(title_text="Score", row=1, col=1)
    fig.update_yaxes(title_text="Energy", row=1, col=2)
    fig.update_yaxes(title_text="Power (AU)", row=2, col=1)
    fig.update_yaxes(title_text="Flicker Energy", row=2, col=2)
    
    fig.update_layout(height=800, showlegend=True, title_text="Dithering Performance Dashboard")
    
    html_path = output_dir / 'dashboard.html'
    fig.write_html(str(html_path))
    print(f"  ✓ dashboard.html (interactive)")


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='LightwaveOS Dithering Test Suite')
    parser.add_argument('--output', type=str, default='./test_results',
                       help='Output directory for results')
    
    args = parser.parse_args()
    output_dir = Path(args.output)
    
    run_test_suite(output_dir)


if __name__ == '__main__':
    main()
