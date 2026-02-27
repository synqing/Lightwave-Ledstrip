# LightwaveOS Benchmark Tool - Usage Guide

## Installation

```bash
cd v2/tools/benchmark

# Using uv (recommended for Python 3.12+)
uv pip install -e .

# Or using pip
pip install -e .
```

## Quick Start

### 1. Collect Benchmark Data

Collect 60 seconds of benchmark data from your ESP32:

```bash
lwos-bench collect \
  --host lightwaveos.local \
  --run-name "baseline-measurement" \
  --description "Initial baseline before optimization" \
  --duration 60 \
  --tags "baseline,pre-optimization"
```

The tool will:
- Connect to ESP32 via WebSocket
- Subscribe to binary benchmark frames
- Parse 32-byte frames in real-time
- Store samples in SQLite database
- Compute statistics (mean, percentiles)
- Display run ID for later reference

### 2. List Benchmark Runs

```bash
lwos-bench list-runs --limit 20
```

Copy the run ID for the next steps.

### 3. Compare Two Runs (A/B Testing)

After making an optimization, collect another run and compare:

```bash
lwos-bench compare <baseline_run_id> <optimized_run_id>
```

Output includes:
- Independent t-test results (p-values)
- Cohen's d effect sizes
- Statistical significance at α=0.05
- Human-readable interpretation

Example output:
```
Benchmark Comparison: baseline vs optimized
============================================================

Sample sizes: 1200 vs 1150
Significance level: α = 0.05

Average Total Time:
  Mean difference: -23.5µs
  p-value: 0.0001 (significant)
  Cohen's d: 0.87 (large)

CPU Load:
  Mean difference: -2.3%
  p-value: 0.0234 (significant)
  Cohen's d: 0.45 (small)
```

### 4. Export Data for External Analysis

Export to CSV for analysis in Excel/Pandas:

```bash
lwos-bench export <run_id> --format csv --output data.csv
```

Export to JSON:

```bash
lwos-bench export <run_id> --format json --output data.json
```

### 5. Interactive Dashboard

Launch web-based dashboard for visualization:

```bash
lwos-bench serve --port 8050
```

Open browser to `http://localhost:8050` for:
- Time series plots of processing time
- Distribution histograms
- CPU load graphs
- Run-to-run comparisons
- Auto-refreshing data

## Advanced Usage

### Programmatic Collection (Python API)

```python
import asyncio
from pathlib import Path
from lwos_benchmark.collectors.websocket import WebSocketCollector
from lwos_benchmark.storage.database import BenchmarkDatabase
from lwos_benchmark.storage.models import BenchmarkRun
from lwos_benchmark.analysis.statistics import compute_run_statistics

async def collect_benchmark():
    # Initialize
    database = BenchmarkDatabase(Path("benchmark.db"))

    # Create run
    run = BenchmarkRun(
        name="my-benchmark",
        host="lightwaveos.local",
        tags=["experiment-1"]
    )
    database.create_run(run)

    # Collect
    collector = WebSocketCollector("lightwaveos.local", database=database)
    await collector.connect()
    await collector.collect(run, duration=60.0)
    await collector.disconnect()

    # Analyze
    compute_run_statistics(database, run)
    run.mark_complete()
    database.update_run(run)

    print(f"Mean processing time: {run.avg_total_us_mean:.1f}µs")
    print(f"P95: {run.avg_total_us_p95:.1f}µs")

    database.close()

asyncio.run(collect_benchmark())
```

### Statistical Analysis

```python
from lwos_benchmark.analysis.comparison import compare_runs, cohens_d
from lwos_benchmark.storage.database import BenchmarkDatabase

database = BenchmarkDatabase("benchmark.db")

run_a = database.get_run("run_a_uuid")
run_b = database.get_run("run_b_uuid")

# Full comparison report
result = compare_runs(database, run_a, run_b, alpha=0.05)

print(f"Significant: {result.avg_total_us_significant}")
print(f"Effect size: {result.avg_total_us_effect_size}")
print(f"Mean difference: {result.avg_total_us_diff:.1f}µs")
```

### Custom Analysis with NumPy/Pandas

```python
from lwos_benchmark.storage.database import BenchmarkDatabase
import numpy as np
import pandas as pd

database = BenchmarkDatabase("benchmark.db")
samples = database.get_samples("run_uuid")

# Convert to DataFrame
df = pd.DataFrame([
    {
        "timestamp_ms": s.timestamp_ms,
        "avg_total_us": s.avg_total_us,
        "cpu_load_percent": s.cpu_load_percent,
    }
    for s in samples
])

# Custom analysis
print(f"Median: {df['avg_total_us'].median():.1f}µs")
print(f"Std dev: {df['avg_total_us'].std():.1f}µs")

# Detect outliers
from lwos_benchmark.analysis.statistics import detect_outliers

data = df['avg_total_us'].values
outlier_indices, outlier_values = detect_outliers(data, threshold=3.0)
print(f"Outliers: {len(outlier_indices)} samples")
```

