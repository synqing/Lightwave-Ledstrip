"""
EdgeMixer evaluation metrics module.

Analyses captured LED frames to evaluate EdgeMixer colour transform
effectiveness. Each captured frame is a (320, 3) uint8 numpy array where
indices 0-159 are strip1 (unmodified) and 160-319 are strip2 (modified by
EdgeMixer).

EdgeMixer operates on strip2 only, applying hue/saturation transforms for
LGP depth differentiation. See firmware-v3/src/effects/enhancement/EdgeMixer.h
for the composable 3-stage pipeline (Temporal, Colour, Spatial).
"""

import numpy as np
from numpy.typing import NDArray

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

LEDS_PER_STRIP = 160
TOTAL_LEDS = 320

# Near-black threshold matching firmware EdgeMixer::process() skip logic
NEAR_BLACK_THRESHOLD = 6

# Minimum value for "active" pixels in HSV analysis (avoids garbage hue)
MIN_VALUE_FOR_HUE = 0.02

# Spatial gradient evaluation zones (strip-local indices, 0-based)
CENTRE_LEDS = slice(75, 85)   # LEDs 75-84 (around physical centre 79/80)
EDGE_LEDS_LEFT = slice(0, 10)   # LEDs 0-9
EDGE_LEDS_RIGHT = slice(150, 160)  # LEDs 150-159

# Safety gate thresholds for automated pass/fail evaluation
#
# NOTE: Even in MIRROR mode, strips are NOT byte-identical. The effect
# renderer and zone composer may produce different strip1/strip2 content
# before EdgeMixer runs. Real hardware shows ~0.86 L2 divergence and ~2.3
# near-black "leaks" per frame at MIRROR baseline. Gates must account for
# this pre-existing differentiation.
SAFETY_GATES = {
    "near_black_leaks": 10,             # Per-frame mean; allows baseline differentiation
    "brightness_ratio_min": 0.55,       # Strip2 must not lose >45% brightness
    "brightness_ratio_max": 1.30,       # Strip2 must not gain >30% brightness
    "max_frame_divergence_mirror": 5.0,  # MIRROR mode mean L2; real HW shows ~0.86
}


# ---------------------------------------------------------------------------
# Frame splitting
# ---------------------------------------------------------------------------

def separate_strips(
    frame: NDArray[np.uint8],
) -> tuple[NDArray[np.uint8], NDArray[np.uint8]]:
    """Split a (320, 3) captured frame into two (160, 3) strip arrays.

    Parameters
    ----------
    frame : ndarray of shape (320, 3), dtype uint8
        Full captured frame (strip1 followed by strip2).

    Returns
    -------
    strip1 : ndarray of shape (160, 3), dtype uint8
        Unmodified strip (indices 0-159).
    strip2 : ndarray of shape (160, 3), dtype uint8
        EdgeMixer-processed strip (indices 160-319).
    """
    assert frame.shape == (TOTAL_LEDS, 3), (
        f"Expected frame shape ({TOTAL_LEDS}, 3), got {frame.shape}"
    )
    return frame[:LEDS_PER_STRIP].copy(), frame[LEDS_PER_STRIP:].copy()


# ---------------------------------------------------------------------------
# Colour space conversion
# ---------------------------------------------------------------------------

