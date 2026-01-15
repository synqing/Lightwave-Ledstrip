#!/usr/bin/env python3
"""Quickstart example for lwos-benchmark.

This example demonstrates basic usage:
1. Connect to ESP32
2. Collect benchmark data
3. Compute statistics
4. Save to database
"""

import asyncio
import logging
from pathlib import Path

from lwos_benchmark.analysis.statistics import compute_run_statistics
from lwos_benchmark.collectors.websocket import WebSocketCollector
from lwos_benchmark.storage.database import BenchmarkDatabase
from lwos_benchmark.storage.models import BenchmarkRun

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)


async def main() -> None:
    """Run benchmark collection example."""
    # Configuration
    ESP32_HOST = "lightwaveos.local"
    DB_PATH = Path("benchmark.db")
    COLLECTION_DURATION = 30.0  # seconds

    # Initialize database
    logger.info(f"Initializing database: {DB_PATH}")
    database = BenchmarkDatabase(DB_PATH)

    # Create benchmark run
    run = BenchmarkRun(
        name="quickstart-example",
        description="Example benchmark collection",
        host=ESP32_HOST,
        tags=["example", "quickstart"],
    )

    run_id = database.create_run(run)
    logger.info(f"Created run: {run.name} (ID: {run_id})")

    # Initialize collector
    collector = WebSocketCollector(
        host=ESP32_HOST,
        database=database,
    )

    try:
        # Connect to ESP32
        logger.info(f"Connecting to {ESP32_HOST}...")
        await collector.connect()

        # Collect data
        logger.info(f"Collecting data for {COLLECTION_DURATION}s...")
        sample_count = await collector.collect(
            run=run,
            duration=COLLECTION_DURATION,
        )

        logger.info(f"Collected {sample_count} samples")

        # Compute statistics
        logger.info("Computing statistics...")
        compute_run_statistics(database, run)
        run.mark_complete()
        database.update_run(run)

        # Display results
        print("\n" + "=" * 60)
        print(f"Benchmark Results: {run.name}")
        print("=" * 60)
        print(f"Samples collected: {run.sample_count}")
        print(f"Duration: {run.duration_seconds:.1f}s")
        print(f"\nProcessing Time:")
        print(f"  Mean: {run.avg_total_us_mean:.1f}µs")
        print(f"  P50:  {run.avg_total_us_p50:.1f}µs")
        print(f"  P95:  {run.avg_total_us_p95:.1f}µs")
        print(f"  P99:  {run.avg_total_us_p99:.1f}µs")
        print(f"\nCPU Load:")
        print(f"  Mean: {run.cpu_load_mean:.1f}%")
        print(f"  P95:  {run.cpu_load_p95:.1f}%")
        print(f"\nConfiguration:")
        print(f"  FFT Hop Count: {run.hop_count}")
        print(f"  Goertzel Filters: {run.goertzel_count}")
        print("=" * 60)

    except Exception as e:
        logger.error(f"Collection failed: {e}")
        raise

    finally:
        # Cleanup
        await collector.disconnect()
        database.close()
        logger.info("Cleanup complete")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
