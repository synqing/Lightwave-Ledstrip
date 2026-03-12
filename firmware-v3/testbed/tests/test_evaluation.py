"""Tests for the quality evaluation pipeline (L0–L3).

Covers:
    - Frame parser: encode → decode roundtrip
    - L0: black/clipped frame detection, brightness, spatial variance
    - L1: temporal stability, flicker, frequency consistency, PSNR
    - L2: energy correlation, beat alignment, onset response, salience weighting
    - L3: TS3IM trend/variability/structure similarity
    - Pipeline: multiplicative composition, edge cases
"""

import io
import struct

import numpy as np
import pytest

# ---------------------------------------------------------------------------
# Frame parser
# ---------------------------------------------------------------------------

class TestFrameParser:
    """Binary serial v2 encode/decode roundtrip tests."""

    def _make_frame(self, **overrides):
        from testbed.evaluation.frame_parser import (
            CaptureFrame, FrameMetrics, NUM_BANDS,
        )
        defaults = dict(
            version=2, tap=1, effect_id=5, palette_id=3,
            brightness=200, speed=128, frame_index=42,
            timestamp_us=123456,
            rgb=np.random.randint(0, 256, (320, 3), dtype=np.uint8),
            metrics=FrameMetrics(
                show_us=350, rms=0.45, bands=np.random.rand(NUM_BANDS).astype(np.float32),
                beat=True, onset=False, flux=0.6, heap_free=120000,
                show_skips=2, bpm=128.5, tempo_confidence=0.85,
            ),
        )
        defaults.update(overrides)
        return CaptureFrame(**defaults)

    def test_roundtrip_single_frame(self):
        """Encode a frame, decode it, compare fields."""
        from testbed.evaluation.frame_parser import (
            CaptureSequence, save_capture, load_capture,
        )
        import tempfile, os

        frame = self._make_frame()
        seq = CaptureSequence(frames=[frame])

        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            path = f.name

        try:
            save_capture(seq, path)
            loaded = load_capture(path)
            assert loaded.num_frames == 1

            got = loaded.frames[0]
            assert got.version == frame.version
            assert got.tap == frame.tap
            assert got.effect_id == frame.effect_id
            assert got.palette_id == frame.palette_id
            assert got.brightness == frame.brightness
            assert got.speed == frame.speed
            assert got.frame_index == frame.frame_index
            assert got.timestamp_us == frame.timestamp_us
            np.testing.assert_array_equal(got.rgb, frame.rgb)

            # Metrics — quantised, so allow tolerance
            assert abs(got.metrics.rms - frame.metrics.rms) < 0.001
            assert got.metrics.beat == frame.metrics.beat
            assert got.metrics.onset == frame.metrics.onset
            assert abs(got.metrics.flux - frame.metrics.flux) < 0.001
            assert got.metrics.heap_free == frame.metrics.heap_free
            assert abs(got.metrics.bpm - frame.metrics.bpm) < 0.02
        finally:
            os.unlink(path)

    def test_roundtrip_multi_frame(self):
        """Multiple frames survive encode/decode."""
        from testbed.evaluation.frame_parser import (
            CaptureSequence, save_capture, load_capture,
        )
        import tempfile, os

        frames = [self._make_frame(frame_index=i, timestamp_us=i * 8333)
                  for i in range(50)]
        seq = CaptureSequence(frames=frames)

        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            path = f.name

        try:
            save_capture(seq, path)
            loaded = load_capture(path)
            assert loaded.num_frames == 50
            assert loaded.frames[49].frame_index == 49
        finally:
            os.unlink(path)

    def test_sequence_accessors(self):
        """CaptureSequence numpy accessors produce correct shapes."""
        from testbed.evaluation.frame_parser import CaptureSequence

        frames = [self._make_frame(frame_index=i, timestamp_us=i * 8333)
                  for i in range(20)]
        seq = CaptureSequence(frames=frames)

        assert seq.rgb_array().shape == (20, 320, 3)
        assert seq.rgb_float().dtype == np.float32
        assert seq.rgb_float().max() <= 1.0
        assert seq.rms_array().shape == (20,)
        assert seq.beat_array().shape == (20,)
        assert seq.bands_array().shape == (20, 8)
        assert seq.timestamp_array().shape == (20,)

    def test_timestamp_wrap_handling(self):
        """uint32 timestamp wrap-around handled correctly."""
        from testbed.evaluation.frame_parser import CaptureSequence

        frames = [
            self._make_frame(frame_index=0, timestamp_us=2**32 - 1000),
            self._make_frame(frame_index=1, timestamp_us=500),  # wrapped
        ]
        seq = CaptureSequence(frames=frames)

        ts = seq.timestamp_array()
        assert ts[0] == 0.0
        assert ts[1] > 0  # should be ~1500 us = 0.0015s
        assert ts[1] < 0.01  # not a huge negative number