def rgb_to_hsv(
    rgb_array: NDArray[np.uint8],
) -> tuple[NDArray[np.float64], NDArray[np.float64], NDArray[np.float64]]:
    """Convert (N, 3) uint8 RGB to separate H, S, V float arrays.

    Fully vectorised — no per-pixel Python loops.

    Parameters
    ----------
    rgb_array : ndarray of shape (N, 3), dtype uint8
        RGB pixel data.

    Returns
    -------
    h : ndarray of shape (N,), float64
        Hue in degrees, 0-360.
    s : ndarray of shape (N,), float64
        Saturation, 0-1.
    v : ndarray of shape (N,), float64
        Value (brightness), 0-1.
    """
    rgb_f = rgb_array.astype(np.float64) / 255.0
    r, g, b = rgb_f[:, 0], rgb_f[:, 1], rgb_f[:, 2]

    cmax = np.maximum(np.maximum(r, g), b)
    cmin = np.minimum(np.minimum(r, g), b)
    delta = cmax - cmin

    # Hue calculation
    h = np.zeros_like(cmax)
    nonzero = delta > 0.0

    # Red is max
    mask = nonzero & (cmax == r)
    h[mask] = 60.0 * (((g[mask] - b[mask]) / delta[mask]) % 6.0)

    # Green is max
    mask = nonzero & (cmax == g)
    h[mask] = 60.0 * (((b[mask] - r[mask]) / delta[mask]) + 2.0)

    # Blue is max
    mask = nonzero & (cmax == b)
    h[mask] = 60.0 * (((r[mask] - g[mask]) / delta[mask]) + 4.0)

    # Wrap negative hue values
    h[h < 0.0] += 360.0

    # Saturation (guard division to avoid NaN where cmax is zero)
    safe_cmax = np.where(cmax > 0.0, cmax, 1.0)
    s = np.where(cmax > 0.0, delta / safe_cmax, 0.0)

    # Value
    v = cmax

    return h, s, v


# ---------------------------------------------------------------------------
# Per-frame metrics
# ---------------------------------------------------------------------------

def compute_frame_metrics(
    frame: NDArray[np.uint8],
    metadata: dict | None = None,
) -> dict:
    """Compute per-frame EdgeMixer evaluation metrics.

    Parameters
    ----------
    frame : ndarray of shape (320, 3), dtype uint8
        Full captured frame.
    metadata : dict, optional
        Capture metadata trailer. Expected keys: 'rms' (float), 'beat' (bool).

    Returns
    -------
    dict
        Metric name to value mapping. See module docstring for field descriptions.
    """
    strip1, strip2 = separate_strips(frame)

    # Cast to float for arithmetic
    s1f = strip1.astype(np.float64)
    s2f = strip2.astype(np.float64)

    # --- RGB divergence ---
    pixel_diff = s2f - s1f
    pixel_l2 = np.sqrt(np.sum(pixel_diff ** 2, axis=1))  # (160,)
    strip_divergence_l2 = float(np.mean(pixel_l2))
    strip_divergence_max = float(np.max(pixel_l2))

    # --- HSV analysis ---
    h1, s1_sat, v1 = rgb_to_hsv(strip1)
    h2, s2_sat, v2 = rgb_to_hsv(strip2)

    # Active pixel mask: both strips must have sufficient brightness
    both_active = (v1 > MIN_VALUE_FOR_HUE) & (v2 > MIN_VALUE_FOR_HUE)
    s1_active = v1 > MIN_VALUE_FOR_HUE

    # Circular hue difference (0-180 degrees)
    if np.any(both_active):
        hue_abs_diff = np.abs(h1[both_active] - h2[both_active])
        hue_circular = np.minimum(hue_abs_diff, 360.0 - hue_abs_diff)
        hue_shift_mean = float(np.mean(hue_circular))
        hue_shift_std = float(np.std(hue_circular))
    else:
        hue_shift_mean = 0.0
        hue_shift_std = 0.0

    # Saturation delta (strip2 - strip1, positive = more saturated)
    if np.any(s1_active):
        sat_delta = s2_sat[s1_active] - s1_sat[s1_active]
        saturation_delta_mean = float(np.mean(sat_delta))
    else:
        saturation_delta_mean = 0.0

    # Brightness ratio
    if np.any(s1_active):
        mean_v2 = float(np.mean(v2[s1_active]))
        mean_v1 = float(np.mean(v1[s1_active]))
        brightness_ratio = mean_v2 / mean_v1 if mean_v1 > 0.0 else float("nan")
    else:
        brightness_ratio = float("nan")

    # --- Near-black leak detection ---
    # Matches firmware: maxC = max(r, g, b) per pixel
    s1_max_c = np.max(strip1, axis=1)  # (160,) uint8
    s2_max_c = np.max(strip2, axis=1)
    near_black_mask = s1_max_c < NEAR_BLACK_THRESHOLD
    # Leak = strip1 is near-black but strip2 differs from strip1
    if np.any(near_black_mask):
        differs = np.any(strip1[near_black_mask] != strip2[near_black_mask], axis=1)
        near_black_leaks = int(np.sum(differs))
    else:
        near_black_leaks = 0

    # Active pixel count (pixels EdgeMixer would actually process)
    active_pixel_count = int(np.sum(s1_max_c >= NEAR_BLACK_THRESHOLD))

    # --- Spatial gradient effectiveness ---
    # Compare divergence at centre vs edges (meaningful for CENTRE_GRADIENT mode)
    centre_l2 = pixel_l2[CENTRE_LEDS]
    edge_l2 = np.concatenate([pixel_l2[EDGE_LEDS_LEFT], pixel_l2[EDGE_LEDS_RIGHT]])

    centre_div = float(np.mean(centre_l2))
    edge_div = float(np.mean(edge_l2))

    if centre_div > 0.0:
        spatial_gradient_effectiveness = edge_div / centre_div
    else:
        spatial_gradient_effectiveness = float("nan")

    # --- Metadata extraction ---
    rms = float("nan")
    beat = False
    if metadata is not None:
        rms = float(metadata.get("rms", float("nan")))
        beat = bool(metadata.get("beat", False))

    return {
        "strip_divergence_l2": strip_divergence_l2,
        "strip_divergence_max": strip_divergence_max,
        "hue_shift_mean": hue_shift_mean,
        "hue_shift_std": hue_shift_std,
        "saturation_delta_mean": saturation_delta_mean,
        "brightness_ratio": brightness_ratio,
        "near_black_leaks": near_black_leaks,
        "active_pixel_count": active_pixel_count,
        "spatial_gradient_effectiveness": spatial_gradient_effectiveness,
        "rms": rms,
        "beat": beat,
    }


