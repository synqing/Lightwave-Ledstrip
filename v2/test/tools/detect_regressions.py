#!/usr/bin/env python3
"""
Detect performance regressions by comparing current benchmark results
against baseline metrics with configurable thresholds.
"""

import json
import sys
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple
from enum import Enum


class RegressionSeverity(Enum):
    """Severity levels for regression detection"""
    PASS = "pass"
    WARNING = "warning"
    FAILURE = "failure"


@dataclass
class Threshold:
    """Threshold configuration for a metric"""
    warning_percent: float
    failure_percent: float
    higher_is_better: bool = True  # False if lower values are better


@dataclass
class RegressionResult:
    """Result of regression detection for a single metric"""
    metric_name: str
    baseline_value: float
    current_value: float
    change_percent: float
    severity: RegressionSeverity
    message: str


class RegressionDetector:
    """Detect performance regressions using statistical thresholds"""

    # Default thresholds for each metric
    DEFAULT_THRESHOLDS = {
        'snr_db': Threshold(
            warning_percent=-5.0,   # -5% warning
            failure_percent=-10.0,  # -10% failure
            higher_is_better=True
        ),
        'false_trigger_rate': Threshold(
            warning_percent=50.0,   # +50% warning
            failure_percent=100.0,  # +100% failure
            higher_is_better=False
        ),
        'cpu_load_percent': Threshold(
            warning_percent=20.0,   # +20% warning
            failure_percent=40.0,   # +40% failure
            higher_is_better=False
        ),
        'latency_ms': Threshold(
            warning_percent=15.0,   # +15% warning
            failure_percent=30.0,   # +30% failure
            higher_is_better=False
        ),
        'memory_usage_kb': Threshold(
            warning_percent=25.0,   # +25% warning
            failure_percent=50.0,   # +50% failure
            higher_is_better=False
        ),
    }

    def __init__(self, thresholds: Optional[Dict[str, Threshold]] = None):
        """Initialize with custom or default thresholds"""
        self.thresholds = thresholds or self.DEFAULT_THRESHOLDS

    def calculate_change_percent(self, baseline: float, current: float) -> float:
        """Calculate percentage change from baseline to current"""
        if baseline == 0:
            return float('inf') if current > 0 else 0.0
        return ((current - baseline) / baseline) * 100.0

    def check_metric(
        self,
        metric_name: str,
        baseline_value: float,
        current_value: float
    ) -> RegressionResult:
        """Check a single metric for regression"""

        threshold = self.thresholds.get(metric_name)
        if not threshold:
            # Unknown metric - return pass with info
            return RegressionResult(
                metric_name=metric_name,
                baseline_value=baseline_value,
                current_value=current_value,
                change_percent=self.calculate_change_percent(baseline_value, current_value),
                severity=RegressionSeverity.PASS,
                message=f"No threshold configured for {metric_name}"
            )

        change_percent = self.calculate_change_percent(baseline_value, current_value)

        # Determine severity based on direction and thresholds
        severity = RegressionSeverity.PASS
        message = "Performance maintained"

        if threshold.higher_is_better:
            # For metrics where higher is better (e.g., SNR)
            if change_percent <= threshold.failure_percent:
                severity = RegressionSeverity.FAILURE
                message = f"Performance regression: {change_percent:.1f}% decrease (threshold: {threshold.failure_percent}%)"
            elif change_percent <= threshold.warning_percent:
                severity = RegressionSeverity.WARNING
                message = f"Performance warning: {change_percent:.1f}% decrease (threshold: {threshold.warning_percent}%)"
            elif change_percent > 0:
                message = f"Performance improved: {change_percent:.1f}% increase"
        else:
            # For metrics where lower is better (e.g., latency, CPU load)
            if change_percent >= threshold.failure_percent:
                severity = RegressionSeverity.FAILURE
                message = f"Performance regression: {change_percent:.1f}% increase (threshold: {threshold.failure_percent}%)"
            elif change_percent >= threshold.warning_percent:
                severity = RegressionSeverity.WARNING
                message = f"Performance warning: {change_percent:.1f}% increase (threshold: {threshold.warning_percent}%)"
            elif change_percent < 0:
                message = f"Performance improved: {abs(change_percent):.1f}% decrease"

        return RegressionResult(
            metric_name=metric_name,
            baseline_value=baseline_value,
            current_value=current_value,
            change_percent=change_percent,
            severity=severity,
            message=message
        )

    def check_all_metrics(
        self,
        baseline: Dict[str, float],
        current: Dict[str, float]
    ) -> List[RegressionResult]:
        """Check all metrics in baseline against current results"""
        results = []

        # Check all metrics present in baseline
        for metric_name, baseline_value in baseline.items():
            if metric_name not in current:
                results.append(RegressionResult(
                    metric_name=metric_name,
                    baseline_value=baseline_value,
                    current_value=0.0,
                    change_percent=float('-inf'),
                    severity=RegressionSeverity.FAILURE,
                    message=f"Metric {metric_name} missing from current results"
                ))
                continue

            current_value = current[metric_name]
            result = self.check_metric(metric_name, baseline_value, current_value)
            results.append(result)

        # Check for new metrics not in baseline
        for metric_name in current:
            if metric_name not in baseline:
                results.append(RegressionResult(
                    metric_name=metric_name,
                    baseline_value=0.0,
                    current_value=current[metric_name],
                    change_percent=float('inf'),
                    severity=RegressionSeverity.WARNING,
                    message=f"New metric {metric_name} not in baseline"
                ))

        return results

    def get_overall_status(self, results: List[RegressionResult]) -> RegressionSeverity:
        """Get overall status from all regression results"""
        if any(r.severity == RegressionSeverity.FAILURE for r in results):
            return RegressionSeverity.FAILURE
        if any(r.severity == RegressionSeverity.WARNING for r in results):
            return RegressionSeverity.WARNING
        return RegressionSeverity.PASS


