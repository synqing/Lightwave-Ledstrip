"""Calibration module for comparing PyTorch transport core against firmware reference.

Exports:
- CalibrationBridge: Main class for running firmware vs PyTorch comparisons
- Metrics: Per-pixel L2, energy conservation, tone map agreement functions
"""

from .bridge import CalibrationBridge, CalibrationReport
from .metrics import (
    per_pixel_l2,
    cosine_similarity,
    energy_total,
    energy_divergence,
    tone_map_match_rate,
)

__all__ = [
    "CalibrationBridge",
    "CalibrationReport",
    "per_pixel_l2",
    "cosine_similarity",
    "energy_total",
    "energy_divergence",
    "tone_map_match_rate",
]
