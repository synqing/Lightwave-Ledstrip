"""Emotiscope pipeline simulator.

Replicates Emotiscope 1.2/2.0 rendering pipeline:
1. Gamma correction (1.5 or 2.0, via 2048-entry LUT in v2.0)
2. Emotiscope sigma-delta quantiser (error accumulation)
3. RMT transmission (not simulated, output is uint8)
"""

import numpy as np
from .base import BasePipeline
from ..quantisers.emotiscope import EmotiscopeQuantiser


class EmotiscopePipeline(BasePipeline):
    """Emotiscope 1.2/2.0 complete rendering pipeline."""
    
    def __init__(self, num_leds: int, config: dict):
        """
        Initialize Emotiscope pipeline.
        
        Args:
            num_leds: Number of LEDs
            config: Configuration dict with keys:
                - gamma: Gamma value (default 1.5, like Emotiscope 2.0)
                - temporal_enabled: Enable Emotiscope quantiser (default True)
                - seed: Random seed for error initialisation (default 42)
        """
        super().__init__(num_leds, config)
        
        self.gamma_value = config.get('gamma', 1.5)
        self.temporal_enabled = config.get('temporal_enabled', True)
        seed = config.get('seed', 42)
        
        # Build gamma LUT (2048-entry like Emotiscope 2.0)
        self.gamma_lut = self._build_gamma_lut_2048(self.gamma_value)
        
        self.quantiser = EmotiscopeQuantiser(num_leds, seed=seed)
    
    def _build_gamma_lut_2048(self, gamma: float) -> np.ndarray:
        """
        Build 2048-entry gamma LUT (as per Emotiscope 2.0).
        
        Higher resolution than typical 256-entry LUT for smoother response.
        """
        lut = np.zeros(2048, dtype=np.float32)
        for i in range(2048):
            normalized = i / 2047.0
            corrected = normalized ** gamma
            lut[i] = corrected
        return lut
    
    def _apply_gamma(self, input_float: np.ndarray) -> np.ndarray:
        """
        Apply gamma correction via 2048-entry LUT.
        
        Input range [0..1] → LUT index [0..2047] → output [0..1]
        """
        # Map [0..1] to [0..2047]
        indices = np.clip(input_float * 2047.0, 0, 2047).astype(np.int32)
        
        # Lookup
        gamma_corrected = self.gamma_lut[indices]
        
        return gamma_corrected
    
    def process_frame(self, input_float: np.ndarray) -> np.ndarray:
        """
        Process frame through Emotiscope pipeline.
        
        Pipeline:
        1. Gamma correction (2048-entry LUT)
        2. Emotiscope quantiser (if enabled)
        
        Args:
            input_float: shape (num_leds, 3), float32, [0..1]
        
        Returns:
            shape (num_leds, 3), uint8, [0..255]
        """
        # Stage 1: Gamma correction
        gamma_corrected = self._apply_gamma(input_float)
        
        # Stage 2: Quantisation
        if self.temporal_enabled:
            output = self.quantiser.quantise_vectorised(gamma_corrected)
        else:
            output = self.quantiser.quantise_no_dither(gamma_corrected)
        
        return output
    
    def reset(self):
        """Reset quantiser error state."""
        self.quantiser.reset(seed=self.config.get('seed', 42))
    
    def get_name(self) -> str:
        """Get pipeline name."""
        return "Emotiscope 1.2/2.0"
    
    def get_description(self) -> str:
        """Get detailed description."""
        stages = [f"Gamma {self.gamma_value} (2048-LUT)"]
        if self.temporal_enabled:
            stages.append("Emotiscope Sigma-Delta")
        else:
            stages.append("No dither")
        
        return f"Emotiscope: {' → '.join(stages)}"
