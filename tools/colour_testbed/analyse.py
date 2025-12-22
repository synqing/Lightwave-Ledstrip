#!/usr/bin/env python3
"""
Colour Pipeline Analysis Engine

Computes quantitative metrics for comparing baseline vs candidate colour processing.
"""

import numpy as np
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass
from frame_parser import Frame, FrameMetadata, get_frame_statistics

# ============================================================================
# Metric Results
# ============================================================================

@dataclass
class PerPixelMetrics:
    """Per-pixel comparison metrics"""
    l1_delta: float  # Sum of absolute differences
    l2_delta: float  # Euclidean distance
    max_error: int   # Maximum per-channel difference
    psnr: float      # Peak Signal-to-Noise Ratio
    mean_error: float
    std_error: float

@dataclass
class EdgeAmplitudeMetrics:
    """Edge amplitude relationship metrics"""
    i1_mean: float      # Mean amplitude of strip 1
    i2_mean: float      # Mean amplitude of strip 2
    i1_i2_ratio: float # I₁/I₂ ratio
    ratio_stability: float  # Coefficient of variation of ratio over time
    spatial_symmetry: float  # Correlation between left/right halves

@dataclass
class TemporalMetrics:
    """Temporal stability metrics"""
    frame_to_frame_delta: float  # Mean frame-to-frame energy
    trail_persistence_q25: float  # 25th percentile of low-value pixel lifetimes
    trail_persistence_q50: float  # Median
    trail_persistence_q75: float  # 75th percentile

@dataclass
class ChannelTransferCurve:
    """Channel transfer curve (empirical LUT)"""
    input_values: np.ndarray   # Input values (0-255)
    output_values: np.ndarray  # Output values (0-255)
    gamma_estimate: float      # Fitted gamma value

@dataclass
class AnalysisResults:
    """Complete analysis results for a comparison"""
    per_pixel: PerPixelMetrics
    edge_amplitude: EdgeAmplitudeMetrics
    temporal: TemporalMetrics
    transfer_curves: Dict[str, ChannelTransferCurve]  # Key: 'r', 'g', 'b'
    passed: bool  # Overall pass/fail
    failures: List[str]  # List of failed metric checks

# ============================================================================
# Per-Pixel Metrics
# ============================================================================

def calculate_per_pixel_metrics(baseline: Frame, candidate: Frame) -> PerPixelMetrics:
    """
    Calculate per-pixel comparison metrics.
    
    Args:
        baseline: Baseline frame
        candidate: Candidate frame
        
    Returns:
        PerPixelMetrics object
    """
    # Ensure same shape
    if baseline.rgb_data.shape != candidate.rgb_data.shape:
        raise ValueError("Frame shapes must match")
    
    # Calculate differences
    diff = candidate.rgb_data.astype(np.int16) - baseline.rgb_data.astype(np.int16)
    
    # L1 delta (sum of absolute differences)
    l1_delta = np.sum(np.abs(diff))
    
    # L2 delta (Euclidean distance)
    l2_delta = np.sqrt(np.sum(diff ** 2))
    
    # Max error
    max_error = int(np.max(np.abs(diff)))
    
    # Mean and std error
    mean_error = float(np.mean(np.abs(diff)))
    std_error = float(np.std(diff))
    
    # PSNR (Peak Signal-to-Noise Ratio)
    mse = np.mean(diff ** 2)
    if mse == 0:
        psnr = float('inf')
    else:
        psnr = 20 * np.log10(255.0 / np.sqrt(mse))
    
    return PerPixelMetrics(
        l1_delta=l1_delta,
        l2_delta=l2_delta,
        max_error=max_error,
        psnr=psnr,
        mean_error=mean_error,
        std_error=std_error
    )

# ============================================================================
# Edge Amplitude Metrics
# ============================================================================

def calculate_luma(rgb: np.ndarray) -> np.ndarray:
    """Calculate BT.601 perceptual luminance"""
    return 0.299 * rgb[:, 0] + 0.587 * rgb[:, 1] + 0.114 * rgb[:, 2]

