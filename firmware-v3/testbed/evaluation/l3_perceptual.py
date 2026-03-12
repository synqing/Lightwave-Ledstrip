"""L3 — Perceptual similarity metrics for 1D LED time series.

Native 1D metrics that avoid forcing LED strip data into 2D image frameworks.
Computes structural similarity along the strip and across time without
the distortion of treating 320×1 pixels as a 2D image.

Implements:
    T1.1  TS3IM — Structural Similarity for Time Series
                  (trend, variability, structural integrity decomposition)

References:
    - TS3IM: arXiv:2405.06234
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(slots=True)
class L3Result:
    """Results from L3 perceptual similarity evaluation."""

    ts3im_score: float          # TS3IM composite similarity [0, 1]
    trend_similarity: float     # Direction-of-change agreement [0, 1]
    variability_similarity: float  # Amplitude dynamics match [0, 1]
    structure_similarity: float # Pattern shape preservation [0, 1]


# ---------------------------------------------------------------------------
# T1.1  TS3IM — Structural Similarity for Time Series
# ---------------------------------------------------------------------------

def _trend_similarity(a: np.ndarray, b: np.ndarray) -> float:
    """Trend component: agreement in direction of change.

    Measures whether two time series are increasing/decreasing at the
    same times. Uses sign of first-order differences.

    Args:
        a, b: 1D float arrays of equal length.

    Returns:
        Similarity in [0, 1]. 1.0 = trends always agree.
    """
    if len(a) < 2:
        return 1.0

    da = np.sign(np.diff(a))
    db = np.sign(np.diff(b))

    # Agreement: both positive, both negative, or both zero
    agreement = np.mean(da * db >= 0)
    return float(agreement)


def _variability_similarity(a: np.ndarray, b: np.ndarray) -> float:
    """Variability component: amplitude dynamics match.

    Compares the standard deviation (volatility) of the two series
    using a ratio-based similarity.

    Args:
        a, b: 1D float arrays of equal length.

    Returns:
        Similarity in [0, 1]. 1.0 = identical variability.
    """
    std_a = np.std(a)
    std_b = np.std(b)

    if std_a < 1e-10 and std_b < 1e-10:
        return 1.0  # Both constant

    # Ratio similarity: 2*min/(a+b)
    denom = std_a + std_b
    if denom < 1e-10:
        return 1.0

    return float(2.0 * min(std_a, std_b) / denom)


def _structure_similarity(a: np.ndarray, b: np.ndarray) -> float:
    """Structure component: pattern shape preservation.

    Normalised cross-correlation (Pearson r) measuring whether the
    series have the same shape regardless of scale/offset.

    Args:
        a, b: 1D float arrays of equal length.

    Returns:
        Similarity in [0, 1]. 1.0 = identical shape.
    """
    std_a = np.std(a)
    std_b = np.std(b)

    if std_a < 1e-10 or std_b < 1e-10:
        # One or both are constant — if both constant, perfect; else no structure
        if std_a < 1e-10 and std_b < 1e-10:
            return 1.0
        return 0.0

    r = np.mean((a - np.mean(a)) * (b - np.mean(b))) / (std_a * std_b)
    # Map from [-1, 1] to [0, 1]
    return float(np.clip((r + 1.0) / 2.0, 0.0, 1.0))


def ts3im(
    series_a: np.ndarray,
    series_b: np.ndarray,
    weights: tuple[float, float, float] = (0.33, 0.33, 0.34),
) -> tuple[float, float, float, float]:
    """TS3IM — Structural Similarity for Time Series.

    Decomposes similarity into trend (direction), variability (amplitude),
    and structure (shape) components. Native 1D metric — no 2D image hacks.

    Args:
        series_a: Reference 1D time series.
        series_b: Test 1D time series (same length).
        weights: (trend_w, variability_w, structure_w), must sum to 1.

    Returns:
        (composite, trend, variability, structure) all in [0, 1].
    """
    n = min(len(series_a), len(series_b))
    a = series_a[:n].astype(np.float64)
    b = series_b[:n].astype(np.float64)

    trend = _trend_similarity(a, b)
    var = _variability_similarity(a, b)
    struct = _structure_similarity(a, b)

    composite = weights[0] * trend + weights[1] * var + weights[2] * struct
    return (float(composite), trend, var, struct)


def ts3im_frames(
    frames_a: np.ndarray,
    frames_b: np.ndarray,
) -> L3Result:
    """Apply TS3IM to full LED frame sequences.

    Computes TS3IM across multiple channels:
    - Per-pixel luminance time series (320 series, each of length N)
    - Global brightness time series (1 series of length N)
    - Per-band colour time series (R, G, B mean over strip)

    Aggregates across all channels.

    Args:
        frames_a: (N, 320, 3) float32 reference sequence.
        frames_b: (N, 320, 3) float32 test sequence (same shape).

    Returns:
        L3Result with TS3IM metrics.
    """
    n = min(frames_a.shape[0], frames_b.shape[0])
    if n < 2:
        return L3Result(ts3im_score=1.0, trend_similarity=1.0,
                        variability_similarity=1.0, structure_similarity=1.0)

    a = frames_a[:n]
    b = frames_b[:n]

    trends = []
    vars_ = []
    structs = []

    # 1. Global brightness (mean over all pixels)
    bright_a = np.mean(a, axis=(1, 2))
    bright_b = np.mean(b, axis=(1, 2))
    c, t, v, s = ts3im(bright_a, bright_b)
    trends.append(t)
    vars_.append(v)
    structs.append(s)

    # 2. Per-channel global mean (R, G, B)
    for ch in range(3):
        ch_a = np.mean(a[:, :, ch], axis=1)
        ch_b = np.mean(b[:, :, ch], axis=1)
        c, t, v, s = ts3im(ch_a, ch_b)
        trends.append(t)
        vars_.append(v)
        structs.append(s)

    # 3. Spatial luminance at sampled positions (every 32nd pixel = 10 samples)
    lum_a = 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]
    lum_b = 0.2126 * b[:, :, 0] + 0.7152 * b[:, :, 1] + 0.0722 * b[:, :, 2]
    for px in range(0, 320, 32):
        c, t, v, s = ts3im(lum_a[:, px], lum_b[:, px])
        trends.append(t)
        vars_.append(v)
        structs.append(s)

    trend_mean = float(np.mean(trends))
    var_mean = float(np.mean(vars_))
    struct_mean = float(np.mean(structs))
    composite = 0.33 * trend_mean + 0.33 * var_mean + 0.34 * struct_mean

    return L3Result(
        ts3im_score=composite,
        trend_similarity=trend_mean,
        variability_similarity=var_mean,
        structure_similarity=struct_mean,
    )


def evaluate_l3(
    frames: np.ndarray,
    reference: np.ndarray | None = None,
) -> L3Result:
    """Run all L3 perceptual similarity metrics.

    If no reference is provided, uses self-consistency metrics:
    compares first half vs second half of the sequence as a
    temporal self-similarity baseline.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        reference: Optional (N, 320, 3) reference sequence.

    Returns:
        L3Result with perceptual similarity metrics.
    """
    if reference is not None:
        return ts3im_frames(reference, frames)

    # Self-consistency: compare first half vs second half
    n = frames.shape[0]
    if n < 4:
        return L3Result(ts3im_score=1.0, trend_similarity=1.0,
                        variability_similarity=1.0, structure_similarity=1.0)

    mid = n // 2
    return ts3im_frames(frames[:mid], frames[mid:2 * mid])


__all__ = [
    "L3Result",
    "ts3im",
    "ts3im_frames",
    "evaluate_l3",
]
