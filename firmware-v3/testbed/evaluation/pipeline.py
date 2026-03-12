"""Quality evaluation pipeline — composes L0–L3 metrics into aggregate scores.

Implements:
    T1.9  ToolRLA multiplicative composition — geometric mean prevents
          one good metric from hiding a bad one.

Usage:
    from testbed.evaluation.pipeline import evaluate_sequence
    from testbed.evaluation.frame_parser import load_capture

    seq = load_capture("capture.bin")
    result = evaluate_sequence(seq)
    print(result.summary())
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .frame_parser import CaptureSequence
from .l1_temporal import L1Result, evaluate_l1
from .l2_audiovisual import L2Result, evaluate_l2
from .l3_perceptual import L3Result, evaluate_l3


@dataclass(slots=True)
class L0Result:
    """L0 — Per-frame quality checks."""

    black_frame_ratio: float    # Fraction of all-black frames [0, 1]
    clipped_frame_ratio: float  # Fraction of fully-saturated frames [0, 1]
    mean_brightness: float      # Mean brightness across all frames [0, 1]
    spatial_variance: float     # Mean spatial variance (strip utilisation) [0, 1]


@dataclass
class PipelineResult:
    """Full evaluation pipeline result across L0–L3."""

    l0: L0Result
    l1: L1Result
    l2: L2Result
    l3: L3Result
    composite: float            # Multiplicative composite [0, 1]
    num_frames: int
    duration_s: float
    fps: float

    def summary(self) -> str:
        """Human-readable summary of all metrics."""
        lines = [
            f"=== Quality Evaluation ({self.num_frames} frames, "
            f"{self.duration_s:.1f}s @ {self.fps:.0f} FPS) ===",
            "",
            "L0 Frame Quality:",
            f"  Black frames:    {self.l0.black_frame_ratio:.1%}",
            f"  Clipped frames:  {self.l0.clipped_frame_ratio:.1%}",
            f"  Mean brightness: {self.l0.mean_brightness:.3f}",
            f"  Spatial variance:{self.l0.spatial_variance:.3f}",
            "",
            "L1 Temporal Coherence:",
            f"  Stability:       {self.l1.stability_score:.3f}",
            f"  Flicker:         {self.l1.flicker_score:.3f}  (lower = better)",
            f"  Freq consistency:{self.l1.freq_consistency:.3f}",
            f"  Motion PSNR:     {self.l1.motion_weighted_psnr:.1f} dB",
            "",
            "L2 Audio-Visual Alignment:",
            f"  Energy corr:     {self.l2.energy_correlation:.3f}",
            f"  Beat alignment:  {self.l2.beat_visual_alignment:.3f}",
            f"  Onset response:  {self.l2.onset_response_score:.2f}x",
            f"  Salience corr:   {self.l2.salience_weighted_corr:.3f}",
            "",
            "L3 Perceptual Similarity:",
            f"  TS3IM composite: {self.l3.ts3im_score:.3f}",
            f"    Trend:         {self.l3.trend_similarity:.3f}",
            f"    Variability:   {self.l3.variability_similarity:.3f}",
            f"    Structure:     {self.l3.structure_similarity:.3f}",
            "",
            f"COMPOSITE SCORE:   {self.composite:.3f}",
        ]
        return "\n".join(lines)


def evaluate_l0(frames: np.ndarray) -> L0Result:
    """L0 — Per-frame quality checks.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].

    Returns:
        L0Result with frame-level quality metrics.
    """
    n = frames.shape[0]

    # Black frames: mean brightness < 0.005 (effectively off)
    brightness_per_frame = np.mean(frames, axis=(1, 2))
    black_ratio = float(np.mean(brightness_per_frame < 0.005))

    # Clipped frames: more than 50% of pixels at max value
    clipped_pixels = np.mean(frames > 0.99, axis=(1, 2))
    clipped_ratio = float(np.mean(clipped_pixels > 0.5))

    # Mean brightness
    mean_bright = float(np.mean(brightness_per_frame))

    # Spatial variance: how well the strip is utilised (not all same colour)
    # High variance = diverse patterns, low = monotone
    spatial_var = float(np.mean(np.var(frames, axis=1)))

    return L0Result(
        black_frame_ratio=black_ratio,
        clipped_frame_ratio=clipped_ratio,
        mean_brightness=mean_bright,
        spatial_variance=spatial_var,
    )


def _multiplicative_composite(
    l0: L0Result,
    l1: L1Result,
    l2: L2Result,
    l3: L3Result,
) -> float:
    """ToolRLA-style multiplicative composition (T1.9).

    Geometric mean ensures no single bad metric can be hidden by good ones.
    Each layer contributes a [0, 1] score; the composite is their geometric mean.

    Layer scores:
        L0: 1 - max(black_ratio, clipped_ratio)  [penalise bad frames]
        L1: stability × freq_consistency × (1 - flicker)
        L2: (energy_corr + 1) / 2  [map from [-1,1] to [0,1]]
        L3: ts3im_score

    Returns:
        Composite in [0, 1]. Higher = better.
    """
    # L0 score: penalise black/clipped frames
    l0_score = 1.0 - max(l0.black_frame_ratio, l0.clipped_frame_ratio)
    l0_score = max(l0_score, 0.01)  # floor to avoid zero-killing the geometric mean

    # L1 score: product of stability components
    l1_score = l1.stability_score * l1.freq_consistency * (1.0 - l1.flicker_score)
    l1_score = max(l1_score, 0.01)

    # L2 score: map correlation from [-1, 1] to [0, 1]
    # Use salience-weighted correlation as the primary L2 signal
    l2_score = (l2.salience_weighted_corr + 1.0) / 2.0
    l2_score = max(l2_score, 0.01)

    # L3 score: TS3IM composite
    l3_score = max(l3.ts3im_score, 0.01)

    # Geometric mean (multiplicative composition)
    composite = (l0_score * l1_score * l2_score * l3_score) ** 0.25

    return float(composite)


def evaluate_sequence(
    seq: CaptureSequence,
    reference: CaptureSequence | None = None,
) -> PipelineResult:
    """Run the full L0–L3 evaluation pipeline on a captured sequence.

    Args:
        seq: Captured LED frame sequence.
        reference: Optional reference sequence for L3 comparison.

    Returns:
        PipelineResult with all metrics and composite score.
    """
    frames = seq.rgb_float()
    fps = seq.fps if seq.fps > 0 else 120.0

    # L0
    l0 = evaluate_l0(frames)

    # L1
    l1 = evaluate_l1(frames, fps)

    # L2 — needs audio data from the metrics trailer
    l2 = evaluate_l2(
        frames=frames,
        audio_rms=seq.rms_array(),
        audio_flux=seq.flux_array(),
        beats=seq.beat_array(),
        onsets=seq.onset_array(),
    )

    # L3 — needs reference sequence if available
    ref_frames = reference.rgb_float() if reference is not None else None
    l3 = evaluate_l3(frames, ref_frames)

    # Composite
    composite = _multiplicative_composite(l0, l1, l2, l3)

    return PipelineResult(
        l0=l0,
        l1=l1,
        l2=l2,
        l3=l3,
        composite=composite,
        num_frames=seq.num_frames,
        duration_s=seq.duration_s,
        fps=fps,
    )


__all__ = [
    "L0Result",
    "PipelineResult",
    "evaluate_l0",
    "evaluate_sequence",
]