# ---------------------------------------------------------------------------
# Run-level aggregation
# ---------------------------------------------------------------------------

def compute_run_metrics(
    frames_and_metadata: list[tuple[NDArray[np.uint8], dict | None]],
) -> dict:
    """Aggregate metrics across N captured frames.

    Parameters
    ----------
    frames_and_metadata : list of (frame, metadata) tuples
        Each frame is (320, 3) uint8. Metadata may be None.

    Returns
    -------
    dict
        Per-frame arrays, summary statistics, and aggregate counters.
    """
    n = len(frames_and_metadata)
    if n == 0:
        return {"n_frames": 0}

    # Collect per-frame metrics
    frame_metrics_list = [
        compute_frame_metrics(frame, meta)
        for frame, meta in frames_and_metadata
    ]

    # Numeric metric keys (excludes 'beat' which is boolean)
    numeric_keys = [
        "strip_divergence_l2",
        "strip_divergence_max",
        "hue_shift_mean",
        "hue_shift_std",
        "saturation_delta_mean",
        "brightness_ratio",
        "near_black_leaks",
        "active_pixel_count",
        "spatial_gradient_effectiveness",
        "rms",
    ]

    result: dict = {}

    # Build per-frame arrays
    for key in numeric_keys:
        result[key] = np.array([m[key] for m in frame_metrics_list], dtype=np.float64)

    # Beat array (boolean)
    beat_array = np.array([m["beat"] for m in frame_metrics_list], dtype=bool)
    result["beat"] = beat_array

    # Summary statistics for each numeric metric
    for key in numeric_keys:
        arr = result[key]
        # Filter out NaN for statistics
        valid = arr[~np.isnan(arr)]
        if len(valid) > 0:
            result[f"{key}_mean"] = float(np.mean(valid))
            result[f"{key}_std"] = float(np.std(valid))
            result[f"{key}_p5"] = float(np.percentile(valid, 5))
            result[f"{key}_p95"] = float(np.percentile(valid, 95))
        else:
            result[f"{key}_mean"] = float("nan")
            result[f"{key}_std"] = float("nan")
            result[f"{key}_p5"] = float("nan")
            result[f"{key}_p95"] = float("nan")

    # Temporal responsiveness: Pearson correlation(rms, divergence_l2)
    rms_arr = result["rms"]
    div_arr = result["strip_divergence_l2"]
    valid_mask = ~np.isnan(rms_arr) & ~np.isnan(div_arr)

    if np.sum(valid_mask) >= 3:
        rms_valid = rms_arr[valid_mask]
        div_valid = div_arr[valid_mask]
        # Require non-constant arrays for correlation
        if np.std(rms_valid) > 0.0 and np.std(div_valid) > 0.0:
            corr_matrix = np.corrcoef(rms_valid, div_valid)
            result["temporal_responsiveness"] = float(corr_matrix[0, 1])
        else:
            result["temporal_responsiveness"] = float("nan")
    else:
        result["temporal_responsiveness"] = float("nan")

    # Aggregate counters
    result["near_black_total_leaks"] = int(np.nansum(result["near_black_leaks"]))
    result["n_frames"] = n
    result["n_beats"] = int(np.sum(beat_array))

    return result


