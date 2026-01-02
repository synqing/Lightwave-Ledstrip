#!/usr/bin/env python3
"""Example: Compare two benchmark runs for A/B testing.

This demonstrates statistical comparison of optimization changes:
1. Load two benchmark runs
2. Perform t-tests and compute effect sizes
3. Display comparison report
"""

import sys
from pathlib import Path
from uuid import UUID

from lwos_benchmark.analysis.comparison import compare_runs
from lwos_benchmark.storage.database import BenchmarkDatabase


def main() -> None:
    """Compare two benchmark runs."""
    if len(sys.argv) != 4:
        print("Usage: compare_runs.py <db_path> <run_a_id> <run_b_id>")
        sys.exit(1)

    db_path = Path(sys.argv[1])
    run_a_id = UUID(sys.argv[2])
    run_b_id = UUID(sys.argv[3])

    # Load database
    database = BenchmarkDatabase(db_path)

    # Load runs
    run_a = database.get_run(run_a_id)
    run_b = database.get_run(run_b_id)

    if not run_a or not run_b:
        print("Error: One or both runs not found")
        sys.exit(1)

    # Perform comparison
    print(f"\nComparing: {run_a.name} vs {run_b.name}")
    print("=" * 60)

    result = compare_runs(database, run_a, run_b, alpha=0.05)

    # Display results
    print(f"\nSample Sizes:")
    print(f"  {run_a.name}: {result.n_samples_a}")
    print(f"  {run_b.name}: {result.n_samples_b}")

    print(f"\nAverage Total Time:")
    print(f"  Mean difference: {result.avg_total_us_diff:+.1f}µs")
    print(f"  p-value: {result.avg_total_us_pvalue:.4f}")
    print(f"  Significant: {'YES' if result.avg_total_us_significant else 'NO'}")
    print(f"  Cohen's d: {result.avg_total_us_cohens_d:.2f}")
    print(f"  Effect size: {result.avg_total_us_effect_size}")

    print(f"\nCPU Load:")
    print(f"  Mean difference: {result.cpu_load_diff:+.1f}%")
    print(f"  p-value: {result.cpu_load_pvalue:.4f}")
    print(f"  Significant: {'YES' if result.cpu_load_significant else 'NO'}")
    print(f"  Cohen's d: {result.cpu_load_cohens_d:.2f}")
    print(f"  Effect size: {result.cpu_load_effect_size}")

    print("\n" + "=" * 60)

    # Interpretation
    print("\nInterpretation:")
    if result.avg_total_us_significant:
        direction = "faster" if result.avg_total_us_diff < 0 else "slower"
        print(f"  ✓ {run_b.name} is significantly {direction} than {run_a.name}")
        print(f"    (p < {result.alpha})")
    else:
        print(f"  ✗ No significant difference in processing time")
        print(f"    (p >= {result.alpha})")

    if result.cpu_load_significant:
        direction = "lower" if result.cpu_load_diff < 0 else "higher"
        print(f"  ✓ {run_b.name} has significantly {direction} CPU load than {run_a.name}")
    else:
        print(f"  ✗ No significant difference in CPU load")

    database.close()


if __name__ == "__main__":
    main()
