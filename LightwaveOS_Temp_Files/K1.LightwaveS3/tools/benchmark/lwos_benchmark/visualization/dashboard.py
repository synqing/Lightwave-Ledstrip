"""Interactive Dash/Plotly dashboard for benchmark visualization."""

import logging
from pathlib import Path

import dash
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from dash import dcc, html
from dash.dependencies import Input, Output

from ..storage.database import BenchmarkDatabase
from ..storage.models import BenchmarkRun

logger = logging.getLogger(__name__)


def create_dashboard(
    database: BenchmarkDatabase,
    port: int = 8050,
    debug: bool = False,
) -> dash.Dash:
    """Create and configure Dash dashboard for benchmark visualization.

    Args:
        database: BenchmarkDatabase instance
        port: Port to serve dashboard on
        debug: Enable debug mode

    Returns:
        Configured Dash app instance
    """
    app = dash.Dash(
        __name__,
        title="LightwaveOS Benchmark Dashboard",
        update_title="Loading...",
    )

    # Layout
    app.layout = html.Div([
        html.H1("LightwaveOS Audio Pipeline Benchmark Dashboard"),

        html.Div([
            html.Label("Select Benchmark Run:"),
            dcc.Dropdown(
                id="run-selector",
                options=[],
                value=None,
            ),
        ], style={"width": "50%", "margin": "20px 0"}),

        html.Div([
            html.H3(id="run-info"),
            html.P(id="run-stats"),
        ], style={"margin": "20px 0"}),

        dcc.Tabs([
            dcc.Tab(label="Time Series", children=[
                dcc.Graph(id="time-series-graph"),
            ]),
            dcc.Tab(label="Distributions", children=[
                dcc.Graph(id="distribution-graph"),
            ]),
            dcc.Tab(label="CPU Load", children=[
                dcc.Graph(id="cpu-load-graph"),
            ]),
            dcc.Tab(label="Comparison", children=[
                html.Div([
                    html.Label("Compare with:"),
                    dcc.Dropdown(
                        id="compare-selector",
                        options=[],
                        value=None,
                    ),
                ], style={"width": "50%", "margin": "20px 0"}),
                dcc.Graph(id="comparison-graph"),
            ]),
        ]),

        dcc.Interval(
            id="refresh-interval",
            interval=5000,  # 5 second refresh
            n_intervals=0,
        ),
    ], style={"padding": "20px"})

    # Callbacks
    @app.callback(
        [Output("run-selector", "options"), Output("compare-selector", "options")],
        Input("refresh-interval", "n_intervals"),
    )
    def update_run_list(n: int) -> tuple[list[dict], list[dict]]:
        """Update list of available runs."""
        runs = database.list_runs(limit=100)
        options = [
            {"label": f"{r.name} ({r.sample_count} samples)", "value": str(r.id)}
            for r in runs
        ]
        return options, options

    @app.callback(
        [Output("run-info", "children"), Output("run-stats", "children")],
        Input("run-selector", "value"),
    )
    def update_run_info(run_id: str | None) -> tuple[str, str]:
        """Update run information display."""
        if not run_id:
            return "Select a run to view details", ""

        run = database.get_run(run_id)
        if not run:
            return "Run not found", ""

        info = f"Run: {run.name}"
        stats = (
            f"Samples: {run.sample_count} | "
            f"Mean: {run.avg_total_us_mean:.1f}µs | "
            f"P95: {run.avg_total_us_p95:.1f}µs | "
            f"CPU: {run.cpu_load_mean:.1f}% | "
            f"Duration: {run.duration_seconds:.1f}s" if run.duration_seconds else ""
        )

        return info, stats

    @app.callback(
        Output("time-series-graph", "figure"),
        Input("run-selector", "value"),
    )
    def update_time_series(run_id: str | None) -> go.Figure:
        """Update time series graph."""
        if not run_id:
            return go.Figure()

        samples = database.get_samples(run_id)
        if not samples:
            return go.Figure()

        df = pd.DataFrame([
            {
                "timestamp_ms": s.timestamp_ms,
                "avg_total_us": s.avg_total_us,
                "avg_goertzel_us": s.avg_goertzel_us,
                "peak_total_us": s.peak_total_us,
            }
            for s in samples
        ])

        fig = go.Figure()

        fig.add_trace(go.Scatter(
            x=df["timestamp_ms"],
            y=df["avg_total_us"],
            mode="lines",
            name="Avg Total",
            line={"color": "blue"},
        ))

        fig.add_trace(go.Scatter(
            x=df["timestamp_ms"],
            y=df["avg_goertzel_us"],
            mode="lines",
            name="Avg Goertzel",
            line={"color": "orange"},
        ))

        fig.add_trace(go.Scatter(
            x=df["timestamp_ms"],
            y=df["peak_total_us"],
            mode="markers",
            name="Peak Total",
            marker={"color": "red", "size": 3},
        ))

        fig.update_layout(
            title="Processing Time Over Time",
            xaxis_title="Timestamp (ms)",
            yaxis_title="Time (µs)",
            hovermode="x unified",
        )

        return fig

    @app.callback(
        Output("distribution-graph", "figure"),
        Input("run-selector", "value"),
    )
    def update_distribution(run_id: str | None) -> go.Figure:
        """Update distribution histogram."""
        if not run_id:
            return go.Figure()

        samples = database.get_samples(run_id)
        if not samples:
            return go.Figure()

        df = pd.DataFrame([
            {"avg_total_us": s.avg_total_us}
            for s in samples
        ])

        fig = px.histogram(
            df,
            x="avg_total_us",
            nbins=50,
            title="Distribution of Average Processing Time",
            labels={"avg_total_us": "Time (µs)"},
        )

        fig.update_layout(
            xaxis_title="Average Total Time (µs)",
            yaxis_title="Count",
        )

        return fig

    @app.callback(
        Output("cpu-load-graph", "figure"),
        Input("run-selector", "value"),
    )
    def update_cpu_load(run_id: str | None) -> go.Figure:
        """Update CPU load graph."""
        if not run_id:
            return go.Figure()

        samples = database.get_samples(run_id)
        if not samples:
            return go.Figure()

        df = pd.DataFrame([
            {
                "timestamp_ms": s.timestamp_ms,
                "cpu_load_percent": s.cpu_load_percent,
            }
            for s in samples
        ])

        fig = go.Figure()

        fig.add_trace(go.Scatter(
            x=df["timestamp_ms"],
            y=df["cpu_load_percent"],
            mode="lines",
            fill="tozeroy",
            name="CPU Load",
            line={"color": "green"},
        ))

        fig.update_layout(
            title="CPU Load Over Time",
            xaxis_title="Timestamp (ms)",
            yaxis_title="CPU Load (%)",
            hovermode="x unified",
        )

        return fig

    @app.callback(
        Output("comparison-graph", "figure"),
        [Input("run-selector", "value"), Input("compare-selector", "value")],
    )
    def update_comparison(run_a_id: str | None, run_b_id: str | None) -> go.Figure:
        """Update comparison box plot."""
        if not run_a_id or not run_b_id:
            return go.Figure()

        samples_a = database.get_samples(run_a_id)
        samples_b = database.get_samples(run_b_id)

        if not samples_a or not samples_b:
            return go.Figure()

        run_a = database.get_run(run_a_id)
        run_b = database.get_run(run_b_id)

        fig = go.Figure()

        fig.add_trace(go.Box(
            y=[s.avg_total_us for s in samples_a],
            name=run_a.name if run_a else "Run A",
            marker_color="blue",
        ))

        fig.add_trace(go.Box(
            y=[s.avg_total_us for s in samples_b],
            name=run_b.name if run_b else "Run B",
            marker_color="orange",
        ))

        fig.update_layout(
            title="Processing Time Comparison",
            yaxis_title="Average Total Time (µs)",
        )

        return fig

    return app


def run_dashboard(
    db_path: Path | str,
    port: int = 8050,
    debug: bool = False,
) -> None:
    """Run the dashboard server.

    Args:
        db_path: Path to SQLite database
        port: Port to serve on
        debug: Enable debug mode
    """
    database = BenchmarkDatabase(db_path)
    app = create_dashboard(database, port, debug)

    logger.info(f"Starting dashboard on http://localhost:{port}")
    app.run_server(debug=debug, port=port)
