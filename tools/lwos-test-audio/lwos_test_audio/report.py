"""
Report generation for LightwaveOS Audio Effect Validation.

Generates rich terminal output, HTML reports, and JSON summaries from
analyzer results.
"""

import json
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

from .analyzers import LatencyResult, MotionResult, SmoothnessResult
from .models import ValidationSession


class ValidationReport:
    """
    Generates comprehensive validation reports in multiple formats.

    Supports:
    - Rich terminal output with colored tables
    - HTML reports with embedded charts (requires matplotlib)
    - JSON summaries for programmatic consumption
    """

    def __init__(self, session: ValidationSession) -> None:
        """
        Initialize report generator.

        Args:
            session: Validation session metadata
        """
        self.session = session
        self.console = Console()

    def print_terminal_report(
        self,
        motion_result: Optional[MotionResult] = None,
        smoothness_result: Optional[SmoothnessResult] = None,
        latency_result: Optional[LatencyResult] = None,
    ) -> None:
        """
        Print rich terminal report with colored tables and status indicators.

        Args:
            motion_result: Motion analysis results
            smoothness_result: Smoothness analysis results
            latency_result: Latency analysis results
        """
        self.console.print()
        self.console.print(
            Panel.fit(
                f"[bold cyan]LightwaveOS Audio Effect Validation Report[/bold cyan]\n"
                f"Effect: {self.session.effect_name}\n"
                f"Session: {self.session.session_id}\n"
                f"Duration: {self.session.actual_duration_seconds:.1f}s",
                border_style="cyan",
            )
        )
        self.console.print()

        # Session summary
        summary_table = Table(title="Session Summary", show_header=True, header_style="bold magenta")
        summary_table.add_column("Metric", style="cyan")
        summary_table.add_column("Value", style="white")

        summary_table.add_row("Device", self.session.device_url)
        summary_table.add_row("Total Samples", str(self.session.total_samples))
        summary_table.add_row(
            "Sample Rate",
            f"{self.session.average_sample_rate_hz:.2f} Hz" if self.session.average_sample_rate_hz else "N/A",
        )
        summary_table.add_row("Dropped Frames", str(self.session.dropped_frames))
        if self.session.firmware_version:
            summary_table.add_row("Firmware", self.session.firmware_version)
        if self.session.heap_free_kb:
            summary_table.add_row("Heap Free", f"{self.session.heap_free_kb} KB")

        self.console.print(summary_table)
        self.console.print()

        # Analysis results
        results_table = Table(
            title="Analysis Results", show_header=True, header_style="bold yellow"
        )
        results_table.add_column("Test", style="cyan")
        results_table.add_column("Status", justify="center")
        results_table.add_column("Details", style="white")

        overall_pass = True

        if motion_result:
            status = self._format_status(motion_result.passed)
            results_table.add_row("Motion (Jog-Dial)", status, motion_result.details)
            overall_pass = overall_pass and motion_result.passed

        if smoothness_result:
            status = self._format_status(smoothness_result.passed)
            results_table.add_row("Smoothness (Jerk)", status, smoothness_result.details)
            overall_pass = overall_pass and smoothness_result.passed

        if latency_result:
            status = self._format_status(latency_result.passed)
            results_table.add_row("Latency", status, latency_result.details)
            overall_pass = overall_pass and latency_result.passed

        self.console.print(results_table)
        self.console.print()

        # Detailed metrics
        if motion_result:
            self._print_motion_details(motion_result)

        if smoothness_result:
            self._print_smoothness_details(smoothness_result)

        if latency_result:
            self._print_latency_details(latency_result)

        # Overall status
        if overall_pass:
            self.console.print(
                Panel.fit(
                    "[bold green]VALIDATION PASSED[/bold green]\n"
                    "All tests passed acceptance criteria",
                    border_style="green",
                )
            )
        else:
            self.console.print(
                Panel.fit(
                    "[bold red]VALIDATION FAILED[/bold red]\n"
                    "One or more tests failed acceptance criteria",
                    border_style="red",
                )
            )

        self.console.print()

    def _format_status(self, passed: bool) -> str:
        """Format pass/fail status with color."""
        if passed:
            return "[bold green]PASS[/bold green]"
        return "[bold red]FAIL[/bold red]"

    def _print_motion_details(self, result: MotionResult) -> None:
        """Print detailed motion analysis metrics."""
        table = Table(title="Motion Analysis Details", show_header=True)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="white")

        table.add_row("Reversal Count", str(result.reversal_count))
        table.add_row("Reversal Rate", f"{result.reversal_rate:.3f} /sec")
        table.add_row("Duration", f"{result.duration_seconds:.1f}s")
        table.add_row("Total Samples", str(result.total_samples))

        if result.reversal_indices:
            table.add_row(
                "Reversal Samples",
                f"{len(result.reversal_indices)} events at indices: "
                + ", ".join(map(str, result.reversal_indices[:5]))
                + ("..." if len(result.reversal_indices) > 5 else ""),
            )

        self.console.print(table)
        self.console.print()

    def _print_smoothness_details(self, result: SmoothnessResult) -> None:
        """Print detailed smoothness analysis metrics."""
        table = Table(title="Smoothness Analysis Details", show_header=True)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="white")

        table.add_row("Average Absolute Jerk", f"{result.avg_abs_jerk:.6f}")
        table.add_row("Maximum Jerk", f"{result.max_jerk:.6f}")
        table.add_row("Jerk Threshold", f"{result.jerk_threshold:.6f}")
        table.add_row("Consistency Score", f"{result.consistency_score:.1f}%")
        table.add_row("Duration", f"{result.duration_seconds:.1f}s")
        table.add_row("Total Samples", str(result.total_samples))

        self.console.print(table)
        self.console.print()

    def _print_latency_details(self, result: LatencyResult) -> None:
        """Print detailed latency analysis metrics."""
        table = Table(title="Latency Analysis Details", show_header=True)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="white")

        table.add_row("Events Detected", str(result.events_detected))
        table.add_row("Events Responded", str(result.events_responded))
        table.add_row("Response Rate", f"{result.response_rate:.1f}%")

        if result.avg_latency_ms is not None:
            table.add_row("Average Latency", f"{result.avg_latency_ms:.1f} ms")
            table.add_row("Minimum Latency", f"{result.min_latency_ms:.1f} ms")
            table.add_row("Maximum Latency", f"{result.max_latency_ms:.1f} ms")
        else:
            table.add_row("Latency", "No measurements")

        table.add_row("Threshold", f"{result.latency_threshold_ms:.1f} ms")
        table.add_row("Duration", f"{result.duration_seconds:.1f}s")
        table.add_row("Total Samples", str(result.total_samples))

        self.console.print(table)
        self.console.print()

    def generate_json_summary(
        self,
        motion_result: Optional[MotionResult] = None,
        smoothness_result: Optional[SmoothnessResult] = None,
        latency_result: Optional[LatencyResult] = None,
        output_path: Optional[Path] = None,
    ) -> dict[str, Any]:
        """
        Generate JSON summary of validation results.

        Args:
            motion_result: Motion analysis results
            smoothness_result: Smoothness analysis results
            latency_result: Latency analysis results
            output_path: Optional path to write JSON file

        Returns:
            Dictionary containing all results (also written to file if path provided)
        """
        summary = {
            "session": {
                "session_id": self.session.session_id,
                "effect_name": self.session.effect_name,
                "device_url": self.session.device_url,
                "started_at": self.session.started_at.isoformat(),
                "ended_at": self.session.ended_at.isoformat() if self.session.ended_at else None,
                "total_samples": self.session.total_samples,
                "duration_seconds": self.session.actual_duration_seconds,
                "average_sample_rate_hz": self.session.average_sample_rate_hz,
                "dropped_frames": self.session.dropped_frames,
                "firmware_version": self.session.firmware_version,
                "heap_free_kb": self.session.heap_free_kb,
            },
            "results": {},
            "timestamp": datetime.now().isoformat(),
        }

        overall_pass = True

        if motion_result:
            summary["results"]["motion"] = {
                "passed": motion_result.passed,
                "reversal_count": motion_result.reversal_count,
                "reversal_rate": motion_result.reversal_rate,
                "duration_seconds": motion_result.duration_seconds,
                "total_samples": motion_result.total_samples,
                "reversal_indices": motion_result.reversal_indices,
                "details": motion_result.details,
            }
            overall_pass = overall_pass and motion_result.passed

        if smoothness_result:
            summary["results"]["smoothness"] = {
                "passed": smoothness_result.passed,
                "avg_abs_jerk": smoothness_result.avg_abs_jerk,
                "max_jerk": smoothness_result.max_jerk,
                "jerk_threshold": smoothness_result.jerk_threshold,
                "consistency_score": smoothness_result.consistency_score,
                "duration_seconds": smoothness_result.duration_seconds,
                "total_samples": smoothness_result.total_samples,
                "details": smoothness_result.details,
            }
            overall_pass = overall_pass and smoothness_result.passed

        if latency_result:
            summary["results"]["latency"] = {
                "passed": latency_result.passed,
                "avg_latency_ms": latency_result.avg_latency_ms,
                "max_latency_ms": latency_result.max_latency_ms,
                "min_latency_ms": latency_result.min_latency_ms,
                "latency_threshold_ms": latency_result.latency_threshold_ms,
                "events_detected": latency_result.events_detected,
                "events_responded": latency_result.events_responded,
                "response_rate": latency_result.response_rate,
                "duration_seconds": latency_result.duration_seconds,
                "total_samples": latency_result.total_samples,
                "details": latency_result.details,
            }
            overall_pass = overall_pass and latency_result.passed

        summary["overall_pass"] = overall_pass

        if output_path:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, "w") as f:
                json.dump(summary, f, indent=2)

        return summary

    def generate_html_report(
        self,
        output_path: Path,
        motion_result: Optional[MotionResult] = None,
        smoothness_result: Optional[SmoothnessResult] = None,
        latency_result: Optional[LatencyResult] = None,
        include_charts: bool = False,
    ) -> None:
        """
        Generate HTML report with optional matplotlib charts.

        Args:
            output_path: Path to write HTML file
            motion_result: Motion analysis results
            smoothness_result: Smoothness analysis results
            latency_result: Latency analysis results
            include_charts: Whether to generate embedded charts (requires matplotlib)
        """
        html_parts = [
            "<!DOCTYPE html>",
            "<html>",
            "<head>",
            "<meta charset='utf-8'>",
            "<title>LightwaveOS Validation Report</title>",
            "<style>",
            self._get_html_styles(),
            "</style>",
            "</head>",
            "<body>",
            self._generate_html_header(),
            self._generate_html_session_info(),
        ]

        if motion_result:
            html_parts.append(self._generate_html_motion_section(motion_result))

        if smoothness_result:
            html_parts.append(self._generate_html_smoothness_section(smoothness_result))

        if latency_result:
            html_parts.append(self._generate_html_latency_section(latency_result))

        html_parts.extend([
            self._generate_html_footer(motion_result, smoothness_result, latency_result),
            "</body>",
            "</html>",
        ])

        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w") as f:
            f.write("\n".join(html_parts))

    def _get_html_styles(self) -> str:
        """Get CSS styles for HTML report."""
        return """
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }
        h2 { color: #34495e; margin-top: 30px; border-bottom: 2px solid #95a5a6; padding-bottom: 5px; }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th { background: #3498db; color: white; padding: 12px; text-align: left; }
        td { padding: 10px; border-bottom: 1px solid #ecf0f1; }
        tr:hover { background: #f8f9fa; }
        .pass { color: #27ae60; font-weight: bold; }
        .fail { color: #e74c3c; font-weight: bold; }
        .metric-value { font-family: 'Courier New', monospace; color: #2980b9; }
        .footer { margin-top: 40px; padding-top: 20px; border-top: 1px solid #bdc3c7; color: #7f8c8d; font-size: 0.9em; }
        """

    def _generate_html_header(self) -> str:
        """Generate HTML header section."""
        return f"""
        <div class='container'>
        <h1>LightwaveOS Audio Effect Validation Report</h1>
        <p><strong>Effect:</strong> {self.session.effect_name}</p>
        <p><strong>Session ID:</strong> {self.session.session_id}</p>
        <p><strong>Generated:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        """

    def _generate_html_session_info(self) -> str:
        """Generate session information table."""
        return f"""
        <h2>Session Information</h2>
        <table>
        <tr><th>Metric</th><th>Value</th></tr>
        <tr><td>Device URL</td><td>{self.session.device_url}</td></tr>
        <tr><td>Duration</td><td class='metric-value'>{self.session.actual_duration_seconds:.1f}s</td></tr>
        <tr><td>Total Samples</td><td class='metric-value'>{self.session.total_samples}</td></tr>
        <tr><td>Sample Rate</td><td class='metric-value'>{self.session.average_sample_rate_hz:.2f} Hz</td></tr>
        <tr><td>Dropped Frames</td><td class='metric-value'>{self.session.dropped_frames}</td></tr>
        {"<tr><td>Firmware</td><td>" + self.session.firmware_version + "</td></tr>" if self.session.firmware_version else ""}
        </table>
        """

    def _generate_html_motion_section(self, result: MotionResult) -> str:
        """Generate motion analysis HTML section."""
        status_class = "pass" if result.passed else "fail"
        status_text = "PASS" if result.passed else "FAIL"

        return f"""
        <h2>Motion Analysis (Jog-Dial Detection)</h2>
        <p><strong>Status:</strong> <span class='{status_class}'>{status_text}</span></p>
        <p>{result.details}</p>
        <table>
        <tr><th>Metric</th><th>Value</th></tr>
        <tr><td>Reversal Count</td><td class='metric-value'>{result.reversal_count}</td></tr>
        <tr><td>Reversal Rate</td><td class='metric-value'>{result.reversal_rate:.3f} /sec</td></tr>
        <tr><td>Duration</td><td class='metric-value'>{result.duration_seconds:.1f}s</td></tr>
        </table>
        """

    def _generate_html_smoothness_section(self, result: SmoothnessResult) -> str:
        """Generate smoothness analysis HTML section."""
        status_class = "pass" if result.passed else "fail"
        status_text = "PASS" if result.passed else "FAIL"

        return f"""
        <h2>Smoothness Analysis (Jerk Measurement)</h2>
        <p><strong>Status:</strong> <span class='{status_class}'>{status_text}</span></p>
        <p>{result.details}</p>
        <table>
        <tr><th>Metric</th><th>Value</th></tr>
        <tr><td>Average Absolute Jerk</td><td class='metric-value'>{result.avg_abs_jerk:.6f}</td></tr>
        <tr><td>Maximum Jerk</td><td class='metric-value'>{result.max_jerk:.6f}</td></tr>
        <tr><td>Consistency Score</td><td class='metric-value'>{result.consistency_score:.1f}%</td></tr>
        <tr><td>Jerk Threshold</td><td class='metric-value'>{result.jerk_threshold:.6f}</td></tr>
        </table>
        """

    def _generate_html_latency_section(self, result: LatencyResult) -> str:
        """Generate latency analysis HTML section."""
        status_class = "pass" if result.passed else "fail"
        status_text = "PASS" if result.passed else "FAIL"

        avg_latency_str = (
            f"{result.avg_latency_ms:.1f} ms"
            if result.avg_latency_ms is not None
            else "N/A"
        )
        max_latency_str = (
            f"{result.max_latency_ms:.1f} ms"
            if result.max_latency_ms is not None
            else "N/A"
        )

        return f"""
        <h2>Latency Analysis</h2>
        <p><strong>Status:</strong> <span class='{status_class}'>{status_text}</span></p>
        <p>{result.details}</p>
        <table>
        <tr><th>Metric</th><th>Value</th></tr>
        <tr><td>Events Detected</td><td class='metric-value'>{result.events_detected}</td></tr>
        <tr><td>Events Responded</td><td class='metric-value'>{result.events_responded}</td></tr>
        <tr><td>Response Rate</td><td class='metric-value'>{result.response_rate:.1f}%</td></tr>
        <tr><td>Average Latency</td><td class='metric-value'>{avg_latency_str}</td></tr>
        <tr><td>Maximum Latency</td><td class='metric-value'>{max_latency_str}</td></tr>
        <tr><td>Threshold</td><td class='metric-value'>{result.latency_threshold_ms:.1f} ms</td></tr>
        </table>
        """

    def _generate_html_footer(
        self,
        motion_result: Optional[MotionResult],
        smoothness_result: Optional[SmoothnessResult],
        latency_result: Optional[LatencyResult],
    ) -> str:
        """Generate HTML footer with overall status."""
        overall_pass = True
        if motion_result:
            overall_pass = overall_pass and motion_result.passed
        if smoothness_result:
            overall_pass = overall_pass and smoothness_result.passed
        if latency_result:
            overall_pass = overall_pass and latency_result.passed

        status_class = "pass" if overall_pass else "fail"
        status_text = "VALIDATION PASSED" if overall_pass else "VALIDATION FAILED"

        return f"""
        <div class='footer'>
        <h2>Overall Result</h2>
        <p><strong><span class='{status_class}'>{status_text}</span></strong></p>
        <p>Report generated by LightwaveOS Audio Effect Validation Framework</p>
        </div>
        </div>
        """
