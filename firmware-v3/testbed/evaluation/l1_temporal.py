"""L1 — Temporal coherence metrics for LED output sequences.

All metrics operate on (N, 320, 3) float32 arrays in [0, 1] range,
where N is the number of frames, 320 is the LED count, and 3 is RGB.

Implements:
    T1.2  elaTCSF    — Psychophysical temporal contrast sensitivity for flicker
    T1.3  Hi-Light   — Lighting-specific temporal stability score
    T1.4  VMCR       — Frequency-domain temporal consistency loss
    T1.12 PSNR_DIV   — Motion-divergence-weighted temporal quality

References:
    - elaTCSF: arXiv:2503.16759 (SIGGRAPH Asia 2024)
    - Hi-Light: "Hi-Light: A Path to high-fidelity, high-resolution video relighting"
    - VMCR: arXiv:2501.05484 (GLC-Diffusion)
    - PSNR_DIV: arXiv:2510.01361
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(slots=True)
class L1Result:
    """Results from L1 temporal coherence evaluation."""

    stability_score: float      # Hi-Light stability [0, 1], higher = more stable
    flicker_score: float        # elaTCSF flicker severity [0, 1], lower = less flicker
    freq_consistency: float     # VMCR frequency-domain consistency [0, 1], higher = better
    motion_weighted_psnr: float # PSNR_DIV in dB, higher = better temporal quality
    raw_frame_diff_mean: float  # Mean absolute frame-to-frame difference [0, 1]


# ---------------------------------------------------------------------------
# T1.3  Hi-Light Light Stability Score
# ---------------------------------------------------------------------------

def hi_light_stability(frames: np.ndarray) -> float:
    """Lighting-specific temporal stability score.

    Computes normalised frame-to-frame luminance stability, penalising
    unexpected luminance jumps while tolerating smooth transitions.

    Based on Hi-Light's Light Stability Score (LSS), adapted for 1D LED strips.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].

    Returns:
        Stability score in [0, 1]. 1.0 = perfectly stable, 0.0 = maximally unstable.
    """
    if frames.shape[0] < 2:
        return 1.0

    # Compute per-pixel luminance (Rec. 709)
    lum = 0.2126 * frames[:, :, 0] + 0.7152 * frames[:, :, 1] + 0.0722 * frames[:, :, 2]
    # Shape: (N, 320)

    # Frame-to-frame luminance differences (signed, not abs)
    # Preserving sign is critical: an alternating +0.8/-0.8 pattern has
    # constant abs-diff but massive signed jerk.
    delta_lum = np.diff(lum, axis=0)  # (N-1, 320), signed

    # Second-order differences — acceleration of luminance change
    # Penalises jerk (sudden changes in rate of change), not smooth ramps
    if frames.shape[0] >= 3:
        accel = np.abs(np.diff(delta_lum, axis=0))  # (N-2, 320)
        jerk_penalty = np.mean(accel)
    else:
        jerk_penalty = np.mean(np.abs(delta_lum))

    # Normalise: max possible jerk is 2.0 (from 0→1→0 in consecutive frames)
    score = 1.0 - np.clip(jerk_penalty / 0.5, 0.0, 1.0)

    return float(score)


# ---------------------------------------------------------------------------
# T1.2  elaTCSF — Temporal Contrast Sensitivity for Flicker Detection
# ---------------------------------------------------------------------------

def elatcsf_flicker(frames: np.ndarray, fps: float = 120.0) -> float:
    """Psychophysically-calibrated flicker detection.

    Models human temporal contrast sensitivity: the eye is most sensitive
    to flicker in the 8–20 Hz range and attenuates sensitivity at higher
    temporal frequencies. This weights frame-to-frame differences by
    the temporal contrast sensitivity function (tCSF).

    Simplified implementation of elaTCSF (arXiv:2503.16759):
    - Compute per-pixel temporal frequency content via FFT
    - Weight by psychophysical sensitivity curve
    - Aggregate weighted energy as flicker severity

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        fps: Capture frame rate (default 120 Hz).

    Returns:
        Flicker severity in [0, 1]. 0.0 = no perceptible flicker,
        1.0 = maximum flicker.
    """
    if frames.shape[0] < 4:
        return 0.0

    n_frames = frames.shape[0]

    # Convert to luminance
    lum = 0.2126 * frames[:, :, 0] + 0.7152 * frames[:, :, 1] + 0.0722 * frames[:, :, 2]
    # Shape: (N, 320)

    # Remove DC component (mean luminance per pixel)
    lum_ac = lum - np.mean(lum, axis=0, keepdims=True)

    # FFT along temporal axis
    spectrum = np.fft.rfft(lum_ac, axis=0)
    power = np.abs(spectrum) ** 2  # (N//2+1, 320)

    # Frequency axis
    freqs = np.fft.rfftfreq(n_frames, d=1.0 / fps)

    # Temporal CSF — simplified Kelly (1979) / Robson (1966) model
    # Peak sensitivity ~10 Hz, roll-off at low and high frequencies
    # S(f) = f * exp(-f / f_peak) / f_peak * exp(-1)
    # Normalised so peak = 1.0 at f = f_peak
    f_peak = 10.0  # Hz — peak flicker sensitivity
    with np.errstate(divide="ignore", invalid="ignore"):
        sensitivity = np.where(
            freqs > 0,
            (freqs / f_peak) * np.exp(1.0 - freqs / f_peak),
            0.0,
        )
    sensitivity = np.clip(sensitivity, 0.0, 1.0)

    # Weight power spectrum by sensitivity
    weighted_power = power * sensitivity[:, np.newaxis] ** 2

    # Aggregate: mean weighted power across all pixels and frequencies
    total_weighted = np.mean(weighted_power)

    # Normalise against total AC power to get relative flicker severity
    total_power = np.mean(power) + 1e-10
    flicker = total_weighted / total_power

    return float(np.clip(flicker, 0.0, 1.0))


# ---------------------------------------------------------------------------
# T1.4  VMCR — Frequency-Domain Temporal Consistency
# ---------------------------------------------------------------------------

def vmcr_freq_consistency(frames: np.ndarray, fps: float = 120.0) -> float:
    """Frequency-domain temporal consistency metric.

    Based on GLC-Diffusion's VMCR loss (arXiv:2501.05484): computes
    the temporal FFT of the frame sequence and measures the ratio of
    low-frequency (smooth) energy to total energy. High consistency
    means most temporal variation is smooth/gradual, not jittery.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        fps: Capture frame rate.

    Returns:
        Consistency score in [0, 1]. 1.0 = perfectly smooth temporal
        evolution, 0.0 = dominated by high-frequency jitter.
    """
    if frames.shape[0] < 4:
        return 1.0

    n_frames = frames.shape[0]

    # Flatten spatial: (N, 320*3)
    flat = frames.reshape(n_frames, -1)

    # FFT along temporal axis
    spectrum = np.fft.rfft(flat, axis=0)
    power = np.abs(spectrum) ** 2

    # Frequency axis
    freqs = np.fft.rfftfreq(n_frames, d=1.0 / fps)

    # Low-frequency cutoff: anything below fps/8 is "smooth"
    # At 120 FPS, this means < 15 Hz — transitions slower than ~67ms
    cutoff_hz = fps / 8.0
    low_mask = freqs <= cutoff_hz
    # Exclude DC (index 0) — we want variation, not static level
    low_mask[0] = False

    low_energy = np.sum(power[low_mask, :])
    total_energy = np.sum(power[1:, :]) + 1e-10  # exclude DC

    consistency = low_energy / total_energy
    return float(np.clip(consistency, 0.0, 1.0))


# ---------------------------------------------------------------------------
# T1.12  PSNR_DIV — Motion-Divergence Weighted Temporal Quality
# ---------------------------------------------------------------------------

def psnr_div(frames: np.ndarray) -> float:
    """Motion-divergence-weighted temporal quality metric.

    Based on arXiv:2510.01361: weights frame-to-frame quality by the
    local "motion field divergence" — regions with high pixel displacement
    are weighted more heavily, since temporal artefacts in moving regions
    are more perceptually salient than in static regions.

    For 1D LED strips, "motion" is the per-pixel brightness/colour shift
    between consecutive frames. The divergence is the spatial derivative
    of this shift along the strip.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].

    Returns:
        Weighted PSNR in dB. Higher = better temporal quality.
        Returns 100.0 for perfectly static sequences.
    """
    if frames.shape[0] < 3:
        return 100.0

    # Frame-to-frame differences (our "motion field")
    flow = np.diff(frames, axis=0)  # (N-1, 320, 3)

    # Motion magnitude per pixel
    motion_mag = np.sqrt(np.sum(flow ** 2, axis=-1))  # (N-1, 320)

    # Divergence: spatial derivative of motion along the strip
    # Uses central differences along the LED axis
    divergence = np.abs(np.gradient(motion_mag, axis=1))  # (N-1, 320)

    # Weight: higher divergence = more attention
    # Add small epsilon so static regions still contribute
    weight = divergence + 0.01
    weight = weight / np.mean(weight)  # normalise to mean 1.0

    # Compute frame-to-frame MSE (comparing consecutive pairs)
    # Triple: frame t vs frame t+1 vs frame t+2 — middle should be interpolatable
    if frames.shape[0] >= 3:
        # For temporal quality: compare frame[t+1] against mean(frame[t], frame[t+2])
        interp = (frames[:-2] + frames[2:]) / 2.0  # (N-2, 320, 3)
        actual = frames[1:-1]                        # (N-2, 320, 3)

        err = (actual - interp) ** 2  # (N-2, 320, 3)
        err_per_pixel = np.mean(err, axis=-1)  # (N-2, 320)

        # Apply divergence weight (trim to match)
        w = weight[:-1]  # (N-2, 320)
        weighted_mse = np.sum(err_per_pixel * w) / np.sum(w)
    else:
        weighted_mse = np.mean(flow ** 2)

    if weighted_mse < 1e-10:
        return 100.0

    psnr = 10.0 * np.log10(1.0 / weighted_mse)
    return float(np.clip(psnr, 0.0, 100.0))


# ---------------------------------------------------------------------------
# Aggregate L1 evaluation
# ---------------------------------------------------------------------------

def evaluate_l1(
    frames: np.ndarray,
    fps: float = 120.0,
) -> L1Result:
    """Run all L1 temporal coherence metrics.

    Args:
        frames: (N, 320, 3) float32 in [0, 1].
        fps: Capture frame rate.

    Returns:
        L1Result with all temporal metrics.
    """
    # Raw frame diff for reference
    if frames.shape[0] >= 2:
        raw_diff = np.mean(np.abs(np.diff(frames, axis=0)))
    else:
        raw_diff = 0.0

    return L1Result(
        stability_score=hi_light_stability(frames),
        flicker_score=elatcsf_flicker(frames, fps),
        freq_consistency=vmcr_freq_consistency(frames, fps),
        motion_weighted_psnr=psnr_div(frames),
        raw_frame_diff_mean=float(raw_diff),
    )


__all__ = [
    "L1Result",
    "hi_light_stability",
    "elatcsf_flicker",
    "vmcr_freq_consistency",
    "psnr_div",
    "evaluate_l1",
]
