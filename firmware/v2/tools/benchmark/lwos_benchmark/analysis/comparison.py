"""Statistical comparison of benchmark runs for A/B testing."""

import logging
from typing import Any

import numpy as np
from numpy.typing import NDArray
from scipy import stats

from ..storage.database import BenchmarkDatabase
from ..storage.models import BenchmarkRun, ComparisonResult

logger = logging.getLogger(__name__)


def cohens_d(
    group_a: NDArray[np.float64],
    group_b: NDArray[np.float64],
) -> float:
    """Compute Cohen's d effect size.

    Cohen's d is a standardized measure of the difference between two means,
    expressed in standard deviation units.

    Interpretation:
    - |d| < 0.2: negligible
    - 0.2 <= |d| < 0.5: small
    - 0.5 <= |d| < 0.8: medium
    - |d| >= 0.8: large

    Args:
        group_a: First group data
        group_b: Second group data

    Returns:
        Cohen's d effect size
    """
    n_a = len(group_a)
    n_b = len(group_b)

    if n_a == 0 or n_b == 0:
        return 0.0

    mean_a = np.mean(group_a)
    mean_b = np.mean(group_b)

    var_a = np.var(group_a, ddof=1) if n_a > 1 else 0.0
    var_b = np.var(group_b, ddof=1) if n_b > 1 else 0.0

    # Pooled standard deviation
    pooled_std = np.sqrt(((n_a - 1) * var_a + (n_b - 1) * var_b) / (n_a + n_b - 2))

    if pooled_std == 0:
        return 0.0

    return float((mean_b - mean_a) / pooled_std)


def independent_t_test(
    group_a: NDArray[np.float64],
    group_b: NDArray[np.float64],
    alpha: float = 0.05,
) -> tuple[float, float, bool]:
    """Perform independent samples t-test.

    Args:
        group_a: First group data
        group_b: Second group data
        alpha: Significance level (default 0.05)

    Returns:
        Tuple of (t_statistic, p_value, is_significant)
    """
    if len(group_a) == 0 or len(group_b) == 0:
        return 0.0, 1.0, False

    t_stat, p_value = stats.ttest_ind(group_a, group_b)

    is_significant = p_value < alpha

    return float(t_stat), float(p_value), is_significant


def compare_runs(
    database: BenchmarkDatabase,
    run_a: BenchmarkRun,
    run_b: BenchmarkRun,
    alpha: float = 0.05,
) -> ComparisonResult:
    """Compare two benchmark runs with statistical tests.

    Performs independent t-tests and computes Cohen's d effect sizes
    for key metrics:
    - avg_total_us
    - cpu_load_percent

    Args:
        database: Database instance
        run_a: First benchmark run (baseline)
        run_b: Second benchmark run (comparison)
        alpha: Significance level for hypothesis tests

    Returns:
        ComparisonResult with statistical analysis
    """
    logger.info(f"Comparing runs: '{run_a.name}' vs '{run_b.name}'")

    # Fetch samples
    samples_a = database.get_samples(run_a.id)
    samples_b = database.get_samples(run_b.id)

    if not samples_a or not samples_b:
        msg = "One or both runs have no samples"
        raise ValueError(msg)

    # Extract metrics
    avg_total_a = np.array([s.avg_total_us for s in samples_a])
    avg_total_b = np.array([s.avg_total_us for s in samples_b])

    cpu_load_a = np.array([s.cpu_load_percent for s in samples_a])
    cpu_load_b = np.array([s.cpu_load_percent for s in samples_b])

    # T-tests
    _, total_pvalue, total_sig = independent_t_test(avg_total_a, avg_total_b, alpha)
    _, cpu_pvalue, cpu_sig = independent_t_test(cpu_load_a, cpu_load_b, alpha)

    # Effect sizes
    total_cohens = cohens_d(avg_total_a, avg_total_b)
    cpu_cohens = cohens_d(cpu_load_a, cpu_load_b)

    # Mean differences
    total_diff = float(np.mean(avg_total_b) - np.mean(avg_total_a))
    cpu_diff = float(np.mean(cpu_load_b) - np.mean(cpu_load_a))

    result = ComparisonResult(
        run_a_id=run_a.id,
        run_a_name=run_a.name,
        run_b_id=run_b.id,
        run_b_name=run_b.name,
        n_samples_a=len(samples_a),
        n_samples_b=len(samples_b),
        avg_total_us_diff=total_diff,
        cpu_load_diff=cpu_diff,
        avg_total_us_pvalue=total_pvalue,
        cpu_load_pvalue=cpu_pvalue,
        avg_total_us_cohens_d=total_cohens,
        cpu_load_cohens_d=cpu_cohens,
        alpha=alpha,
    )

    logger.info(
        f"avg_total_us: diff={total_diff:.1f}µs, "
        f"p={total_pvalue:.4f}, d={total_cohens:.2f} ({result.avg_total_us_effect_size})"
    )
    logger.info(
        f"cpu_load: diff={cpu_diff:.1f}%, "
        f"p={cpu_pvalue:.4f}, d={cpu_cohens:.2f} ({result.cpu_load_effect_size})"
    )

    return result


def compute_confidence_interval(
    data: NDArray[np.float64],
    confidence: float = 0.95,
) -> tuple[float, float]:
    """Compute confidence interval for the mean.

    Args:
        data: Sample data
        confidence: Confidence level (default 0.95)

    Returns:
        Tuple of (lower_bound, upper_bound)
    """
    if len(data) == 0:
        return 0.0, 0.0

    mean = np.mean(data)
    sem = stats.sem(data)

    margin = sem * stats.t.ppf((1 + confidence) / 2, len(data) - 1)

    return float(mean - margin), float(mean + margin)


def welch_t_test(
    group_a: NDArray[np.float64],
    group_b: NDArray[np.float64],
    alpha: float = 0.05,
) -> tuple[float, float, bool]:
    """Perform Welch's t-test (unequal variances).

    More robust than standard t-test when variances differ.

    Args:
        group_a: First group data
        group_b: Second group data
        alpha: Significance level

    Returns:
        Tuple of (t_statistic, p_value, is_significant)
    """
    if len(group_a) == 0 or len(group_b) == 0:
        return 0.0, 1.0, False

    t_stat, p_value = stats.ttest_ind(group_a, group_b, equal_var=False)

    is_significant = p_value < alpha

    return float(t_stat), float(p_value), is_significant


def mann_whitney_u_test(
    group_a: NDArray[np.float64],
    group_b: NDArray[np.float64],
    alpha: float = 0.05,
) -> tuple[float, float, bool]:
    """Perform Mann-Whitney U test (non-parametric alternative to t-test).

    Useful when data is not normally distributed.

    Args:
        group_a: First group data
        group_b: Second group data
        alpha: Significance level

    Returns:
        Tuple of (u_statistic, p_value, is_significant)
    """
    if len(group_a) == 0 or len(group_b) == 0:
        return 0.0, 1.0, False

    u_stat, p_value = stats.mannwhitneyu(group_a, group_b, alternative="two-sided")

    is_significant = p_value < alpha

    return float(u_stat), float(p_value), is_significant
