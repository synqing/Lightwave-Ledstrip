"""
SensoryBridge 4-phase Temporal Threshold Quantiser

Exact replication of the dithering algorithm from SensoryBridge 4.1.1.

Reference: /Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
           Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/led_utilities.h

Algorithm Overview:
-------------------
1. Multiply float RGB [0..1] by 254 (not 255!) to get decimal representation
2. Split into whole and fractional parts
3. Compare fractional part against dither_table[(noise_origin + led_index) % 4]
4. If fract >= threshold, round up (+1)
5. Per-frame: increment noise_origin_r/g/b by 1 (wraps at 256)
6. dither_step cycles 0→1→2→3→0 per frame (not used in threshold comparison)

Key Differences from Other Algorithms:
- Uses 254 scale factor (not 255) for quantisation
- Per-channel noise origins shift independently each frame
- 4-phase threshold pattern: [0.25, 0.50, 0.75, 1.00]
- Spatial pattern varies per LED: (noise_origin + i) % 4
- Explicitly disables FastLED.setDither(false) before show()

Assumptions:
- Input: float RGB in [0..1] range (matches leds_scaled[] buffer type)
- Output: uint8 RGB in [0..254] range (can reach 254, rarely 255)
- State: noise_origin per channel persists across frames, wraps at 256
"""

import numpy as np
from typing import Tuple


# Constants from SensoryBridge constants.h
DITHER_TABLE = np.array([0.25, 0.50, 0.75, 1.00], dtype=np.float32)


class SensoryBridgeQuantiser:
    """
    SensoryBridge 4-phase temporal threshold quantiser.
    
    Provides both oracle (line-by-line) and vectorised implementations.
    """
    
    def __init__(self):
        """Initialise quantiser state."""
        # Per-channel noise origins (static in original, persist across frames)
        self.noise_origin_r = 0
        self.noise_origin_g = 0
        self.noise_origin_b = 0
        
        # Global dither step (cycles 0→3, not used in threshold but maintained for completeness)
        self.dither_step = 0
        
    def reset(self):
        """Reset quantiser state to initial conditions."""
        self.noise_origin_r = 0
        self.noise_origin_g = 0
        self.noise_origin_b = 0
        self.dither_step = 0
    
    def quantise_oracle(self, leds_float: np.ndarray) -> np.ndarray:
        """
        Oracle implementation: line-by-line faithful to SensoryBridge source.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..254]
        """
        num_leds = leds_float.shape[0]
        leds_out = np.zeros((num_leds, 3), dtype=np.uint8)
        
        # Update dither step (cycles 0→3)
        self.dither_step += 1
        if self.dither_step >= 4:
            self.dither_step = 0
        
        # Increment noise origins (wraps at 256 naturally via uint8)
        self.noise_origin_r = (self.noise_origin_r + 1) % 256
        self.noise_origin_g = (self.noise_origin_g + 1) % 256
        self.noise_origin_b = (self.noise_origin_b + 1) % 256
        
        for i in range(num_leds):
            # RED channel
            decimal_r = leds_float[i, 0] * 254.0
            whole_r = int(decimal_r)
            fract_r = decimal_r - whole_r
            
            threshold_idx_r = (self.noise_origin_r + i) % 4
            if fract_r >= DITHER_TABLE[threshold_idx_r]:
                whole_r += 1
            
            leds_out[i, 0] = np.clip(whole_r, 0, 255)
            
            # GREEN channel
            decimal_g = leds_float[i, 1] * 254.0
            whole_g = int(decimal_g)
            fract_g = decimal_g - whole_g
            
            threshold_idx_g = (self.noise_origin_g + i) % 4
            if fract_g >= DITHER_TABLE[threshold_idx_g]:
                whole_g += 1
            
            leds_out[i, 1] = np.clip(whole_g, 0, 255)
            
            # BLUE channel
            decimal_b = leds_float[i, 2] * 254.0
            whole_b = int(decimal_b)
            fract_b = decimal_b - whole_b
            
            threshold_idx_b = (self.noise_origin_b + i) % 4
            if fract_b >= DITHER_TABLE[threshold_idx_b]:
                whole_b += 1
            
            leds_out[i, 2] = np.clip(whole_b, 0, 255)
        
        return leds_out
    
    def quantise_vectorised(self, leds_float: np.ndarray) -> np.ndarray:
        """
        Vectorised implementation: NumPy-optimised version of quantise_oracle.
        
        Must produce identical results to oracle implementation.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..254]
        """
        num_leds = leds_float.shape[0]
        
        # Update dither step
        self.dither_step = (self.dither_step + 1) % 4
        
        # Increment noise origins
        self.noise_origin_r = (self.noise_origin_r + 1) % 256
        self.noise_origin_g = (self.noise_origin_g + 1) % 256
        self.noise_origin_b = (self.noise_origin_b + 1) % 256
        
        # Scale to 254 (not 255!)
        decimal = leds_float * 254.0
        
        # Split into whole and fractional parts
        whole = np.floor(decimal).astype(np.int32)
        fract = decimal - whole
        
        # Generate threshold indices for each LED and channel
        led_indices = np.arange(num_leds)
        
        threshold_idx_r = (self.noise_origin_r + led_indices) % 4
        threshold_idx_g = (self.noise_origin_g + led_indices) % 4
        threshold_idx_b = (self.noise_origin_b + led_indices) % 4
        
        # Lookup thresholds
        thresholds = np.zeros((num_leds, 3), dtype=np.float32)
        thresholds[:, 0] = DITHER_TABLE[threshold_idx_r]
        thresholds[:, 1] = DITHER_TABLE[threshold_idx_g]
        thresholds[:, 2] = DITHER_TABLE[threshold_idx_b]
        
        # Apply dithering: if fract >= threshold, round up
        dither_mask = (fract >= thresholds).astype(np.int32)
        result = whole + dither_mask
        
        # Clip to [0, 255] and convert to uint8
        leds_out = np.clip(result, 0, 255).astype(np.uint8)
        
        return leds_out
    
    def quantise_no_dither(self, leds_float: np.ndarray) -> np.ndarray:
        """
        No-dither path from SensoryBridge (when CONFIG.TEMPORAL_DITHERING == false).
        
        Simple truncation to uint8 with 255 scale factor.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8
        """
        # Direct conversion: uint8(value * 255)
        leds_out = np.clip(leds_float * 255.0, 0, 255).astype(np.uint8)
        return leds_out
    
    def get_state(self) -> dict:
        """Get current quantiser state for debugging/verification."""
        return {
            'noise_origin_r': self.noise_origin_r,
            'noise_origin_g': self.noise_origin_g,
            'noise_origin_b': self.noise_origin_b,
            'dither_step': self.dither_step,
        }
