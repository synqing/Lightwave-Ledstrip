#!/usr/bin/env python3
"""
Basic usage example for lwos-test-audio package.

Demonstrates how to record validation data, analyze it, and generate reports
programmatically using the Python API.
"""

import asyncio
from pathlib import Path

from rich.console import Console

from lwos_test_audio.collector import ValidationCollector
from lwos_test_audio.analyzer import ValidationAnalyzer


async def record_session_example():
    """Example: Record 30 seconds of validation data."""
    console = Console()
    console.print("[bold cyan]Recording validation data...[/bold cyan]")

    # Create collector
    collector = ValidationCollector(
        host="lightwaveos.local",
        db_path=Path("example_validation.db"),
        console=console,
    )

    # Record for 30 seconds
    await collector.collect(
        duration=30,
        effect_name="Audio Pulse Effect",
    )

    console.print("[green]✓ Recording complete![/green]")
    return collector.session.session_id if collector.session else None


def analyze_session_example(session_id: int):
    """Example: Analyze a recorded session."""
    console = Console()
    console.print(f"[bold cyan]Analyzing session {session_id}...[/bold cyan]")

    # Create analyzer
    analyzer = ValidationAnalyzer(Path("example_validation.db"))

    # Analyze session
    result = analyzer.analyze_session(session_id)

    # Display results
    console.print("\n[bold]Performance Metrics:[/bold]")
    console.print(f"  FPS: {result.fps_mean:.2f} ± {result.fps_std:.2f}")
    console.print(f"  FPS Range: {result.fps_min:.2f} - {result.fps_max:.2f}")
    console.print(f"  Frame Time: {result.frame_time_mean_us:.0f} µs")

    console.print("\n[bold]Audio Metrics:[/bold]")
    console.print(f"  Bass Energy: {result.bass_energy_mean:.4f} ± {result.bass_energy_std:.4f}")
    console.print(f"  Mid Energy: {result.mid_energy_mean:.4f} ± {result.mid_energy_std:.4f}")
    console.print(f"  Treble Energy: {result.treble_energy_mean:.4f} ± {result.treble_energy_std:.4f}")

    console.print("\n[bold]Correlations:[/bold]")
    console.print(f"  Audio-Visual: {result.audio_visual_correlation:.3f}")
    console.print(f"  Frequency Response Smoothness: {result.frequency_response_smoothness:.3f}")

    console.print("\n[bold]Anomalies:[/bold]")
    console.print(f"  Frame Drops: {result.frame_drops}")
    console.print(f"  Render Spikes: {result.render_spikes}")
    console.print(f"  Audio Saturation Events: {result.audio_saturation_events}")

    # Export to JSON
    output_path = Path(f"session_{session_id}_analysis.json")
    import json

    with open(output_path, "w") as f:
        json.dump(result.to_dict(), f, indent=2)

    console.print(f"\n[green]✓ Analysis saved to {output_path}[/green]")

    # Export timeseries to CSV
    csv_path = Path(f"session_{session_id}_timeseries.csv")
    analyzer.export_timeseries(session_id, csv_path)
    console.print(f"[green]✓ Timeseries exported to {csv_path}[/green]")


def compare_sessions_example():
    """Example: Compare multiple sessions."""
    console = Console()
    console.print("[bold cyan]Comparing sessions...[/bold cyan]")

    analyzer = ValidationAnalyzer(Path("example_validation.db"))

    # Get all sessions
    sessions = analyzer.get_sessions()
    if len(sessions) < 2:
        console.print("[yellow]Need at least 2 sessions for comparison[/yellow]")
        return

    # Compare first two sessions
    session_ids = [sessions[0]["session_id"], sessions[1]["session_id"]]
    comparison = analyzer.compare_sessions(session_ids)

    console.print(f"\n[bold]Comparing {len(session_ids)} sessions:[/bold]")

    for session_data in comparison["sessions"]:
        console.print(f"\nSession {session_data['session_id']}:")
        console.print(f"  Effect: {session_data.get('effect_name', 'Unknown')}")
        console.print(f"  FPS: {session_data['performance']['fps_mean']:.2f}")
        console.print(
            f"  Audio-Visual Correlation: {session_data['correlations']['audio_visual_correlation']:.3f}"
        )

    console.print("\n[bold]Comparison Summary:[/bold]")
    console.print(
        f"  FPS Range: {comparison['comparison']['fps_range'][0]:.2f} - {comparison['comparison']['fps_range'][1]:.2f}"
    )
    console.print(f"  Total Frame Drops: {comparison['comparison']['total_frame_drops']}")
    console.print(f"  Total Render Spikes: {comparison['comparison']['total_render_spikes']}")


async def main():
    """Run all examples."""
    console = Console()

    console.print("[bold magenta]LightwaveOS Audio Validation Examples[/bold magenta]\n")

    # Example 1: Record a session
    console.print("[bold]Example 1: Recording Validation Data[/bold]")
    session_id = await record_session_example()

    if session_id:
        # Example 2: Analyze the session
        console.print("\n[bold]Example 2: Analyzing Session Data[/bold]")
        analyze_session_example(session_id)

        # Example 3: Compare sessions (if multiple exist)
        console.print("\n[bold]Example 3: Comparing Sessions[/bold]")
        compare_sessions_example()

    console.print("\n[bold green]All examples complete![/bold green]")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        Console().print("\n[yellow]Examples interrupted by user[/yellow]")
