# lwos-test-audio Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          LightwaveOS Device                              │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ Effect Renderer (120 FPS)                                         │  │
│  │  • Audio processing pipeline                                      │  │
│  │  • LED effect computation                                         │  │
│  │  • Performance monitoring                                         │  │
│  └───────────────┬───────────────────────────────────────────────────┘  │
│                  │ Validation Framework                                  │
│                  ▼                                                        │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ EffectValidationSample::serialize()                               │  │
│  │  • Packs 128-byte binary frame                                    │  │
│  │  • Includes audio metrics, performance, effect data               │  │
│  └───────────────┬───────────────────────────────────────────────────┘  │
│                  │                                                        │
│                  ▼                                                        │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ WebSocket Server (/ws endpoint)                                   │  │
│  │  • Binary frame streaming                                         │  │
│  │  • Subscription-based (validation.subscribe)                      │  │
│  │  • 120 frames/sec → 15.36 KB/sec bandwidth                        │  │
│  └───────────────┬───────────────────────────────────────────────────┘  │
└──────────────────┼───────────────────────────────────────────────────────┘
                   │ ws://lightwaveos.local/ws
                   │ Binary WebSocket Protocol
                   │
                   ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       lwos-test-audio (Python)                           │
│                                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ CLI Layer (cli.py)                                                 │ │
│  │  ┌──────────┬──────────┬──────────┬──────────┬──────────┐         │ │
│  │  │ record   │ analyze  │ sessions │ report   │  live    │         │ │
│  │  └──────────┴──────────┴──────────┴──────────┴──────────┘         │ │
│  │  • Click-based command routing                                    │ │
│  │  • Rich terminal UI (progress bars, tables, panels)               │ │
│  │  • Argument parsing and validation                                │ │
│  └────────────┬───────────────────────────────────────────────────────┘ │
│               │                                                           │
│               ▼                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ Collector Layer (collector.py)                                     │ │
│  │  • ValidationCollector class                                       │ │
│  │  • Async WebSocket client (websockets library)                    │ │
│  │  • Binary frame parsing (struct module)                           │ │
│  │  • Session management                                             │ │
│  │  • Real-time progress tracking                                    │ │
│  └────────────┬───────────────────────────────────────────────────────┘ │
│               │ Parsed samples                                            │
│               ▼                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ Data Models (models.py)                                            │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │ EffectValidationSample                                       │ │ │
│  │  │  • timestamp: int                                            │ │ │
│  │  │  • effect_id: int                                            │ │ │
│  │  │  • effect_type: EffectType                                   │ │ │
│  │  │  • audio_metrics: AudioMetrics                               │ │ │
│  │  │  • performance_metrics: PerformanceMetrics                   │ │ │
│  │  │  • effect_data: bytes                                        │ │ │
│  │  └──────────────────────────────────────────────────────────────┘ │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │ AudioMetrics (32 bytes)                                      │ │ │
│  │  │  • bass_energy, mid_energy, treble_energy                    │ │ │
│  │  │  • total_energy, peak_frequency                              │ │ │
│  │  │  • spectral_centroid, spectral_spread, zero_crossing_rate   │ │ │
│  │  └──────────────────────────────────────────────────────────────┘ │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │ PerformanceMetrics (24 bytes)                                │ │ │
│  │  │  • frame_time_us, render_time_us, fps                        │ │ │
│  │  │  • heap_free, heap_fragmentation, cpu_usage                  │ │ │
│  │  └──────────────────────────────────────────────────────────────┘ │ │
│  └────────────┬───────────────────────────────────────────────────────┘ │
│               │ Store                                                     │
│               ▼                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ SQLite Database (sqlite-utils)                                     │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │ sessions table                                               │ │ │
│  │  │  PK: session_id                                              │ │ │
│  │  │  • device_host, start_time, end_time                         │ │ │
│  │  │  • effect_name, effect_id, sample_count, notes               │ │ │
│  │  └──────────────────────────────────────────────────────────────┘ │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │ samples table                                                │ │ │
│  │  │  PK: id                                                      │ │ │
│  │  │  Indexes: session_id, effect_id, timestamp                   │ │ │
│  │  │  • Flattened audio metrics (8 columns)                       │ │ │
│  │  │  • Flattened performance metrics (6 columns)                 │ │ │
│  │  │  • effect_data (BLOB)                                        │ │ │
│  │  └──────────────────────────────────────────────────────────────┘ │ │
│  └────────────┬───────────────────────────────────────────────────────┘ │
│               │ Load & analyze                                            │
│               ▼                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ Analyzer Layer (analyzer.py)                                       │ │
│  │  • ValidationAnalyzer class                                        │ │
│  │  • NumPy structured arrays for efficiency                         │ │
│  │  • Statistical analysis (mean, std, min, max)                     │ │
│  │  • Correlation analysis (audio-visual, frequency smoothness)      │ │
│  │  • Anomaly detection (frame drops, render spikes, saturation)     │ │
│  │  • SciPy signal processing (filtering, cross-correlation)         │ │
│  └────────────┬───────────────────────────────────────────────────────┘ │
│               │ Results                                                   │
│               ▼                                                           │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │ Output Layer                                                       │ │
│  │  ┌──────────────┬──────────────┬──────────────┬──────────────┐    │ │
│  │  │ Rich Tables  │ JSON Export  │ CSV Export   │ HTML Report  │    │ │
│  │  └──────────────┴──────────────┴──────────────┴──────────────┘    │ │
│  │  • Terminal: Rich tables, progress bars, panels                   │ │
│  │  • JSON: Analysis results with full metadata                      │ │
│  │  • CSV: Timeseries data for external tools (Excel, Jupyter)       │ │
│  │  • HTML: Comprehensive reports with charts (future: Chart.js)     │ │
│  └────────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Recording Flow

