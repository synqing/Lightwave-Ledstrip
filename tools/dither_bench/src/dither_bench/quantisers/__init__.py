"""
Quantiser/Dithering Algorithm Implementations

Each module provides both:
- Oracle implementation (line-by-line faithful to source)
- Optimised implementation (vectorised NumPy)

With unit tests to ensure they match exactly.
"""

from .sensorybridge import SensoryBridgeQuantiser
from .emotiscope import EmotiscopeQuantiser
from .lwos import LWOSBayerDither, LWOSTemporalModel

__all__ = [
    "SensoryBridgeQuantiser",
    "EmotiscopeQuantiser",
    "LWOSBayerDither",
    "LWOSTemporalModel",
]
