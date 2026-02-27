"""Command-line interface for LightwaveOS benchmark tools."""

import asyncio
import json
import logging
import sys
from pathlib import Path
from uuid import UUID

import click
import pandas as pd

from .analysis.comparison import compare_runs
from .analysis.statistics import compute_run_statistics
from .collectors.websocket import WebSocketCollector
from .storage.database import BenchmarkDatabase
from .storage.models import BenchmarkRun
from .visualization.dashboard import run_dashboard

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)


DEFAULT_DB_PATH = Path.home() / ".lwos_benchmark" / "benchmark.db"


@click.group()
@click.option(
    "--db",
    type=click.Path(path_type=Path),
    default=DEFAULT_DB_PATH,
    help="Database path",
)
@click.pass_context
def cli(ctx: click.Context, db: Path) -> None:
    """LightwaveOS Audio Pipeline Benchmark Tool.

    Collect, analyze, and visualize benchmark data from ESP32.
    """
    ctx.ensure_object(dict)
    ctx.obj["db_path"] = db
    ctx.obj["database"] = BenchmarkDatabase(db)
    logger.info(f"Using database: {db}")


@cli.command()
@click.option(
    "--host",
    default="lightwaveos.local",
    help="ESP32 hostname or IP",
)
@click.option(
    "--port",
    default=80,
    type=int,
    help="WebSocket port",
)
@click.option(
    "--run-name",
    required=True,
    help="Benchmark run name",
)
@click.option(
    "--description",
    default="",
    help="Run description",
)
@click.option(
    "--duration",
    type=float,
    help="Collection duration in seconds",
)
@click.option(
    "--max-samples",
    type=int,
    help="Maximum samples to collect",
)
@click.option(
    "--tags",
    help="Comma-separated tags",
)
@click.pass_context
def collect(
    ctx: click.Context,
    host: str,
    port: int,
    run_name: str,
    description: str,
    duration: float | None,
    max_samples: int | None,
    tags: str | None,
) -> None:
    """Collect benchmark data from ESP32."""
    database: BenchmarkDatabase = ctx.obj["database"]

    # Create run
    run = BenchmarkRun(
        name=run_name,
        description=description,
        host=host,
        tags=tags.split(",") if tags else [],
    )

    database.create_run(run)
    logger.info(f"Created run '{run.name}' (ID: {run.id})")

    # Collect data
    collector = WebSocketCollector(host, port, database)

    async def collect_async() -> None:
        try:
            await collector.connect()
            sample_count = await collector.collect(run, duration, max_samples)

            # Compute statistics
            compute_run_statistics(database, run)
            run.mark_complete()
            database.update_run(run)

            click.echo(f"✓ Collected {sample_count} samples")
            click.echo(f"✓ Run ID: {run.id}")

        except Exception as e:
            logger.error(f"Collection failed: {e}")
            sys.exit(1)
        finally:
            await collector.disconnect()

    try:
        asyncio.run(collect_async())
    except KeyboardInterrupt:
        click.echo("\nCollection interrupted by user")
        sys.exit(1)


@cli.command()
@click.argument("run_a_id", type=str)
@click.argument("run_b_id", type=str)
@click.option(
    "--alpha",
    default=0.05,
    type=float,
    help="Significance level",
)
@click.option(
    "--output",
    type=click.Path(path_type=Path),
    help="Save report to JSON file",
)
@click.pass_context
def compare(
    ctx: click.Context,
    run_a_id: str,
    run_b_id: str,
    alpha: float,
    output: Path | None,
) -> None:
    """Compare two benchmark runs statistically."""
    database: BenchmarkDatabase = ctx.obj["database"]

    # Load runs
    run_a = database.get_run(run_a_id)
    run_b = database.get_run(run_b_id)

    if not run_a or not run_b:
        click.echo("Error: One or both runs not found", err=True)
        sys.exit(1)

    # Perform comparison
    result = compare_runs(database, run_a, run_b, alpha)

    # Display results
    click.echo(f"\n{'='*60}")
    click.echo(f"Benchmark Comparison: {run_a.name} vs {run_b.name}")
    click.echo(f"{'='*60}\n")

    click.echo(f"Sample sizes: {result.n_samples_a} vs {result.n_samples_b}")
    click.echo(f"Significance level: α = {alpha}\n")

    click.echo("Average Total Time:")
    click.echo(f"  Mean difference: {result.avg_total_us_diff:+.1f}µs")
    click.echo(f"  p-value: {result.avg_total_us_pvalue:.4f} "
               f"({'significant' if result.avg_total_us_significant else 'not significant'})")
    click.echo(f"  Cohen's d: {result.avg_total_us_cohens_d:.2f} "
               f"({result.avg_total_us_effect_size})\n")

    click.echo("CPU Load:")
    click.echo(f"  Mean difference: {result.cpu_load_diff:+.1f}%")
    click.echo(f"  p-value: {result.cpu_load_pvalue:.4f} "
               f"({'significant' if result.cpu_load_significant else 'not significant'})")
    click.echo(f"  Cohen's d: {result.cpu_load_cohens_d:.2f} "
               f"({result.cpu_load_effect_size})\n")

    # Save to file if requested
    if output:
        with output.open("w") as f:
            json.dump(result.model_dump(), f, indent=2, default=str)
        click.echo(f"✓ Report saved to {output}")


