"""
Command-line interface for LightwaveOS audio validation testing.

Provides commands for recording, analyzing, and reporting on validation data.
"""

import asyncio
import json
import logging
from pathlib import Path
from typing import Optional

import click
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.live import Live
from rich.layout import Layout

from lwos_test_audio import __version__
from lwos_test_audio.collector import ValidationCollector
from lwos_test_audio.analyzer import ValidationAnalyzer

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)


@click.group()
@click.version_option(version=__version__)
@click.pass_context
def main(ctx: click.Context) -> None:
    """
    LightwaveOS Audio Effect Validation Framework

    Tools for capturing, analyzing, and validating audio-reactive LED effects.
    """
    ctx.ensure_object(dict)


@main.command()
@click.option(
    "--host",
    default="lightwaveos.local",
    help="Device hostname or IP address",
    show_default=True,
)
@click.option(
    "--db",
    type=click.Path(path_type=Path),
    default=Path("validation.db"),
    help="SQLite database path",
    show_default=True,
)
@click.option(
    "--duration",
    type=int,
    default=None,
    help="Recording duration in seconds (default: unlimited)",
)
@click.option(
    "--max-samples",
    type=int,
    default=None,
    help="Maximum number of samples to collect (default: unlimited)",
)
@click.option(
    "--effect",
    type=str,
    default=None,
    help="Name of the effect being validated",
)
def record(
    host: str,
    db: Path,
    duration: Optional[int],
    max_samples: Optional[int],
    effect: Optional[str],
) -> None:
    """
    Record validation frames from LightwaveOS device.

    Connects to the device's WebSocket endpoint, subscribes to binary validation
    frames, and stores samples to SQLite database for later analysis.

    Example:
        lwos-test record --duration 60 --effect "Audio Pulse"
    """
    console = Console()

    console.print(
        Panel.fit(
            f"[bold cyan]LightwaveOS Validation Recorder[/bold cyan]\n"
            f"Device: {host}\n"
            f"Database: {db}\n"
            f"Duration: {duration or 'unlimited'}s\n"
            f"Max samples: {max_samples or 'unlimited'}",
            border_style="cyan",
        )
    )

    collector = ValidationCollector(host, db, console)

    try:
        asyncio.run(collector.collect(duration, max_samples, effect))
    except KeyboardInterrupt:
        console.print("\n[yellow]Recording stopped by user[/yellow]")
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        logger.exception("Recording failed")
        raise click.Abort()