def calculate_edge_amplitude_metrics(frames: List[Frame]) -> EdgeAmplitudeMetrics:
    """
    Calculate edge amplitude relationship metrics for a sequence of frames.
    
    Args:
        frames: List of frames to analyze
        
    Returns:
        EdgeAmplitudeMetrics object
    """
    if not frames:
        raise ValueError("Frame list cannot be empty")
    
    # Calculate luma for each strip in each frame
    i1_values = []
    i2_values = []
    ratios = []
    
    for frame in frames:
        # Calculate mean luma per strip
        luma1 = calculate_luma(frame.strip1)
        luma2 = calculate_luma(frame.strip2)
        
        i1_mean = np.mean(luma1)
        i2_mean = np.mean(luma2)
        
        i1_values.append(i1_mean)
        i2_values.append(i2_mean)
        
        # Calculate ratio (avoid division by zero)
        if i2_mean > 0:
            ratio = i1_mean / i2_mean
            ratios.append(ratio)
        else:
            ratios.append(float('inf') if i1_mean > 0 else 1.0)
    
    i1_values = np.array(i1_values)
    i2_values = np.array(i2_values)
    ratios = np.array(ratios)
    
    # Calculate spatial symmetry (correlation between left/right halves)
    # For each frame, compare left half of strip1 vs right half of strip1
    symmetry_values = []
    for frame in frames:
        left_half = frame.strip1[0:80]  # LEDs 0-79
        right_half = frame.strip1[80:160]  # LEDs 80-159
        
        left_luma = calculate_luma(left_half)
        right_luma = calculate_luma(right_half)
        
        # Correlation coefficient
        if np.std(left_luma) > 0 and np.std(right_luma) > 0:
            correlation = np.corrcoef(left_luma, right_luma)[0, 1]
            symmetry_values.append(correlation)
        else:
            symmetry_values.append(0.0)
    
    symmetry = np.mean(symmetry_values) if symmetry_values else 0.0
    
    # Ratio stability (coefficient of variation)
    ratio_mean = np.mean(ratios)
    ratio_std = np.std(ratios)
    ratio_stability = (ratio_std / ratio_mean) if ratio_mean > 0 else 0.0
    
    return EdgeAmplitudeMetrics(
        i1_mean=float(np.mean(i1_values)),
        i2_mean=float(np.mean(i2_values)),
        i1_i2_ratio=float(ratio_mean),
        ratio_stability=float(ratio_stability),
        spatial_symmetry=float(symmetry)
    )

# ============================================================================
# Temporal Metrics
# ============================================================================

def calculate_temporal_metrics(frames: List[Frame], low_value_threshold: int = 50) -> TemporalMetrics:
    """
    Calculate temporal stability metrics.
    
    Args:
        frames: List of frames to analyze
        low_value_threshold: Threshold for "low value" pixels (for trail persistence)
        
    Returns:
        TemporalMetrics object
    """
    if len(frames) < 2:
        raise ValueError("Need at least 2 frames for temporal analysis")
    
    # Frame-to-frame delta (energy of differences)
    frame_deltas = []
    for i in range(1, len(frames)):
        diff = frames[i].rgb_data.astype(np.int16) - frames[i-1].rgb_data.astype(np.int16)
        energy = np.sum(diff ** 2)
        frame_deltas.append(energy)
    
    mean_frame_delta = float(np.mean(frame_deltas)) if frame_deltas else 0.0
    
    # Trail persistence (lifetime of low-value pixels)
    # Track how long pixels stay below threshold
    trail_lifetimes = []
    
    for pixel_idx in range(320):
        lifetime = 0
        max_lifetime = 0
        
        for frame in frames:
            # Calculate luma for this pixel
            rgb = frame.rgb_data[pixel_idx]
            luma = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]
            
            if luma < low_value_threshold:
                lifetime += 1
            else:
                if lifetime > 0:
                    trail_lifetimes.append(lifetime)
                    max_lifetime = max(max_lifetime, lifetime)
                lifetime = 0
        
        # Add final lifetime if still active
        if lifetime > 0:
            trail_lifetimes.append(lifetime)
    
    if trail_lifetimes:
        q25 = float(np.percentile(trail_lifetimes, 25))
        q50 = float(np.percentile(trail_lifetimes, 50))
        q75 = float(np.percentile(trail_lifetimes, 75))
    else:
        q25 = q50 = q75 = 0.0
    
    return TemporalMetrics(
        frame_to_frame_delta=mean_frame_delta,
        trail_persistence_q25=q25,
        trail_persistence_q50=q50,
        trail_persistence_q75=q75
    )

# ============================================================================
# Channel Transfer Curves
# ============================================================================

