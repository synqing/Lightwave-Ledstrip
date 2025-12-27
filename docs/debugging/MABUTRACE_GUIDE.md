# MabuTrace Integration Guide

This guide explains how to use MabuTrace for Perfetto timeline visualization of the LightwaveOS audio pipeline.

## Overview

MabuTrace is a lightweight tracing library that produces Perfetto-compatible trace files. When enabled, it records timing information for each phase of the audio pipeline, allowing detailed performance analysis in the Perfetto UI.

**Key Benefits:**
- Visualize audio pipeline timing on a timeline
- Identify bottlenecks and jitter sources
- Track CPU load over time
- Detect false trigger events
- Compare performance across firmware versions

## Prerequisites

- PlatformIO installed
- ESP32-S3 with audio hardware (SPH0645 I2S microphone)
- Web browser for Perfetto UI (https://ui.perfetto.dev)

## Enabling MabuTrace

### Build with Tracing Enabled

```bash
# Build the audio trace environment
pio run -e esp32dev_audio_trace

# Upload to device
pio run -e esp32dev_audio_trace -t upload
```

This environment:
- Extends `esp32dev_audio` (includes WiFi and audio sync)
- Adds `FEATURE_MABUTRACE=1` build flag
- Includes `mabuware/mabutrace` library dependency

### Manual Enable (Alternative)

Add to your `platformio.ini`:

```ini
[env:my_custom_trace]
extends = env:esp32dev_audio
build_flags =
    ${env:esp32dev_audio.build_flags}
    -D FEATURE_MABUTRACE=1
lib_deps =
    ${env:esp32dev_audio.lib_deps}
    mabuware/mabutrace
```

## Trace Points

The following trace events are instrumented:

### Scoped Traces (Duration)

| Name | Location | Description |
|------|----------|-------------|
| `audio_pipeline` | `processHop()` | Entire audio processing hop (~16ms budget) |
| `goertzel_analyze` | Goertzel phase | 8-band frequency analysis |
| `chroma_analyze` | Chroma phase | 12-bin chromagram extraction |

### Counter Traces (Metrics)

| Name | Update Rate | Description |
|------|-------------|-------------|
| `cpu_load` | ~1 Hz | CPU utilization percentage (x100 for precision) |

### Instant Events (Markers)

| Name | Trigger | Description |
|------|---------|-------------|
| `FALSE_TRIGGER` | On detection | Activity gate triggered but no band energy detected |

## Capturing Traces

### Method 1: Serial Command (Recommended)

```bash
# Connect to serial monitor
pio device monitor -b 115200

# Start capture (in serial menu)
trace_start

# Run your test scenario for 10-30 seconds

# Stop and dump trace
trace_dump
```

The trace data will be output as base64-encoded binary. Copy this output.

### Method 2: WebSocket API

```javascript
// Connect to WebSocket
const ws = new WebSocket('ws://lightwaveos.local/ws');

// Start trace capture
ws.send(JSON.stringify({cmd: 'trace.start', buffer_kb: 64}));

// After test scenario, stop and retrieve
ws.send(JSON.stringify({cmd: 'trace.stop'}));
ws.send(JSON.stringify({cmd: 'trace.dump'}));
```

### Method 3: REST API

```bash
# Start trace
curl -X POST http://lightwaveos.local/api/v1/debug/trace/start

# Stop and download
curl http://lightwaveos.local/api/v1/debug/trace/dump > trace.bin
```

## Viewing Traces in Perfetto

1. Open https://ui.perfetto.dev in Chrome or Firefox
2. Click "Open trace file"
3. Drag and drop your trace file (or paste base64 data)
4. Use the timeline to zoom and pan

### Perfetto Navigation Tips

- **Scroll**: Zoom in/out on timeline
- **Click + Drag**: Select time range
- **W/A/S/D**: Pan and zoom
- **F**: Fit view to selection
- **M**: Mark current selection

### Analyzing Audio Pipeline

1. **Find audio_pipeline slices**: These show each 16ms hop
2. **Check for gaps**: Gaps indicate missed hops or jitter
3. **Compare nested slices**: goertzel_analyze and chroma_analyze should complete within audio_pipeline
4. **Monitor cpu_load**: Should stay below 50% for headroom
5. **Investigate FALSE_TRIGGER**: Clusters indicate noise floor issues

## Example Analysis

### Normal Operation
```
[audio_pipeline: 8.2ms]
  [goertzel_analyze: 3.1ms]
  [chroma_analyze: 2.4ms]
[audio_pipeline: 7.9ms]
  [goertzel_analyze: 3.0ms]
  [chroma_analyze: 2.3ms]
```

### Jitter Issue
```
[audio_pipeline: 8.1ms]
  [goertzel_analyze: 3.2ms]
  [chroma_analyze: 2.5ms]
        <-- 5ms gap (missed hop deadline!)
[audio_pipeline: 12.4ms]  <-- Overrun!
  [goertzel_analyze: 4.8ms]  <-- GC pause?
  [chroma_analyze: 2.6ms]
```

### False Trigger Storm
```
[audio_pipeline: 7.8ms]
  FALSE_TRIGGER
  FALSE_TRIGGER
  FALSE_TRIGGER
[audio_pipeline: 7.9ms]
  FALSE_TRIGGER
```

This indicates the noise floor threshold needs adjustment.

## Performance Impact

MabuTrace is designed for minimal overhead:

| Metric | With Tracing | Without |
|--------|--------------|---------|
| RAM usage | +2-4 KB (ring buffer) | Baseline |
| CPU per hop | +10-20 us | Baseline |
| Flash size | +8-12 KB | Baseline |

When `FEATURE_MABUTRACE=0` (default), all trace macros compile to no-ops with zero runtime cost.

## Troubleshooting

### Trace Buffer Full

If traces appear truncated:
```cpp
// Increase buffer size in AudioActor::onStart()
TRACE_INIT(128);  // 128 KB instead of default 64 KB
```

### No Trace Output

1. Verify build environment: `pio run -e esp32dev_audio_trace -t clean && pio run -e esp32dev_audio_trace`
2. Check serial output for "MabuTrace initialized" message
3. Ensure audio pipeline is running (check "Audio alive" logs)

### Trace File Won't Load in Perfetto

1. Ensure file is not truncated (check file size)
2. Try "Open with legacy parser" option in Perfetto
3. Verify base64 decoding if captured from serial

## Combining with Benchmark Metrics

For maximum insight, enable both tracing and benchmarking:

```ini
[env:esp32dev_audio_full_debug]
extends = env:esp32dev_audio
build_flags =
    ${env:esp32dev_audio.build_flags}
    -D FEATURE_MABUTRACE=1
    -D FEATURE_AUDIO_BENCHMARK=1
lib_deps =
    ${env:esp32dev_audio.lib_deps}
    mabuware/mabutrace
```

This provides:
- Perfetto timeline (visual, interactive)
- Benchmark stats (numerical, aggregated)

## API Reference

See `v2/src/audio/AudioBenchmarkTrace.h` for the complete macro API:

```cpp
TRACE_SCOPE(name)           // Scoped duration trace
TRACE_BEGIN(name)           // Manual span start
TRACE_END()                 // Manual span end
TRACE_COUNTER(name, value)  // Counter/metric value
TRACE_INSTANT(name)         // Point-in-time marker
TRACE_INIT(buffer_kb)       // Initialize trace system
TRACE_FLUSH()               // Flush pending events
TRACE_IS_ENABLED()          // Check if tracing active
```

## Related Documentation

- [Audio Pipeline Architecture](../architecture/audio-pipeline.md)
- [Benchmark Metrics Guide](../analysis/audio-benchmark-analysis.md)
- [Performance Optimization](../optimization/PERFORMANCE_REPORT.md)
