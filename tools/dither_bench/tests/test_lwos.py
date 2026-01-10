"""Unit tests for LWOS Bayer and temporal dithering."""

import pytest
import numpy as np
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from dither_bench.quantisers.lwos import LWOSBayerDither, LWOSTemporalModel, BAYER_4X4


class TestLWOSBayerDither:
    """Test LWOS Bayer 4×4 ordered dithering."""
    
    def test_initialization(self):
        """Test Bayer dithering initializes correctly."""
        dither = LWOSBayerDither()
        assert dither is not None
    
    def test_bayer_matrix_shape(self):
        """Test Bayer matrix has correct shape."""
        assert BAYER_4X4.shape == (4, 4)
        assert BAYER_4X4.dtype == np.uint8
    
    def test_bayer_matrix_values(self):
        """Test Bayer matrix contains expected values."""
        expected = np.array([
            [ 0,  8,  2, 10],
            [12,  4, 14,  6],
            [ 3, 11,  1,  9],
            [15,  7, 13,  5]
        ], dtype=np.uint8)
        np.testing.assert_array_equal(BAYER_4X4, expected)
    
    def test_oracle_vs_vectorised_match(self):
        """Test that oracle and vectorised implementations match."""
        dither = LWOSBayerDither()
        
        np.random.seed(42)
        
        for _ in range(10):
            # Generate random test input
            test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
            
            result_oracle = dither.apply_oracle(test_input.copy())
            result_vectorised = dither.apply_vectorised(test_input.copy())
            
            np.testing.assert_array_equal(result_oracle, result_vectorised)
    
    def test_no_modification_when_low_nibble_zero(self):
        """Test that values with low nibble = 0 are not modified."""
        dither = LWOSBayerDither()
        
        # Values divisible by 16 (low nibble = 0)
        test_input = np.array([
            [0, 16, 32],
            [48, 64, 80],
            [96, 112, 128],
            [144, 160, 176]
        ], dtype=np.uint8)
        
        result = dither.apply_oracle(test_input)
        
        # Should be unchanged (low nibble = 0, never > any threshold)
        np.testing.assert_array_equal(result, test_input)
    
    def test_saturation_at_255(self):
        """Test that 255 values are never incremented."""
        dither = LWOSBayerDither()
        
        test_input = np.full((4, 3), 255, dtype=np.uint8)
        
        result = dither.apply_oracle(test_input)
        
        # Should remain 255
        np.testing.assert_array_equal(result, test_input)
    
    def test_spatial_pattern_variation(self):
        """Test that spatial pattern varies correctly."""
        dither = LWOSBayerDither()
        
        # Create uniform input with low nibble = 15 (maximum)
        # This will exceed all thresholds except 15
        test_input = np.full((16, 3), 0x0F, dtype=np.uint8)  # Low nibble = 15
        
        result = dither.apply_oracle(test_input)
        
        # Most should be incremented to 0x10, except where threshold = 15
        # Verify spatial variation exists
        assert np.unique(result[:, 0]).size > 1  # Not all same value
    
    def test_low_nibble_extraction(self):
        """Test low nibble extraction and comparison."""
        dither = LWOSBayerDither()
        
        # Create values with specific low nibbles
        test_input = np.array([
            [0x10, 0x21, 0x32],  # Low nibbles: 0, 1, 2
            [0x43, 0x54, 0x65],  # Low nibbles: 3, 4, 5
            [0x76, 0x87, 0x98],  # Low nibbles: 6, 7, 8
            [0xA9, 0xBA, 0xCB],  # Low nibbles: 9, A, B
        ], dtype=np.uint8)
        
        result = dither.apply_oracle(test_input)
        
        # Verify some values incremented based on threshold
        assert not np.array_equal(result, test_input)  # Should have changes
    
    def test_determinism(self):
        """Test that same input produces same output."""
        dither = LWOSBayerDither()
        
        test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
        
        result1 = dither.apply_oracle(test_input.copy())
        result2 = dither.apply_oracle(test_input.copy())
        
        np.testing.assert_array_equal(result1, result2)


