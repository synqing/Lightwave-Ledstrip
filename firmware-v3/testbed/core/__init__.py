"""
LightwaveOS Testbed Core — PyTorch implementations of ESP32 firmware physics.

Modules:
    transport_core: BeatPulseTransportCore PDE engine (advection + diffusion + edge sink)
    render_utils: Ring profiles, blend modes, tone mapping, beat timing helpers
    context: AudioContext / EffectContext dataclasses + sweep generators
"""

from .transport_core import BeatPulseTransportCore
from .render_utils import (
    clamp01, lerp, floatToByte, scaleBrightness,
    RingProfile, BlendMode, BeatPulseHTML, BeatPulseTiming,
)
from .context import (
    AudioContext, EffectContext,
    AudioContextGenerator, EffectContextSweeper,
)

__all__ = [
    'BeatPulseTransportCore',
    'clamp01', 'lerp', 'floatToByte', 'scaleBrightness',
    'RingProfile', 'BlendMode', 'BeatPulseHTML', 'BeatPulseTiming',
    'AudioContext', 'EffectContext',
    'AudioContextGenerator', 'EffectContextSweeper',
]