# ---------------------------------------------------------------------------
# L0 — Frame quality
# ---------------------------------------------------------------------------

class TestL0:
    """L0 per-frame quality metrics."""

    def test_black_frames_detected(self):
        from testbed.evaluation.pipeline import evaluate_l0

        # 100 frames, 50 are black
        frames = np.random.rand(100, 320, 3).astype(np.float32) * 0.3
        frames[50:, :, :] = 0.001  # effectively black
        result = evaluate_l0(frames)
        assert 0.4 < result.black_frame_ratio < 0.6

    def test_clipped_frames_detected(self):
        from testbed.evaluation.pipeline import evaluate_l0

        frames = np.random.rand(100, 320, 3).astype(np.float32) * 0.5
        # Make 20 frames fully saturated
        frames[:20, :, :] = 0.995
        result = evaluate_l0(frames)
        assert 0.15 < result.clipped_frame_ratio < 0.25

    def test_perfect_frames(self):
        from testbed.evaluation.pipeline import evaluate_l0

        frames = np.random.rand(200, 320, 3).astype(np.float32) * 0.5 + 0.25
        result = evaluate_l0(frames)
        assert result.black_frame_ratio == 0.0
        assert result.clipped_frame_ratio == 0.0
        assert 0.4 < result.mean_brightness < 0.6
        assert result.spatial_variance > 0.0


# ---------------------------------------------------------------------------
# L1 — Temporal coherence
# ---------------------------------------------------------------------------

