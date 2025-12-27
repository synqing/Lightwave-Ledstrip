# Quick Start Guide

Get started with `lwos-test-audio` in 5 minutes.

## Prerequisites

- Python 3.10 or higher
- LightwaveOS device on the network
- Device must be running firmware with validation framework enabled

## Installation

### Option 1: Using uv (fastest)

```bash
cd tools/lwos-test-audio
uv venv
source .venv/bin/activate
uv pip install -e .
```

### Option 2: Using pip

```bash
cd tools/lwos-test-audio
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

## Verify Installation

```bash
lwos-test --version
# Should output: lwos-test, version 0.1.0
```

## Basic Workflow

### Step 1: Start an effect on your device

Using the web interface at `http://lightwaveos.local`, select an audio-reactive effect like "Audio Pulse" or "Spectral Waves".

### Step 2: Record validation data

```bash
lwos-test record --duration 30 --effect "Audio Pulse"
```

This will:
- Connect to `ws://lightwaveos.local/ws`
- Subscribe to validation frames
- Record 30 seconds of data
- Save to `validation.db`

You should see output like:
```
Connecting to ws://lightwaveos.local/ws...
Connected! Session ID: 1
Collecting frames... Press Ctrl+C to stop
Samples: 3600 | FPS: 120.0
✓ Collected 3600 samples in session 1
```

### Step 3: Analyze the data

```bash
lwos-test analyze --session 1
```

This displays:
- Performance metrics (FPS, frame times)
- Audio metrics (bass/mid/treble energy)
- Correlation analysis
- Anomaly detection (frame drops, render spikes)

Example output:
```
┌─────────────────────────────────────┐
│ Analysis Complete                   │
│ Session: 1                          │
│ Effect: Audio Pulse (ID: 10)        │
│ Samples: 3600                       │
└─────────────────────────────────────┘

Performance Metrics
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Metric              Value
FPS Mean            118.45
FPS Std Dev         2.31
FPS Range           112.50 - 120.00
Frame Time Mean     8439 µs
```

### Step 4: Export results

Save analysis to JSON:
```bash
lwos-test analyze --session 1 --output results.json
```

Export timeseries to CSV:
```bash
lwos-test analyze --session 1 --export-csv data.csv
```

### Step 5: Generate HTML report

```bash
lwos-test report --output report.html
```

Opens `report.html` in your browser to see comprehensive analysis.

## Advanced Usage

### Record from specific device

```bash
lwos-test record --host 192.168.1.100 --duration 60
```

### Record unlimited samples (stop with Ctrl+C)

```bash
lwos-test record --effect "Spectral Analysis"
```

### List all sessions

```bash
lwos-test sessions
```

Output:
```
Validation Sessions (3 total)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
ID  Effect         Effect ID  Samples  Start Time          Duration
1   Audio Pulse    10         3600     2025-01-15 14:30:00 00:00:30
2   Spectral Wave  12         7200     2025-01-15 14:35:00 00:01:00
3   Bass Pulse     15         1800     2025-01-15 14:40:00 00:00:15
```

### Compare multiple sessions

```bash
lwos-test report --output comparison.html --sessions 1,2,3
```

## Programmatic Usage

```python
import asyncio
from pathlib import Path
from lwos_test_audio.collector import ValidationCollector
from lwos_test_audio.analyzer import ValidationAnalyzer

# Record data
async def record():
    collector = ValidationCollector("lightwaveos.local", Path("test.db"))
    await collector.collect(duration=30, effect_name="Audio Pulse")

asyncio.run(record())

# Analyze data
analyzer = ValidationAnalyzer(Path("test.db"))
result = analyzer.analyze_session(1)
print(f"FPS: {result.fps_mean:.2f}")
print(f"Audio-Visual Correlation: {result.audio_visual_correlation:.3f}")
```

## Troubleshooting

### "Connection refused" error

**Problem**: Can't connect to device WebSocket

**Solutions**:
1. Verify device is on network: `ping lightwaveos.local`
2. Check device has WiFi enabled: Use `esp32dev_wifi` build
3. Try IP address instead: `lwos-test record --host 192.168.1.100`

### No validation frames received

**Problem**: Connected but no data

**Solutions**:
1. Ensure device firmware has `FEATURE_VALIDATION_FRAMEWORK` enabled
2. Check effect is audio-reactive (type = AUDIO_REACTIVE)
3. Verify audio input is active on device

### Low sample rate

**Problem**: Only getting a few samples per second

**Solutions**:
1. Check device performance (should be 120 FPS)
2. Verify network latency: `ping lightwaveos.local`
3. Check WiFi signal strength on device

### Database locked error

**Problem**: Can't write to database

**Solutions**:
1. Close other connections to the database
2. Use different database file: `--db validation2.db`
3. Check file permissions

## Next Steps

- Read the full [README.md](README.md) for detailed documentation
- Check [examples/basic_usage.py](examples/basic_usage.py) for Python API examples
- Explore the binary frame format for custom analysis
- Build custom analysis scripts using NumPy/SciPy

## Getting Help

- Check the full README for detailed command documentation
- Review example code in `examples/`
- File issues on GitHub with `[lwos-test-audio]` prefix