def load_json_file(filepath: str) -> Dict:
    """Load JSON from file, filtering out metadata fields"""
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)

        # Filter out metadata fields (those starting with _)
        filtered_data = {
            k: v for k, v in data.items()
            if not k.startswith('_') and isinstance(v, (int, float))
        }

        return filtered_data
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
        sys.exit(1)


def format_markdown_report(results: List[RegressionResult]) -> str:
    """Format regression results as markdown for PR comments"""
    detector = RegressionDetector()
    overall_status = detector.get_overall_status(results)

    # Status emoji
    status_emoji = {
        RegressionSeverity.PASS: ":white_check_mark:",
        RegressionSeverity.WARNING: ":warning:",
        RegressionSeverity.FAILURE: ":x:"
    }

    report = f"## Audio Benchmark Results {status_emoji[overall_status]}\n\n"

    if overall_status == RegressionSeverity.FAILURE:
        report += "**Status**: REGRESSION DETECTED - Performance degraded beyond acceptable thresholds\n\n"
    elif overall_status == RegressionSeverity.WARNING:
        report += "**Status**: WARNING - Performance approaching regression thresholds\n\n"
    else:
        report += "**Status**: PASS - All metrics within acceptable ranges\n\n"

    # Summary table
    report += "| Metric | Baseline | Current | Change | Status |\n"
    report += "|--------|----------|---------|--------|--------|\n"

    for result in results:
        emoji = status_emoji[result.severity]
        change_str = f"{result.change_percent:+.1f}%" if abs(result.change_percent) != float('inf') else "N/A"

        report += f"| {result.metric_name} | {result.baseline_value:.2f} | {result.current_value:.2f} | {change_str} | {emoji} |\n"

    # Details section
    report += "\n### Details\n\n"
    for result in results:
        if result.severity != RegressionSeverity.PASS or abs(result.change_percent) > 5:
            report += f"- **{result.metric_name}**: {result.message}\n"

    # Thresholds reference
    report += "\n### Configured Thresholds\n\n"
    report += "| Metric | Warning | Failure | Direction |\n"
    report += "|--------|---------|---------|----------|\n"

    for metric_name, threshold in detector.thresholds.items():
        direction = "↑ Higher is better" if threshold.higher_is_better else "↓ Lower is better"
        report += f"| {metric_name} | {threshold.warning_percent:+.0f}% | {threshold.failure_percent:+.0f}% | {direction} |\n"

    return report


