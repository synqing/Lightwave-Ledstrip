"""Test stimulus generators (stub)."""

import numpy as np


def generate_ramp(num_leds: int, **kwargs) -> np.ndarray:
    """Stub - will be implemented later."""
    return np.zeros((num_leds, 3), dtype=np.float32)


def generate_constant_field(num_leds: int, **kwargs) -> np.ndarray:
    """Stub - will be implemented later."""
    return np.zeros((num_leds, 3), dtype=np.float32)


def generate_lgp_gradient(num_leds: int, **kwargs) -> np.ndarray:
    """Stub - will be implemented later."""
    return np.zeros((num_leds, 3), dtype=np.float32)


def generate_palette_blend(num_leds: int, **kwargs) -> np.ndarray:
    """Stub - will be implemented later."""
    return np.zeros((num_leds, 3), dtype=np.float32)