```
User Command
    │
    ▼
┌───────────────────────────────────┐
│ lwos-test record                  │
│  --host lightwaveos.local         │
│  --duration 60                    │
│  --effect "Audio Pulse"           │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ ValidationCollector.__init__()    │
│  • Create DB connection           │
│  • Initialize session             │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ WebSocket Connection              │
│  ws://lightwaveos.local/ws        │
│  • Send: {"command": "validation.subscribe"} │
└───────────┬───────────────────────┘
            │
            ▼ Binary frames (128 bytes each)
┌───────────────────────────────────┐
│ Frame Reception Loop              │
│  while running:                   │
│    message = await ws.recv()      │
│    if isinstance(message, bytes): │
│      sample = parse_frame(message)│
│      store_to_db(sample)          │
└───────────┬───────────────────────┘
            │ Every frame
            ▼
┌───────────────────────────────────┐
│ EffectValidationSample.from_binary_frame() │
│  1. Validate size (128 bytes)     │
│  2. struct.unpack header          │
│  3. Parse audio metrics           │
│  4. Parse performance metrics     │
│  5. Extract effect data           │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ SQLite INSERT                     │
│  • Flatten nested structures      │
│  • Insert into samples table      │
│  • Update session sample_count    │
│  • Commit transaction             │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ Progress Display                  │
│  Rich Progress Bar Update:        │
│  "Samples: 1234 | FPS: 118.5"     │
└───────────────────────────────────┘
```

### Analysis Flow

```
User Command
    │
    ▼
┌───────────────────────────────────┐
│ lwos-test analyze                 │
│  --session 1                      │
│  --output results.json            │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ ValidationAnalyzer.analyze_session() │
│  1. Load session metadata         │
│  2. Load all samples into NumPy   │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ get_session_samples()             │
│  • SELECT * FROM samples WHERE... │
│  • Convert to NumPy structured array │
│  dtype = [("timestamp", "i8"),    │
│            ("fps", "f4"), ...]    │
└───────────┬───────────────────────┘
            │ NumPy array (efficient column ops)
            ▼
┌───────────────────────────────────┐
│ Statistical Analysis              │
│  • np.mean(data["fps"])           │
│  • np.std(data["fps"])            │
│  • np.min/max(data["fps"])        │
│  • Same for all metrics           │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ Correlation Analysis              │
│  • np.corrcoef(audio, render)     │
│  • signal.butter() for smoothing  │
│  • signal.filtfilt() application  │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ Anomaly Detection                 │
│  • Frame drops: fps < 60          │
│  • Render spikes: > mean + 2σ     │
│  • Audio saturation: energy ≈ 1.0 │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ AnalysisResult object             │
│  • All statistics packaged        │
│  • to_dict() for JSON             │
└───────────┬───────────────────────┘
            │
            ▼
┌───────────────────────────────────┐
│ Output Formatting                 │
│  • Rich tables (terminal)         │
│  • JSON export (file)             │
│  • CSV export (timeseries)        │
└───────────────────────────────────┘
```