# ---------------------------------------------------------------------------
# Run comparison (delta)
# ---------------------------------------------------------------------------

def compute_delta(baseline_run: dict, candidate_run: dict) -> dict:
    """Compare two run metric dicts to quantify improvement or regression.

    All deltas are signed: positive means the candidate has MORE of that
    metric than the baseline.

    Parameters
    ----------
    baseline_run : dict
        Output from ``compute_run_metrics`` for the baseline configuration.
    candidate_run : dict
        Output from ``compute_run_metrics`` for the candidate configuration.

    Returns
    -------
    dict
        Delta values for key metrics.
    """
    def _delta(key_suffix: str) -> float:
        b = baseline_run.get(key_suffix, float("nan"))
        c = candidate_run.get(key_suffix, float("nan"))
        return float(c - b)

    # Brightness ratio delta: distance from 1.0 (ideal)
    b_br = baseline_run.get("brightness_ratio_mean", float("nan"))
    c_br = candidate_run.get("brightness_ratio_mean", float("nan"))
    delta_brightness_ratio = float(abs(c_br - 1.0) - abs(b_br - 1.0))

    return {
        "delta_divergence": _delta("strip_divergence_l2_mean"),
        "delta_hue_shift": _delta("hue_shift_mean_mean"),
        "delta_saturation": _delta("saturation_delta_mean_mean"),
        "delta_brightness_ratio": delta_brightness_ratio,
        "delta_near_black_leaks": _delta("near_black_total_leaks"),
        "delta_temporal_responsiveness": _delta("temporal_responsiveness"),
        "baseline_n_frames": baseline_run.get("n_frames", 0),
        "candidate_n_frames": candidate_run.get("n_frames", 0),
    }


# ---------------------------------------------------------------------------
# L0 — Basic Frame Health
# ---------------------------------------------------------------------------

