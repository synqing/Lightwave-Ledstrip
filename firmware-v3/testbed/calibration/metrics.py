"""Comparison metrics for firmware vs PyTorch calibration.

This module provides standalone metric functions that work with both numpy
and torch tensors. All functions are designed for comparing transport core
outputs (shape [B, radial_len, 3] or [radial_len, 3]).
"""

from typing import Union, Tuple
import numpy as np


def per_pixel_l2(
    output_a: Union[np.ndarray, "torch.Tensor"],
    output_b: Union[np.ndarray, "torch.Tensor"],
) -> float:
    """Compute per-pixel L2 distance normalised to [0, 1].

    For outputs of shape [B, radial_len, 3] or [radial_len, 3], computes
    the L2 distance between all pixels and normalises by the maximum possible
    distance (sqrt(3) for [0,1] values).

    Args:
        output_a: Reference output, shape [B, radial_len, 3] or [radial_len, 3]
        output_b: Test output, same shape

    Returns:
        Normalised L2 distance in range [0, 1], where 0 = perfect match.
    """
    # Convert to numpy if torch tensor
    if hasattr(output_a, "cpu"):
        output_a = output_a.detach().cpu().numpy()
    if hasattr(output_b, "cpu"):
        output_b = output_b.detach().cpu().numpy()

    # Ensure float type
    output_a = np.asarray(output_a, dtype=np.float32)
    output_b = np.asarray(output_b, dtype=np.float32)

    # Compute per-pixel L2
    diff = output_a - output_b
    per_pixel_dist = np.linalg.norm(diff, axis=-1)  # Shape: [B, radial_len] or [radial_len]

    # Normalise by max possible distance (sqrt(3) for [0,1] range)
    max_dist = np.sqrt(3.0)
    normalised = np.mean(per_pixel_dist) / max_dist

    return float(normalised)


def cosine_similarity(
    output_a: Union[np.ndarray, "torch.Tensor"],
    output_b: Union[np.ndarray, "torch.Tensor"],
) -> float:
    """Compute per-frame cosine similarity between outputs.

    For outputs of shape [B, radial_len, 3], computes cosine similarity
    for each frame by flattening spatial dimensions and averaging.

    Args:
        output_a: Reference output, shape [B, radial_len, 3]
        output_b: Test output, same shape

    Returns:
        Mean cosine similarity over all frames, in range [-1, 1].
        1.0 = perfect alignment, 0.0 = orthogonal, -1.0 = opposite.
    """
    # Convert to numpy if torch tensor
    if hasattr(output_a, "cpu"):
        output_a = output_a.detach().cpu().numpy()
    if hasattr(output_b, "cpu"):
        output_b = output_b.detach().cpu().numpy()

    output_a = np.asarray(output_a, dtype=np.float32)
    output_b = np.asarray(output_b, dtype=np.float32)

    # Flatten spatial dimensions for each batch element
    # From [B, radial_len, 3] to [B, radial_len*3]
    original_shape = output_a.shape
    if len(original_shape) == 3:
        a_flat = output_a.reshape(original_shape[0], -1)
        b_flat = output_b.reshape(original_shape[0], -1)
    else:
        # Single frame: [radial_len, 3] -> [radial_len*3]
        a_flat = output_a.reshape(-1)
        b_flat = output_b.reshape(-1)

    # Compute cosine similarity: dot(a,b) / (||a|| * ||b||)
    if len(original_shape) == 3:
        # Batch case
        dots = np.sum(a_flat * b_flat, axis=1)
        norms_a = np.linalg.norm(a_flat, axis=1)
        norms_b = np.linalg.norm(b_flat, axis=1)
        similarities = dots / (norms_a * norms_b + 1e-8)
        return float(np.mean(similarities))
    else:
        # Single frame
        dot = np.sum(a_flat * b_flat)
        norm_a = np.linalg.norm(a_flat)
        norm_b = np.linalg.norm(b_flat)
        return float(dot / (norm_a * norm_b + 1e-8))


