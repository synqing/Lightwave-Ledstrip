"""Training data generation and dataset utilities for DIFNO.

Exports:
- TrainingDataGenerator: Generates (params, output, [jacobian]) training pairs
- TransportDataset: PyTorch Dataset wrapper for DIFNO training
"""

from .data_generator import TrainingDataGenerator
from .dataset import TransportDataset

__all__ = [
    "TrainingDataGenerator",
    "TransportDataset",
]
