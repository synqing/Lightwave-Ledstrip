#!/usr/bin/env python3
"""CLI script to generate training data using the PyTorch transport core.

Usage:
    python -m testbed.scripts.generate_training_data --n-pairs 10000
    python -m testbed.scripts.generate_training_data --n-pairs 5000 --with-jacobians
    python -m testbed.scripts.generate_training_data --n-pairs 1000 --method sobol --output custom_data.npz
"""

import argparse
import sys
from pathlib import Path

try:
    import torch
except ImportError:
    print("Error: PyTorch required for data generation")
    sys.exit(1)

from testbed.core import BeatPulseTransportCore
from testbed.training import TrainingDataGenerator


def main():
    """Main entry point for training data generation."""
    parser = argparse.ArgumentParser(
        description="Generate training data pairs for DIFNO using PyTorch transport core"
    )
    parser.add_argument(
        "--n-pairs",
        type=int,
        default=10000,
        help="Number of training pairs to generate (default: 10000)",
    )
    parser.add_argument(
        "--n-frames",
        type=int,
        default=100,
        help="Number of frames simulated per pair (default: 100)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="data/training_pairs.npz",
        help="Output path for .npz file (default: data/training_pairs.npz)",
    )
    parser.add_argument(
        "--method",
        type=str,
        default="sobol",
        choices=["sobol", "random", "grid"],
        help="Parameter sampling method (default: sobol)",
    )
    parser.add_argument(
        "--with-jacobians",
        action="store_true",
        help="Compute and save Jacobians via autograd (slower, larger file)",
    )
    parser.add_argument(
        "--device",
        type=str,
        default="cpu",
        help="torch device (default: cpu)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed (default: 42)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print detailed progress",
    )

    args = parser.parse_args()

    # Set random seed
    import numpy as np
    np.random.seed(args.seed)
    torch.manual_seed(args.seed)

    print(f"Generating {args.n_pairs} training pairs ({args.method} sampling)...")
    print(f"Device: {args.device}")
    print(f"With jacobians: {args.with_jacobians}")

    # Create transport core
    try:
        transport_core = BeatPulseTransportCore(
            radial_len=80,
            n_params=4,  # intensity, wavelength, angle, diffusion
            hidden_dim=64,
        )
        transport_core = transport_core.to(args.device)
        transport_core.eval()
        if args.verbose:
            print(f"Created TransportCore on device {args.device}")
    except Exception as e:
        print(f"Error creating transport core: {e}")
        sys.exit(1)

    # Create data generator
    try:
        generator = TrainingDataGenerator(
            transport_core=transport_core,
            radial_len=80,
            device=args.device,
        )
    except Exception as e:
        print(f"Error creating data generator: {e}")
        sys.exit(1)

    # Generate pairs
    try:
        if args.with_jacobians:
            print("Generating pairs with Jacobians (this may take a while)...")
            data = generator.generate_with_jacobians(
                n_pairs=args.n_pairs,
                n_frames=args.n_frames,
                method=args.method,
            )
        else:
            data = generator.generate_pairs(
                n_pairs=args.n_pairs,
                n_frames=args.n_frames,
                method=args.method,
            )
    except Exception as e:
        print(f"Error during generation: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    # Save to disk
    try:
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        generator.save_pairs(data, output_path)

        # Print summary
        print(f"\nGeneration complete!")
        print(f"  Params shape:  {data['params'].shape}")
        print(f"  Outputs shape: {data['outputs'].shape}")
        if "jacobians" in data:
            print(f"  Jacobians shape: {data['jacobians'].shape}")
        print(f"  Saved to: {output_path}")

    except Exception as e:
        print(f"Error saving data: {e}")
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()