def energy_total(state: Union[np.ndarray, "torch.Tensor"]) -> float:
    """Compute total energy as sum of all channel values.

    For a state of shape [radial_len, 3], computes the sum of all values
    as an energy proxy.

    Args:
        state: Transport state, shape [radial_len, 3]

    Returns:
        Total energy (sum of all channel values).
    """
    if hasattr(state, "cpu"):
        state = state.detach().cpu().numpy()

    state = np.asarray(state, dtype=np.float32)
    return float(np.sum(state))


def energy_divergence(
    trajectory_a: Union[np.ndarray, "torch.Tensor"],
    trajectory_b: Union[np.ndarray, "torch.Tensor"],
    n_frames: int,
) -> float:
    """Compute relative energy difference after n_frames.

    For trajectories of shape [n_frames, radial_len, 3], computes the
    absolute energy at frame n_frames for both and returns the relative
    difference.

    Args:
        trajectory_a: Reference trajectory, shape [n_frames, radial_len, 3]
        trajectory_b: Test trajectory, same shape
        n_frames: Number of frames to consider (typically trajectory_a.shape[0])

    Returns:
        Relative energy divergence in range [0, inf).
        0.0 = perfect energy conservation match.
    """
    # Convert to numpy if torch tensor
    if hasattr(trajectory_a, "cpu"):
        trajectory_a = trajectory_a.detach().cpu().numpy()
    if hasattr(trajectory_b, "cpu"):
        trajectory_b = trajectory_b.detach().cpu().numpy()

    trajectory_a = np.asarray(trajectory_a, dtype=np.float32)
    trajectory_b = np.asarray(trajectory_b, dtype=np.float32)

    # Get final frame energy
    energy_a = energy_total(trajectory_a[-1])
    energy_b = energy_total(trajectory_b[-1])

    # Compute relative difference
    avg_energy = (energy_a + energy_b) / 2.0 + 1e-8
    rel_diff = abs(energy_a - energy_b) / avg_energy

    return float(rel_diff)


def tone_map_match_rate(
    float_output: Union[np.ndarray, "torch.Tensor"],
    uint8_reference: Union[np.ndarray, "torch.Tensor"],
    tolerance: int = 1,
) -> float:
    """Compute fraction of pixels matching within ±tolerance LSB after tone mapping.

    Converts float output to 8-bit via standard tone mapping (linear scaling to [0,255])
    and compares against 8-bit reference. Counts pixels matching within ±tolerance.

    Args:
        float_output: Float-valued output, shape [B, radial_len, 3] or [radial_len, 3]
                      with values in [0, 1]
        uint8_reference: 8-bit reference output, same shape, values in [0, 255]
        tolerance: Tolerance in LSB (default 1 LSB)

    Returns:
        Fraction of pixels matching within tolerance, in range [0, 1].
        1.0 = perfect match, 0.0 = no match.
    """
    # Convert to numpy if torch tensor
    if hasattr(float_output, "cpu"):
        float_output = float_output.detach().cpu().numpy()
    if hasattr(uint8_reference, "cpu"):
        uint8_reference = uint8_reference.detach().cpu().numpy()

    float_output = np.asarray(float_output, dtype=np.float32)
    uint8_reference = np.asarray(uint8_reference, dtype=np.uint8)

    # Tone map float to 8-bit (simple linear scaling)
    float_scaled = np.clip(float_output * 255.0, 0, 255).astype(np.uint8)

    # Count matches within tolerance
    diff = np.abs(float_scaled.astype(np.int16) - uint8_reference.astype(np.int16))
    matches = np.sum(diff <= tolerance)
    total = float_output.size

    return float(matches) / float(total)


__all__ = [
    "per_pixel_l2",
    "cosine_similarity",
    "energy_total",
    "energy_divergence",
    "tone_map_match_rate",
]