## Binary Frame Format

The ESP32 sends 32-byte compact frames:

```
Offset | Type    | Field            | Description
-------|---------|------------------|---------------------------
0      | uint32  | magic            | 0x004D4241 (validation)
4      | uint32  | timestamp_ms     | Device uptime in ms
8      | float32 | avgTotalUs       | Average processing time
12     | float32 | avgGoertzelUs    | Average Goertzel time
16     | float32 | cpuLoadPercent   | CPU utilization
20     | uint16  | peakTotalUs      | Peak processing time
22     | uint16  | peakGoertzelUs   | Peak Goertzel time
24     | uint32  | hopCount         | FFT hop count (config)
28     | uint16  | goertzelCount    | Number of Goertzel filters
30     | uint8   | flags            | Status bitfield
31     | uint8   | reserved         | Reserved
```

### Status Flags (offset 30)

```
Bit 0: Buffer overflow detected
Bit 1: Timing warning (processing too slow)
Bit 2: Audio active (input detected)
Bits 3-7: Reserved
```

## WebSocket Protocol

### Subscribe to Benchmark Stream

Send JSON message:
```json
{"type": "benchmark.subscribe"}
```

ESP32 will start sending binary frames at configured interval (typically 100-500ms).

### Unsubscribe

Send JSON message:
```json
{"type": "benchmark.unsubscribe"}
```

## Database Schema

SQLite database with two tables:

### `benchmark_runs`
- Stores run metadata and aggregated statistics
- Includes percentiles (P50, P95, P99)
- Duration, sample count, tags

### `benchmark_samples`
- Individual data points
- Foreign key to `benchmark_runs`
- Indexed on `run_id` and `timestamp_utc`

## Example Workflow: Optimization Testing

```bash
# 1. Baseline measurement
lwos-bench collect \
  --run-name "baseline-goertzel-8" \
  --duration 120 \
  --tags "baseline,goertzel-8"

# Note the run ID: a1b2c3d4-...

# 2. Make code changes (e.g., reduce Goertzel count from 8 to 4)

# 3. Measurement after optimization
lwos-bench collect \
  --run-name "optimized-goertzel-4" \
  --duration 120 \
  --tags "optimized,goertzel-4"

# Note the run ID: e5f6g7h8-...

# 4. Statistical comparison
lwos-bench compare a1b2c3d4-... e5f6g7h8-...

# 5. Export for documentation
lwos-bench export e5f6g7h8-... --format csv --output results.csv

# 6. Visualize in dashboard
lwos-bench serve
```

## Performance Tips

### Collection
- **Sample rate**: ESP32 sends ~2-10 frames/sec depending on config
- **Duration**: 60-120s recommended for stable statistics
- **Max samples**: Set `--max-samples` to limit database size

### Analysis
- **Percentiles**: P95/P99 more robust than max for latency
- **Effect size**: Cohen's d > 0.5 indicates meaningful improvement
- **Sample size**: 500+ samples recommended for reliable t-tests

### Dashboard
- Auto-refreshes every 5 seconds
- Large datasets (>10k samples) may slow rendering
- Use `--limit` when exporting large runs

## Troubleshooting

### Connection Timeout
```
Error: Connection timeout after 10.0s
```

**Solutions**:
- Verify ESP32 is on network: `ping lightwaveos.local`
- Check WiFi credentials in `network_config.h`
- Ensure WebServer feature is enabled (`FEATURE_WEB_SERVER`)

### Invalid Frame Magic
```
Warning: Invalid frame magic: 0xDEADBEEF
```

**Solutions**:
- ESP32 firmware may not be sending benchmark frames
- Verify benchmark feature is compiled in
- Check ESP32 serial output for errors

### No Samples Collected
```
Error: No samples found for run
```

**Solutions**:
- ESP32 may not have subscribed properly
- Check WebSocket connection in ESP32 logs
- Verify benchmark transmission is enabled

## API Reference

Full API documentation in docstrings:

```python
# Parser
from lwos_benchmark.parsers.binary import BenchmarkFrame
help(BenchmarkFrame.from_bytes)

# Database
from lwos_benchmark.storage.database import BenchmarkDatabase
help(BenchmarkDatabase)

# Statistics
from lwos_benchmark.analysis.statistics import compute_run_statistics
help(compute_run_statistics)

# Comparison
from lwos_benchmark.analysis.comparison import compare_runs
help(compare_runs)
```

## Development

Run tests:
```bash
pytest tests/
```

Type checking:
```bash
mypy lwos_benchmark
```

Linting:
```bash
ruff check lwos_benchmark
```

Format code:
```bash
ruff format lwos_benchmark
```