class TestL1:
    """L1 temporal coherence metrics."""

    def _smooth_sequence(self, n=200, noise=0.01):
        """Generate a smooth, slowly-varying LED sequence."""
        t = np.linspace(0, 4 * np.pi, n)
        base = (np.sin(t) * 0.3 + 0.5)[:, None, None]
        frames = np.broadcast_to(base, (n, 320, 3)).copy().astype(np.float32)
        frames += np.random.randn(n, 320, 3).astype(np.float32) * noise
        return np.clip(frames, 0, 1)

    def _flickery_sequence(self, n=200):
        """Generate a rapidly alternating sequence (worst-case flicker)."""
        frames = np.zeros((n, 320, 3), dtype=np.float32)
        frames[::2, :, :] = 0.9  # bright
        frames[1::2, :, :] = 0.1  # dark
        return frames

    def test_stability_smooth(self):
        from testbed.evaluation.l1_temporal import hi_light_stability

        score = hi_light_stability(self._smooth_sequence())
        assert score > 0.8, f"Smooth sequence should have high stability, got {score}"

    def test_stability_flickery(self):
        from testbed.evaluation.l1_temporal import hi_light_stability

        score = hi_light_stability(self._flickery_sequence())
        assert score < 0.5, f"Flickery sequence should have low stability, got {score}"

    def test_flicker_smooth(self):
        from testbed.evaluation.l1_temporal import elatcsf_flicker

        score = elatcsf_flicker(self._smooth_sequence(), fps=120.0)
        assert score < 0.3, f"Smooth sequence should have low flicker, got {score}"

    def test_flicker_alternating(self):
        from testbed.evaluation.l1_temporal import elatcsf_flicker

        # At 120 FPS, frame-alternating is 60Hz — correctly invisible per Kelly/Robson.
        # Use ~10Hz flicker (alternate every 6 frames) which is peak human sensitivity.
        n = 240
        frames = np.zeros((n, 320, 3), dtype=np.float32)
        period = 12  # 6 bright + 6 dark = 10Hz at 120 FPS
        for i in range(n):
            if (i // (period // 2)) % 2 == 0:
                frames[i, :, :] = 0.9
            else:
                frames[i, :, :] = 0.1
        score = elatcsf_flicker(frames, fps=120.0)
        assert score > 0.3, f"10Hz flicker should be detectable, got {score}"

    def test_freq_consistency_smooth(self):
        from testbed.evaluation.l1_temporal import vmcr_freq_consistency

        score = vmcr_freq_consistency(self._smooth_sequence(), fps=120.0)
        assert score > 0.5, f"Smooth sequence should have high freq consistency, got {score}"

    def test_psnr_identical(self):
        from testbed.evaluation.l1_temporal import psnr_div

        frames = np.ones((100, 320, 3), dtype=np.float32) * 0.5
        score = psnr_div(frames)
        # Identical frames → infinite PSNR, capped at 100
        assert score >= 80.0, f"Static frames should have very high PSNR, got {score}"

    def test_evaluate_l1_returns_result(self):
        from testbed.evaluation.l1_temporal import evaluate_l1, L1Result

        result = evaluate_l1(self._smooth_sequence(), fps=120.0)
        assert isinstance(result, L1Result)
        assert 0 <= result.stability_score <= 1
        assert 0 <= result.flicker_score <= 1
        assert 0 <= result.freq_consistency <= 1


# ---------------------------------------------------------------------------
# L2 — Audio-visual alignment
# ---------------------------------------------------------------------------

class TestL2:
    """L2 audio-visual alignment metrics."""

    def _correlated_av(self, n=300):
        """Generate audio-visual data where visual tracks audio."""
        rms = np.sin(np.linspace(0, 6 * np.pi, n)).astype(np.float32) * 0.4 + 0.5
        # Visual brightness follows RMS
        frames = np.zeros((n, 320, 3), dtype=np.float32)
        for i in range(n):
            frames[i, :, :] = rms[i]
        flux = np.abs(np.diff(rms, prepend=rms[0]))
        beats = np.zeros(n, dtype=bool)
        beats[::30] = True  # beat every 30 frames
        onsets = np.zeros(n, dtype=bool)
        onsets[::45] = True
        return frames, rms, flux, beats, onsets

    def _uncorrelated_av(self, n=300):
        """Audio and visual are independent."""
        rms = np.random.rand(n).astype(np.float32)
        frames = np.random.rand(n, 320, 3).astype(np.float32)
        flux = np.abs(np.diff(rms, prepend=rms[0]))
        beats = np.zeros(n, dtype=bool)
        beats[::30] = True
        onsets = np.zeros(n, dtype=bool)
        onsets[::45] = True
        return frames, rms, flux, beats, onsets

    def test_energy_correlation_correlated(self):
        from testbed.evaluation.l2_audiovisual import energy_correlation

        frames, rms, *_ = self._correlated_av()
        corr = energy_correlation(frames, rms)
        assert corr > 0.8, f"Correlated A/V should have high energy corr, got {corr}"

    def test_energy_correlation_uncorrelated(self):
        from testbed.evaluation.l2_audiovisual import energy_correlation

        frames, rms, *_ = self._uncorrelated_av()
        corr = energy_correlation(frames, rms)
        assert abs(corr) < 0.3, f"Uncorrelated A/V should have low energy corr, got {corr}"

    def test_beat_alignment_perfect(self):
        from testbed.evaluation.l2_audiovisual import beat_visual_alignment

        n = 300
        frames = np.ones((n, 320, 3), dtype=np.float32) * 0.3
        beats = np.zeros(n, dtype=bool)
        beat_positions = [30, 60, 90, 120, 150, 180, 210, 240, 270]
        for bp in beat_positions:
            beats[bp] = True
            # Visual change at beat position
            frames[bp, :, :] = 0.9

        score = beat_visual_alignment(frames, beats, window=3)
        assert score > 0.7, f"Perfect beat alignment should score high, got {score}"

    def test_onset_response_responsive(self):
        from testbed.evaluation.l2_audiovisual import onset_response

        n = 300
        frames = np.ones((n, 320, 3), dtype=np.float32) * 0.3
        onsets = np.zeros(n, dtype=bool)
        onset_pos = [50, 100, 150, 200, 250]
        for op in onset_pos:
            onsets[op] = True
            # Big visual change at onset
            frames[op, :, :] = 0.9

        score = onset_response(frames, onsets)
        assert score > 1.5, f"Responsive onsets should amplify visual change, got {score}"

    def test_evaluate_l2_returns_result(self):
        from testbed.evaluation.l2_audiovisual import evaluate_l2, L2Result

        frames, rms, flux, beats, onsets = self._correlated_av()
        result = evaluate_l2(frames, rms, flux, beats, onsets)
        assert isinstance(result, L2Result)


# ---------------------------------------------------------------------------
# L3 — Perceptual similarity (TS3IM)
# ---------------------------------------------------------------------------

class TestL3:
    """L3 perceptual similarity metrics."""

    def test_ts3im_identical(self):
        from testbed.evaluation.l3_perceptual import ts3im

        series = np.sin(np.linspace(0, 10, 200)).astype(np.float32)
        c, t, v, s = ts3im(series, series)
        assert t > 0.99, f"Identical series should have trend sim ~1, got {t}"
        assert v > 0.99, f"Identical series should have variability sim ~1, got {v}"
        assert s > 0.99, f"Identical series should have structure sim ~1, got {s}"
        assert c > 0.99, f"Identical series should have composite ~1, got {c}"

    def test_ts3im_inverted(self):
        from testbed.evaluation.l3_perceptual import ts3im

        series_a = np.sin(np.linspace(0, 10, 200)).astype(np.float32)
        series_b = -series_a
        c, t, v, s = ts3im(series_a, series_b)
        # Inverted: trend should be low, variability should be high (same std),
        # structure should be low (negative correlation)
        assert t < 0.3, f"Inverted trends should be dissimilar, got {t}"
        assert v > 0.9, f"Same variability magnitude should match, got {v}"
        assert s < 0.2, f"Anti-correlated structure should be low, got {s}"

    def test_ts3im_frames_self_consistent(self):
        """A smooth sequence should be self-consistent (first half ≈ second half)."""
        from testbed.evaluation.l3_perceptual import evaluate_l3

        t = np.linspace(0, 4 * np.pi, 200)
        frames = np.zeros((200, 320, 3), dtype=np.float32)
        for i in range(200):
            frames[i, :, :] = (np.sin(t[i]) * 0.3 + 0.5)

        result = evaluate_l3(frames, reference=None)
        assert result.ts3im_score > 0.6, (
            f"Self-consistent sequence should have moderate-high TS3IM, got {result.ts3im_score}"
        )

    def test_ts3im_frames_with_reference(self):
        """Identical frames vs reference should score high."""
        from testbed.evaluation.l3_perceptual import evaluate_l3

        frames = np.random.rand(100, 320, 3).astype(np.float32) * 0.5 + 0.25
        result = evaluate_l3(frames, reference=frames)
        assert result.ts3im_score > 0.95, (
            f"Identical sequences should score near 1.0, got {result.ts3im_score}"
        )


# ---------------------------------------------------------------------------
# Pipeline — Multiplicative composition
# ---------------------------------------------------------------------------

class TestPipeline:
    """End-to-end pipeline and composition tests."""

    def test_composite_all_perfect(self):
        from testbed.evaluation.pipeline import _multiplicative_composite, L0Result
        from testbed.evaluation.l1_temporal import L1Result
        from testbed.evaluation.l2_audiovisual import L2Result
        from testbed.evaluation.l3_perceptual import L3Result

        l0 = L0Result(black_frame_ratio=0, clipped_frame_ratio=0,
                       mean_brightness=0.5, spatial_variance=0.05)
        l1 = L1Result(stability_score=1.0, flicker_score=0.0,
                       freq_consistency=1.0, motion_weighted_psnr=60.0,
                       raw_frame_diff_mean=0.0)
        l2 = L2Result(energy_correlation=1.0, beat_visual_alignment=1.0,
                       onset_response_score=2.0, salience_weighted_corr=1.0)
        l3 = L3Result(ts3im_score=1.0, trend_similarity=1.0,
                       variability_similarity=1.0, structure_similarity=1.0)

        score = _multiplicative_composite(l0, l1, l2, l3)
        assert score > 0.95, f"All-perfect should score near 1.0, got {score}"

    def test_composite_one_bad_kills_score(self):
        """Multiplicative composition: one bad layer pulls the whole score down."""
        from testbed.evaluation.pipeline import _multiplicative_composite, L0Result
        from testbed.evaluation.l1_temporal import L1Result
        from testbed.evaluation.l2_audiovisual import L2Result
        from testbed.evaluation.l3_perceptual import L3Result

        l0 = L0Result(black_frame_ratio=0.9, clipped_frame_ratio=0,
                       mean_brightness=0.05, spatial_variance=0.01)
        l1 = L1Result(stability_score=1.0, flicker_score=0.0,
                       freq_consistency=1.0, motion_weighted_psnr=60.0,
                       raw_frame_diff_mean=0.0)
        l2 = L2Result(energy_correlation=1.0, beat_visual_alignment=1.0,
                       onset_response_score=2.0, salience_weighted_corr=1.0)
        l3 = L3Result(ts3im_score=1.0, trend_similarity=1.0,
                       variability_similarity=1.0, structure_similarity=1.0)

        score = _multiplicative_composite(l0, l1, l2, l3)
        # L0 score = 1 - 0.9 = 0.1, geometric mean with three 1.0s ≈ 0.56
        assert score < 0.65, f"90% black frames should drag composite down, got {score}"

    def test_composite_floor_prevents_zero(self):
        """Floor of 0.01 prevents zero-killing the geometric mean."""
        from testbed.evaluation.pipeline import _multiplicative_composite, L0Result
        from testbed.evaluation.l1_temporal import L1Result
        from testbed.evaluation.l2_audiovisual import L2Result
        from testbed.evaluation.l3_perceptual import L3Result

        l0 = L0Result(black_frame_ratio=1.0, clipped_frame_ratio=0,
                       mean_brightness=0, spatial_variance=0)
        l1 = L1Result(stability_score=1.0, flicker_score=0.0,
                       freq_consistency=1.0, motion_weighted_psnr=60.0,
                       raw_frame_diff_mean=0.0)
        l2 = L2Result(energy_correlation=1.0, beat_visual_alignment=1.0,
                       onset_response_score=2.0, salience_weighted_corr=1.0)
        l3 = L3Result(ts3im_score=1.0, trend_similarity=1.0,
                       variability_similarity=1.0, structure_similarity=1.0)

        score = _multiplicative_composite(l0, l1, l2, l3)
        assert score > 0.0, "Floor should prevent score from reaching zero"
        assert score < 0.5, "All-black should still score very low"

    def test_evaluate_sequence_smoke(self):
        """Smoke test: evaluate_sequence runs without crashing on synthetic data."""
        from testbed.evaluation.frame_parser import (
            CaptureFrame, CaptureSequence, FrameMetrics,
        )
        from testbed.evaluation.pipeline import evaluate_sequence

        np.random.seed(42)
        n = 120
        frames = []
        for i in range(n):
            brightness = np.sin(i / 20.0) * 0.3 + 0.5
            rgb = (np.random.rand(320, 3) * brightness * 255).astype(np.uint8)
            m = FrameMetrics(
                show_us=300, rms=float(np.sin(i / 20.0) * 0.3 + 0.4),
                bands=np.random.rand(8).astype(np.float32),
                beat=(i % 30 == 0), onset=(i % 45 == 0),
                flux=float(np.random.rand() * 0.5),
                heap_free=120000, show_skips=0,
                bpm=128.0, tempo_confidence=0.9,
            )
            frames.append(CaptureFrame(
                version=2, tap=1, effect_id=5, palette_id=3,
                brightness=200, speed=128, frame_index=i,
                timestamp_us=i * 8333, rgb=rgb, metrics=m,
            ))

        seq = CaptureSequence(frames=frames)
        result = evaluate_sequence(seq)

        assert 0 < result.composite <= 1.0
        assert result.num_frames == n
        assert result.fps > 0

        # Summary should not crash
        summary = result.summary()
        assert "COMPOSITE SCORE" in summary
        assert "L0 Frame Quality" in summary
