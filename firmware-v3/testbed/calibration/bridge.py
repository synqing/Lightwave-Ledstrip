"""Calibration bridge for comparing PyTorch transport core against firmware reference.

The CalibrationBridge loads firmware reference pairs and runs them through
the PyTorch transport core, comparing outputs via multiple metrics.
"""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Any, Optional, Tuple
import numpy as np

try:
    import torch
    import torch.nn as nn
except ImportError:
    nn = None
    torch = None

from .metrics import (
    per_pixel_l2,
    cosine_similarity,
    energy_total,
    energy_divergence,
    tone_map_match_rate,
)


@dataclass
class CalibrationMetrics:
    """Container for individual metric results."""

    per_pixel_l2: float = 0.0
    cosine_similarity: float = 0.0
    energy_divergence: float = 0.0  # percent
    tone_map_agreement: float = 0.0  # fraction [0, 1]

    def to_dict(self) -> Dict[str, float]:
        """Convert to dictionary."""
        return {
            "per_pixel_l2": self.per_pixel_l2,
            "cosine_similarity": self.cosine_similarity,
            "energy_divergence_percent": self.energy_divergence,
            "tone_map_agreement": self.tone_map_agreement,
        }


@dataclass
class CalibrationReport:
    """Complete calibration report with pass/fail verdict."""

    reference_count: int = 0
    metrics: CalibrationMetrics = field(default_factory=CalibrationMetrics)
    per_reference_results: list = field(default_factory=list)

    # Pass/fail thresholds
    l2_threshold: float = 0.01
    energy_threshold: float = 1.0  # percent
    tone_map_threshold: float = 0.99  # fraction

    def passes_all(self) -> bool:
        """Check if all metrics pass their thresholds."""
        return (
            self.metrics.per_pixel_l2 < self.l2_threshold
            and self.metrics.energy_divergence < self.energy_threshold
            and self.metrics.tone_map_agreement > self.tone_map_threshold
        )

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "reference_count": self.reference_count,
            "passes_all": self.passes_all(),
            "metrics": self.metrics.to_dict(),
            "thresholds": {
                "l2_threshold": self.l2_threshold,
                "energy_threshold": self.energy_threshold,
                "tone_map_threshold": self.tone_map_threshold,
            },
            "per_reference_results": self.per_reference_results,
        }

    def summary_string(self) -> str:
        """Generate human-readable summary."""
        lines = [
            "=" * 70,
            "CALIBRATION REPORT",
            "=" * 70,
            f"Reference pairs tested: {self.reference_count}",
            "",
            "METRICS:",
            f"  Per-pixel L2 error:        {self.metrics.per_pixel_l2:.6f}",
            f"    Threshold:               < {self.l2_threshold:.4f} ",
            f"    Status:                  {'PASS' if self.metrics.per_pixel_l2 < self.l2_threshold else 'FAIL'}",
            "",
            f"  Cosine similarity:         {self.metrics.cosine_similarity:.6f}",
            "",
            f"  Energy divergence:         {self.metrics.energy_divergence:.4f}%",
            f"    Threshold:               < {self.energy_threshold:.2f}%",
            f"    Status:                  {'PASS' if self.metrics.energy_divergence < self.energy_threshold else 'FAIL'}",
            "",
            f"  Tone map agreement (±1):   {self.metrics.tone_map_agreement:.4f}",
            f"    Threshold:               > {self.tone_map_threshold:.4f}",
            f"    Status:                  {'PASS' if self.metrics.tone_map_agreement > self.tone_map_threshold else 'FAIL'}",
            "",
            "=" * 70,
            f"OVERALL:                     {'PASS' if self.passes_all() else 'FAIL'}",
            "=" * 70,
        ]
        return "\n".join(lines)