## Binary Frame Parsing

### Frame Structure (128 bytes)

```
Offset  Size  Type      Field                 Description
──────────────────────────────────────────────────────────────────────
0       8     uint64    timestamp             Microseconds since boot
8       4     uint32    effect_id             Effect index (0-45)
12      1     uint8     effect_type           0=Standard, 1=Audio, 2=LGP, 3=Physics
13      3     padding   reserved              Alignment padding

16      4     float     bass_energy           Bass band energy (0.0-1.0)
20      4     float     mid_energy            Mid band energy (0.0-1.0)
24      4     float     treble_energy         Treble band energy (0.0-1.0)
28      4     float     total_energy          Total energy (0.0-1.0)
32      4     float     peak_frequency        Dominant frequency (Hz)
36      4     float     spectral_centroid     Spectral center (Hz)
40      4     float     spectral_spread       Spectral width (Hz)
44      4     float     zero_crossing_rate    ZCR (0.0-1.0)

48      4     uint32    frame_time_us         Total frame time (µs)
52      4     uint32    render_time_us        Effect render time (µs)
56      4     float     fps                   Current FPS
60      4     uint32    heap_free             Free heap bytes
64      4     float     heap_fragmentation    Fragmentation (0.0-1.0)
68      4     uint32    cpu_usage             CPU usage percentage

72      56    bytes     effect_data           Effect-specific payload
──────────────────────────────────────────────────────────────────────
Total: 128 bytes
```

### Parsing Code

```python
# Header
timestamp, effect_id, effect_type_raw = struct.unpack_from("<QIB3x", data, 0)

# Audio metrics (offset 16)
audio_values = struct.unpack_from("<8f", data, 16)
# [bass, mid, treble, total, peak_freq, centroid, spread, zcr]

# Performance metrics (offset 48)
perf_values = struct.unpack_from("<IIfIfI", data, 48)
# [frame_time, render_time, fps, heap_free, fragmentation, cpu]

# Effect data (offset 72)
effect_data = data[72:128]  # 56 bytes
```

## Database Schema

### Tables

```sql
-- Sessions table
CREATE TABLE sessions (
    session_id INTEGER PRIMARY KEY,
    device_host TEXT,
    start_time TEXT,      -- ISO8601 format
    end_time TEXT,        -- ISO8601 format
    effect_name TEXT,
    effect_id INTEGER,
    sample_count INTEGER,
    notes TEXT
);

-- Samples table
CREATE TABLE samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER,
    timestamp INTEGER,
    effect_id INTEGER,
    effect_type TEXT,
    captured_at TEXT,     -- ISO8601 format

    -- Audio metrics (8 columns)
    bass_energy REAL,
    mid_energy REAL,
    treble_energy REAL,
    total_energy REAL,
    peak_frequency REAL,
    spectral_centroid REAL,
    spectral_spread REAL,
    zero_crossing_rate REAL,

    -- Performance metrics (6 columns)
    frame_time_us INTEGER,
    render_time_us INTEGER,
    fps REAL,
    heap_free INTEGER,
    heap_fragmentation REAL,
    cpu_usage REAL,

    -- Effect data
    effect_data BLOB,

    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);

-- Indexes for fast queries
CREATE INDEX idx_samples_session_id ON samples(session_id);
CREATE INDEX idx_samples_effect_id ON samples(effect_id);
CREATE INDEX idx_samples_timestamp ON samples(timestamp);
```

### Query Patterns

```python
# Load session samples (efficient with index)
SELECT * FROM samples
WHERE session_id = ?
ORDER BY timestamp;

# Get session statistics
SELECT
    COUNT(*) as count,
    AVG(fps) as avg_fps,
    MIN(fps) as min_fps,
    MAX(fps) as max_fps
FROM samples
WHERE session_id = ?;

# Find anomalies
SELECT timestamp, fps, render_time_us
FROM samples
WHERE session_id = ?
  AND fps < 60.0
ORDER BY timestamp;
```

## NumPy Analysis Pipeline

### Structured Array Creation

```python
# Define dtype matching database columns
dtype = [
    ("timestamp", "i8"),
    ("effect_id", "i4"),
    ("bass_energy", "f4"),
    ("mid_energy", "f4"),
    # ... all metrics
    ("fps", "f4"),
]

# Preallocate array
data = np.zeros(len(samples), dtype=dtype)

# Fill from database rows
for i, sample in enumerate(samples):
    data[i]["timestamp"] = sample["timestamp"]
    data[i]["fps"] = sample["fps"]
    # ... fill all fields
```

