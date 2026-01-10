"""Temporal stability measurement tools.

Quantifies flicker, shimmer, and temporal artifacts.
"""

import numpy as np
from scipy import signal


def measure_temporal_stability(frames: np.ndarray) -> dict:
    """
    Measure temporal stability across frame sequence.
    
    Analyzes per-LED variance and flicker energy.
    
    Args:
        frames: shape (num_frames, num_leds, 3), uint8
    
    Returns:
        dict with metrics:
        - per_led_variance: Mean variance across all LEDs
        - flicker_energy: Total energy in high-frequency components
        - max_led_variance: Worst-case single LED variance
        - temporal_snr: Signal-to-noise ratio (mean/std)
    """
    if frames.ndim == 2:
        raise ValueError("Need multiple frames for temporal analysis")
    
    num_frames, num_leds, num_channels = frames.shape
    
    # Per-LED variance over time
    per_led_variance = np.var(frames, axis=0)  # shape: (num_leds, 3)
    mean_variance = np.mean(per_led_variance)
    max_variance = np.max(per_led_variance)
    
    # Temporal SNR (signal-to-noise ratio)
    mean_values = np.mean(frames, axis=0)
    std_values = np.std(frames, axis=0)
    
    # Avoid division by zero
    with np.errstate(divide='ignore', invalid='ignore'):
        snr = mean_values / (std_values + 1e-6)
        snr = np.where(np.isfinite(snr), snr, 0.0)
    
    temporal_snr = np.mean(snr)
    
    # Flicker energy (high-frequency power)
    flicker_energy = _compute_flicker_energy(frames)
    
    # Stability score: low variance + high SNR = stable
    # Normalize to [0..1] where 1 = perfectly stable
    stability_score = 1.0 / (1.0 + mean_variance / 10.0)
    
    return {
        'per_led_variance': float(mean_variance),
        'flicker_energy': float(flicker_energy),
        'max_led_variance': float(max_variance),
        'temporal_snr': float(temporal_snr),
        'stability_score': float(np.clip(stability_score, 0.0, 1.0)),
    }


def _compute_flicker_energy(frames: np.ndarray) -> float:
    """
    Compute flicker energy via FFT.
    
    High-frequency power indicates visible flicker.
    """
    num_frames, num_leds, num_channels = frames.shape
    
    # Average across LEDs and channels for temporal signal
    temporal_signal = np.mean(frames, axis=(1, 2))
    
    # FFT
    fft = np.fft.rfft(temporal_signal)
    power = np.abs(fft) ** 2
    
    # High-frequency energy (upper half of spectrum)
    cutoff = len(power) // 2
    hf_energy = np.sum(power[cutoff:])
    
    # Normalize by total energy
    total_energy = np.sum(power)
    if total_energy > 0:
        flicker_ratio = hf_energy / total_energy
    else:
        flicker_ratio = 0.0
    
    return flicker_ratio
