"""PyTorch Dataset wrapper for DIFNO training data.

Loads pre-generated training pairs and provides a standard PyTorch Dataset
interface for use in DataLoaders.
"""

from pathlib import Path
from typing import Dict, List, Optional, Union
import numpy as np

try:
    import torch
    from torch.utils.data import Dataset, DataLoader
except ImportError:
    torch = None
    Dataset = object
    DataLoader = None


class TransportDataset(Dataset):
    """PyTorch Dataset for transport core training pairs.

    Loads (params, output, [jacobian]) triples from disk and provides
    standard Dataset interface for DataLoaders.
    """

    def __init__(
        self,
        data_path: Union[str, Path],
        include_jacobians: bool = False,
    ):
        """Initialize dataset from saved .npz file.

        Args:
            data_path: Path to .npz file with 'params' and 'outputs' keys
            include_jacobians: If True, load 'jacobians' key if available
        """
        if torch is None:
            raise ImportError("PyTorch required for TransportDataset")

        self.data_path = Path(data_path)
        if not self.data_path.exists():
            raise FileNotFoundError(f"Data file not found: {data_path}")

        # Load data
        self.data = np.load(self.data_path)
        self.params = self.data["params"].astype(np.float32)
        self.outputs = self.data["outputs"].astype(np.float32)

        # Load jacobians if requested
        self.jacobians: Optional[np.ndarray] = None
        self.include_jacobians = include_jacobians
        if include_jacobians and "jacobians" in self.data.files:
            self.jacobians = self.data["jacobians"].astype(np.float32)

        # Store metadata if available
        self.param_names = list(self.data.get("param_names", []))
        if isinstance(self.param_names[0], np.ndarray):
            self.param_names = [
                str(name.tobytes(), 'utf-8') if isinstance(name, np.ndarray) else str(name)
                for name in self.param_names
            ]

        self.n_frames = int(self.data.get("n_frames", [100])[0]) if "n_frames" in self.data.files else 100

        print(f"Loaded TransportDataset: {len(self)} pairs, shape {self.outputs.shape}")
        if self.jacobians is not None:
            print(f"  Jacobians available: {self.jacobians.shape}")

    def __len__(self) -> int:
        """Return number of samples."""
        return len(self.params)

    def __getitem__(self, idx: int) -> Dict[str, torch.Tensor]:
        """Return a single sample.

        Args:
            idx: Index of sample

        Returns:
            Dictionary with keys:
            - 'params': [n_params] parameter vector
            - 'output': [radial_len, 3] output
            - 'jacobian': [radial_len*3, n_params] (optional)
        """
        sample = {
            "params": torch.from_numpy(self.params[idx]),
            "output": torch.from_numpy(self.outputs[idx]),
        }

        if self.include_jacobians and self.jacobians is not None:
            sample["jacobian"] = torch.from_numpy(self.jacobians[idx])

        return sample

    @staticmethod
    def collate_fn(batch: List[Dict[str, torch.Tensor]]) -> Dict[str, torch.Tensor]:
        """Custom collate function for DataLoader.

        Stacks individual samples into batches, handling optional jacobians.

        Args:
            batch: List of samples from __getitem__

        Returns:
            Dictionary with batched tensors:
            - 'params': [batch_size, n_params]
            - 'output': [batch_size, radial_len, 3]
            - 'jacobian': [batch_size, radial_len*3, n_params] (if present)
        """
        params = torch.stack([s["params"] for s in batch], dim=0)
        outputs = torch.stack([s["output"] for s in batch], dim=0)

        batched = {
            "params": params,
            "output": outputs,
        }

        # Handle jacobians if present
        if "jacobian" in batch[0]:
            jacobians = torch.stack([s["jacobian"] for s in batch], dim=0)
            batched["jacobian"] = jacobians

        return batched

    def get_dataloader(
        self,
        batch_size: int = 32,
        shuffle: bool = True,
        num_workers: int = 0,
        pin_memory: bool = False,
    ) -> "DataLoader":
        """Create a PyTorch DataLoader from this dataset.

        Args:
            batch_size: Batch size (default 32)
            shuffle: Shuffle batches (default True)
            num_workers: Number of data loading workers (default 0)
            pin_memory: Pin memory for faster GPU transfer (default False)

        Returns:
            DataLoader instance
        """
        if DataLoader is None:
            raise ImportError("PyTorch required")

        return DataLoader(
            self,
            batch_size=batch_size,
            shuffle=shuffle,
            num_workers=num_workers,
            pin_memory=pin_memory,
            collate_fn=self.collate_fn,
        )

    def get_param_stats(self) -> Dict[str, Dict[str, float]]:
        """Compute statistics for each parameter.

        Returns:
            Dictionary mapping param names to {min, max, mean, std}
        """
        stats = {}
        for i, name in enumerate(self.param_names):
            param_vals = self.params[:, i]
            stats[name] = {
                "min": float(np.min(param_vals)),
                "max": float(np.max(param_vals)),
                "mean": float(np.mean(param_vals)),
                "std": float(np.std(param_vals)),
            }
        return stats

    def get_output_stats(self) -> Dict[str, float]:
        """Compute statistics for outputs.

        Returns:
            Dictionary with min, max, mean, std over all output values
        """
        return {
            "min": float(np.min(self.outputs)),
            "max": float(np.max(self.outputs)),
            "mean": float(np.mean(self.outputs)),
            "std": float(np.std(self.outputs)),
        }

    def __repr__(self) -> str:
        """String representation."""
        jacob_str = f", jacobians {self.jacobians.shape}" if self.jacobians is not None else ""
        return (
            f"TransportDataset("
            f"{len(self)} pairs, "
            f"params {self.params.shape}, "
            f"outputs {self.outputs.shape}{jacob_str})"
        )


__all__ = ["TransportDataset"]