### Efficient Column Operations

```python
# Calculate statistics (vectorized)
fps_mean = np.mean(data["fps"])
fps_std = np.std(data["fps"])

# Boolean indexing for anomalies
frame_drops = np.sum(data["fps"] < 60.0)

# Cross-correlation
correlation = np.corrcoef(
    data["total_energy"],
    data["render_time_us"]
)[0, 1]
```

### Signal Processing

```python
from scipy import signal

# Design low-pass filter
b, a = signal.butter(N=3, Wn=0.1)

# Apply to frequency data
filtered = signal.filtfilt(b, a, data["peak_frequency"])

# Measure smoothness
deviation = np.std(data["peak_frequency"] - filtered)
smoothness = 1.0 / (1.0 + deviation)
```

## Class Hierarchy

```
lwos_test_audio/
│
├── models.py
│   ├── EffectType (Enum)
│   ├── AudioMetrics (dataclass)
│   │   ├── from_bytes() classmethod
│   │   └── to_dict() method
│   ├── PerformanceMetrics (dataclass)
│   │   ├── from_bytes() classmethod
│   │   └── to_dict() method
│   ├── EffectValidationSample (dataclass)
│   │   ├── from_binary_frame() classmethod
│   │   └── to_dict() method
│   └── ValidationSession (dataclass)
│       └── to_dict() method
│
├── collector.py
│   └── ValidationCollector
│       ├── __init__(host, db_path, console)
│       ├── collect(duration, max_samples, effect_name) async
│       ├── _init_database()
│       ├── _create_session()
│       ├── _store_sample()
│       ├── _finalize_session()
│       └── _subscribe_validation_frames() async
│
├── analyzer.py
│   ├── AnalysisResult (dataclass)
│   │   └── to_dict()
│   └── ValidationAnalyzer
│       ├── __init__(db_path)
│       ├── get_sessions()
│       ├── get_session_samples(session_id) → np.ndarray
│       ├── analyze_session(session_id) → AnalysisResult
│       ├── compare_sessions(session_ids) → dict
│       └── export_timeseries(session_id, output_path)
│
└── cli.py
    └── main() - Click group
        ├── record(host, db, duration, max_samples, effect)
        ├── analyze(db, session, output, export_csv)
        ├── sessions(db)
        ├── report(db, output, sessions)
        └── live(host, db)
```

## Dependencies Graph

```
websockets ──┐
             ├─→ ValidationCollector ──┐
rich ────────┘                         │
                                       ├─→ CLI commands
sqlite-utils ─────────────────────────┤
                                       │
numpy ───────┐                         │
             ├─→ ValidationAnalyzer ───┘
scipy ───────┘

click ──────────→ CLI framework
pydantic ───────→ Data validation (future)
```

## Performance Characteristics

- **WebSocket throughput**: 120 FPS × 128 bytes = 15.36 KB/s
- **SQLite write rate**: ~1000 inserts/sec (batch commits)
- **Analysis time**: <1s for 10k samples (NumPy vectorization)
- **Memory usage**: ~100 KB per 1000 samples (structured array)
- **Disk usage**: ~500 KB per minute (compressed SQLite)

## Error Handling Strategy

```
WebSocket Connection
    ├─ ConnectionError → Retry with exponential backoff
    ├─ TimeoutError → Log and continue (non-fatal)
    └─ Binary parse error → Skip frame, log warning

Database Operations
    ├─ DatabaseLocked → Retry with delay
    ├─ IntegrityError → Log and skip (duplicate)
    └─ IOError → Abort with clear error message

Analysis
    ├─ ValueError (bad session_id) → User-friendly error
    ├─ NumPy warning → Suppress or log at DEBUG
    └─ SciPy convergence issue → Fallback to simpler method
```

## Extensibility Points

1. **Custom Analyzers** - Add new analysis modules
2. **Effect-Specific Validators** - Per-effect validation logic
3. **Export Formats** - Parquet, HDF5, etc.
4. **Report Templates** - Custom HTML/PDF generators
5. **Real-Time Alerts** - Threshold-based notifications

This architecture provides a robust, performant, and maintainable framework for LightwaveOS audio effect validation.