def compute_l0_metrics(frames: list[NDArray[np.uint8]]) -> dict:
    """Compute basic frame health metrics across a sequence of LED frames.

    Evaluates whether frames are well-exposed: not too dark (black frames),
    not clipped (over-exposed), and maintaining reasonable brightness.

    Parameters
    ----------
    frames : list of ndarray, each shape (320, 3), dtype uint8
        Captured LED frames.

    Returns
    -------
    dict
        ``black_frame_ratio`` : float
            Fraction of frames where mean per-pixel max-channel brightness < 3.
        ``clipped_frame_ratio`` : float
            Fraction of frames where >20% of pixels have any channel at 255.
        ``mean_brightness`` : float
            Average normalised brightness (max channel / 255) across all
            pixels and frames. Range 0-1.
    """
    n = len(frames)
    if n == 0:
        return {
            "black_frame_ratio": float("nan"),
            "clipped_frame_ratio": float("nan"),
            "mean_brightness": float("nan"),
        }

    # Stack all frames: (N, 320, 3)
    all_frames = np.stack(frames)

    # Per-pixel luminance proxy: max channel value, shape (N, 320)
    max_channel = np.max(all_frames, axis=2)

    # Black frame: mean of per-pixel max-channel < 3
    per_frame_mean_brightness = np.mean(max_channel, axis=1)  # (N,)
    black_frame_ratio = float(np.mean(per_frame_mean_brightness < 3.0))

    # Clipped frame: >20% of pixels have any channel == 255
    any_clipped = np.any(all_frames == 255, axis=2)  # (N, 320) bool
    clipped_fraction_per_frame = np.mean(any_clipped, axis=1)  # (N,)
    clipped_frame_ratio = float(np.mean(clipped_fraction_per_frame > 0.20))

    # Mean brightness: average of (max_channel / 255.0) across everything
    mean_brightness = float(np.mean(max_channel / 255.0))

    return {
        "black_frame_ratio": black_frame_ratio,
        "clipped_frame_ratio": clipped_frame_ratio,
        "mean_brightness": mean_brightness,
    }


# ---------------------------------------------------------------------------
# L1 — Temporal Stability
# ---------------------------------------------------------------------------

def compute_l1_metrics(frames: list[NDArray[np.uint8]]) -> dict:
    """Compute temporal stability metrics across consecutive LED frames.

    Measures how smoothly the LED output evolves over time. Stable effects
    produce high LSS, low flicker, and high motion consistency.

    Parameters
    ----------
    frames : list of ndarray, each shape (320, 3), dtype uint8
        Captured LED frames in temporal order.

    Returns
    -------
    dict
        ``stability_lss`` : float
            Luminance Stability Score. 1.0 minus the mean normalised
            per-pixel luminance change between consecutive frames. Range 0-1.
        ``flicker_elatcsf`` : float
            Simplified temporal contrast sensitivity measure. Standard
            deviation of per-frame mean brightness divided by mean brightness.
            Higher values indicate more flicker.
        ``vmcr`` : float
            Visual Motion Consistency Ratio. Mean Pearson correlation of
            per-pixel luminance between consecutive frames. Higher values
            indicate coherent spatial motion. Range -1 to 1.
    """
    n = len(frames)
    if n < 2:
        return {
            "stability_lss": float("nan"),
            "flicker_elatcsf": float("nan"),
            "vmcr": float("nan"),
        }

    # Stack all frames: (N, 320, 3)
    all_frames = np.stack(frames)

    # Per-pixel luminance proxy: max channel, shape (N, 320)
    lum = np.max(all_frames, axis=2).astype(np.float64)

    # --- Luminance Stability Score (LSS) ---
    # Absolute per-pixel luminance change between consecutive frames
    lum_diff = np.abs(lum[1:] - lum[:-1])  # (N-1, 320)
    stability_lss = float(1.0 - np.mean(lum_diff / 255.0))

    # --- Flicker (simplified eLATCSF) ---
    # Per-frame mean brightness
    per_frame_mean = np.mean(lum / 255.0, axis=1)  # (N,)
    mean_of_means = np.mean(per_frame_mean)
    if mean_of_means > 0.0:
        flicker_elatcsf = float(np.std(per_frame_mean) / mean_of_means)
    else:
        flicker_elatcsf = 0.0

    # --- Visual Motion Consistency Ratio (VMCR) ---
    # Pearson correlation of pixel luminance between consecutive frame pairs
    correlations = np.empty(n - 1, dtype=np.float64)
    for i in range(n - 1):
        a = lum[i]
        b = lum[i + 1]
        std_a = np.std(a)
        std_b = np.std(b)
        if std_a > 0.0 and std_b > 0.0:
            correlations[i] = np.corrcoef(a, b)[0, 1]
        else:
            # Constant frame — perfect stability but undefined correlation
            correlations[i] = 1.0
    vmcr = float(np.mean(correlations))

    return {
        "stability_lss": stability_lss,
        "flicker_elatcsf": flicker_elatcsf,
        "vmcr": vmcr,
    }


