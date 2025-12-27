# lwos-test-audio Package Overview

Complete Python test orchestrator for LightwaveOS audio effect validation.

## Package Structure

```
lwos-test-audio/
├── pyproject.toml              # Package configuration (hatchling, dependencies)
├── README.md                   # Full documentation
├── QUICKSTART.md              # 5-minute getting started guide
├── .gitignore                 # Python/IDE/database exclusions
│
├── lwos_test_audio/           # Main package
│   ├── __init__.py            # Package exports and version
│   ├── models.py              # Data models (128-byte frame parsing)
│   ├── collector.py           # WebSocket client for frame capture
│   ├── analyzer.py            # Statistical analysis engine
│   ├── cli.py                 # Click-based CLI (record/analyze/report)
│   ├── report.py              # HTML report generation (existing)
│   └── analyzers/             # Specialized analyzers (existing)
│       ├── __init__.py
│       ├── latency_analyzer.py
│       ├── motion_analyzer.py
│       └── smoothness_analyzer.py
│
├── tests/                     # Unit tests
│   ├── __init__.py
│   └── test_models.py         # Model parsing tests
│
└── examples/                  # Usage examples
    └── basic_usage.py         # Python API demonstration
```

## Core Components

### 1. Data Models (`models.py`)

**Binary Frame Format (128 bytes)**:
- Header (16 bytes): timestamp, effect_id, effect_type
- Audio Metrics (32 bytes): 8 floats for bass/mid/treble energy, frequency analysis
- Performance Metrics (24 bytes): frame timing, FPS, heap, CPU
- Effect Data (56 bytes): effect-specific payload

**Key Classes**:
- `EffectValidationSample` - Complete 128-byte frame
- `AudioMetrics` - Audio processing metrics
- `PerformanceMetrics` - Device performance metrics
- `ValidationSession` - Recording session metadata

### 2. Collector (`collector.py`)

**Purpose**: Real-time WebSocket client for frame capture

**Features**:
- Async WebSocket connection to `ws://host/ws`
- Binary frame subscription with `validation.subscribe` command
- SQLite storage with session management
- Rich progress bars and live statistics
- Duration and sample count limits
- Graceful shutdown on Ctrl+C

**Database Schema**:
- `sessions` table: session metadata, timing, effect info
- `samples` table: flattened frame data with indices on session_id, effect_id, timestamp

### 3. Analyzer (`analyzer.py`)

**Purpose**: Statistical analysis and anomaly detection

**Capabilities**:
- Performance statistics (FPS mean/std/min/max, frame times)
- Audio metrics statistics (energy distributions)
- Correlation analysis:
  - Audio-visual correlation (render time vs energy)
  - Frequency response smoothness (stability)
- Anomaly detection:
  - Frame drops (< 60 FPS)
  - Render spikes (> 2σ above mean)
  - Audio saturation events
- NumPy structured arrays for efficient analysis
- CSV export for external tools

### 4. CLI (`cli.py`)

**Commands**:

1. **`record`** - Capture validation frames
   ```bash
   lwos-test record --host lightwaveos.local --duration 60 --effect "Audio Pulse"
   ```
   - Options: --host, --db, --duration, --max-samples, --effect
   - Creates session, subscribes to frames, stores to SQLite
   - Rich progress display with live FPS

2. **`analyze`** - Statistical analysis
   ```bash
   lwos-test analyze --session 1 --output results.json --export-csv data.csv
   ```
   - Options: --db, --session, --output, --export-csv
   - Rich tables for performance, audio, anomalies
   - JSON export and CSV timeseries

3. **`sessions`** - List all sessions
   ```bash
   lwos-test sessions --db validation.db
   ```
   - Rich table with effect names, sample counts, durations

4. **`report`** - Generate HTML report
   ```bash
   lwos-test report --output report.html --sessions 1,2,3
   ```
   - Options: --db, --output, --sessions
   - HTML with charts and comparative analysis

