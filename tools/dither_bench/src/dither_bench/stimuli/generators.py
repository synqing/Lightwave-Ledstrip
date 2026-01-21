"""Test stimulus/pattern generators for dithering analysis.

Generates deterministic test stimuli for evaluating dithering algorithms.
"""

import numpy as np
from typing import Optional


def generate_ramp(num_leds: int, channel: str = 'all', start: float = 0.0, 
                  end: float = 1.0, **kwargs) -> np.ndarray:
    """
    Generate linear ramp from start to end.
    
    Critical for testing banding and quantisation artifacts.
    
    Args:
        num_leds: Number of LEDs
        channel: 'r', 'g', 'b', or 'all' (grayscale ramp)
        start: Starting value [0..1]
        end: Ending value [0..1]
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    ramp = np.linspace(start, end, num_leds, dtype=np.float32)
    
    if channel == 'all':
        # Grayscale ramp
        output = np.repeat(ramp[:, np.newaxis], 3, axis=1)
    elif channel == 'r':
        output = np.zeros((num_leds, 3), dtype=np.float32)
        output[:, 0] = ramp
    elif channel == 'g':
        output = np.zeros((num_leds, 3), dtype=np.float32)
        output[:, 1] = ramp
    elif channel == 'b':
        output = np.zeros((num_leds, 3), dtype=np.float32)
        output[:, 2] = ramp
    else:
        raise ValueError(f"Unknown channel: {channel}")
    
    return output


def generate_constant_field(num_leds: int, value: float = 0.5, 
                            color: Optional[tuple] = None, **kwargs) -> np.ndarray:
    """
    Generate constant color field with optional tiny deltas for shimmer testing.
    
    Critical for measuring temporal stability (flicker/shimmer).
    
    Args:
        num_leds: Number of LEDs
        value: Constant value [0..1] (if color not specified)
        color: Optional (r, g, b) tuple [0..1]
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    if color is not None:
        output = np.full((num_leds, 3), color, dtype=np.float32)
    else:
        output = np.full((num_leds, 3), value, dtype=np.float32)
    
    return output


def generate_lgp_gradient(num_leds: int, center: int = 79, **kwargs) -> np.ndarray:
    """
    Generate centre-heavy LGP-like gradient (symmetric around centre).
    
    Mimics LightwaveOS centre-origin pattern.
    
    Args:
        num_leds: Number of LEDs
        center: Center LED index (default 79 for LWOS)
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    output = np.zeros((num_leds, 3), dtype=np.float32)
    
    for i in range(num_leds):
        # Distance from center (normalized)
        dist = abs(i - center) / center
        
        # Gaussian-like falloff from center
        intensity = np.exp(-3.0 * dist**2)
        
        # Apply to all channels (white gradient)
        output[i, :] = intensity
    
    return output


def generate_palette_blend(num_leds: int, colors: Optional[list] = None,
                           **kwargs) -> np.ndarray:
    """
    Generate palette blend across LED strip.
    
    Tests smooth color transitions and hue quantisation.
    
    Args:
        num_leds: Number of LEDs
        colors: List of (r, g, b) tuples [0..1] to blend between
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    if colors is None:
        # Default: Red -> Green -> Blue
        colors = [
            (1.0, 0.0, 0.0),  # Red
            (0.0, 1.0, 0.0),  # Green
            (0.0, 0.0, 1.0),  # Blue
        ]
    
    num_colors = len(colors)
    output = np.zeros((num_leds, 3), dtype=np.float32)
    
    for i in range(num_leds):
        # Position in palette [0..num_colors-1]
        pos = (i / (num_leds - 1)) * (num_colors - 1)
        
        # Indices for interpolation
        idx1 = int(np.floor(pos))
        idx2 = min(idx1 + 1, num_colors - 1)
        
        # Interpolation factor
        frac = pos - idx1
        
        # Blend between colors
        c1 = np.array(colors[idx1], dtype=np.float32)
        c2 = np.array(colors[idx2], dtype=np.float32)
        output[i, :] = c1 * (1.0 - frac) + c2 * frac
    
    return output


def generate_step_function(num_leds: int, num_steps: int = 8, **kwargs) -> np.ndarray:
    """
    Generate stepped gradient (posterised).
    
    Tests dithering effectiveness on pre-quantised input.
    
    Args:
        num_leds: Number of LEDs
        num_steps: Number of discrete levels
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    output = np.zeros((num_leds, 3), dtype=np.float32)
    
    for i in range(num_leds):
        # Quantise position to num_steps levels
        pos = i / (num_leds - 1)
        step = int(pos * num_steps) / num_steps
        output[i, :] = step
    
    return output


def generate_near_black(num_leds: int, max_value: float = 0.1, **kwargs) -> np.ndarray:
    """
    Generate near-black values with tiny variations.
    
    Critical for testing shimmer artifacts in shadows.
    
    Args:
        num_leds: Number of LEDs
        max_value: Maximum brightness [0..1]
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..max_value]
    """
    ramp = np.linspace(0.0, max_value, num_leds, dtype=np.float32)
    output = np.repeat(ramp[:, np.newaxis], 3, axis=1)
    return output


def generate_sine_wave(num_leds: int, frequency: float = 2.0, 
                       phase: float = 0.0, **kwargs) -> np.ndarray:
    """
    Generate sine wave pattern.
    
    Tests smooth transitions and temporal behavior.
    
    Args:
        num_leds: Number of LEDs
        frequency: Number of complete cycles
        phase: Phase offset [0..2π]
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    x = np.linspace(0, 2 * np.pi * frequency, num_leds, dtype=np.float32)
    wave = (np.sin(x + phase) + 1.0) / 2.0  # Map [-1,1] to [0,1]
    
    output = np.repeat(wave[:, np.newaxis], 3, axis=1)
    return output


def generate_random_field(num_leds: int, seed: Optional[int] = None, 
                         mean: float = 0.5, std: float = 0.2, **kwargs) -> np.ndarray:
    """
    Generate random color field (Gaussian distribution).
    
    Tests dithering on noise-like input.
    
    Args:
        num_leds: Number of LEDs
        seed: Random seed for reproducibility
        mean: Mean value [0..1]
        std: Standard deviation
    
    Returns:
        LED buffer, shape (num_leds, 3), float32, range [0..1]
    """
    if seed is not None:
        np.random.seed(seed)
    
    output = np.random.normal(mean, std, size=(num_leds, 3)).astype(np.float32)
    output = np.clip(output, 0.0, 1.0)
    
    return output