# ---------------------------------------------------------------------------
# L2 — Audio-Visual Coupling
# ---------------------------------------------------------------------------

def compute_l2_metrics(
    frames: list[NDArray[np.uint8]],
    metadata_list: list[dict],
) -> dict:
    """Compute audio-visual coupling metrics between LED output and audio.

    Measures how well the LED brightness responds to audio energy, beat
    events, and onset events.

    Parameters
    ----------
    frames : list of ndarray, each shape (320, 3), dtype uint8
        Captured LED frames.
    metadata_list : list of dict
        Per-frame audio metadata. Expected keys: ``rms`` (float, 0-1),
        ``beat`` (bool), ``onset`` (bool). Missing keys default to
        NaN / False.

    Returns
    -------
    dict
        ``energy_correlation`` : float
            Pearson correlation between per-frame mean brightness and RMS.
            Range -1 to 1. NaN if RMS is constant or insufficient data.
        ``beat_alignment`` : float
            Fraction of beat events where brightness exceeds the mean of
            the 3 preceding frames. Range 0-1. NaN if no beats.
        ``onset_response`` : float
            Ratio of mean brightness during onset frames to mean brightness
            during non-onset frames. >1.0 indicates brighter during onsets.
            NaN if no onsets.
    """
    n = len(frames)
    if n == 0:
        return {
            "energy_correlation": float("nan"),
            "beat_alignment": float("nan"),
            "onset_response": float("nan"),
        }

    # Stack frames and compute per-frame mean brightness
    all_frames = np.stack(frames)
    max_channel = np.max(all_frames, axis=2).astype(np.float64)  # (N, 320)
    brightness = np.mean(max_channel / 255.0, axis=1)  # (N,)

    # Extract metadata arrays
    rms_arr = np.array(
        [float(m.get("rms", float("nan"))) for m in metadata_list],
        dtype=np.float64,
    )
    beat_arr = np.array(
        [bool(m.get("beat", False)) for m in metadata_list],
        dtype=bool,
    )
    onset_arr = np.array(
        [bool(m.get("onset", False)) for m in metadata_list],
        dtype=bool,
    )

    # --- Energy Correlation ---
    valid_rms = ~np.isnan(rms_arr)
    valid_count = int(np.sum(valid_rms))
    if valid_count >= 3:
        rms_valid = rms_arr[valid_rms]
        bright_valid = brightness[valid_rms]
        if np.std(rms_valid) > 0.0 and np.std(bright_valid) > 0.0:
            energy_correlation = float(np.corrcoef(bright_valid, rms_valid)[0, 1])
        else:
            energy_correlation = float("nan")
    else:
        energy_correlation = float("nan")

    # --- Beat Alignment ---
    beat_indices = np.where(beat_arr)[0]
    if len(beat_indices) > 0:
        aligned_count = 0
        total_beats = 0
        for idx in beat_indices:
            # Need at least 3 frames before the beat for comparison
            if idx < 3:
                continue
            pre_mean = np.mean(brightness[idx - 3:idx])
            if brightness[idx] > pre_mean:
                aligned_count += 1
            total_beats += 1
        if total_beats > 0:
            beat_alignment = float(aligned_count / total_beats)
        else:
            beat_alignment = float("nan")
    else:
        beat_alignment = float("nan")

    # --- Onset Response ---
    if np.any(onset_arr) and np.any(~onset_arr):
        onset_brightness = float(np.mean(brightness[onset_arr]))
        non_onset_brightness = float(np.mean(brightness[~onset_arr]))
        if non_onset_brightness > 0.0:
            onset_response = onset_brightness / non_onset_brightness
        else:
            onset_response = float("nan")
    else:
        onset_response = float("nan")

    return {
        "energy_correlation": energy_correlation,
        "beat_alignment": beat_alignment,
        "onset_response": onset_response,
    }


# ---------------------------------------------------------------------------
# Composite Score
# ---------------------------------------------------------------------------