5. **`live`** - Real-time dashboard (TODO)
   ```bash
   lwos-test live --host lightwaveos.local
   ```
   - Rich Live display with auto-updating metrics

## Technology Stack

### Core Dependencies
- **websockets 12.0+** - Async WebSocket client
- **numpy 1.26+** - Numerical analysis with structured arrays
- **scipy 1.11+** - Signal processing (filters, correlation)
- **click 8.1+** - CLI framework with decorators
- **rich 13.7+** - Beautiful terminal output
- **sqlite-utils 3.36+** - SQLite operations
- **pydantic 2.5+** - Data validation

### Development Tools
- **pytest** - Unit testing with async support
- **ruff** - Fast linting and formatting (replaces black/isort/flake8)
- **mypy** - Static type checking
- **pytest-cov** - Coverage reporting

## Key Features

### Modern Python Patterns

1. **Type Hints Throughout**
   ```python
   def analyze_session(self, session_id: int) -> AnalysisResult:
       """Fully typed function signatures."""
   ```

2. **Dataclasses for Models**
   ```python
   @dataclass
   class AudioMetrics:
       bass_energy: float
       mid_energy: float
       # ... clean, typed data structures
   ```

3. **Async/Await for WebSocket**
   ```python
   async with websockets.connect(ws_url) as websocket:
       message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
   ```

4. **Context Managers**
   ```python
   with Progress(...) as progress:
       # Automatic cleanup
   ```

5. **Structured Arrays for Performance**
   ```python
   dtype = [("timestamp", "i8"), ("fps", "f4"), ...]
   data = np.zeros(len(samples), dtype=dtype)
   # Efficient column operations
   ```

### Binary Frame Parsing

Robust parsing with struct module:
```python
@classmethod
def from_binary_frame(cls, data: bytes) -> "EffectValidationSample":
    if len(data) != 128:
        raise ValueError(f"Expected 128 bytes, got {len(data)}")

    # Parse header
    timestamp, effect_id, effect_type = struct.unpack_from("<QIB3x", data, 0)

    # Parse metrics
    audio_metrics = AudioMetrics.from_bytes(data, offset=16)
    performance_metrics = PerformanceMetrics.from_bytes(data, offset=48)

    # Extract effect data
    effect_data = data[72:128]

    return cls(...)
```

### Rich Terminal UI

Beautiful progress bars and tables:
```python
with Progress(
    SpinnerColumn(),
    TextColumn("[progress.description]{task.description}"),
    BarColumn(),
    TaskProgressColumn(),
) as progress:
    task = progress.add_task("Collecting samples...", total=max_samples)
    progress.update(task, completed=count, description=f"FPS: {fps:.1f}")
```

### Statistical Analysis

SciPy-powered signal processing:
```python
# Low-pass filter for frequency smoothness
b, a = signal.butter(3, 0.1)
filtered_freq = signal.filtfilt(b, a, data["peak_frequency"])
freq_deviation = np.std(data["peak_frequency"] - filtered_freq)
freq_response_smoothness = 1.0 / (1.0 + freq_deviation)
```

## Usage Patterns

### Quick Validation Workflow

```bash
# 1. Start effect on device (web UI)
# 2. Record validation data
lwos-test record --duration 30 --effect "Audio Pulse"

# 3. Analyze results
lwos-test analyze --session 1

# 4. Export for deeper analysis
lwos-test analyze --session 1 --export-csv data.csv

# 5. Generate report
lwos-test report --output validation_report.html
```

### Python API Workflow

```python
import asyncio
from pathlib import Path
from lwos_test_audio import ValidationCollector, ValidationAnalyzer

# Record
collector = ValidationCollector("lightwaveos.local", Path("test.db"))
await collector.collect(duration=30)

# Analyze
analyzer = ValidationAnalyzer(Path("test.db"))
result = analyzer.analyze_session(1)

# Check metrics
print(f"FPS: {result.fps_mean:.2f} ± {result.fps_std:.2f}")
print(f"Frame drops: {result.frame_drops}")
print(f"Audio-visual correlation: {result.audio_visual_correlation:.3f}")

# Export
analyzer.export_timeseries(1, Path("data.csv"))
```