def format_console_report(results: List[RegressionResult]) -> str:
    """Format regression results for console output"""
    detector = RegressionDetector()
    overall_status = detector.get_overall_status(results)

    lines = []
    lines.append("\n" + "="*70)
    lines.append("AUDIO BENCHMARK REGRESSION DETECTION")
    lines.append("="*70)

    status_str = overall_status.value.upper()
    lines.append(f"\nOverall Status: {status_str}\n")

    # Group by severity
    failures = [r for r in results if r.severity == RegressionSeverity.FAILURE]
    warnings = [r for r in results if r.severity == RegressionSeverity.WARNING]
    passes = [r for r in results if r.severity == RegressionSeverity.PASS]

    if failures:
        lines.append("\n[FAILURES]")
        for result in failures:
            lines.append(f"  ✗ {result.metric_name}: {result.message}")
            lines.append(f"    Baseline: {result.baseline_value:.2f}, Current: {result.current_value:.2f}")

    if warnings:
        lines.append("\n[WARNINGS]")
        for result in warnings:
            lines.append(f"  ⚠ {result.metric_name}: {result.message}")
            lines.append(f"    Baseline: {result.baseline_value:.2f}, Current: {result.current_value:.2f}")

    if passes:
        lines.append("\n[PASSING]")
        for result in passes:
            if abs(result.change_percent) < 5:
                status = "maintained"
            else:
                status = f"{result.change_percent:+.1f}%"
            lines.append(f"  ✓ {result.metric_name}: {status}")

    lines.append("\n" + "="*70 + "\n")

    return "\n".join(lines)


def main():
    """CLI interface for regression detection"""
    import argparse

    parser = argparse.ArgumentParser(
        description='Detect audio benchmark performance regressions'
    )
    parser.add_argument(
        'baseline',
        help='Path to baseline benchmark results (JSON)'
    )
    parser.add_argument(
        'current',
        help='Path to current benchmark results (JSON)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['console', 'markdown', 'json'],
        default='console',
        help='Output format (default: console)'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output file path (default: stdout)'
    )
    parser.add_argument(
        '--fail-on-warning',
        action='store_true',
        help='Exit with error code on warnings (default: only failures)'
    )

    args = parser.parse_args()

    # Load data
    baseline_data = load_json_file(args.baseline)
    current_data = load_json_file(args.current)

    # Run regression detection
    detector = RegressionDetector()
    results = detector.check_all_metrics(baseline_data, current_data)
    overall_status = detector.get_overall_status(results)

    # Format output
    if args.format == 'console':
        output = format_console_report(results)
    elif args.format == 'markdown':
        output = format_markdown_report(results)
    elif args.format == 'json':
        output_data = {
            'overall_status': overall_status.value,
            'results': [
                {
                    'metric': r.metric_name,
                    'baseline': r.baseline_value,
                    'current': r.current_value,
                    'change_percent': r.change_percent,
                    'severity': r.severity.value,
                    'message': r.message
                }
                for r in results
            ]
        }
        output = json.dumps(output_data, indent=2)

    # Write output
    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"Report written to {args.output}")
    else:
        print(output)

    # Exit code
    if overall_status == RegressionSeverity.FAILURE:
        sys.exit(1)
    elif overall_status == RegressionSeverity.WARNING and args.fail_on_warning:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == '__main__':
    main()
