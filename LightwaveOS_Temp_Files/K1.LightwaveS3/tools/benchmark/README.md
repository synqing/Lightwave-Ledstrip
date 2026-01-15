# LightwaveOS Benchmark Collector

Python-based benchmark data collection and analysis tool for LightwaveOS audio pipeline performance monitoring.

## Features

- **Real-time WebSocket Collection**: Subscribe to binary benchmark frames from ESP32
- **SQLite Storage**: Persistent storage of benchmark runs and samples
- **Statistical Analysis**: Percentiles, EMA, t-tests, Cohen's d for A/B testing
- **Interactive Dashboard**: Plotly/Dash web interface for visualization
- **CLI Tools**: Collect, compare, export, and serve benchmark data

## Installation

```bash
# Using uv (recommended)
uv pip install -e .

# Using pip
pip install -e .
```

## Usage

### Collect Benchmark Data

```bash
# Collect from default host (lightwaveos.local)
lwos-bench collect --run-name "baseline" --duration 60

# Collect from custom host
lwos-bench collect --host 192.168.1.100 --run-name "optimization-test" --duration 120
```

### Compare Benchmark Runs

```bash
# Statistical comparison of two runs
lwos-bench compare run1_id run2_id

# Export comparison report
lwos-bench compare run1_id run2_id --output report.json
```

### Export Data

```bash
# Export to CSV
lwos-bench export run_id --format csv --output data.csv

# Export to JSON
lwos-bench export run_id --format json --output data.json
```

### Launch Dashboard

```bash
# Start interactive web dashboard
lwos-bench serve --port 8050
```

## Binary Frame Format

32-byte compact frame structure:

| Offset | Type    | Field            | Description                    |
|--------|---------|------------------|--------------------------------|
| 0      | uint32  | magic            | 0x004D4241 (validation)        |
| 4      | uint32  | timestamp_ms     | Milliseconds since boot        |
| 8      | float32 | avgTotalUs       | Average total processing (µs)  |
| 12     | float32 | avgGoertzelUs    | Average Goertzel time (µs)     |
| 16     | float32 | cpuLoadPercent   | CPU load percentage            |
| 20     | uint16  | peakTotalUs      | Peak total processing (µs)     |
| 22     | uint16  | peakGoertzelUs   | Peak Goertzel time (µs)        |
| 24     | uint32  | hopCount         | FFT hop count                  |
| 28     | uint16  | goertzelCount    | Goertzel filter count          |
| 30     | uint8   | flags            | Status flags                   |
| 31     | uint8   | reserved         | Reserved for future use        |

## Architecture

```
lwos_benchmark/
├── cli.py                    # Click CLI interface
├── collectors/
│   └── websocket.py         # WebSocket binary frame collector
├── parsers/
│   └── binary.py            # Binary frame parser
├── storage/
│   ├── models.py            # Pydantic data models
│   └── database.py          # SQLite storage layer
├── analysis/
│   ├── statistics.py        # Statistical calculations
│   └── comparison.py        # A/B testing and comparison
└── visualization/
    └── dashboard.py         # Dash/Plotly dashboard
```

## Development

```bash
# Install with dev dependencies
uv pip install -e ".[dev]"

# Run tests
pytest

# Type checking
mypy lwos_benchmark

# Linting
ruff check lwos_benchmark
```
