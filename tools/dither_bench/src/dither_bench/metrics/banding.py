"""Banding measurement tools.

Quantifies visible banding artifacts in quantised output.
"""

import numpy as np
from scipy import stats


def measure_banding(frames: np.ndarray) -> dict:
    """
    Measure banding artifacts in frame sequence.
    
    Analyzes spatial derivatives to detect abrupt transitions.
    
    Args:
        frames: shape (num_frames, num_leds, 3), uint8
    
    Returns:
        dict with metrics:
        - step_count: Number of discrete steps detected
        - derivative_std: Standard deviation of spatial derivatives
        - entropy: Shannon entropy of value distribution
        - max_jump: Maximum single-LED transition
    """
    if frames.ndim == 2:
        # Single frame, add dimension
        frames = frames[np.newaxis, ...]
    
    num_frames, num_leds, num_channels = frames.shape
    
    # Compute spatial derivatives (LED-to-LED transitions)
    derivatives = np.diff(frames, axis=1)  # shape: (num_frames, num_leds-1, 3)
    
    # Metrics
    step_count = _count_discrete_steps(frames)
    derivative_std = np.std(derivatives)
    entropy = _shannon_entropy(frames)
    max_jump = np.max(np.abs(derivatives))
    
    # Banding score: high derivative variance + low entropy = banding
    # Normalize to [0..1] where 1 = severe banding
    banding_score = (derivative_std / 50.0) * (1.0 - min(entropy / 8.0, 1.0))
    
    return {
        'step_count': int(step_count),
        'derivative_std': float(derivative_std),
        'entropy': float(entropy),
        'max_jump': int(max_jump),
        'banding_score': float(np.clip(banding_score, 0.0, 1.0)),
    }


def _count_discrete_steps(frames: np.ndarray) -> int:
    """
    Count number of discrete intensity levels.
    
    Fewer unique values = more posterisation/banding.
    """
    # Average across frames and channels
    mean_frame = np.mean(frames, axis=(0, 2))  # Average across frames and channels
    
    # Count unique values
    unique_values = np.unique(mean_frame)
    return len(unique_values)


def _shannon_entropy(frames: np.ndarray) -> float:
    """
    Calculate Shannon entropy of value distribution.
    
    Low entropy = few distinct values = banding.
    High entropy = many distinct values = smooth.
    """
    # Flatten to 1D histogram
    flat = frames.flatten()
    
    # Build histogram
    hist, _ = np.histogram(flat, bins=256, range=(0, 256), density=True)
    
    # Shannon entropy
    hist = hist[hist > 0]  # Remove zero bins
    entropy = -np.sum(hist * np.log2(hist))
    
    return entropy
