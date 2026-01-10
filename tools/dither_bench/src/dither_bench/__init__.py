"""
DitherBench: Quantitative Dithering Assessment Framework

A reproducible harness for comparing LED dithering algorithms:
- LightwaveOS v2 (Bayer 4×4 + FastLED temporal)
- SensoryBridge 4.1.1 (4-phase threshold temporal)
- Emotiscope (sigma-delta error accumulation)
"""

__version__ = "1.0.0"
__author__ = "LightwaveOS Project"

from . import quantisers
from . import pipelines
from . import stimuli
from . import metrics
from . import utils

__all__ = [
    "quantisers",
    "pipelines",
    "stimuli",
    "metrics",
    "utils",
]
