"""LWOS pipeline simulator.

Replicates LightwaveOS v2 rendering pipeline:
1. Gamma correction (2.2 LUT)
2. Float→uint8 quantisation
3. Bayer 4×4 ordered dithering (optional)
4. FastLED temporal dithering (optional, modelled)
"""

import numpy as np
from .base import BasePipeline
from ..quantisers.lwos import LWOSBayerDither, LWOSTemporalModel


class LWOSPipeline(BasePipeline):
    """LightwaveOS v2 complete rendering pipeline."""
    
    def __init__(self, num_leds: int, config: dict):
        """
        Initialize LWOS pipeline.
        
        Args:
            num_leds: Number of LEDs
            config: Configuration dict with keys:
                - gamma: Gamma value (default 2.2)
                - bayer_enabled: Enable Bayer dithering (default True)
                - temporal_enabled: Enable temporal model (default True)
        """
        super().__init__(num_leds, config)
        
        # Build gamma LUT
        self.gamma_value = config.get('gamma', 2.2)
        self.gamma_lut = self._build_gamma_lut(self.gamma_value)
        
        # Dithering stages
        self.bayer_enabled = config.get('bayer_enabled', True)
        self.temporal_enabled = config.get('temporal_enabled', True)
        
        self.bayer = LWOSBayerDither()
        self.temporal = LWOSTemporalModel()
    
    def _build_gamma_lut(self, gamma: float) -> np.ndarray:
        """Build gamma correction LUT."""
        lut = np.zeros(256, dtype=np.uint8)
        for i in range(256):
            normalized = i / 255.0
            corrected = normalized ** gamma
            lut[i] = int(corrected * 255.0 + 0.5)  # Round to nearest
        return lut
    
    def process_frame(self, input_float: np.ndarray) -> np.ndarray:
        """
        Process frame through LWOS pipeline.
        
        Pipeline:
        1. Gamma correction (2.2)
        2. Float→uint8 quantisation
        3. Bayer dithering (if enabled)
        4. Temporal dithering (if enabled)
        
        Args:
            input_float: shape (num_leds, 3), float32, [0..1]
        
        Returns:
            shape (num_leds, 3), uint8, [0..255]
        """
        # Stage 1: Gamma correction via LUT
        # Convert to uint8 index, lookup, back to float
        input_uint8_idx = np.clip(input_float * 255.0, 0, 255).astype(np.uint8)
        gamma_corrected_uint8 = self.gamma_lut[input_uint8_idx]
        
        # Stage 2: Already uint8 from gamma LUT
        output = gamma_corrected_uint8.copy()
        
        # Stage 3: Bayer dithering (spatial)
        if self.bayer_enabled:
            output = self.bayer.apply_vectorised(output)
        
        # Stage 4: Temporal dithering (time-based)
        if self.temporal_enabled:
            output = self.temporal.apply_vectorised(output)
        
        return output
    
    def reset(self):
        """Reset temporal dithering state."""
        self.temporal.reset()
    
    def get_name(self) -> str:
        """Get pipeline name."""
        return "LWOS v2"
    
    def get_description(self) -> str:
        """Get detailed description."""
        stages = []
        stages.append(f"Gamma {self.gamma_value}")
        if self.bayer_enabled:
            stages.append("Bayer 4×4")
        if self.temporal_enabled:
            stages.append("Temporal (FastLED model)")
        
        return f"LWOS v2: {' → '.join(stages)}"
