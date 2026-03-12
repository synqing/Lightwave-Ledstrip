"""Training data generator for DIFNO using PyTorch transport core.

Generates (params, output, [jacobian]) training pairs by sampling from
parameter space and running through the transport core.
"""

from dataclasses import dataclass
from typing import Dict, Optional, Tuple, Union
import numpy as np
from pathlib import Path

try:
    import torch
    import torch.nn as nn
except ImportError:
    torch = None
    nn = None


# Default parameter ranges for transport core
DEFAULT_RANGES = {
    "intensity": (0.1, 1.0),
    "wavelength": (400, 700),  # nm
    "angle": (0.0, 2 * np.pi),
    "diffusion": (0.0, 0.5),
}


class TrainingDataGenerator:
    """Generates training pairs using PyTorch transport core.

    Supports multiple sampling methods (Sobol, random, grid) and optional
    Jacobian computation via autograd.
    """

    def __init__(
        self,
        transport_core: "nn.Module",
        radial_len: int = 80,
        device: str = "cpu",
    ):
        """Initialize generator with transport core.

        Args:
            transport_core: PyTorch TransportCore module
            radial_len: Radial dimension of output (default 80)
            device: torch device for computation (default "cpu")
        """
        if torch is None:
            raise ImportError("PyTorch required for data generation")

        self.transport_core = transport_core
        self.radial_len = radial_len
        self.device = device

        self.transport_core.to(device)
        self.transport_core.eval()

    def _sample_parameters(
        self,
        n_samples: int,
        param_ranges: Dict[str, Tuple[float, float]],
        method: str = "sobol",
    ) -> np.ndarray:
        """Sample parameters using specified method.

        Args:
            n_samples: Number of samples to generate
            param_ranges: Dictionary mapping param names to (min, max) tuples
            method: Sampling method ("sobol", "random", or "grid")

        Returns:
            Parameter array of shape [n_samples, D] where D = len(param_ranges)
        """
        param_names = sorted(param_ranges.keys())
        n_params = len(param_names)

        if method == "sobol":
            # Sobol quasi-random sampling
            try:
                from scipy.stats import qmc
                sampler = qmc.Sobol(d=n_params, scramble=True)
                samples = sampler.random(n_samples)  # [0, 1]
            except ImportError:
                print(
                    "scipy not available, falling back to random sampling"
                )
                samples = np.random.rand(n_samples, n_params)
        elif method == "random":
            # Uniform random sampling
            samples = np.random.rand(n_samples, n_params)
        elif method == "grid":
            # Grid sampling (warning: exponential in D)
            n_per_dim = int(np.round(n_samples ** (1.0 / n_params)))
            grids = [
                np.linspace(0, 1, n_per_dim) for _ in range(n_params)
            ]
            mesh = np.meshgrid(*grids, indexing="ij")
            samples = np.stack([m.flatten() for m in mesh], axis=1)
            samples = samples[: n_samples]  # Truncate if needed
        else:
            raise ValueError(f"Unknown sampling method: {method}")

        # Scale samples to parameter ranges
        params = np.zeros((len(samples), n_params), dtype=np.float32)
        for i, name in enumerate(param_names):
            min_val, max_val = param_ranges[name]
            params[:, i] = samples[:, i] * (max_val - min_val) + min_val

        return params

    def generate_pairs(
        self,
        n_pairs: int,
        n_frames: int = 100,
        param_ranges: Optional[Dict[str, Tuple[float, float]]] = None,
        method: str = "sobol",
    ) -> Dict[str, np.ndarray]:
        """Generate training pairs (params, output).

        Args:
            n_pairs: Number of training pairs to generate
            n_frames: Number of frames to simulate (for reference, not used in output)
            param_ranges: Parameter ranges (default: DEFAULT_RANGES)
            method: Sampling method ("sobol", "random", "grid")

        Returns:
            Dictionary with:
            - 'params': [n_pairs, n_params] parameter array
            - 'outputs': [n_pairs, radial_len, 3] transport output
            - 'param_names': list of parameter names
            - 'n_frames': number of frames used
        """
        if param_ranges is None:
            param_ranges = DEFAULT_RANGES

        # Sample parameters
        params = self._sample_parameters(n_pairs, param_ranges, method)

        # Run through transport core
        outputs = []
        with torch.no_grad():
            for i, param in enumerate(params):
                # Convert to tensor
                param_tensor = torch.from_numpy(param).float().to(self.device)

                # Forward pass
                output = self.transport_core(param_tensor)

                # Convert to numpy
                if hasattr(output, "cpu"):
                    output = output.cpu().numpy()

                # Ensure correct shape [radial_len, 3]
                output = np.asarray(output, dtype=np.float32)
                if output.ndim == 1:
                    output = output.reshape(-1, 3)

                outputs.append(output)

                if (i + 1) % max(1, n_pairs // 10) == 0:
                    print(f"  Generated {i + 1}/{n_pairs} pairs...")

        outputs = np.stack(outputs, axis=0)  # [n_pairs, radial_len, 3]

        param_names = sorted(param_ranges.keys())

        return {
            "params": params,
            "outputs": outputs,
            "param_names": param_names,
            "n_frames": n_frames,
        }

    def generate_with_jacobians(
        self,
        n_pairs: int,
        n_frames: int = 100,
        param_ranges: Optional[Dict[str, Tuple[float, float]]] = None,
        method: str = "sobol",
    ) -> Dict[str, np.ndarray]:
        """Generate pairs WITH Jacobian ∂output/∂params via torch.autograd.

        Args:
            n_pairs: Number of training pairs to generate
            n_frames: Number of frames to simulate
            param_ranges: Parameter ranges (default: DEFAULT_RANGES)
            method: Sampling method ("sobol", "random", "grid")

        Returns:
            Dictionary with:
            - 'params': [n_pairs, n_params]
            - 'outputs': [n_pairs, radial_len, 3]
            - 'jacobians': [n_pairs, radial_len*3, n_params] via autograd
            - 'param_names': list of parameter names
            - 'n_frames': number of frames used
        """
        if param_ranges is None:
            param_ranges = DEFAULT_RANGES

        # Sample parameters
        params = self._sample_parameters(n_pairs, param_ranges, method)

        # Get number of output elements
        n_outputs = self.radial_len * 3

        # Run through transport core with autograd
        param_names = sorted(param_ranges.keys())
        n_params = len(param_names)
        jacobians = np.zeros((n_pairs, n_outputs, n_params), dtype=np.float32)
        outputs = []

        for i, param in enumerate(params):
            # Convert to tensor with grad enabled
            param_tensor = torch.from_numpy(param).float().to(self.device)
            param_tensor.requires_grad = True

            # Forward pass
            output = self.transport_core(param_tensor)

            # Ensure output is 1D for Jacobian computation
            output_flat = output.reshape(-1)

            # Compute Jacobian row by row (per output element)
            for j in range(len(output_flat)):
                # Zero gradients
                if param_tensor.grad is not None:
                    param_tensor.grad.zero_()

                # Backward for this output element
                output_flat[j].backward(retain_graph=(j < len(output_flat) - 1))

                # Extract gradient
                if param_tensor.grad is not None:
                    jacobians[i, j, :] = param_tensor.grad.cpu().numpy()

            # Get final output (without grad)
            with torch.no_grad():
                output = self.transport_core(param_tensor)
                if hasattr(output, "cpu"):
                    output = output.cpu().numpy()

            output = np.asarray(output, dtype=np.float32)
            if output.ndim == 1:
                output = output.reshape(-1, 3)

            outputs.append(output)

            if (i + 1) % max(1, n_pairs // 10) == 0:
                print(f"  Generated {i + 1}/{n_pairs} pairs (with Jacobians)...")

        outputs = np.stack(outputs, axis=0)  # [n_pairs, radial_len, 3]

        return {
            "params": params,
            "outputs": outputs,
            "jacobians": jacobians,
            "param_names": param_names,
            "n_frames": n_frames,
        }

    def save_pairs(
        self,
        data: Dict[str, np.ndarray],
        output_path: Union[str, Path],
    ) -> None:
        """Save generated pairs to disk as .npz.

        Args:
            data: Dictionary returned by generate_pairs() or generate_with_jacobians()
            output_path: Path to save .npz file
        """
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        np.savez_compressed(output_path, **data)
        print(f"Saved {len(data['params'])} pairs to {output_path}")

    def load_pairs(self, input_path: Union[str, Path]) -> Dict[str, np.ndarray]:
        """Load previously generated pairs from disk.

        Args:
            input_path: Path to .npz file

        Returns:
            Dictionary with 'params', 'outputs', and optionally 'jacobians'
        """
        input_path = Path(input_path)
        if not input_path.exists():
            raise FileNotFoundError(f"Data file not found: {input_path}")

        data = np.load(input_path)
        return {key: data[key] for key in data.files}


__all__ = ["TrainingDataGenerator", "DEFAULT_RANGES"]
