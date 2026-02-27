"""Statistical analysis for benchmark data."""

import logging
from typing import Any

import numpy as np
from numpy.typing import NDArray

from ..storage.database import BenchmarkDatabase
from ..storage.models import BenchmarkRun, BenchmarkSample

logger = logging.getLogger(__name__)


def compute_percentiles(
    data: NDArray[np.float64],
    percentiles: list[float] | None = None,
) -> dict[str, float]:
    """Compute percentiles for a data array.

    Args:
        data: NumPy array of values
        percentiles: List of percentiles to compute (default: [50, 95, 99])

    Returns:
        Dictionary mapping percentile to value
    """
    if percentiles is None:
        percentiles = [50.0, 95.0, 99.0]

    if len(data) == 0:
        return {f"p{int(p)}": 0.0 for p in percentiles}

    values = np.percentile(data, percentiles)
    return {f"p{int(p)}": float(v) for p, v in zip(percentiles, values, strict=False)}


def compute_ema(
    data: NDArray[np.float64],
    alpha: float = 0.2,
) -> NDArray[np.float64]:
    """Compute exponential moving average.

    Args:
        data: Input data array
        alpha: Smoothing factor (0 < alpha <= 1)

    Returns:
        EMA array of same length as input
    """
    if len(data) == 0:
        return np.array([])

    ema = np.zeros_like(data)
    ema[0] = data[0]

    for i in range(1, len(data)):
        ema[i] = alpha * data[i] + (1 - alpha) * ema[i - 1]

    return ema


def compute_run_statistics(
    database: BenchmarkDatabase,
    run: BenchmarkRun,
) -> BenchmarkRun:
    """Compute aggregated statistics for a benchmark run.

    Updates the run with computed statistics:
    - Mean values
    - Percentiles (p50, p95, p99)
    - Configuration from first sample

    Args:
        database: Database instance
        run: BenchmarkRun to compute statistics for

    Returns:
        Updated BenchmarkRun with statistics
    """
    logger.info(f"Computing statistics for run '{run.name}'")

    # Fetch all samples
    samples = database.get_samples(run.id)

    if not samples:
        logger.warning("No samples found for run")
        return run

    # Extract metrics into arrays
    avg_total_us = np.array([s.avg_total_us for s in samples])
    cpu_load = np.array([s.cpu_load_percent for s in samples])

    # Compute mean values
    run.avg_total_us_mean = float(np.mean(avg_total_us))
    run.cpu_load_mean = float(np.mean(cpu_load))

    # Compute percentiles
    total_percentiles = compute_percentiles(avg_total_us, [50.0, 95.0, 99.0])
    run.avg_total_us_p50 = total_percentiles["p50"]
    run.avg_total_us_p95 = total_percentiles["p95"]
    run.avg_total_us_p99 = total_percentiles["p99"]

    cpu_percentiles = compute_percentiles(cpu_load, [95.0])
    run.cpu_load_p95 = cpu_percentiles["p95"]

    # Extract configuration from first sample
    first_sample = samples[0]
    run.hop_count = first_sample.hop_count
    run.goertzel_count = first_sample.goertzel_count
    run.device_uptime_ms = first_sample.timestamp_ms

    # Update sample count
    run.sample_count = len(samples)

    logger.info(
        f"Statistics: mean={run.avg_total_us_mean:.1f}µs, "
        f"p95={run.avg_total_us_p95:.1f}µs, "
        f"cpu_mean={run.cpu_load_mean:.1f}%"
    )

    return run


def extract_time_series(
    samples: list[BenchmarkSample],
    metric: str,
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Extract time series data for a metric.

    Args:
        samples: List of BenchmarkSample instances
        metric: Metric name (e.g., "avg_total_us", "cpu_load_percent")

    Returns:
        Tuple of (timestamps, values) as NumPy arrays
    """
    timestamps = np.array([s.timestamp_ms for s in samples], dtype=np.float64)
    values = np.array([getattr(s, metric) for s in samples], dtype=np.float64)

    return timestamps, values


def compute_descriptive_stats(data: NDArray[np.float64]) -> dict[str, float]:
    """Compute comprehensive descriptive statistics.

    Args:
        data: NumPy array of values

    Returns:
        Dictionary with mean, std, min, max, percentiles
    """
    if len(data) == 0:
        return {
            "mean": 0.0,
            "std": 0.0,
            "min": 0.0,
            "max": 0.0,
            "p50": 0.0,
            "p95": 0.0,
            "p99": 0.0,
        }

    percentiles = compute_percentiles(data, [50.0, 95.0, 99.0])

    return {
        "mean": float(np.mean(data)),
        "std": float(np.std(data)),
        "min": float(np.min(data)),
        "max": float(np.max(data)),
        **percentiles,
    }


def detect_outliers(
    data: NDArray[np.float64],
    threshold: float = 3.0,
) -> tuple[NDArray[np.int_], NDArray[np.float64]]:
    """Detect outliers using modified Z-score method.

    Args:
        data: NumPy array of values
        threshold: Modified Z-score threshold (default 3.0)

    Returns:
        Tuple of (outlier_indices, outlier_values)
    """
    if len(data) == 0:
        return np.array([]), np.array([])

    median = np.median(data)
    mad = np.median(np.abs(data - median))

    if mad == 0:
        return np.array([]), np.array([])

    modified_z_scores = 0.6745 * (data - median) / mad
    outlier_mask = np.abs(modified_z_scores) > threshold

    outlier_indices = np.where(outlier_mask)[0]
    outlier_values = data[outlier_mask]

    return outlier_indices, outlier_values


def compute_moving_statistics(
    data: NDArray[np.float64],
    window_size: int,
) -> dict[str, NDArray[np.float64]]:
    """Compute moving window statistics.

    Args:
        data: Input data array
        window_size: Size of moving window

    Returns:
        Dictionary with moving mean, std, min, max
    """
    if len(data) < window_size:
        return {
            "mean": np.array([]),
            "std": np.array([]),
            "min": np.array([]),
            "max": np.array([]),
        }

    n = len(data) - window_size + 1
    moving_mean = np.zeros(n)
    moving_std = np.zeros(n)
    moving_min = np.zeros(n)
    moving_max = np.zeros(n)

    for i in range(n):
        window = data[i : i + window_size]
        moving_mean[i] = np.mean(window)
        moving_std[i] = np.std(window)
        moving_min[i] = np.min(window)
        moving_max[i] = np.max(window)

    return {
        "mean": moving_mean,
        "std": moving_std,
        "min": moving_min,
        "max": moving_max,
    }