class TestLWOSTemporalModel:
    """Test LWOS FastLED temporal dithering model."""
    
    def test_initialization(self):
        """Test temporal model initializes correctly."""
        model = LWOSTemporalModel()
        assert model.frame_counter == 0
    
    def test_reset(self):
        """Test state reset functionality."""
        model = LWOSTemporalModel()
        
        # Advance state
        test_input = np.zeros((4, 3), dtype=np.uint8)
        model.apply_oracle(test_input)
        model.apply_oracle(test_input)
        
        assert model.frame_counter == 2
        
        # Reset
        model.reset()
        assert model.frame_counter == 0
    
    def test_oracle_vs_vectorised_match(self):
        """Test that oracle and vectorised implementations match."""
        np.random.seed(42)
        
        for _ in range(10):
            test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
            
            model_oracle = LWOSTemporalModel()
            model_vectorised = LWOSTemporalModel()
            
            result_oracle = model_oracle.apply_oracle(test_input.copy())
            result_vectorised = model_vectorised.apply_vectorised(test_input.copy())
            
            np.testing.assert_array_equal(result_oracle, result_vectorised)
    
    def test_frame_alternation(self):
        """Test that dithering alternates between frames."""
        model = LWOSTemporalModel()
        
        # Create input with mid-range low nibbles (should toggle on even frames)
        test_input = np.array([
            [0x15, 0x26, 0x37],  # Low nibbles: 5, 6, 7 (in range 4-11)
        ], dtype=np.uint8)
        
        # Frame 0 (even): should toggle
        result0 = model.apply_oracle(test_input.copy())
        
        # Frame 1 (odd): should not toggle
        result1 = model.apply_oracle(test_input.copy())
        
        # Frame 0 should differ from input
        assert not np.array_equal(result0, test_input)
        
        # Frame 1 should match input
        np.testing.assert_array_equal(result1, test_input)
    
    def test_lsb_toggle(self):
        """Test LSB toggling behavior."""
        model = LWOSTemporalModel()
        
        # Value with low nibble = 5 (should toggle on even frames)
        test_input = np.array([[0x25, 0x25, 0x25]], dtype=np.uint8)
        
        result = model.apply_oracle(test_input)
        
        # Should toggle LSB: 0x25 -> 0x24
        expected = np.array([[0x24, 0x24, 0x24]], dtype=np.uint8)
        np.testing.assert_array_equal(result, expected)
    
    def test_no_toggle_outside_range(self):
        """Test that values outside 4-11 range are not toggled."""
        model = LWOSTemporalModel()
        
        # Values with low nibbles outside 4-11
        test_input = np.array([
            [0x10, 0x21, 0x32],  # Low nibbles: 0, 1, 2
            [0x4C, 0x5D, 0x6E],  # Low nibbles: C, D, E
        ], dtype=np.uint8)
        
        result = model.apply_oracle(test_input)
        
        # Should remain unchanged
        np.testing.assert_array_equal(result, test_input)
    
    def test_frame_counter_increment(self):
        """Test that frame counter increments correctly."""
        model = LWOSTemporalModel()
        
        test_input = np.zeros((4, 3), dtype=np.uint8)
        
        for i in range(10):
            assert model.frame_counter == i
            model.apply_oracle(test_input)
        
        assert model.frame_counter == 10
    
    def test_determinism_per_frame(self):
        """Test that same input on same frame produces same output."""
        model1 = LWOSTemporalModel()
        model2 = LWOSTemporalModel()
        
        test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
        
        result1 = model1.apply_oracle(test_input.copy())
        result2 = model2.apply_oracle(test_input.copy())
        
        np.testing.assert_array_equal(result1, result2)
    
    def test_get_state(self):
        """Test state retrieval."""
        model = LWOSTemporalModel()
        
        state = model.get_state()
        assert state['frame_counter'] == 0
        
        test_input = np.zeros((4, 3), dtype=np.uint8)
        model.apply_oracle(test_input)
        
        state = model.get_state()
        assert state['frame_counter'] == 1


class TestLWOSCombined:
    """Test combined Bayer + Temporal dithering."""
    
    def test_bayer_then_temporal(self):
        """Test applying Bayer followed by temporal."""
        bayer = LWOSBayerDither()
        temporal = LWOSTemporalModel()
        
        test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
        
        # Apply Bayer first
        after_bayer = bayer.apply_oracle(test_input)
        
        # Then apply temporal
        after_temporal = temporal.apply_oracle(after_bayer)
        
        # Verify both stages had effect
        assert not np.array_equal(after_bayer, test_input)  # Bayer changed something
        # Temporal may or may not change (depends on values and frame)
    
    def test_pipeline_consistency(self):
        """Test that full pipeline is consistent across runs."""
        test_input = np.random.randint(0, 256, size=(160, 3), dtype=np.uint8)
        
        # Run 1
        bayer1 = LWOSBayerDither()
        temporal1 = LWOSTemporalModel()
        result1 = temporal1.apply_oracle(bayer1.apply_oracle(test_input.copy()))
        
        # Run 2
        bayer2 = LWOSBayerDither()
        temporal2 = LWOSTemporalModel()
        result2 = temporal2.apply_oracle(bayer2.apply_oracle(test_input.copy()))
        
        np.testing.assert_array_equal(result1, result2)
