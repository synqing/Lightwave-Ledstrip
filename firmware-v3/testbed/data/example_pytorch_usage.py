#!/usr/bin/env python3
"""
Example: Using reference pairs for PyTorch model training

This demonstrates loading the reference data and setting up a basic
neural network to learn the BeatPulseTransportCore mapping.

Usage:
    python3 example_pytorch_usage.py
"""

import numpy as np
import read_reference
from pathlib import Path

# Try to import PyTorch (optional)
try:
    import torch
    import torch.nn as nn
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False
    print("Warning: PyTorch not available. Install with: pip3 install torch")


def load_reference_data():
    """Load reference pairs from binary file."""
    reference_file = Path(__file__).parent / 'reference_pairs.bin'

    if not reference_file.exists():
        print(f"Error: {reference_file} not found")
        print("Generate it with: pio run -e native_test")
        return None

    data = read_reference.read_reference_file(str(reference_file))
    return data


def analyze_reference_data(data):
    """Print analysis of reference data structure."""
    print("Reference Data Analysis")
    print("=" * 60)

    params = data['params']
    state_16bit = data['state_16bit']
    state_8bit = data['state_8bit']

    print(f"Number of pairs: {data['num_pairs']}")
    print(f"Radial bins: 80 (0 to 79)")
    print(f"Color channels: 3 (R, G, B)")
    print()

    print("Parameter Grid")
    print("-" * 60)
    for name, ranges in data['param_ranges'].items():
        print(f"  {name:12s}: {ranges['min']:8.5f} to {ranges['max']:8.5f}")
        print(f"                 ({ranges['unique']} unique values)")
    print()

    print("State Statistics (16-bit HDR)")
    print("-" * 60)
    print(f"  Shape: {state_16bit.shape}")
    print(f"  Range: [{state_16bit.min()}, {state_16bit.max()}]")
    print(f"  Mean: {state_16bit.astype(np.float32).mean():.1f}")
    print(f"  Std:  {state_16bit.astype(np.float32).std():.1f}")
    print()

    print("State Statistics (8-bit Tone-Mapped)")
    print("-" * 60)
    print(f"  Shape: {state_8bit.shape}")
    print(f"  Range: [{state_8bit.min()}, {state_8bit.max()}]")
    print(f"  Mean: {state_8bit.astype(np.float32).mean():.1f}")
    print(f"  Std:  {state_8bit.astype(np.float32).std():.1f}")


def example_numpy_usage(data):
    """Demonstrate NumPy-only usage (no PyTorch required)."""
    print("\nNumPy Usage Example")
    print("=" * 60)

    params = data['params']
    state_8bit = data['state_8bit']

    # Find the state with maximum peak value
    peak_per_pair = state_8bit.max(axis=(1, 2))
    peak_idx = peak_per_pair.argmax()

    print(f"Pair with highest peak: #{peak_idx}")
    print(f"  Params: offset={params[peak_idx, 0]:.2f}, "
          f"persistence={params[peak_idx, 1]:.3f}, "
          f"diffusion={params[peak_idx, 2]:.2f}")
    print(f"  Peak brightness: {peak_per_pair[peak_idx]}")
    print()

    # Find pairs with uniform parameters
    offset_0p5 = (np.abs(params[:, 0] - 0.5) < 0.001)
    persistence_0p99 = (np.abs(params[:, 1] - 0.99) < 0.001)
    matching = offset_0p5 & persistence_0p99

    print(f"Pairs with offset=0.5 and persistence=0.99: {matching.sum()}")
    if matching.sum() > 0:
        idx = np.where(matching)[0][0]
        print(f"  First match: pair #{idx}")
        print(f"  Full params: {params[idx]}")
    print()

    # Compute energy per pair (sum of all pixel values)
    energy = state_8bit.astype(np.float32).sum(axis=(1, 2))
    print(f"Energy distribution:")
    print(f"  Min: {energy.min():.0f}")
    print(f"  Max: {energy.max():.0f}")
    print(f"  Mean: {energy.mean():.0f}")
    print(f"  Median: {np.median(energy):.0f}")


def example_pytorch_usage(data):
    """Demonstrate PyTorch usage for model training."""
    if not TORCH_AVAILABLE:
        print("\nPyTorch Example (requires: pip3 install torch)")
        print("=" * 60)
        print("PyTorch not available")
        return

    print("\nPyTorch Usage Example")
    print("=" * 60)

    # Convert to tensors
    params = torch.from_numpy(data['params']).float()  # [576, 6]
    state_8bit = torch.from_numpy(data['state_8bit']).float() / 255.0  # Normalize to [0,1]

    print(f"Params tensor: {params.shape} (6 parameters)")
    print(f"State tensor: {state_8bit.shape} (80 spatial bins × 3 color channels)")
    print()

    # Split train/validation
    n_total = len(params)
    n_train = int(0.8 * n_total)

    train_idx = torch.arange(0, n_train)
    val_idx = torch.arange(n_train, n_total)

    params_train = params[train_idx]
    state_train = state_8bit[train_idx]
    params_val = params[val_idx]
    state_val = state_8bit[val_idx]

    print(f"Train set: {len(train_idx)} pairs")
    print(f"Val set:   {len(val_idx)} pairs")
    print()

    # Define a simple model
    class TransportModel(nn.Module):
        def __init__(self):
            super().__init__()
            self.fc1 = nn.Linear(6, 128)
            self.fc2 = nn.Linear(128, 256)
            self.fc3 = nn.Linear(256, 80 * 3)

        def forward(self, x):
            x = torch.relu(self.fc1(x))
            x = torch.relu(self.fc2(x))
            x = torch.sigmoid(self.fc3(x))  # Output in [0,1]
            return x

    model = TransportModel()
    print(f"Model parameters: {sum(p.numel() for p in model.parameters())}")
    print()

    # Training loop (just 5 epochs for demonstration)
    loss_fn = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)

    print("Training (5 epochs):")
    for epoch in range(5):
        # Training
        pred_train = model(params_train)
        loss_train = loss_fn(pred_train, state_train)

        # Validation
        with torch.no_grad():
            pred_val = model(params_val)
            loss_val = loss_fn(pred_val, state_val)

        print(f"  Epoch {epoch+1:2d}: train_loss={loss_train.item():.6f}, "
              f"val_loss={loss_val.item():.6f}")

        optimizer.zero_grad()
        loss_train.backward()
        optimizer.step()

    print()
    print("Model is ready for further training or deployment!")
    print("Next steps:")
    print("  1. Train longer with learning rate scheduling")
    print("  2. Add regularization (dropout, L2)")
    print("  3. Use smaller output dimension (spatial bottleneck)")
    print("  4. Quantize for firmware deployment")


def main():
    print("\n" + "=" * 60)
    print("BeatPulseTransportCore Reference Data Examples")
    print("=" * 60)

    # Load data
    print("\nLoading reference data...")
    data = load_reference_data()

    if data is None:
        return

    # Analyze structure
    analyze_reference_data(data)

    # NumPy examples
    example_numpy_usage(data)

    # PyTorch examples
    example_pytorch_usage(data)

    print("\n" + "=" * 60)
    print("Examples completed!")
    print("=" * 60 + "\n")


if __name__ == '__main__':
    main()