@cli.command()
@click.argument("run_id", type=str)
@click.option(
    "--format",
    type=click.Choice(["csv", "json"]),
    default="csv",
    help="Export format",
)
@click.option(
    "--output",
    type=click.Path(path_type=Path),
    required=True,
    help="Output file path",
)
@click.pass_context
def export(
    ctx: click.Context,
    run_id: str,
    format: str,
    output: Path,
) -> None:
    """Export benchmark data to CSV or JSON."""
    database: BenchmarkDatabase = ctx.obj["database"]

    # Load run and samples
    run = database.get_run(run_id)
    if not run:
        click.echo(f"Error: Run {run_id} not found", err=True)
        sys.exit(1)

    samples = database.get_samples(run_id)
    if not samples:
        click.echo("Error: No samples found for run", err=True)
        sys.exit(1)

    # Convert to DataFrame
    df = pd.DataFrame([
        {
            "timestamp_utc": s.timestamp_utc.isoformat(),
            "timestamp_ms": s.timestamp_ms,
            "avg_total_us": s.avg_total_us,
            "avg_goertzel_us": s.avg_goertzel_us,
            "peak_total_us": s.peak_total_us,
            "peak_goertzel_us": s.peak_goertzel_us,
            "cpu_load_percent": s.cpu_load_percent,
            "hop_count": s.hop_count,
            "goertzel_count": s.goertzel_count,
            "flags": s.flags,
        }
        for s in samples
    ])

    # Export
    if format == "csv":
        df.to_csv(output, index=False)
    else:  # json
        df.to_json(output, orient="records", indent=2)

    click.echo(f"✓ Exported {len(samples)} samples to {output}")


@cli.command()
@click.option(
    "--port",
    default=8050,
    type=int,
    help="Dashboard port",
)
@click.option(
    "--debug",
    is_flag=True,
    help="Enable debug mode",
)
@click.pass_context
def serve(
    ctx: click.Context,
    port: int,
    debug: bool,
) -> None:
    """Launch interactive web dashboard."""
    db_path: Path = ctx.obj["db_path"]

    click.echo(f"Starting dashboard on http://localhost:{port}")
    click.echo("Press Ctrl+C to stop")

    try:
        run_dashboard(db_path, port, debug)
    except KeyboardInterrupt:
        click.echo("\nShutdown complete")


@cli.command()
@click.option(
    "--limit",
    default=20,
    type=int,
    help="Number of runs to display",
)
@click.option(
    "--tags",
    help="Filter by tags (comma-separated)",
)
@click.pass_context
def list_runs(
    ctx: click.Context,
    limit: int,
    tags: str | None,
) -> None:
    """List benchmark runs."""
    database: BenchmarkDatabase = ctx.obj["database"]

    tag_list = tags.split(",") if tags else None
    runs = database.list_runs(limit=limit, tags=tag_list)

    if not runs:
        click.echo("No runs found")
        return

    click.echo(f"\n{'ID':<36} | {'Name':<30} | {'Samples':>8} | {'Status':<10}")
    click.echo("-" * 100)

    for run in runs:
        status = "complete" if run.is_complete else "incomplete"
        click.echo(f"{run.id} | {run.name:<30} | {run.sample_count:8d} | {status:<10}")

    click.echo(f"\nTotal: {len(runs)} runs")


@cli.command()
@click.argument("run_id", type=str)
@click.pass_context
def delete(
    ctx: click.Context,
    run_id: str,
) -> None:
    """Delete a benchmark run and its samples."""
    database: BenchmarkDatabase = ctx.obj["database"]

    # Confirm deletion
    run = database.get_run(run_id)
    if not run:
        click.echo(f"Error: Run {run_id} not found", err=True)
        sys.exit(1)

    click.confirm(
        f"Delete run '{run.name}' with {run.sample_count} samples?",
        abort=True,
    )

    if database.delete_run(run_id):
        click.echo(f"✓ Deleted run {run_id}")
    else:
        click.echo("Error: Failed to delete run", err=True)
        sys.exit(1)


if __name__ == "__main__":
    cli()
