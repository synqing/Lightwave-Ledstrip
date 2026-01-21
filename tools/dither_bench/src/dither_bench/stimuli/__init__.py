"""Test stimulus/pattern generators for dithering analysis."""

from .generators import (
    generate_ramp,
    generate_constant_field,
    generate_lgp_gradient,
    generate_palette_blend
)

__all__ = [
    "generate_ramp",
    "generate_constant_field",
    "generate_lgp_gradient",
    "generate_palette_blend",
]
