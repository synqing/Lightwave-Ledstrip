#!/usr/bin/env python3
"""CLI script to run calibration: load reference pairs, run PyTorch, compare.

Usage:
    python -m testbed.scripts.run_calibration --reference data/reference_pairs.npz
    python -m testbed.scripts.run_calibration --reference data/reference_pairs.npz --subset 100
"""

import argparse
import sys
from pathlib import Path
from typing import Optional

try:
    import torch
    import torch.nn as nn
except ImportError:
    print("Error: PyTorch required for calibration")
    sys.exit(1)

from testbed.calibration import CalibrationBridge, CalibrationReport
from testbed.core import BeatPulseTransportCore


def create_transport_core(device: str = "cpu") -> nn.Module:
    """Create and initialize the PyTorch transport core.

    Args:
        device: torch device (default "cpu")

    Returns:
        Initialized TransportCore module
    """
    core = BeatPulseTransportCore(
        radial_len=80,
        n_params=4,  # intensity, wavelength, angle, diffusion
        hidden_dim=64,
    )
    core = core.to(device)
    core.eval()
    return core


def main():
    """Main entry point for calibration CLI."""
    parser = argparse.ArgumentParser(
        description="Calibrate PyTorch transport core against firmware reference"
    )
    parser.add_argument(
        "--reference",
        type=str,
        default="data/reference_pairs.npz",
        help="Path to reference pairs .npz file (default: data/reference_pairs.npz)",
    )
    parser.add_argument(
        "--subset",
        type=int,
        default=None,
        help="Test only subset of N random reference pairs (optional)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for subset selection (default: 42)",
    )
    parser.add_argument(
        "--device",
        type=str,
        default="cpu",
        help="torch device (default: cpu)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default=None,
        help="Save report to file (optional, JSON format)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print detailed progress",
    )

    args = parser.parse_args()

    # Check if reference file exists
    reference_path = Path(args.reference)
    if not reference_path.exists():
        print(f"Error: Reference file not found: {args.reference}")
        sys.exit(1)

    print(f"Loading calibration bridge from {args.reference}...")

    # Create transport core
    try:
        transport_core = create_transport_core(device=args.device)
        print(f"Created TransportCore on device {args.device}")
    except Exception as e:
        print(f"Error creating transport core: {e}")
        sys.exit(1)

    # Create calibration bridge
    try:
        bridge = CalibrationBridge(
            reference_path=str(reference_path),
            transport_core=transport_core,
            device=args.device,
        )
        bridge.load_reference()
    except Exception as e:
        print(f"Error loading reference: {e}")
        sys.exit(1)

    # Determine subset if requested
    subset_indices = None
    if args.subset is not None:
        import numpy as np
        rng = np.random.RandomState(args.seed)
        n_total = len(bridge.reference_params)
        subset_size = min(args.subset, n_total)
        subset_indices = rng.choice(n_total, size=subset_size, replace=False)
        print(f"Testing subset of {subset_size} pairs (seed={args.seed})")

    # Run calibration
    print("\nRunning calibration comparison...")
    try:
        report = bridge.run_comparison(
            n_frames=100,
            subset_indices=subset_indices,
            verbose=args.verbose,
        )
    except Exception as e:
        print(f"Error during calibration: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    # Print report
    print(report.summary_string())

    # Save to file if requested
    if args.output:
        import json
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        with open(output_path, "w") as f:
            json.dump(report.to_dict(), f, indent=2)
        print(f"\nReport saved to {output_path}")

    # Exit with appropriate code
    sys.exit(0 if report.passes_all() else 1)


if __name__ == "__main__":
    main()
