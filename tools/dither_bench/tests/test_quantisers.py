"""Unit tests for dithering quantiser implementations."""

import pytest
import numpy as np
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from dither_bench.quantisers.sensorybridge import SensoryBridgeQuantiser, DITHER_TABLE
from dither_bench.quantisers.emotiscope import EmotiscopeQuantiser, DITHER_ERROR_THRESHOLD


class TestSensoryBridgeQuantiser:
    """Test SensoryBridge 4-phase temporal threshold quantiser."""
    
    def test_initialization(self):
        """Test quantiser initialises with correct state."""
        q = SensoryBridgeQuantiser()
        assert q.noise_origin_r == 0
        assert q.noise_origin_g == 0
        assert q.noise_origin_b == 0
        assert q.dither_step == 0
    
    def test_reset(self):
        """Test state reset functionality."""
        q = SensoryBridgeQuantiser()
        
        # Advance state
        test_input = np.zeros((4, 3), dtype=np.float32)
        q.quantise_oracle(test_input)
        
        # Verify state changed
        assert q.dither_step == 1
        assert q.noise_origin_r == 1
        
        # Reset
        q.reset()
        assert q.noise_origin_r == 0
        assert q.noise_origin_g == 0
        assert q.noise_origin_b == 0
        assert q.dither_step == 0
    
    def test_oracle_vs_vectorised_match(self):
        """Test that oracle and vectorised implementations produce identical results."""
        num_leds = 160
        num_frames = 32
        
        np.random.seed(42)
        
        for frame_idx in range(num_frames):
            # Generate random test input
            test_input = np.random.rand(num_leds, 3).astype(np.float32)
            
            # Create two quantisers with same initial state
            q_oracle = SensoryBridgeQuantiser()
            q_vectorised = SensoryBridgeQuantiser()
            
            # Sync state
            q_oracle.noise_origin_r = frame_idx * 11
            q_oracle.noise_origin_g = frame_idx * 13
            q_oracle.noise_origin_b = frame_idx * 17
            q_oracle.dither_step = frame_idx % 4
            
            q_vectorised.noise_origin_r = frame_idx * 11
            q_vectorised.noise_origin_g = frame_idx * 13
            q_vectorised.noise_origin_b = frame_idx * 17
            q_vectorised.dither_step = frame_idx % 4
            
            # Run both implementations
            result_oracle = q_oracle.quantise_oracle(test_input)
            result_vectorised = q_vectorised.quantise_vectorised(test_input)
            
            # Results must match exactly
            np.testing.assert_array_equal(
                result_oracle,
                result_vectorised,
                err_msg=f"Mismatch at frame {frame_idx}"
            )
            
            # State must match exactly
            assert q_oracle.noise_origin_r == q_vectorised.noise_origin_r
            assert q_oracle.noise_origin_g == q_vectorised.noise_origin_g
            assert q_oracle.noise_origin_b == q_vectorised.noise_origin_b
            assert q_oracle.dither_step == q_vectorised.dither_step
    
    def test_edge_case_zero(self):
        """Test quantisation of pure black (0.0)."""
        q = SensoryBridgeQuantiser()
        test_input = np.zeros((4, 3), dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # 0.0 * 254 = 0.0, fract = 0.0
        # 0.0 < any threshold in DITHER_TABLE, so no rounding up
        expected = np.zeros((4, 3), dtype=np.uint8)
        np.testing.assert_array_equal(result, expected)
    
    def test_edge_case_one(self):
        """Test quantisation of pure white (1.0)."""
        q = SensoryBridgeQuantiser()
        test_input = np.ones((4, 3), dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # 1.0 * 254 = 254.0, fract = 0.0
        # 0.0 < any threshold, so output = 254
        expected = np.full((4, 3), 254, dtype=np.uint8)
        np.testing.assert_array_equal(result, expected)
    
    def test_fractional_boundaries_near_thresholds(self):
        """Test quantisation behavior near dither thresholds."""
        q = SensoryBridgeQuantiser()
        
        # Create values with fractional parts near thresholds
        # DITHER_TABLE = [0.25, 0.50, 0.75, 1.00]
        # noise_origin starts at 0, increments to 1 on first frame
        
        # Frame 1: noise_origin_r = 1
        # LED 0: threshold_idx = (1 + 0) % 4 = 1 → threshold = 0.50
        # LED 1: threshold_idx = (1 + 1) % 4 = 2 → threshold = 0.75
        # LED 2: threshold_idx = (1 + 2) % 4 = 3 → threshold = 1.00
        # LED 3: threshold_idx = (1 + 3) % 4 = 0 → threshold = 0.25
        
        # Create input with fractional parts that straddle thresholds
        # Using exact fractional values: (whole + fract) / 254.0
        test_input = np.array([
            [(10.0 + 0.49)/254.0, 0.0, 0.0],  # fract = 0.49 < 0.50 → no round up
            [(10.0 + 0.75)/254.0, 0.0, 0.0],  # fract = 0.75 >= 0.75 → round up
            [(10.0 + 0.99)/254.0, 0.0, 0.0],  # fract = 0.99 < 1.00 → no round up
            [(10.0 + 0.25)/254.0, 0.0, 0.0],  # fract = 0.25 >= 0.25 → round up
        ], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # Expected: [10, 11, 10, 11]
        assert result[0, 0] == 10
        assert result[1, 0] == 11
        assert result[2, 0] == 10
        assert result[3, 0] == 11
    
    def test_254_scale_factor(self):
        """Verify 254 scale factor (not 255) is used."""
        q = SensoryBridgeQuantiser()
        
        # Test a value that would differ between 254 and 255 scaling
        test_input = np.array([[1.0, 1.0, 1.0]], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # With 254 scale: 1.0 * 254 = 254.0, whole=254, fract=0.0 → output=254
        # With 255 scale: 1.0 * 255 = 255.0, whole=255, fract=0.0 → output=255
        assert result[0, 0] == 254
        assert result[0, 1] == 254
        assert result[0, 2] == 254
    
    def test_per_channel_noise_origins(self):
        """Test that each channel has independent noise origin."""
        q = SensoryBridgeQuantiser()
        
        # Create input where fractional part is sensitive to threshold
        # fract ≈ 0.5, so will round up if threshold <= 0.5
        test_value = 10.5 / 254.0
        test_input = np.full((4, 3), test_value, dtype=np.float32)
        
        # Run first frame
        result1 = q.quantise_oracle(test_input)
        
        # After frame 1: noise origins are all 1
        # LED 0: all channels have threshold_idx = (1+0)%4 = 1 → threshold = 0.50
        # With fract = 0.5, should have 0.5 >= 0.5 → round up to 11
        assert result1[0, 0] == 11  # R
        assert result1[0, 1] == 11  # G
        assert result1[0, 2] == 11  # B
        
        # Run second frame
        result2 = q.quantise_oracle(test_input)
        
        # After frame 2: noise origins are all 2
        # LED 0: threshold_idx = (2+0)%4 = 2 → threshold = 0.75
        # With fract = 0.5, should have 0.5 < 0.75 → no round up, stays 10
        assert result2[0, 0] == 10  # R
        assert result2[0, 1] == 10  # G
        assert result2[0, 2] == 10  # B
    
    def test_spatial_variation(self):
        """Test that spatial dither pattern varies across LEDs."""
        q = SensoryBridgeQuantiser()
        
        # Create uniform input with fractional part
        test_value = (10.0 + 0.5) / 254.0  # fract = 0.5
        test_input = np.zeros((4, 3), dtype=np.float32)
        test_input[:, 0] = test_value  # Only R channel, G and B are 0
        
        result = q.quantise_oracle(test_input)
        
        # After first frame: noise_origin_r = 1
        # LED 0: (1+0)%4 = 1 → threshold = 0.50 → 0.5 >= 0.5 → 11
        # LED 1: (1+1)%4 = 2 → threshold = 0.75 → 0.5 < 0.75 → 10
        # LED 2: (1+2)%4 = 3 → threshold = 1.00 → 0.5 < 1.00 → 10
        # LED 3: (1+3)%4 = 0 → threshold = 0.25 → 0.5 >= 0.25 → 11
        
        assert result[0, 0] == 11
        assert result[1, 0] == 10
        assert result[2, 0] == 10
        assert result[3, 0] == 11
    
    def test_temporal_cycling(self):
        """Test that temporal pattern cycles over frames."""
        q = SensoryBridgeQuantiser()
        
        # Create input with fractional part sensitive to thresholds
        test_value = 10.5 / 254.0
        test_input = np.array([[test_value, test_value, test_value]], dtype=np.float32)
        
        results = []
        for _ in range(8):  # Run 8 frames
            result = q.quantise_oracle(test_input)
            results.append(result[0, 0])  # Track R channel of LED 0
        
        # noise_origin cycles: 0→1→2→3→4→5→6→7→...
        # LED 0, frame N: threshold_idx = (N+1) % 4
        # Frame 0: (0+1)%4=1 → 0.50 → 11
        # Frame 1: (1+1)%4=2 → 0.75 → 10
        # Frame 2: (2+1)%4=3 → 1.00 → 10
        # Frame 3: (3+1)%4=0 → 0.25 → 11
        # Frame 4: (4+1)%4=1 → 0.50 → 11 (cycle repeats)
        
        expected = [11, 10, 10, 11, 11, 10, 10, 11]
        assert results == expected
    
    def test_no_dither_path(self):
        """Test the no-dither quantisation path."""
        q = SensoryBridgeQuantiser()
        
        test_input = np.array([
            [0.0, 0.5, 1.0],
            [0.1, 0.9, 0.01],
        ], dtype=np.float32)
        
        result = q.quantise_no_dither(test_input)
        
        # Simple: uint8(value * 255)
        expected = np.array([
            [0, 127, 255],
            [25, 229, 2],
        ], dtype=np.uint8)
        
        np.testing.assert_array_equal(result, expected)
    
    def test_determinism(self):
        """Test that same input and state produces same output."""
        test_input = np.random.rand(160, 3).astype(np.float32)
        
        q1 = SensoryBridgeQuantiser()
        q2 = SensoryBridgeQuantiser()
        
        result1 = q1.quantise_oracle(test_input)
        result2 = q2.quantise_oracle(test_input)
        
        np.testing.assert_array_equal(result1, result2)
    
    def test_state_persistence(self):
        """Test that state persists correctly across frames."""
        q = SensoryBridgeQuantiser()
        
        initial_state = q.get_state()
        assert initial_state['noise_origin_r'] == 0
        assert initial_state['dither_step'] == 0
        
        # Run one frame
        test_input = np.zeros((4, 3), dtype=np.float32)
        q.quantise_oracle(test_input)
        
        state_after_1 = q.get_state()
        assert state_after_1['noise_origin_r'] == 1
        assert state_after_1['noise_origin_g'] == 1
        assert state_after_1['noise_origin_b'] == 1
        assert state_after_1['dither_step'] == 1
        
        # Run more frames
        for _ in range(3):
            q.quantise_oracle(test_input)
        
        state_after_4 = q.get_state()
        assert state_after_4['noise_origin_r'] == 4
        assert state_after_4['dither_step'] == 0  # Wrapped back to 0
    
    def test_noise_origin_wrap(self):
        """Test that noise origin wraps at 256."""
        q = SensoryBridgeQuantiser()
        q.noise_origin_r = 255
        q.noise_origin_g = 255
        q.noise_origin_b = 255
        
        test_input = np.zeros((4, 3), dtype=np.float32)
        q.quantise_oracle(test_input)
        
        # Should wrap to 0
        assert q.noise_origin_r == 0
        assert q.noise_origin_g == 0
        assert q.noise_origin_b == 0
