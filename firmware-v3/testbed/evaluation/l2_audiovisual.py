"""L2 — Audio-visual alignment metrics for LED output sequences.

Measures how well the visual output tracks the audio input across
beat synchronisation, energy correlation, and onset response.

Implements:
    T1.10 CAE-AV — Per-frame audio-visual agreement gating
    BeatAlign — Cross-correlation of beat timestamps with visual change peaks
    Energy correlation — Pearson correlation of audio energy vs visual energy

References:
    - CAE-AV: arXiv:2602.08309
    - BeatAlign: arXiv:2506.18881
    - corr(|Δ|): arXiv:2506.01482 (Skip-BART)
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(slots=True)
class L2Result:
    """Results from L2 audio-visual alignment evaluation."""

    energy_correlation: float        # Pearson r between audio RMS and visual energy
    beat_visual_alignment: float     # Cross-correlation of beats with visual change peaks
    onset_response_score: float      # Mean visual change magnitude at onset frames
    salience_weighted_corr: float    # Energy correlation weighted by audio salience (CAE-AV)


def _visual_energy(frames: np.ndarray) -> np.ndarray:
    """Compute per-frame visual energy as mean pixel brightness.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].

    Returns:
        (N,) float32 array of per-frame visual energy.
    """
    return np.mean(frames, axis=(1, 2))


def _visual_change(frames: np.ndarray) -> np.ndarray:
    """Compute per-frame visual change magnitude.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].

    Returns:
        (N-1,) float32 array of absolute visual change between consecutive frames.
    """
    if frames.shape[0] < 2:
        return np.array([], dtype=np.float32)
    return np.mean(np.abs(np.diff(frames, axis=0)), axis=(1, 2))


def energy_correlation(
    frames: np.ndarray,
    audio_rms: np.ndarray,
) -> float:
    """Pearson correlation between audio RMS and visual energy.

    Based on corr(|Δ|) from arXiv:2506.01482 but using absolute levels
    rather than first-order differences. Both signals are z-normalised
    before correlation.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        audio_rms: (N,) float32 audio RMS energy per frame.

    Returns:
        Pearson r in [-1, 1]. Higher = better audio-visual energy tracking.
    """
    vis = _visual_energy(frames)

    if len(vis) != len(audio_rms):
        n = min(len(vis), len(audio_rms))
        vis = vis[:n]
        audio_rms = audio_rms[:n]

    if len(vis) < 2:
        return 0.0

    # Z-normalise
    vis_std = np.std(vis)
    rms_std = np.std(audio_rms)
    if vis_std < 1e-8 or rms_std < 1e-8:
        return 0.0

    vis_z = (vis - np.mean(vis)) / vis_std
    rms_z = (audio_rms - np.mean(audio_rms)) / rms_std

    return float(np.mean(vis_z * rms_z))


def beat_visual_alignment(
    frames: np.ndarray,
    beats: np.ndarray,
    window: int = 3,
) -> float:
    """Cross-correlation of beat events with visual change peaks.

    Based on BeatAlign (arXiv:2506.18881): for each beat, check if a
    visual change peak occurs within ±window frames. Score is the fraction
    of beats that align with visual peaks.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        beats: (N,) bool array of beat flags per frame.
        window: Tolerance window in frames (±window).

    Returns:
        Alignment score in [0, 1]. 1.0 = every beat aligns with a visual peak.
    """
    change = _visual_change(frames)
    if len(change) == 0:
        return 0.0

    # Trim beats to match change array length
    beats_trimmed = beats[:len(change)]

    beat_indices = np.where(beats_trimmed)[0]
    if len(beat_indices) == 0:
        return 0.0  # No beats to evaluate

    # Find visual change peaks (local maxima above median).
    # Use >= on the left to handle rectangular pulses (equal adjacent values).
    threshold = np.median(change) + 0.5 * np.std(change)
    peaks = set()
    for i in range(1, len(change) - 1):
        if change[i] >= change[i - 1] and change[i] > change[i + 1] and change[i] > threshold:
            peaks.add(i)

    if not peaks:
        return 0.0

    # For each beat, check if any peak is within ±window
    aligned = 0
    for bi in beat_indices:
        for offset in range(-window, window + 1):
            if (bi + offset) in peaks:
                aligned += 1
                break

    return float(aligned / len(beat_indices))


def onset_response(
    frames: np.ndarray,
    onsets: np.ndarray,
    response_window: int = 5,
) -> float:
    """Mean visual change magnitude at onset frames.

    Measures how much the visualisation reacts to detected onsets
    (snare hits, hi-hat strikes) compared to non-onset frames.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        onsets: (N,) bool array of onset flags per frame.
        response_window: Frames after onset to measure response.

    Returns:
        Response ratio: mean change at onsets / mean change overall.
        > 1.0 means onsets produce above-average visual change.
        Returns 1.0 if no onsets present.
    """
    change = _visual_change(frames)
    if len(change) == 0:
        return 1.0

    onsets_trimmed = onsets[:len(change)]
    onset_indices = np.where(onsets_trimmed)[0]

    if len(onset_indices) == 0:
        return 1.0

    overall_mean = np.mean(change) + 1e-8

    # Collect change values within response_window of each onset
    onset_changes = []
    for oi in onset_indices:
        window_end = min(oi + response_window, len(change))
        onset_changes.append(np.max(change[oi:window_end]))

    onset_mean = np.mean(onset_changes)

    # Normalise: ratio > 1 means onset response is stronger than average
    ratio = onset_mean / overall_mean
    # Clamp to [0, 5] for sanity
    return float(np.clip(ratio, 0.0, 5.0))


def salience_weighted_correlation(
    frames: np.ndarray,
    audio_rms: np.ndarray,
    audio_flux: np.ndarray,
    beats: np.ndarray,
) -> float:
    """Energy correlation weighted by audio salience (CAE-AV pattern).

    Based on arXiv:2602.08309: frames during musically important moments
    (high flux, beat events, high energy) contribute more to the
    correlation score than quiet/static moments.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        audio_rms: (N,) float32 audio RMS per frame.
        audio_flux: (N,) float32 spectral flux per frame.
        beats: (N,) bool beat flags per frame.

    Returns:
        Weighted Pearson r in [-1, 1].
    """
    vis = _visual_energy(frames)
    n = min(len(vis), len(audio_rms), len(audio_flux), len(beats))
    if n < 2:
        return 0.0

    vis = vis[:n]
    rms = audio_rms[:n]
    flux = audio_flux[:n]
    b = beats[:n].astype(np.float32)

    # Salience weight: combination of flux (novelty) and beat (structure)
    # High flux = spectrally interesting moment
    # Beat = rhythmically important moment
    flux_norm = flux / (np.max(flux) + 1e-8)
    weight = 0.5 * flux_norm + 0.3 * b + 0.2 * (rms / (np.max(rms) + 1e-8))
    weight = np.clip(weight, 0.05, 1.0)  # minimum weight so quiet parts aren't ignored

    # Weighted Pearson correlation
    vis_std = np.std(vis)
    rms_std = np.std(rms)
    if vis_std < 1e-8 or rms_std < 1e-8:
        return 0.0

    vis_z = (vis - np.mean(vis)) / vis_std
    rms_z = (rms - np.mean(rms)) / rms_std

    weighted_corr = np.sum(weight * vis_z * rms_z) / np.sum(weight)
    return float(np.clip(weighted_corr, -1.0, 1.0))


def evaluate_l2(
    frames: np.ndarray,
    audio_rms: np.ndarray,
    audio_flux: np.ndarray,
    beats: np.ndarray,
    onsets: np.ndarray,
) -> L2Result:
    """Run all L2 audio-visual alignment metrics.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        audio_rms: (N,) audio RMS per frame.
        audio_flux: (N,) spectral flux per frame.
        beats: (N,) bool beat flags.
        onsets: (N,) bool onset flags.

    Returns:
        L2Result with all audio-visual metrics.
    """
    return L2Result(
        energy_correlation=energy_correlation(frames, audio_rms),
        beat_visual_alignment=beat_visual_alignment(frames, beats),
        onset_response_score=onset_response(frames, onsets),
        salience_weighted_corr=salience_weighted_correlation(
            frames, audio_rms, audio_flux, beats
        ),
    )


__all__ = [
    "L2Result",
    "energy_correlation",
    "beat_visual_alignment",
    "onset_response",
    "salience_weighted_correlation",
    "evaluate_l2",
]