## Installation

### Using uv (recommended - fastest)

```bash
cd tools/lwos-test-audio
uv venv
source .venv/bin/activate
uv pip install -e .
```

### Using pip

```bash
cd tools/lwos-test-audio
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

### Verify Installation

```bash
lwos-test --version
# Output: lwos-test, version 0.1.0

lwos-test --help
# Shows all commands
```

## Development

### Run Tests

```bash
uv pip install -e ".[dev]"
pytest
pytest --cov=lwos_test_audio  # With coverage
```

### Format Code

```bash
ruff format .
ruff check .
```

### Type Checking

```bash
mypy lwos_test_audio/
```

## Integration Points

### Device Side (ESP32)

The device must:
1. Enable `FEATURE_VALIDATION_FRAMEWORK` in build
2. Stream 128-byte frames via WebSocket on `/ws` endpoint
3. Support `validation.subscribe` command
4. Broadcast frames at effect render rate (120 FPS target)

### Binary Protocol

Frame structure matches `EffectValidationSample::serialize()` in C++:
- Little-endian byte order (`<` in struct format)
- Packed structure with explicit padding
- Fixed 128-byte size for consistent streaming

### Database Schema

SQLite tables use sqlite-utils for convenience:
- Automatic indexing on foreign keys
- JSON export capabilities
- Query builder interface

## Future Enhancements

### Planned Features

1. **Live Dashboard** (`live` command)
   - Rich Live display with auto-updating charts
   - Real-time anomaly alerts
   - Multi-panel layout (performance, audio, alerts)

2. **Enhanced HTML Reports**
   - Chart.js integration for interactive charts
   - Comparative session analysis
   - Export to PDF option

3. **Advanced Analysis**
   - Frequency domain analysis (FFT)
   - Beat detection validation
   - Latency measurement (audio input to LED output)

4. **Effect-Specific Validators**
   - Custom analyzers per effect type
   - Physics validation for LGP effects
   - Motion smoothness metrics

### Extensibility Points

- Custom analyzers in `lwos_test_audio/analyzers/`
- Plugin system for effect-specific validation
- Export formats (Parquet, HDF5 for large datasets)
- Integration with CI/CD for automated validation

## Performance Considerations

- **Streaming**: WebSocket handles 120 FPS (7.68 KB/s sustained)
- **Storage**: ~500 KB per minute of recording (compressed SQLite)
- **Analysis**: NumPy structured arrays for memory efficiency
- **Scalability**: Handles multi-hour recordings with pagination

## Best Practices

1. **Use descriptive effect names** in `--effect` flag for tracking
2. **Record at least 30 seconds** for statistical significance
3. **Export CSV for custom analysis** in Jupyter notebooks
4. **Compare sessions** for A/B testing of effect changes
5. **Check frame drops** to validate device performance
6. **Monitor audio-visual correlation** for responsiveness

## Troubleshooting

See QUICKSTART.md for common issues and solutions:
- Connection refused → check network/device
- No frames received → verify FEATURE_VALIDATION_FRAMEWORK
- Low sample rate → check device FPS and network
- Database locked → close other connections

## Related Documentation

- `README.md` - Full package documentation
- `QUICKSTART.md` - 5-minute tutorial
- `examples/basic_usage.py` - Python API examples
- `tests/test_models.py` - Unit test examples
- `../../docs/api/BENCHMARK_API.md` - Device API specification

## Summary

`lwos-test-audio` is a production-ready Python package for validating audio-reactive LED effects on LightwaveOS. It provides:

- **Robust data collection** via WebSocket binary frames
- **Comprehensive analysis** with NumPy/SciPy
- **Beautiful CLI** with Rich terminal UI
- **Extensible architecture** for custom validators
- **Modern Python practices** (type hints, async, dataclasses)

Perfect for regression testing, performance validation, and effect development.
