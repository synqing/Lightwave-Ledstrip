"""SensoryBridge pipeline simulator.

Replicates SensoryBridge 4.1.1 rendering pipeline:
1. Brightness scaling
2. Incandescent filter (optional)
3. Clip to [0..1]
4. SensoryBridge 4-phase temporal quantiser
5. FastLED temporal dithering DISABLED (as per original)
"""

import numpy as np
from .base import BasePipeline
from ..quantisers.sensorybridge import SensoryBridgeQuantiser


class SensoryBridgePipeline(BasePipeline):
    """SensoryBridge 4.1.1 complete rendering pipeline."""
    
    def __init__(self, num_leds: int, config: dict):
        """
        Initialize SensoryBridge pipeline.
        
        Args:
            num_leds: Number of LEDs
            config: Configuration dict with keys:
                - brightness: Master brightness [0..1] (default 1.0)
                - incandescent_mix: Incandescent filter mix [0..1] (default 0.0)
                - temporal_enabled: Enable SB quantiser (default True)
        """
        super().__init__(num_leds, config)
        
        self.brightness = config.get('brightness', 1.0)
        self.incandescent_mix = config.get('incandescent_mix', 0.0)
        self.temporal_enabled = config.get('temporal_enabled', True)
        
        self.quantiser = SensoryBridgeQuantiser()
    
    def _apply_incandescent_filter(self, colors: np.ndarray) -> np.ndarray:
        """
        Apply incandescent filter (warm white reduction).
        
        Reduces blue/green slightly for warmer appearance.
        """
        if self.incandescent_mix == 0.0:
            return colors
        
        # Incandescent reduces cool channels
        filtered = colors.copy()
        filtered[:, 1] *= (1.0 - self.incandescent_mix * 0.15)  # Green -15%
        filtered[:, 2] *= (1.0 - self.incandescent_mix * 0.25)  # Blue -25%
        
        return filtered
    
    def process_frame(self, input_float: np.ndarray) -> np.ndarray:
        """
        Process frame through SensoryBridge pipeline.
        
        Pipeline:
        1. Brightness scaling
        2. Incandescent filter
        3. Clip to [0..1]
        4. SensoryBridge quantiser (if enabled)
        
        Args:
            input_float: shape (num_leds, 3), float32, [0..1]
        
        Returns:
            shape (num_leds, 3), uint8, [0..255]
        """
        # Stage 1: Brightness scaling
        scaled = input_float * self.brightness
        
        # Stage 2: Incandescent filter
        filtered = self._apply_incandescent_filter(scaled)
        
        # Stage 3: Clip to [0..1]
        clipped = np.clip(filtered, 0.0, 1.0)
        
        # Stage 4: Quantisation
        if self.temporal_enabled:
            output = self.quantiser.quantise_vectorised(clipped)
        else:
            output = self.quantiser.quantise_no_dither(clipped)
        
        return output
    
    def reset(self):
        """Reset quantiser state."""
        self.quantiser.reset()
    
    def get_name(self) -> str:
        """Get pipeline name."""
        return "SensoryBridge 4.1.1"
    
    def get_description(self) -> str:
        """Get detailed description."""
        stages = [f"Brightness {self.brightness:.2f}"]
        if self.incandescent_mix > 0:
            stages.append(f"Incandescent {self.incandescent_mix:.2f}")
        stages.append("Clip")
        if self.temporal_enabled:
            stages.append("SB 4-phase Quantiser")
        else:
            stages.append("No dither")
        
        return f"SensoryBridge: {' → '.join(stages)}"