class CalibrationBridge:
    """Bridge for comparing PyTorch transport core against firmware reference.

    Loads reference parameter-output pairs from disk, runs them through the
    PyTorch transport core, and compares outputs via multiple metrics.
    """

    def __init__(
        self,
        reference_path: str,
        transport_core: Optional["nn.Module"] = None,
        device: str = "cpu",
    ):
        """Initialize calibration bridge.

        Args:
            reference_path: Path to firmware reference pairs (e.g., .npz file)
            transport_core: PyTorch TransportCore module (optional, can be set later)
            device: torch device for computation (default: "cpu")
        """
        self.reference_path = Path(reference_path)
        self.transport_core = transport_core
        self.device = device

        self.reference_data: Optional[Dict[str, np.ndarray]] = None
        self.reference_params: Optional[np.ndarray] = None
        self.reference_outputs: Optional[np.ndarray] = None

        if transport_core is not None and torch is not None:
            self.transport_core.to(device)
            self.transport_core.eval()

    def load_reference(self) -> None:
        """Load reference pairs from disk.

        Expects .npz file with keys:
        - 'params': [N, D] parameter array
        - 'outputs': [N, radial_len, 3] output array
        """
        if not self.reference_path.exists():
            raise FileNotFoundError(f"Reference file not found: {self.reference_path}")

        self.reference_data = np.load(self.reference_path)
        self.reference_params = self.reference_data.get("params")
        self.reference_outputs = self.reference_data.get("outputs")

        if self.reference_params is None or self.reference_outputs is None:
            raise ValueError(
                "Reference file must contain 'params' and 'outputs' keys"
            )

        print(
            f"Loaded {len(self.reference_params)} reference pairs "
            f"from {self.reference_path}"
        )

    def set_transport_core(self, transport_core: "nn.Module") -> None:
        """Set or update the transport core.

        Args:
            transport_core: PyTorch TransportCore module
        """
        self.transport_core = transport_core
        if torch is not None:
            self.transport_core.to(self.device)
            self.transport_core.eval()

    def run_comparison(
        self,
        n_frames: int = 100,
        subset_indices: Optional[np.ndarray] = None,
        verbose: bool = True,
    ) -> CalibrationReport:
        """Run all reference params through PyTorch, compare against firmware output.

        Args:
            n_frames: Number of frames to simulate (default 100)
            subset_indices: Optional array of indices to test (default: all)
            verbose: Print progress (default True)

        Returns:
            CalibrationReport with metrics and pass/fail status
        """
        if self.reference_params is None:
            self.load_reference()

        if self.transport_core is None:
            raise RuntimeError("Transport core not set. Call set_transport_core() first.")

        if torch is None:
            raise ImportError("PyTorch required for calibration")

        # Determine which indices to test
        if subset_indices is None:
            subset_indices = np.arange(len(self.reference_params))
        else:
            subset_indices = np.asarray(subset_indices)

        # Initialize accumulators
        l2_errors = []
        cosine_sims = []
        energy_divs = []
        tone_map_rates = []
        per_reference = []

        # Iterate through reference pairs
        for idx in subset_indices:
            params = self.reference_params[idx]
            firmware_output = self.reference_outputs[idx]

            # Convert params to torch tensor
            params_tensor = torch.from_numpy(params).float().to(self.device)

            # Run through PyTorch transport core
            with torch.no_grad():
                pytorch_output = self.transport_core(params_tensor)

            # Convert back to numpy
            if hasattr(pytorch_output, "cpu"):
                pytorch_output = pytorch_output.cpu().numpy()

            # Ensure shapes match
            if pytorch_output.ndim == 1:
                pytorch_output = pytorch_output[np.newaxis, :]
            if firmware_output.ndim == 1:
                firmware_output = firmware_output[np.newaxis, :]

            # Compute metrics
            l2_err = per_pixel_l2(pytorch_output, firmware_output)
            cos_sim = cosine_similarity(pytorch_output, firmware_output)

            # Energy divergence (if trajectory is available, use it; otherwise estimate)
            # For now, assume outputs are final states
            energy_div = energy_divergence(
                pytorch_output[np.newaxis, :, :],
                firmware_output[np.newaxis, :, :],
                n_frames=n_frames,
            )

            # Tone map agreement (assuming firmware_output is uint8)
            tone_rate = tone_map_match_rate(pytorch_output, firmware_output.astype(np.uint8))

            l2_errors.append(l2_err)
            cosine_sims.append(cos_sim)
            energy_divs.append(energy_div * 100)  # Convert to percent
            tone_map_rates.append(tone_rate)

            per_reference.append({
                "index": int(idx),
                "l2_error": float(l2_err),
                "cosine_similarity": float(cos_sim),
                "energy_divergence_percent": float(energy_div * 100),
                "tone_map_rate": float(tone_rate),
            })

            if verbose and (len(l2_errors) % max(1, len(subset_indices) // 10) == 0):
                print(f"  Processed {len(l2_errors)}/{len(subset_indices)} pairs...")

        # Compute aggregate metrics
        report = CalibrationReport(reference_count=len(subset_indices))
        report.metrics = CalibrationMetrics(
            per_pixel_l2=float(np.mean(l2_errors)),
            cosine_similarity=float(np.mean(cosine_sims)),
            energy_divergence=float(np.mean(energy_divs)),
            tone_map_agreement=float(np.mean(tone_map_rates)),
        )
        report.per_reference_results = per_reference

        if verbose:
            print(report.summary_string())

        return report

    def per_pixel_l2(
        self,
        pytorch_output: np.ndarray,
        firmware_output: np.ndarray,
    ) -> float:
        """Per-pixel L2 distance normalised to [0, 1].

        Args:
            pytorch_output: PyTorch output, shape [B, radial_len, 3]
            firmware_output: Firmware output, same shape

        Returns:
            Normalised L2 distance
        """
        return per_pixel_l2(pytorch_output, firmware_output)

    def energy_conservation(
        self,
        pytorch_trajectory: np.ndarray,
        firmware_trajectory: np.ndarray,
        n_frames: int,
    ) -> float:
        """Compare cumulative energy over n_frames.

        Args:
            pytorch_trajectory: PyTorch trajectory, shape [n_frames, radial_len, 3]
            firmware_trajectory: Firmware trajectory, same shape
            n_frames: Number of frames considered

        Returns:
            Relative energy divergence (0.0 = perfect match)
        """
        return energy_divergence(pytorch_trajectory, firmware_trajectory, n_frames)

    def tone_map_agreement(
        self,
        pytorch_float: np.ndarray,
        firmware_uint8: np.ndarray,
    ) -> float:
        """Fraction of pixels where tone-mapped 8-bit output matches within ±1 LSB.

        Args:
            pytorch_float: Float output from PyTorch, shape [B, radial_len, 3]
            firmware_uint8: 8-bit output from firmware, same shape

        Returns:
            Fraction matching within ±1 LSB, in range [0, 1]
        """
        return tone_map_match_rate(pytorch_float, firmware_uint8, tolerance=1)


__all__ = ["CalibrationBridge", "CalibrationReport", "CalibrationMetrics"]