def calculate_transfer_curve(baseline_frames: List[Frame], candidate_frames: List[Frame], channel: str) -> ChannelTransferCurve:
    """
    Calculate empirical transfer curve (input→output mapping) for a channel.
    
    Args:
        baseline_frames: Baseline frames (input)
        candidate_frames: Candidate frames (output)
        channel: Channel name ('r', 'g', or 'b')
        
    Returns:
        ChannelTransferCurve object
    """
    if len(baseline_frames) != len(candidate_frames):
        raise ValueError("Frame lists must have same length")
    
    channel_idx = {'r': 0, 'g': 1, 'b': 2}[channel]
    
    # Collect input→output pairs
    input_output_pairs = []
    
    for baseline, candidate in zip(baseline_frames, candidate_frames):
        baseline_channel = baseline.rgb_data[:, channel_idx].flatten()
        candidate_channel = candidate.rgb_data[:, channel_idx].flatten()
        
        for inp, out in zip(baseline_channel, candidate_channel):
            input_output_pairs.append((int(inp), int(out)))
    
    # Create histogram (input value → mean output value)
    input_bins = np.arange(0, 256)
    output_sums = np.zeros(256)
    output_counts = np.zeros(256)
    
    for inp, out in input_output_pairs:
        output_sums[inp] += out
        output_counts[inp] += 1
    
    # Calculate mean output per input
    output_means = np.zeros(256)
    for i in range(256):
        if output_counts[i] > 0:
            output_means[i] = output_sums[i] / output_counts[i]
        else:
            output_means[i] = i  # No change if no data
    
    # Estimate gamma (fit to power curve: output = input^gamma)
    # Use values where we have data and output > 0
    valid_mask = (output_counts > 0) & (output_means > 0) & (input_bins > 0)
    if np.sum(valid_mask) > 10:
        valid_inputs = input_bins[valid_mask] / 255.0
        valid_outputs = output_means[valid_mask] / 255.0
        
        # Fit: log(output) = gamma * log(input)
        # Use linear regression on log space
        log_inputs = np.log(valid_inputs + 1e-10)
        log_outputs = np.log(valid_outputs + 1e-10)
        
        # Linear fit: y = a*x + b, where a = gamma
        if len(log_inputs) > 1 and np.std(log_inputs) > 0:
            gamma_estimate = float(np.polyfit(log_inputs, log_outputs, 1)[0])
        else:
            gamma_estimate = 1.0
    else:
        gamma_estimate = 1.0
    
    return ChannelTransferCurve(
        input_values=input_bins,
        output_values=output_means,
        gamma_estimate=gamma_estimate
    )

# ============================================================================
# Comparison Functions
# ============================================================================

def compare_baseline_candidate(
    baseline_frames: List[Frame],
    candidate_frames: List[Frame],
    tap_id: int,
    thresholds: Optional[Dict] = None
) -> AnalysisResults:
    """
    Compare baseline vs candidate frames and compute all metrics.
    
    Args:
        baseline_frames: List of baseline frames
        candidate_frames: List of candidate frames
        tap_id: Tap ID (0=Tap A, 1=Tap B, 2=Tap C)
        thresholds: Optional custom thresholds for pass/fail
        
    Returns:
        AnalysisResults object
    """
    if not baseline_frames or not candidate_frames:
        raise ValueError("Frame lists cannot be empty")
    
    # Default thresholds
    if thresholds is None:
        thresholds = {
            'tap_a_exact_match': True,  # Tap A must match exactly
            'tap_b_low_value_preservation': 0.5,  # Values 10-50 must not be reduced by >50%
            'tap_b_edge_ratio_drift': 0.05,  # I₁/I₂ ratio drift <5%
            'tap_b_psnr_min': 30.0,  # Minimum PSNR
            'tap_c_consistency': True,  # Tap C must be consistent with Tap B + FastLED rules
        }
    
    # Per-pixel metrics (compare first frames as example)
    per_pixel = calculate_per_pixel_metrics(baseline_frames[0], candidate_frames[0])
    
    # Edge amplitude metrics
    baseline_edge = calculate_edge_amplitude_metrics(baseline_frames)
    candidate_edge = calculate_edge_amplitude_metrics(candidate_frames)
    
    # Calculate edge metrics for candidate (relative to baseline)
    edge_amplitude = EdgeAmplitudeMetrics(
        i1_mean=candidate_edge.i1_mean,
        i2_mean=candidate_edge.i2_mean,
        i1_i2_ratio=candidate_edge.i1_i2_ratio,
        ratio_stability=candidate_edge.ratio_stability,
        spatial_symmetry=candidate_edge.spatial_symmetry
    )
    
    # Ratio drift (how much did I₁/I₂ change?)
    ratio_drift = abs(candidate_edge.i1_i2_ratio - baseline_edge.i1_i2_ratio) / baseline_edge.i1_i2_ratio if baseline_edge.i1_i2_ratio > 0 else 0.0
    edge_amplitude.ratio_stability = ratio_drift  # Reuse field for drift
    
    # Temporal metrics (skip if single frame)
    if len(candidate_frames) >= 2:
        temporal = calculate_temporal_metrics(candidate_frames)
    else:
        temporal = TemporalMetrics(
            frame_to_frame_delta=0.0,
            trail_persistence_q25=0.0,
            trail_persistence_q50=0.0,
            trail_persistence_q75=0.0
        )
    
    # Transfer curves
    transfer_curves = {}
    for channel in ['r', 'g', 'b']:
        transfer_curves[channel] = calculate_transfer_curve(baseline_frames, candidate_frames, channel)
    
    # Pass/fail evaluation
    passed = True
    failures = []
    
    if tap_id == 0:  # Tap A (pre-correction)
        # Must match exactly
        if per_pixel.l1_delta > 0:
            passed = False
            failures.append(f"Tap A: Expected exact match, but L1 delta = {per_pixel.l1_delta}")
    
    elif tap_id == 1:  # Tap B (post-correction)
        # Low-value preservation check
        # (Check if values 10-50 in baseline are preserved in candidate)
        baseline_low = baseline_frames[0].rgb_data[(baseline_frames[0].rgb_data >= 10) & (baseline_frames[0].rgb_data <= 50)]
        candidate_low = candidate_frames[0].rgb_data[(baseline_frames[0].rgb_data >= 10) & (baseline_frames[0].rgb_data <= 50)]
        
        if len(baseline_low) > 0 and len(candidate_low) > 0:
            reduction = 1.0 - (np.mean(candidate_low) / np.mean(baseline_low))
            if reduction > thresholds['tap_b_low_value_preservation']:
                passed = False
                failures.append(f"Tap B: Low values reduced by {reduction*100:.1f}% (threshold: {thresholds['tap_b_low_value_preservation']*100:.1f}%)")
        
        # Edge ratio drift check
        if ratio_drift > thresholds['tap_b_edge_ratio_drift']:
            passed = False
            failures.append(f"Tap B: Edge ratio drift {ratio_drift*100:.1f}% (threshold: {thresholds['tap_b_edge_ratio_drift']*100:.1f}%)")
        
        # PSNR check
        if per_pixel.psnr < thresholds['tap_b_psnr_min']:
            passed = False
            failures.append(f"Tap B: PSNR {per_pixel.psnr:.1f} dB (threshold: {thresholds['tap_b_psnr_min']:.1f} dB)")
    
    elif tap_id == 2:  # Tap C (pre-WS2812)
        # Must be consistent with Tap B + FastLED rules
        # (This is more complex and may require FastLED correction values)
        pass
    
    return AnalysisResults(
        per_pixel=per_pixel,
        edge_amplitude=edge_amplitude,
        temporal=temporal,
        transfer_curves=transfer_curves,
        passed=passed,
        failures=failures
    )