@main.command()
@click.option(
    "--db",
    type=click.Path(exists=True, path_type=Path),
    default=Path("validation.db"),
    help="SQLite database path",
    show_default=True,
)
@click.option(
    "--session",
    type=int,
    required=True,
    help="Session ID to analyze",
)
@click.option(
    "--output",
    type=click.Path(path_type=Path),
    default=None,
    help="Output JSON file (default: stdout)",
)
@click.option(
    "--export-csv",
    type=click.Path(path_type=Path),
    default=None,
    help="Export timeseries data to CSV",
)
def analyze(
    db: Path,
    session: int,
    output: Optional[Path],
    export_csv: Optional[Path],
) -> None:
    """
    Analyze validation data from a recorded session.

    Performs statistical analysis, correlation analysis, and anomaly detection
    on captured validation frames.

    Example:
        lwos-test analyze --session 1 --output results.json
    """
    console = Console()

    try:
        analyzer = ValidationAnalyzer(db)

        with console.status(f"[cyan]Analyzing session {session}...[/cyan]"):
            result = analyzer.analyze_session(session)

        # Display results
        console.print(
            Panel.fit(
                f"[bold green]Analysis Complete[/bold green]\n"
                f"Session: {result.session_id}\n"
                f"Effect: {result.effect_name or 'Unknown'} (ID: {result.effect_id})\n"
                f"Samples: {result.sample_count}",
                border_style="green",
            )
        )

        # Performance table
        perf_table = Table(title="Performance Metrics", show_header=True)
        perf_table.add_column("Metric", style="cyan")
        perf_table.add_column("Value", style="magenta")

        perf_table.add_row("FPS Mean", f"{result.fps_mean:.2f}")
        perf_table.add_row("FPS Std Dev", f"{result.fps_std:.2f}")
        perf_table.add_row("FPS Range", f"{result.fps_min:.2f} - {result.fps_max:.2f}")
        perf_table.add_row("Frame Time Mean", f"{result.frame_time_mean_us:.0f} µs")
        perf_table.add_row("Frame Time Std Dev", f"{result.frame_time_std_us:.0f} µs")

        console.print(perf_table)

        # Audio metrics table
        audio_table = Table(title="Audio Metrics", show_header=True)
        audio_table.add_column("Band", style="cyan")
        audio_table.add_column("Mean", style="magenta")
        audio_table.add_column("Std Dev", style="yellow")

        audio_table.add_row("Bass", f"{result.bass_energy_mean:.4f}", f"{result.bass_energy_std:.4f}")
        audio_table.add_row("Mid", f"{result.mid_energy_mean:.4f}", f"{result.mid_energy_std:.4f}")
        audio_table.add_row("Treble", f"{result.treble_energy_mean:.4f}", f"{result.treble_energy_std:.4f}")
        audio_table.add_row("Total", f"{result.total_energy_mean:.4f}", f"{result.total_energy_std:.4f}")

        console.print(audio_table)

        # Anomalies table
        anomaly_table = Table(title="Anomalies Detected", show_header=True)
        anomaly_table.add_column("Type", style="cyan")
        anomaly_table.add_column("Count", style="red")

        anomaly_table.add_row("Frame Drops (<60 FPS)", str(result.frame_drops))
        anomaly_table.add_row("Render Spikes", str(result.render_spikes))
        anomaly_table.add_row("Audio Saturation", str(result.audio_saturation_events))

        console.print(anomaly_table)

        # Correlation metrics
        console.print(f"\n[bold]Correlations:[/bold]")
        console.print(f"  Audio-Visual: {result.audio_visual_correlation:.3f}")
        console.print(f"  Frequency Response Smoothness: {result.frequency_response_smoothness:.3f}")

        # Write JSON output if requested
        if output:
            with open(output, "w") as f:
                json.dump(result.to_dict(), f, indent=2)
            console.print(f"\n[green]✓ Results saved to {output}[/green]")

        # Export CSV if requested
        if export_csv:
            analyzer.export_timeseries(session, export_csv)
            console.print(f"[green]✓ Timeseries exported to {export_csv}[/green]")

    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        logger.exception("Analysis failed")
        raise click.Abort()


@main.command()
@click.option(
    "--db",
    type=click.Path(exists=True, path_type=Path),
    default=Path("validation.db"),
    help="SQLite database path",
    show_default=True,
)
@click.option(
    "--output",
    type=click.Path(path_type=Path),
    required=True,
    help="Output HTML report file",
)
@click.option(
    "--sessions",
    type=str,
    default=None,
    help="Comma-separated session IDs (default: all)",
)
def report(
    db: Path,
    output: Path,
    sessions: Optional[str],
) -> None:
    """
    Generate HTML validation report.

    Creates a comprehensive HTML report with charts, statistics, and
    comparative analysis across sessions.

    Example:
        lwos-test report --output report.html --sessions 1,2,3
    """
    console = Console()

    try:
        analyzer = ValidationAnalyzer(db)

        # Parse session IDs
        if sessions:
            session_ids = [int(s.strip()) for s in sessions.split(",")]
        else:
            # Get all sessions
            all_sessions = analyzer.get_sessions()
            session_ids = [s["session_id"] for s in all_sessions]

        if not session_ids:
            console.print("[red]No sessions found in database[/red]")
            raise click.Abort()

        with console.status(f"[cyan]Generating report for {len(session_ids)} sessions...[/cyan]"):
            # Analyze all sessions
            results = [analyzer.analyze_session(sid) for sid in session_ids]

            # Generate HTML report
            html = _generate_html_report(results)

            # Write to file
            with open(output, "w") as f:
                f.write(html)

        console.print(f"[green]✓ Report generated: {output}[/green]")
        console.print(f"[cyan]Sessions analyzed: {len(session_ids)}[/cyan]")

    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        logger.exception("Report generation failed")
        raise click.Abort()


