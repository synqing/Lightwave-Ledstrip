#!/usr/bin/env python3
"""
DitherBench Main Orchestrator

Runs the complete benchmark suite comparing dithering algorithms.

Usage:
    python run_bench.py --output reports/20260111_120000 --frames 512 --seed 123
"""

import argparse
import sys
from pathlib import Path
from datetime import datetime

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent / "src"))

from dither_bench.utils.config import BenchConfig


def main():
    parser = argparse.ArgumentParser(
        description="Run DitherBench quantitative dithering assessment"
    )
    
    # Required arguments
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output directory for results (e.g., reports/20260111_120000)"
    )
    
    # Test configuration
    parser.add_argument(
        "--frames",
        type=int,
        default=512,
        help="Number of frames to simulate (default: 512)"
    )
    
    parser.add_argument(
        "--seed",
        type=int,
        default=123,
        help="Random seed for reproducibility (default: 123)"
    )
    
    parser.add_argument(
        "--leds",
        type=int,
        default=160,
        help="Number of LEDs per strip (default: 160)"
    )
    
    # Feature flags
    parser.add_argument(
        "--no-save-frames",
        action="store_true",
        help="Disable saving frame data (reduces disk usage)"
    )
    
    parser.add_argument(
        "--no-plots",
        action="store_true",
        help="Disable plot generation"
    )
    
    # Pipeline selection
    parser.add_argument(
        "--pipelines",
        type=str,
        default="all",
        help="Comma-separated list of pipelines (lwos,sb,emo) or 'all'"
    )
    
    args = parser.parse_args()
    
    # Create configuration
    config = BenchConfig(
        num_leds=args.leds,
        num_frames=args.frames,
        seed=args.seed,
        save_frames=not args.no_save_frames,
        save_plots=not args.no_plots,
        output_dir=args.output
    )
    
    # Parse pipeline selection
    if args.pipelines != "all":
        pipelines = [p.strip() for p in args.pipelines.split(",")]
        config.enable_lwos_bayer = "lwos" in pipelines
        config.enable_lwos_temporal = "lwos" in pipelines
        config.enable_sb_quantiser = "sb" in pipelines
        config.enable_emo_quantiser = "emo" in pipelines
    
    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Save configuration
    config.save(output_dir / "run_config.json")
    
    print("=" * 80)
    print("DitherBench: Quantitative Dithering Assessment")
    print("=" * 80)
    print(f"Output directory: {output_dir}")
    print(f"Configuration:")
    print(f"  - LEDs: {config.num_leds}")
    print(f"  - Frames: {config.num_frames}")
    print(f"  - Seed: {config.seed}")
    print(f"  - Save frames: {config.save_frames}")
    print(f"  - Generate plots: {config.save_plots}")
    print(f"\nPipelines:")
    print(f"  - LWOS (Bayer + Temporal): {config.enable_lwos_bayer}")
    print(f"  - SensoryBridge: {config.enable_sb_quantiser}")
    print(f"  - Emotiscope: {config.enable_emo_quantiser}")
    print(f"\nGit metadata:")
    print(f"  - SHA: {config.git.sha}")
    print(f"  - Branch: {config.git.branch}")
    print(f"  - Dirty: {config.git.dirty}")
    print("=" * 80)
    print()
    
    # TODO: Implement actual benchmark execution
    # This will be filled in as we implement the quantisers and pipelines
    print("⚠️  Benchmark execution not yet implemented.")
    print("    Configuration saved. Implementation continues in next todos.")
    print()
    print(f"Configuration saved to: {output_dir / 'run_config.json'}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
