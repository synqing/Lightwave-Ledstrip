# lwos-test-audio

Audio effect validation and testing framework for LightwaveOS.

## Overview

`lwos-test-audio` is a Python package for capturing, analyzing, and validating audio-reactive LED effects from LightwaveOS devices. It connects to the device's WebSocket endpoint, captures binary validation frames, and provides comprehensive analysis tools.

## Features

- **Real-time capture**: Subscribe to binary validation frames via WebSocket
- **SQLite storage**: Persistent storage of validation samples for analysis
- **Statistical analysis**: Performance metrics, audio metrics, correlation analysis
- **Anomaly detection**: Frame drops, render spikes, audio saturation
- **Rich CLI**: Beautiful terminal interface with progress bars and tables
- **Export capabilities**: JSON reports and CSV timeseries export

## Installation

### Using uv (recommended)

```bash
cd tools/lwos-test-audio
uv venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
uv pip install -e .
```

### Using pip

```bash
cd tools/lwos-test-audio
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
pip install -e .
```

## Quick Start

### 1. Record validation data

```bash
# Record for 60 seconds
lwos-test record --duration 60 --effect "Audio Pulse"

# Record 1000 samples
lwos-test record --max-samples 1000 --effect "Spectral Waves"

# Record from specific device
lwos-test record --host 192.168.1.100 --duration 30
```

### 2. Analyze a session

```bash
# Analyze session 1
lwos-test analyze --session 1

# Save results to JSON
lwos-test analyze --session 1 --output results.json

# Export timeseries to CSV
lwos-test analyze --session 1 --export-csv timeseries.csv
```

### 3. List sessions

```bash
lwos-test sessions
```

### 4. Generate report

```bash
# Generate HTML report for all sessions
lwos-test report --output report.html

# Generate report for specific sessions
lwos-test report --output report.html --sessions 1,2,3
```

## CLI Commands

### `record`

Capture validation frames from LightwaveOS device.

**Options:**
- `--host` - Device hostname or IP (default: `lightwaveos.local`)
- `--db` - SQLite database path (default: `validation.db`)
- `--duration` - Recording duration in seconds (default: unlimited)
- `--max-samples` - Maximum samples to collect (default: unlimited)
- `--effect` - Name of effect being validated

**Example:**
```bash
lwos-test record --host lightwaveos.local --duration 60 --effect "Bass Pulse"
```

### `analyze`

Analyze validation data from a recorded session.

**Options:**
- `--db` - SQLite database path (default: `validation.db`)
- `--session` - Session ID to analyze (required)
- `--output` - Output JSON file path
- `--export-csv` - Export timeseries to CSV

**Example:**
```bash
lwos-test analyze --session 1 --output analysis.json --export-csv data.csv
```

### `sessions`

List all validation sessions in the database.

**Example:**
```bash
lwos-test sessions
```

### `report`

Generate HTML validation report.

**Options:**
- `--db` - SQLite database path (default: `validation.db`)
- `--output` - Output HTML file (required)
- `--sessions` - Comma-separated session IDs (default: all)

**Example:**
```bash
lwos-test report --output validation_report.html --sessions 1,2,3
```

### `live`

Real-time monitoring dashboard (coming soon).

## Binary Frame Format

Validation frames are 128 bytes:

```
[0-7]     uint64   timestamp (microseconds since boot)
[8-11]    uint32   effect_id
[12]      uint8    effect_type
[13-15]   padding  reserved
[16-47]   floats   audio_metrics (8 floats × 4 bytes)
[48-71]   mixed    performance_metrics (24 bytes)
[72-127]  bytes    effect_data (56 bytes)
```

### Audio Metrics (32 bytes)

- `bass_energy` (float)
- `mid_energy` (float)
- `treble_energy` (float)
- `total_energy` (float)
- `peak_frequency` (float)
- `spectral_centroid` (float)
- `spectral_spread` (float)
- `zero_crossing_rate` (float)

### Performance Metrics (24 bytes)

- `frame_time_us` (uint32)
- `render_time_us` (uint32)
- `fps` (float)
- `heap_free` (uint32)
- `heap_fragmentation` (float)
- `cpu_usage` (uint32)

## Analysis Metrics

### Performance Statistics
- FPS mean, std dev, min, max
- Frame time mean and std dev
- Frame drops (< 60 FPS)
- Render spikes (> 2σ above mean)

### Audio Metrics Statistics
- Bass/mid/treble energy means and std devs
- Total energy distribution
- Audio saturation events

### Correlation Analysis
- **Audio-visual correlation**: How well render time tracks audio energy
- **Frequency response smoothness**: Stability of frequency response

## Python API

```python
from pathlib import Path
from lwos_test_audio.collector import ValidationCollector
from lwos_test_audio.analyzer import ValidationAnalyzer

# Record validation data
collector = ValidationCollector("lightwaveos.local", Path("validation.db"))
await collector.collect(duration=60, effect_name="Audio Pulse")

# Analyze session
analyzer = ValidationAnalyzer(Path("validation.db"))
result = analyzer.analyze_session(session_id=1)

print(f"FPS: {result.fps_mean:.2f} ± {result.fps_std:.2f}")
print(f"Audio-visual correlation: {result.audio_visual_correlation:.3f}")
print(f"Frame drops: {result.frame_drops}")
```

## Database Schema

### `sessions` table

- `session_id` (INTEGER PRIMARY KEY)
- `device_host` (TEXT)
- `start_time` (TEXT ISO8601)
- `end_time` (TEXT ISO8601)
- `effect_name` (TEXT)
- `effect_id` (INTEGER)
- `sample_count` (INTEGER)
- `notes` (TEXT)

### `samples` table

- `id` (INTEGER PRIMARY KEY)
- `session_id` (INTEGER, indexed)
- `timestamp` (INTEGER, indexed)
- `effect_id` (INTEGER, indexed)
- `effect_type` (TEXT)
- `captured_at` (TEXT ISO8601)
- Audio metrics columns (8 floats)
- Performance metrics columns (6 mixed types)
- `effect_data` (BLOB)

## Development

### Install development dependencies

```bash
uv pip install -e ".[dev]"
```

### Run tests

```bash
pytest
```

### Format code

```bash
ruff format .
```

### Type checking

```bash
mypy lwos_test_audio/
```

## Requirements

- Python >= 3.10
- websockets >= 12.0
- numpy >= 1.26.0
- scipy >= 1.11.0
- click >= 8.1.0
- rich >= 13.7.0
- sqlite-utils >= 3.36
- pydantic >= 2.5.0

## License

MIT

## Contributing

Contributions welcome! Please ensure code passes `ruff` and `mypy` checks.