@main.command()
@click.option(
    "--host",
    default="lightwaveos.local",
    help="Device hostname or IP address",
    show_default=True,
)
@click.option(
    "--db",
    type=click.Path(path_type=Path),
    default=Path("validation.db"),
    help="SQLite database path",
    show_default=True,
)
def live(host: str, db: Path) -> None:
    """
    Real-time monitoring dashboard.

    Displays live validation metrics in a terminal dashboard with
    auto-updating statistics and charts.

    Example:
        lwos-test live --host 192.168.1.100
    """
    console = Console()

    console.print(
        Panel.fit(
            f"[bold cyan]LightwaveOS Live Monitoring[/bold cyan]\n"
            f"Device: {host}\n"
            f"Press Ctrl+C to stop",
            border_style="cyan",
        )
    )

    # TODO: Implement live monitoring dashboard
    # This would use Rich's Live display to show real-time metrics
    console.print("[yellow]Live monitoring not yet implemented[/yellow]")


@main.command()
@click.option(
    "--db",
    type=click.Path(exists=True, path_type=Path),
    default=Path("validation.db"),
    help="SQLite database path",
    show_default=True,
)
def sessions(db: Path) -> None:
    """
    List all validation sessions in the database.

    Example:
        lwos-test sessions
    """
    console = Console()

    try:
        analyzer = ValidationAnalyzer(db)
        all_sessions = analyzer.get_sessions()

        if not all_sessions:
            console.print("[yellow]No sessions found in database[/yellow]")
            return

        table = Table(title=f"Validation Sessions ({len(all_sessions)} total)", show_header=True)
        table.add_column("ID", style="cyan")
        table.add_column("Effect", style="magenta")
        table.add_column("Effect ID", style="yellow")
        table.add_column("Samples", style="green")
        table.add_column("Start Time", style="blue")
        table.add_column("Duration", style="white")

        for session in all_sessions:
            effect_name = session.get("effect_name") or "Unknown"
            effect_id = str(session.get("effect_id") or "N/A")
            sample_count = str(session.get("sample_count", 0))

            start_time = session.get("start_time", "")[:19]  # Truncate to datetime
            end_time = session.get("end_time")

            if end_time:
                from datetime import datetime

                start = datetime.fromisoformat(session["start_time"])
                end = datetime.fromisoformat(end_time)
                duration = str(end - start).split(".")[0]  # Remove microseconds
            else:
                duration = "In progress"

            table.add_row(
                str(session["session_id"]),
                effect_name,
                effect_id,
                sample_count,
                start_time,
                duration,
            )

        console.print(table)

    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        logger.exception("Failed to list sessions")
        raise click.Abort()


def _generate_html_report(results: list) -> str:
    """Generate HTML report from analysis results."""
    # TODO: Implement full HTML report with charts using Chart.js or similar
    # For now, return a basic HTML template

    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>LightwaveOS Validation Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        h1 {{ color: #2c3e50; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #3498db; color: white; }}
        tr:nth-child(even) {{ background-color: #f2f2f2; }}
        .metric {{ display: inline-block; margin: 10px; padding: 15px; background: #ecf0f1; border-radius: 5px; }}
    </style>
</head>
<body>
    <h1>LightwaveOS Validation Report</h1>
    <p>Generated: {__import__('datetime').datetime.now().isoformat()}</p>
    <p>Sessions analyzed: {len(results)}</p>

    <h2>Summary</h2>
"""

    for result in results:
        html += f"""
    <h3>Session {result.session_id}: {result.effect_name or 'Unknown'}</h3>
    <div class="metric">
        <strong>FPS:</strong> {result.fps_mean:.2f} ± {result.fps_std:.2f}
    </div>
    <div class="metric">
        <strong>Frame Time:</strong> {result.frame_time_mean_us:.0f} µs
    </div>
    <div class="metric">
        <strong>Audio-Visual Correlation:</strong> {result.audio_visual_correlation:.3f}
    </div>
    <div class="metric">
        <strong>Frame Drops:</strong> {result.frame_drops}
    </div>
"""

    html += """
</body>
</html>
"""
    return html


if __name__ == "__main__":
    main()
