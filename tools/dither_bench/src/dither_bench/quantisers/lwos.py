"""
LWOS Bayer Ordered Dithering + FastLED Temporal Model

Exact replication of the dithering used in LightwaveOS v2.

Reference: /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/
           v2/src/effects/enhancement/ColorCorrectionEngine.cpp

Algorithm Overview:
-------------------

### Bayer 4×4 Ordered Dithering

Applied after gamma correction (gamma 2.2 LUT) to break up banding in low bits:

1. For each LED at position `i`:
   - Compute Bayer threshold: `BAYER_4X4[i % 4][(i / 4) % 4]`
   - Extract low nibble of each channel: `(c.r & 0x0F)`
   - If low_nibble > threshold and value < 255: increment by 1

This creates a spatial 4×4 dither pattern that breaks up posterisation.

**Threshold Matrix (Bayer 4×4):**
```
 0   8   2  10
12   4  14   6
 3  11   1   9
15   7  13   5
```

### FastLED Temporal Dithering (Modelled)

FastLED.setDither(1) enables temporal dithering at the driver level:
- Alternates LSB on/off across frames
- Spreads quantisation error over time
- Creates time-averaged brightness perception

We model this as a simple frame-alternating LSB toggle.

Key Characteristics:
- **Spatial pattern**: Bayer matrix creates structured noise
- **Bit-level**: Operates on lowest 4 bits (breaks up 16-level quantisation)
- **Non-destructive**: Only rounds up, never down
- **Saturation aware**: Never exceeds 255
- **Independent channels**: R, G, B processed separately

Assumptions:
- Input: uint8 RGB in [0..255] range (already quantised from float/16-bit)
- Output: uint8 RGB in [0..255] range (with Bayer dithering applied)
- Spatial: LED index determines threshold
- Temporal: Frame counter determines LSB flip (FastLED temporal model)
"""

import numpy as np


# Bayer 4×4 matrix from LWOS ColorCorrectionEngine.cpp
BAYER_4X4 = np.array([
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5]
], dtype=np.uint8)


class LWOSBayerDither:
    """
    LWOS Bayer 4×4 ordered dithering.
    
    Provides both oracle (line-by-line) and vectorised implementations.
    """
    
    def __init__(self):
        """Initialise Bayer dithering (stateless)."""
        pass
    
    def apply_oracle(self, leds_uint8: np.ndarray) -> np.ndarray:
        """
        Oracle implementation: line-by-line faithful to LWOS source.
        
        Args:
            leds_uint8: Input LED buffer, shape (num_leds, 3), uint8, range [0..255]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        num_leds = leds_uint8.shape[0]
        leds_out = leds_uint8.copy()
        
        for i in range(num_leds):
            # Get Bayer threshold based on LED position
            threshold = BAYER_4X4[i % 4][(i // 4) % 4]
            
            # Apply ordered dithering: if low nibble exceeds threshold, round up
            # RED
            low_nibble_r = leds_out[i, 0] & 0x0F
            if low_nibble_r > threshold and leds_out[i, 0] < 255:
                leds_out[i, 0] += 1
            
            # GREEN
            low_nibble_g = leds_out[i, 1] & 0x0F
            if low_nibble_g > threshold and leds_out[i, 1] < 255:
                leds_out[i, 1] += 1
            
            # BLUE
            low_nibble_b = leds_out[i, 2] & 0x0F
            if low_nibble_b > threshold and leds_out[i, 2] < 255:
                leds_out[i, 2] += 1
        
        return leds_out
    
    def apply_vectorised(self, leds_uint8: np.ndarray) -> np.ndarray:
        """
        Vectorised implementation: NumPy-optimised version of apply_oracle.
        
        Must produce identical results to oracle implementation.
        
        Args:
            leds_uint8: Input LED buffer, shape (num_leds, 3), uint8, range [0..255]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        num_leds = leds_uint8.shape[0]
        leds_out = leds_uint8.copy()
        
        # Generate threshold for each LED
        led_indices = np.arange(num_leds)
        row_indices = led_indices % 4
        col_indices = (led_indices // 4) % 4
        thresholds = BAYER_4X4[row_indices, col_indices]
        
        # Broadcast thresholds to shape (num_leds, 3)
        thresholds_3d = np.repeat(thresholds[:, np.newaxis], 3, axis=1)
        
        # Extract low nibbles
        low_nibbles = leds_out & 0x0F
        
        # Create mask: low_nibble > threshold AND value < 255
        dither_mask = (low_nibbles > thresholds_3d) & (leds_out < 255)
        
        # Apply dithering
        leds_out = leds_out + dither_mask.astype(np.uint8)
        
        return leds_out


class LWOSTemporalModel:
    """
    Model of FastLED temporal dithering.
    
    FastLED.setDither(1) alternates LSB on/off across frames for temporal spreading.
    This is a simplified model for comparison purposes.
    """
    
    def __init__(self):
        """Initialise temporal dithering state."""
        self.frame_counter = 0
    
    def reset(self):
        """Reset frame counter."""
        self.frame_counter = 0
    
    def apply_oracle(self, leds_uint8: np.ndarray) -> np.ndarray:
        """
        Oracle implementation: simple frame-alternating LSB toggle.
        
        Args:
            leds_uint8: Input LED buffer, shape (num_leds, 3), uint8, range [0..255]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        leds_out = leds_uint8.copy()
        
        # On even frames, toggle LSB for values with fractional component
        if self.frame_counter % 2 == 0:
            # For values with low nibble in mid-range (4-11), toggle LSB
            for i in range(leds_out.shape[0]):
                for c in range(3):
                    low_nibble = leds_out[i, c] & 0x0F
                    if 4 <= low_nibble <= 11:
                        # Toggle LSB
                        leds_out[i, c] ^= 0x01
        
        self.frame_counter += 1
        return leds_out
    
    def apply_vectorised(self, leds_uint8: np.ndarray) -> np.ndarray:
        """
        Vectorised implementation: NumPy-optimised version of apply_oracle.
        
        Must produce identical results to oracle implementation.
        
        Args:
            leds_uint8: Input LED buffer, shape (num_leds, 3), uint8, range [0..255]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        leds_out = leds_uint8.copy()
        
        # On even frames, toggle LSB for values with fractional component
        if self.frame_counter % 2 == 0:
            low_nibbles = leds_out & 0x0F
            toggle_mask = (low_nibbles >= 4) & (low_nibbles <= 11)
            leds_out = np.where(toggle_mask, leds_out ^ 0x01, leds_out)
        
        self.frame_counter += 1
        return leds_out
    
    def get_state(self) -> dict:
        """Get current temporal state for debugging/verification."""
        return {
            'frame_counter': self.frame_counter,
        }
