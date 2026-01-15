"""Unit tests for Emotiscope quantiser."""

import pytest
import numpy as np
import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from dither_bench.quantisers.emotiscope import EmotiscopeQuantiser, DITHER_ERROR_THRESHOLD


class TestEmotiscopeQuantiser:
    """Test Emotiscope sigma-delta error-accumulation quantiser."""
    
    def test_initialization(self):
        """Test quantiser initialises with correct state."""
        q = EmotiscopeQuantiser(num_leds=160, seed=42)
        assert q.num_leds == 160
        assert q.dither_error.shape == (160, 3)
        # Error should be initialized in [0..1)
        assert np.all(q.dither_error >= 0.0)
        assert np.all(q.dither_error < 1.0)
    
    def test_deterministic_initialization(self):
        """Test that same seed produces same initial state."""
        q1 = EmotiscopeQuantiser(num_leds=160, seed=42)
        q2 = EmotiscopeQuantiser(num_leds=160, seed=42)
        
        np.testing.assert_array_equal(q1.dither_error, q2.dither_error)
    
    def test_reset(self):
        """Test state reset functionality."""
        q = EmotiscopeQuantiser(num_leds=4, seed=42)
        
        initial_error = q.dither_error.copy()
        
        # Modify state
        test_input = np.full((4, 3), 0.5, dtype=np.float32)
        q.quantise_oracle(test_input)
        
        # Verify state changed
        assert not np.array_equal(q.dither_error, initial_error)
        
        # Reset with same seed
        q.reset(seed=42)
        np.testing.assert_array_equal(q.dither_error, initial_error)
    
    def test_oracle_vs_vectorised_match(self):
        """Test that oracle and vectorised implementations produce identical results."""
        num_leds = 160
        num_frames = 32
        
        np.random.seed(123)
        
        for frame_idx in range(num_frames):
            # Generate random test input
            test_input = np.random.rand(num_leds, 3).astype(np.float32)
            
            # Create two quantisers with same initial state
            q_oracle = EmotiscopeQuantiser(num_leds=num_leds, seed=42)
            q_vectorised = EmotiscopeQuantiser(num_leds=num_leds, seed=42)
            
            # Run a few warmup frames to get into varied state
            for _ in range(frame_idx % 5):
                warmup_input = np.random.rand(num_leds, 3).astype(np.float32)
                q_oracle.quantise_oracle(warmup_input)
                q_vectorised.quantise_vectorised(warmup_input)
            
            # Sync state explicitly
            q_vectorised.dither_error = q_oracle.dither_error.copy()
            
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
            np.testing.assert_array_almost_equal(
                q_oracle.dither_error,
                q_vectorised.dither_error,
                decimal=6,
                err_msg=f"State mismatch at frame {frame_idx}"
            )
    
    def test_edge_case_zero(self):
        """Test quantisation of pure black (0.0)."""
        q = EmotiscopeQuantiser(num_leds=4, seed=42)
        q.dither_error.fill(0.0)  # Reset error to zero for predictability
        
        test_input = np.zeros((4, 3), dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # 0.0 * 255 = 0.0, truncate to 0, error = 0.0
        # 0.0 < DITHER_ERROR_THRESHOLD, so no accumulation
        expected = np.zeros((4, 3), dtype=np.uint8)
        np.testing.assert_array_equal(result, expected)
        
        # Error should still be zero
        np.testing.assert_array_equal(q.dither_error, np.zeros((4, 3)))
    
    def test_edge_case_one(self):
        """Test quantisation of pure white (1.0)."""
        q = EmotiscopeQuantiser(num_leds=4, seed=42)
        q.dither_error.fill(0.0)
        
        test_input = np.ones((4, 3), dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # 1.0 * 255 = 255.0, truncate to 255, error = 0.0
        # 0.0 < DITHER_ERROR_THRESHOLD, so no accumulation
        expected = np.full((4, 3), 255, dtype=np.uint8)
        np.testing.assert_array_equal(result, expected)
    
    def test_error_accumulation(self):
        """Test that error accumulates over frames."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)  # Start with zero error
        
        # Input value with fractional part that creates error
        # 0.5 * 255 = 127.5, truncates to 127, error = 0.5
        test_input = np.array([[0.5, 0.5, 0.5]], dtype=np.float32)
        
        # First frame: error = 0.5 >= 0.055, accumulate
        result1 = q.quantise_oracle(test_input)
        assert result1[0, 0] == 127  # No carry yet
        assert abs(q.dither_error[0, 0] - 0.5) < 0.01
        
        # Second frame: error = 0.5 + 0.5 = 1.0, trigger carry
        result2 = q.quantise_oracle(test_input)
        assert result2[0, 0] == 128  # Carry bit emitted
        assert abs(q.dither_error[0, 0] - 0.0) < 0.01  # Error reset
    
    def test_deadband_threshold(self):
        """Test that small errors below threshold are ignored."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)
        
        # Create input with tiny fractional error below deadband
        # Need error < 0.055
        # (10.0 + 0.05) / 255 * 255 = 10.05, truncate to 10, error = 0.05
        value = (10.0 + 0.05) / 255.0
        test_input = np.array([[value, value, value]], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # Error 0.05 < 0.055, should NOT be accumulated
        assert result[0, 0] == 10
        assert abs(q.dither_error[0, 0]) < 0.01  # Still near zero
    
    def test_error_above_deadband(self):
        """Test that errors above threshold are accumulated."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)
        
        # Create input with error just above deadband
        # (10.0 + 0.06) / 255 * 255 = 10.06, truncate to 10, error = 0.06
        value = (10.0 + 0.06) / 255.0
        test_input = np.array([[value, value, value]], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # Error 0.06 >= 0.055, should be accumulated
        assert result[0, 0] == 10
        assert q.dither_error[0, 0] > 0.055  # Error accumulated
    
    def test_per_led_independence(self):
        """Test that each LED has independent error state."""
        q = EmotiscopeQuantiser(num_leds=3, seed=42)
        q.dither_error.fill(0.0)
        
        # Give each LED different error conditions
        test_input = np.array([
            [0.5, 0.0, 0.0],  # LED 0: error = 0.5 on R
            [0.0, 0.5, 0.0],  # LED 1: error = 0.5 on G
            [0.0, 0.0, 0.5],  # LED 2: error = 0.5 on B
        ], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # Each LED should accumulate error only on its active channel
        assert abs(q.dither_error[0, 0] - 0.5) < 0.01  # LED 0 R
        assert abs(q.dither_error[0, 1]) < 0.01        # LED 0 G
        assert abs(q.dither_error[1, 1] - 0.5) < 0.01  # LED 1 G
        assert abs(q.dither_error[2, 2] - 0.5) < 0.01  # LED 2 B
    
    def test_convergence_to_correct_average(self):
        """Test that time-averaged output converges to correct value."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)
        
        # Input value: 127.5 / 255 ≈ 0.5
        # Should output 127 and 128 alternately
        test_input = np.array([[127.5/255.0, 127.5/255.0, 127.5/255.0]], dtype=np.float32)
        
        outputs = []
        for _ in range(100):
            result = q.quantise_oracle(test_input)
            outputs.append(result[0, 0])
        
        # Time-averaged output should be close to 127.5
        mean_output = np.mean(outputs)
        assert abs(mean_output - 127.5) < 1.0
    
    def test_no_dither_path(self):
        """Test the no-dither quantisation path."""
        q = EmotiscopeQuantiser(num_leds=2, seed=42)
        
        test_input = np.array([
            [0.0, 0.5, 1.0],
            [0.1, 0.9, 0.01],
        ], dtype=np.float32)
        
        result = q.quantise_no_dither(test_input)
        
        # Simple rounding: round(value * 255)
        expected = np.array([
            [0, 128, 255],
            [26, 230, 3],
        ], dtype=np.uint8)
        
        np.testing.assert_array_equal(result, expected)
    
    def test_255_scale_factor(self):
        """Verify 255 scale factor (not 254) is used."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)
        
        test_input = np.array([[1.0, 1.0, 1.0]], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # With 255 scale: 1.0 * 255 = 255.0, truncate to 255
        assert result[0, 0] == 255
        assert result[0, 1] == 255
        assert result[0, 2] == 255
    
    def test_truncation_not_rounding(self):
        """Verify that truncation (floor) is used, not rounding."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.0)
        
        # Value that would round up but should truncate down
        # 10.9 / 255 * 255 = 10.9, truncate to 10 (not round to 11)
        value = 10.9 / 255.0
        test_input = np.array([[value, value, value]], dtype=np.float32)
        
        result = q.quantise_oracle(test_input)
        
        # Should truncate to 10, not round to 11
        assert result[0, 0] == 10
        assert result[0, 1] == 10
        assert result[0, 2] == 10
    
    def test_state_persistence(self):
        """Test that error state persists across frames."""
        q = EmotiscopeQuantiser(num_leds=1, seed=42)
        q.dither_error.fill(0.2)  # Start with some initial error
        
        test_input = np.array([[0.5, 0.5, 0.5]], dtype=np.float32)
        
        # First frame: 0.2 + 0.5 = 0.7
        result1 = q.quantise_oracle(test_input)
        assert abs(q.dither_error[0, 0] - 0.7) < 0.01
        
        # Second frame: 0.7 + 0.5 = 1.2, trigger carry, error = 0.2
        result2 = q.quantise_oracle(test_input)
        assert result2[0, 0] == 128  # Carry emitted
        assert abs(q.dither_error[0, 0] - 0.2) < 0.01
    
    def test_determinism(self):
        """Test that same input and state produces same output."""
        test_input = np.random.rand(160, 3).astype(np.float32)
        
        q1 = EmotiscopeQuantiser(num_leds=160, seed=42)
        q2 = EmotiscopeQuantiser(num_leds=160, seed=42)
        
        result1 = q1.quantise_oracle(test_input)
        result2 = q2.quantise_oracle(test_input)
        
        np.testing.assert_array_equal(result1, result2)
