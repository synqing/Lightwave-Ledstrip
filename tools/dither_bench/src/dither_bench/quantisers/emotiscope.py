"""
Emotiscope Sigma-Delta Error-Accumulation Quantiser

Exact replication of the temporal dithering algorithm from Emotiscope 1.2/2.0.

Reference: /Users/spectrasynq/Workspace_Management/Software/K1.node1/references/
           Emotiscope.sourcecode/Emotiscope-1.2/src/led_driver.h

Algorithm Overview:
-------------------
1. Scale float RGB [0..1] to [0..255] range
2. Truncate to uint8 (not round!)
3. Calculate quantisation error: error = float_value - uint8_value
4. If new error >= threshold (0.055), accumulate it into persistent state
5. If accumulated error >= 1.0, add 1 to output and subtract 1.0 from error
6. Error state persists per-LED, per-channel across frames

Key Characteristics:
- **1st-order sigma-delta modulator**: Integrates quantisation error over time
- **Deadband threshold (0.055)**: Ignores tiny errors to avoid noise accumulation
- **Persistent state**: `dither_error[i].{r,g,b}` carries forward between frames
- **Per-LED independence**: Each LED has its own error accumulator
- **Scale factor 255** (not 254 like SensoryBridge)
- **Truncation, not rounding**: Uses `(uint8_t)value` cast

Differences from SensoryBridge:
- Spatial: No spatial variation (threshold is constant per LED)
- Temporal: Error accumulates over time, not phase-based thresholds
- State size: 3 floats per LED (vs 3 uint8 global counters for SB)
- Convergence: Tends toward correct time-averaged brightness
- Deadband: Small errors ignored to prevent drift

Assumptions:
- Input: float RGB in [0..1] range
- Output: uint8 RGB in [0..255] range
- State: float error per LED per channel, initialized randomly in [0..1)
- Threshold: 0.055 (from Emotiscope 1.2; earlier versions used different values)
"""

import numpy as np
from typing import Optional


# Deadband threshold from Emotiscope 1.2
DITHER_ERROR_THRESHOLD = 0.055


class EmotiscopeQuantiser:
    """
    Emotiscope sigma-delta error-accumulation quantiser.
    
    Provides both oracle (line-by-line) and vectorised implementations.
    """
    
    def __init__(self, num_leds: int, seed: Optional[int] = None):
        """
        Initialise quantiser with per-LED error state.
        
        Args:
            num_leds: Number of LEDs in the strip
            seed: Random seed for error initialisation (for reproducibility)
        """
        self.num_leds = num_leds
        
        # Per-LED, per-channel error accumulator (persistent state)
        # Initialize randomly in [0..1) to match init_random_dither_error()
        if seed is not None:
            np.random.seed(seed)
        
        self.dither_error = np.random.rand(num_leds, 3).astype(np.float32)
    
    def reset(self, seed: Optional[int] = None):
        """Reset error state to random initial conditions."""
        if seed is not None:
            np.random.seed(seed)
        self.dither_error = np.random.rand(self.num_leds, 3).astype(np.float32)
    
    def quantise_oracle(self, leds_float: np.ndarray) -> np.ndarray:
        """
        Oracle implementation: line-by-line faithful to Emotiscope source.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        num_leds = leds_float.shape[0]
        leds_out = np.zeros((num_leds, 3), dtype=np.uint8)
        
        # Scale to [0..255] range (matches dsps_mulc_f32_ansi in original)
        leds_scaled = leds_float * 255.0
        
        for i in range(num_leds):
            # Truncate to uint8 (not round!)
            quantised_r = int(leds_scaled[i, 0])
            quantised_g = int(leds_scaled[i, 1])
            quantised_b = int(leds_scaled[i, 2])
            
            # Calculate quantisation error
            new_error_r = leds_scaled[i, 0] - quantised_r
            new_error_g = leds_scaled[i, 1] - quantised_g
            new_error_b = leds_scaled[i, 2] - quantised_b
            
            # Accumulate error if above deadband threshold
            if new_error_r >= DITHER_ERROR_THRESHOLD:
                self.dither_error[i, 0] += new_error_r
            if new_error_g >= DITHER_ERROR_THRESHOLD:
                self.dither_error[i, 1] += new_error_g
            if new_error_b >= DITHER_ERROR_THRESHOLD:
                self.dither_error[i, 2] += new_error_b
            
            # If accumulated error >= 1.0, emit extra photon and subtract
            if self.dither_error[i, 0] >= 1.0:
                quantised_r += 1
                self.dither_error[i, 0] -= 1.0
            if self.dither_error[i, 1] >= 1.0:
                quantised_g += 1
                self.dither_error[i, 1] -= 1.0
            if self.dither_error[i, 2] >= 1.0:
                quantised_b += 1
                self.dither_error[i, 2] -= 1.0
            
            # Clip to [0, 255] and store
            leds_out[i, 0] = np.clip(quantised_r, 0, 255)
            leds_out[i, 1] = np.clip(quantised_g, 0, 255)
            leds_out[i, 2] = np.clip(quantised_b, 0, 255)
        
        return leds_out
    
    def quantise_vectorised(self, leds_float: np.ndarray) -> np.ndarray:
        """
        Vectorised implementation: NumPy-optimised version of quantise_oracle.
        
        Must produce identical results to oracle implementation.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        # Scale to [0..255]
        leds_scaled = leds_float * 255.0
        
        # Truncate to integer (matches (uint8_t) cast)
        quantised = np.floor(leds_scaled).astype(np.int32)
        
        # Calculate quantisation error
        new_error = leds_scaled - quantised
        
        # Accumulate error if above deadband threshold
        error_mask = (new_error >= DITHER_ERROR_THRESHOLD)
        self.dither_error += new_error * error_mask
        
        # If accumulated error >= 1.0, emit extra photon
        carry_mask = (self.dither_error >= 1.0).astype(np.int32)
        quantised += carry_mask
        self.dither_error -= carry_mask.astype(np.float32)
        
        # Clip to [0, 255] and convert to uint8
        leds_out = np.clip(quantised, 0, 255).astype(np.uint8)
        
        return leds_out
    
    def quantise_no_dither(self, leds_float: np.ndarray) -> np.ndarray:
        """
        No-dither path from Emotiscope (when temporal_dithering == false).
        
        Simple rounding to uint8 with 255 scale factor.
        
        Args:
            leds_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8
        """
        # Direct conversion with rounding: round(value * 255)
        leds_out = np.round(leds_float * 255.0).astype(np.uint8)
        return leds_out
    
    def get_state(self) -> dict:
        """Get current error state for debugging/verification."""
        return {
            'dither_error_mean': np.mean(self.dither_error, axis=0).tolist(),
            'dither_error_max': np.max(self.dither_error, axis=0).tolist(),
            'dither_error_min': np.min(self.dither_error, axis=0).tolist(),
        }