# ============================================================================
# Main (for testing)
# ============================================================================

if __name__ == "__main__":
    import sys
    from frame_parser import parse_serial_dump
    
    if len(sys.argv) < 3:
        print("Usage: analyse.py <baseline_dump> <candidate_dump>")
        sys.exit(1)
    
    baseline_file = sys.argv[1]
    candidate_file = sys.argv[2]
    
    # Parse frames
    baseline_frames = parse_serial_dump(baseline_file)
    candidate_frames = parse_serial_dump(candidate_file)
    
    if not baseline_frames or not candidate_frames:
        print("Error: Could not parse frames")
        sys.exit(1)
    
    # Get tap ID from first frame
    tap_id = baseline_frames[0].metadata.tap_id
    
    # Compare
    results = compare_baseline_candidate(baseline_frames, candidate_frames, tap_id)
    
    # Print results
    print(f"\n=== Analysis Results (Tap {tap_id}) ===")
    print(f"Per-Pixel Metrics:")
    print(f"  L1 Delta: {results.per_pixel.l1_delta:.1f}")
    print(f"  L2 Delta: {results.per_pixel.l2_delta:.1f}")
    print(f"  Max Error: {results.per_pixel.max_error}")
    print(f"  PSNR: {results.per_pixel.psnr:.2f} dB")
    print(f"\nEdge Amplitude Metrics:")
    print(f"  I₁/I₂ Ratio: {results.edge_amplitude.i1_i2_ratio:.3f}")
    print(f"  Ratio Drift: {results.edge_amplitude.ratio_stability*100:.2f}%")
    print(f"  Spatial Symmetry: {results.edge_amplitude.spatial_symmetry:.3f}")
    print(f"\nTemporal Metrics:")
    print(f"  Frame-to-Frame Delta: {results.temporal.frame_to_frame_delta:.1f}")
    print(f"  Trail Persistence (Q50): {results.temporal.trail_persistence_q50:.1f} frames")
    print(f"\nTransfer Curves:")
    for channel, curve in results.transfer_curves.items():
        print(f"  {channel.upper()}: gamma = {curve.gamma_estimate:.2f}")
    print(f"\nOverall: {'PASS' if results.passed else 'FAIL'}")
    if results.failures:
        print(f"Failures:")
        for failure in results.failures:
            print(f"  - {failure}")