def compute_composite_score(l0: dict, l1: dict, l2: dict) -> float:
    """Compute a weighted composite perceptual quality score.

    Combines frame health (L0), temporal stability (L1), and audio-visual
    coupling (L2) into a single scalar. NaN values in L2 metrics are
    treated as zero.

    Parameters
    ----------
    l0 : dict
        Output from :func:`compute_l0_metrics`.
    l1 : dict
        Output from :func:`compute_l1_metrics`.
    l2 : dict
        Output from :func:`compute_l2_metrics`.

    Returns
    -------
    float
        Composite score in the range 0-1.
    """
    def _nan_to_zero(v: float) -> float:
        return 0.0 if np.isnan(v) else v

    # L0 component (weight 0.2): penalise black/clipped frames, reward brightness
    l0_score = (
        (1.0 - _nan_to_zero(l0["black_frame_ratio"]))
        * (1.0 - _nan_to_zero(l0["clipped_frame_ratio"]))
        * min(_nan_to_zero(l0["mean_brightness"]) / 0.4, 1.0)
    )

    # L1 component (weight 0.3): reward stability, penalise flicker
    l1_score = (
        _nan_to_zero(l1["stability_lss"])
        * (1.0 - min(_nan_to_zero(l1["flicker_elatcsf"]), 1.0))
        * max(_nan_to_zero(l1["vmcr"]), 0.0)
    )

    # L2 component (weight 0.5): weighted mix of audio-visual metrics
    energy_corr = max(_nan_to_zero(l2["energy_correlation"]), 0.0)
    beat_align = max(_nan_to_zero(l2["beat_alignment"]), 0.0)
    onset_resp = min(_nan_to_zero(l2["onset_response"]) / 2.0, 1.0)
    l2_score = energy_corr * 0.4 + beat_align * 0.3 + onset_resp * 0.3

    composite = l0_score * 0.2 + l1_score * 0.3 + l2_score * 0.5
    return float(composite)


# ---------------------------------------------------------------------------
# Full Evaluation
# ---------------------------------------------------------------------------

def compute_full_evaluation(
    frames_and_metadata: list[tuple[NDArray[np.uint8], dict | None]],
) -> dict:
    """Run all metric layers and return a unified evaluation dictionary.

    Convenience function that computes L0 (frame health), L1 (temporal
    stability), L2 (audio-visual coupling), the composite score, and
    the existing EdgeMixer-specific run metrics in a single call.

    Parameters
    ----------
    frames_and_metadata : list of (frame, metadata) tuples
        Each frame is (320, 3) uint8. Metadata may be None; missing
        metadata is replaced with an empty dict for L2 computation.

    Returns
    -------
    dict
        Combined results with keys:

        - ``L0`` : dict from :func:`compute_l0_metrics`
        - ``L1`` : dict from :func:`compute_l1_metrics`
        - ``L2`` : dict from :func:`compute_l2_metrics`
        - ``composite`` : float from :func:`compute_composite_score`
        - ``strip_divergence_l2`` : float, mean L2 divergence from run metrics
        - ``near_black_leaks`` : int, total leak count from run metrics
        - All keys from :func:`compute_run_metrics`
    """
    # Separate frames and metadata
    frames = [fm[0] for fm in frames_and_metadata]
    metadata_list = [fm[1] if fm[1] is not None else {} for fm in frames_and_metadata]

    # Perceptual quality layers
    l0 = compute_l0_metrics(frames)
    l1 = compute_l1_metrics(frames)
    l2 = compute_l2_metrics(frames, metadata_list)
    composite = compute_composite_score(l0, l1, l2)

    # Existing EdgeMixer-specific run metrics
    run = compute_run_metrics(frames_and_metadata)

    # Merge everything
    result = dict(run)
    result["L0"] = l0
    result["L1"] = l1
    result["L2"] = l2
    result["composite"] = composite
    result["strip_divergence_l2"] = run.get("strip_divergence_l2_mean", float("nan"))
    result["near_black_leaks"] = run.get("near_black_total_leaks", 0)

    return result
