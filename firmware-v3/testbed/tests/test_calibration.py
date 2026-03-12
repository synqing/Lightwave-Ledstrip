"""Tests for calibration metrics and bridge.

Tests comparison functions and the calibration bridge workflow.
"""

import pytest
import numpy as np

try:
    import torch
except ImportError:
    torch = None


@pytest.mark.skipif(torch is None, reason="PyTorch not installed")
class TestCalibrationMetrics:
    """Test suite for calibration metric functions."""

    def test_per_pixel_l2_perfect_match(self):
        """Test per-pixel L2 for identical outputs."""
        from testbed.calibration import per_pixel_l2

        output = np.random.rand(10, 80, 3).astype(np.float32)
        error = per_pixel_l2(output, output)

        assert error < 1e-6, "Identical outputs should have near-zero L2"

    def test_per_pixel_l2_unity(self):
        """Test per-pixel L2 with unit difference."""
        from testbed.calibration import per_pixel_l2

        output_a = np.ones((10, 80, 3), dtype=np.float32)
        output_b = np.zeros((10, 80, 3), dtype=np.float32)

        error = per_pixel_l2(output_a, output_b)

        # Max diff in each channel is 1.0, so per-pixel dist = sqrt(3) * 1
        # normalized by sqrt(3) should give 1.0 max error
        assert 0.9 < error < 1.1, f"Expected normalized L2 ~1.0, got {error}"

    def test_cosine_similarity_perfect_match(self):
        """Test cosine similarity for identical outputs."""
        from testbed.calibration import cosine_similarity

        output = np.random.rand(10, 80, 3).astype(np.float32)
        sim = cosine_similarity(output, output)

        assert sim > 0.99, "Identical outputs should have cosine similarity ~1.0"

    def test_cosine_similarity_orthogonal(self):
        """Test cosine similarity for orthogonal vectors."""
        from testbed.calibration import cosine_similarity

        # Create orthogonal outputs (different pattern)
        output_a = np.ones((1, 80, 3), dtype=np.float32)
        output_b = np.ones((1, 80, 3), dtype=np.float32)
        output_b[:, :, :] = 0
        output_b[:, :, 0] = 1  # Only red channel

        # These are not perfectly orthogonal due to overlap, but should differ
        sim = cosine_similarity(output_a, output_b)
        assert not np.isnan(sim) and not np.isinf(sim), "Cosine sim should be finite"

    def test_energy_total_sum(self):
        """Test energy computation as sum of all values."""
        from testbed.calibration import energy_total

        output = np.ones((80, 3), dtype=np.float32)
        energy = energy_total(output)

        expected = 80 * 3 * 1.0
        assert abs(energy - expected) < 1e-5, f"Expected {expected}, got {energy}"

    def test_energy_divergence_zero(self):
        """Test energy divergence for identical trajectories."""
        from testbed.calibration import energy_divergence

        trajectory = np.random.rand(100, 80, 3).astype(np.float32)
        div = energy_divergence(trajectory, trajectory, n_frames=100)

        assert div < 1e-5, "Identical trajectories should have zero divergence"

    def test_tone_map_match_rate_perfect(self):
        """Test tone map matching for identical tone-mapped outputs."""
        from testbed.calibration import tone_map_match_rate

        # Create identical float and uint8 outputs
        float_output = np.random.rand(10, 80, 3).astype(np.float32)
        uint8_ref = (float_output * 255).astype(np.uint8).astype(np.float32) / 255.0

        rate = tone_map_match_rate(float_output, (uint8_ref * 255).astype(np.uint8))

        # Should have high agreement
        assert rate > 0.9, f"Expected high match rate, got {rate}"

    def test_tone_map_match_rate_bounds(self):
        """Test that tone map match rate is in [0, 1]."""
        from testbed.calibration import tone_map_match_rate

        float_output = np.random.rand(10, 80, 3).astype(np.float32)
        uint8_ref = np.random.randint(0, 256, size=(10, 80, 3), dtype=np.uint8)

        rate = tone_map_match_rate(float_output, uint8_ref)

        assert 0.0 <= rate <= 1.0, f"Match rate should be in [0,1], got {rate}"


@pytest.mark.skipif(torch is None, reason="PyTorch not installed")
class TestCalibrationBridge:
    """Test suite for CalibrationBridge class."""

    def test_bridge_creation(self):
        """Test CalibrationBridge instantiation."""
        from testbed.calibration import CalibrationBridge

        bridge = CalibrationBridge(reference_path="/tmp/nonexistent.npz")
        assert bridge is not None
        assert bridge.reference_params is None

    def test_calibration_report_pass_fail(self):
        """Test CalibrationReport pass/fail logic."""
        from testbed.calibration import CalibrationReport, CalibrationMetrics

        report = CalibrationReport()
        report.metrics = CalibrationMetrics(
            per_pixel_l2=0.005,
            energy_divergence=0.5,
            tone_map_agreement=0.995,
        )

        assert report.passes_all(), "Report with good metrics should pass"

        report.metrics.per_pixel_l2 = 0.02
        assert not report.passes_all(), "Report with high L2 should fail"
