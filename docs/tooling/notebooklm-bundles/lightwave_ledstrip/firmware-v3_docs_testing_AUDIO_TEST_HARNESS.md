# Audio Effect Validation Test Harness

**Document Version**: 1.0
**Date**: 2025-12-28
**Status**: Production
**Author**: LightwaveOS Engineering

---

## Executive Summary

The LightwaveOS Audio Effect Validation Test Harness is a comprehensive framework for verifying the correctness and performance of audio-reactive LED effects. It addresses the critical need to validate that effects behave as expected across various audio inputs, eliminating issues such as the "jog-dial" bidirectional motion bug and ensuring smooth, aesthetically pleasing visualizations.

The harness consists of three integrated components:

1. **Firmware Instrumentation** - Embedded metrics collection with minimal overhead
2. **WebSocket Streaming** - Real-time binary data transmission to external tools
3. **Python Orchestrator** - Automated test execution, data collection, and analysis

---

## Architecture Overview

```
+------------------------------------------------------------------+
|                        TEST ORCHESTRATOR                          |
|                     (Python - Host Machine)                       |
|                                                                   |
|  +---------------+  +---------------+  +--------------------+     |
|  | Test Scenario |  | Audio Source  |  | Analysis Engine    |     |
|  | Controller    |  | Generator     |  | (Statistics/       |     |
|  |               |  | (Sine/Sweep/  |  |  Comparisons)      |     |
|  |               |  |  Music)       |  |                    |     |
|  +-------+-------+  +-------+-------+  +---------+----------+     |
|          |                  |                    |                |
+----------|------------------|--------------------|-----------------+
           |                  |                    |
           |    WebSocket     |                    |
           |    (Binary)      |                    | SQLite
           v                  v                    v Storage
+------------------------------------------------------------------+
|                        ESP32-S3 FIRMWARE                          |
|                                                                   |
|  +-------------------+    +------------------+                    |
|  | Effect Renderer   |    | Audio Pipeline   |                    |
|  | (LGP Scanner,     |--->| (Goertzel,       |                    |
|  |  Wave Collision,  |    |  ControlBus)     |                    |
|  |  Bloom, Waveform) |    |                  |                    |
|  +--------+----------+    +--------+---------+                    |
|           |                        |                              |
|           v                        v                              |
|  +--------------------------------------------+                   |
|  |        Validation Instrumentation          |                   |
|  |  - EffectValidationSample (128 bytes)      |                   |
|  |  - Lock-free ring buffer (16KB)            |                   |
|  |  - ValidationFrameEncoder                  |                   |
|  +--------------------+-----------------------+                   |
|                       |                                           |
|                       v                                           |
|  +--------------------------------------------+                   |
|  |        WebSocket Broadcaster               |                   |
|  |  - Binary frame encoding                   |                   |
|  |  - 10 Hz transmission rate                 |                   |
|  |  - Up to 16 samples per frame              |                   |
|  +--------------------------------------------+                   |
|                                                                   |
+------------------------------------------------------------------+
```

---

## Components

### 1. Firmware Instrumentation

The firmware instrumentation layer captures effect state with minimal CPU overhead, using a lock-free ring buffer for thread-safe cross-core communication.

**Key Files:**
- `src/validation/EffectValidationMetrics.h` - Core data structures
- `src/validation/EffectValidationMacros.h` - Instrumentation macros
- `src/validation/ValidationFrameEncoder.h` - Binary encoding

**EffectValidationSample Structure:**
```cpp
struct EffectValidationSample {
    // Timing (8 bytes)
    uint32_t timestamp_us;      // Microseconds since boot
    uint32_t hop_seq;           // Audio hop sequence number

    // Identification (4 bytes)
    uint8_t  effect_id;         // Current effect index
    uint8_t  reversal_count;    // Direction reversals this frame
    uint16_t reserved1;         // Alignment padding

    // Phase State (16 bytes)
    float phase;                // Normalized phase (0.0-1.0)
    float phase_delta;          // Rate of change (signed)
    float speed_scale_raw;      // Raw speed before slew
    float speed_scale_smooth;   // Smoothed speed after slew

    // Audio Metrics (16 bytes)
    float dominant_freq_bin;    // Dominant frequency (0.0-7.0)
    float energy_avg;           // Average energy (0.0-1.0)
    float energy_delta;         // Energy change
    float scroll_phase;         // Bloom scroll phase

    // Reserved (84 bytes padding to 128)
    uint8_t reserved2[84];
};
```

**Memory Footprint:**
- Sample size: 128 bytes (cache-line aligned)
- Ring buffer: 128 samples x 128 bytes = 16KB
- CPU overhead: < 0.1% per frame

### 2. WebSocket Streaming

The WebSocket streaming layer transmits validation samples as compact binary frames to connected clients.

**Binary Frame Format:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | magic[0] | 0x54 ('T') |
| 1 | 1 | uint8 | magic[1] | 0x56 ('V') |
| 2 | 1 | uint8 | magic[2] | 0x56 ('V') |
| 3 | 1 | uint8 | sample_count | Number of samples (0-16) |
| 4+ | 128*N | struct | samples[] | N x EffectValidationSample |

**Frame Size:**
- Header: 4 bytes
- Max payload: 16 x 128 = 2048 bytes
- Max frame: 2052 bytes

**Transmission Rate:**
- Default: 10 Hz (100ms interval)
- Configurable: 1-60 Hz via `setDrainRate()`

**Subscription Protocol:**
```json
// Subscribe to validation stream
{"type": "validation.subscribe"}

// Unsubscribe from validation stream
{"type": "validation.unsubscribe"}
```

### 3. Python Orchestrator

The Python orchestrator (`tools/benchmark/`) provides automated test execution and data analysis.

**Package Structure:**
```
tools/benchmark/
├── lwos_benchmark/
│   ├── __init__.py
│   ├── cli.py                    # Command-line interface
│   ├── collectors/
│   │   └── websocket.py          # WebSocket binary collector
│   ├── parsers/
│   │   └── binary.py             # Frame parser
│   ├── storage/
│   │   ├── models.py             # Pydantic data models
│   │   └── database.py           # SQLite storage
│   ├── analysis/
│   │   ├── statistics.py         # Statistical calculations
│   │   └── comparison.py         # A/B testing
│   └── visualization/
│       └── dashboard.py          # Plotly dashboard
├── setup.py
└── tests/
    └── test_binary_parser.py
```

---

## Quick Start Guide

### Prerequisites

1. **Hardware:**
   - ESP32-S3-DevKitC-1 with LightwaveOS v2 firmware
   - SPH0645 I2S MEMS microphone
   - 320 WS2812 LEDs (dual 160-LED strips)

2. **Software:**
   - Python 3.10+
   - PlatformIO CLI
   - uv package manager (recommended)

### Step 1: Build Firmware with Validation

Add the `FEATURE_EFFECT_VALIDATION` flag to enable instrumentation:

```ini
# In platformio.ini, add a new environment or modify existing

[env:esp32dev_validation]
extends = env:esp32dev_audio
build_flags =
    ${env:esp32dev_audio.build_flags}
    -D FEATURE_EFFECT_VALIDATION=1
    -D FEATURE_AUDIO_BENCHMARK=1
```

Build and upload:
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2
pio run -e esp32dev_validation -t upload
```

### Step 2: Install Python Tooling

```bash
cd tools/benchmark

# Using uv (recommended)
uv pip install -e .

# Or using pip
pip install -e .
```

### Step 3: Run Validation Collection

```bash
# Collect 120 seconds of validation data
lwos-bench collect --run-name "baseline-test" --duration 120 --host lightwaveos.local

# View real-time dashboard
lwos-bench serve --port 8050
```

### Step 4: Analyze Results

```bash
# Compare two test runs
lwos-bench compare <run1_id> <run2_id>

# Export data for external analysis
lwos-bench export <run_id> --format csv --output results.csv
```

---

## Build Instructions

### Full Validation Build

Create or modify `platformio.ini`:

```ini
; Audio validation build with full instrumentation
; Build: pio run -e esp32dev_validation -t upload
[env:esp32dev_validation]
extends = env:esp32dev_audio
build_flags =
    ${env:esp32dev_audio.build_flags}
    ; Core validation features
    -D FEATURE_EFFECT_VALIDATION=1
    ; Optional: Enable audio benchmark for pipeline timing
    -D FEATURE_AUDIO_BENCHMARK=1
    ; Optional: Enable MabuTrace for Perfetto visualization
    ; -D FEATURE_MABUTRACE=1
    ; Debug level for validation logging
    -D CORE_DEBUG_LEVEL=3
```

### Minimal Validation Build

For resource-constrained deployments:

```ini
[env:esp32dev_validation_minimal]
extends = env:esp32dev_audio
build_flags =
    ${env:esp32dev_audio.build_flags}
    -D FEATURE_EFFECT_VALIDATION=1
    ; Reduce ring buffer size
    -D VALIDATION_RING_SIZE=64
    ; Lower transmission rate
    -D VALIDATION_DRAIN_RATE_HZ=5
```

### Build Commands

```bash
# Standard validation build
pio run -e esp32dev_validation -t upload

# Clean build (recommended for flag changes)
pio run -e esp32dev_validation -t clean && pio run -e esp32dev_validation -t upload

# Build with uploadfs for web interface
pio run -e esp32dev_validation -t uploadfs && pio run -e esp32dev_validation -t upload

# Monitor serial output
pio device monitor -b 115200
```

---

## Configuration Reference

### Firmware Configuration

**Feature Flags (`src/config/features.h`):**

| Flag | Default | Description |
|------|---------|-------------|
| `FEATURE_EFFECT_VALIDATION` | 0 | Enable effect validation instrumentation |
| `FEATURE_AUDIO_BENCHMARK` | 0 | Enable audio pipeline benchmarking |
| `FEATURE_MABUTRACE` | 0 | Enable Perfetto trace integration |

**Validation Configuration (`ValidationStreamConfig` namespace):**

| Constant | Value | Description |
|----------|-------|-------------|
| `MAGIC` | 0x4C565654 | Frame magic number ("LVVT") |
| `MAX_SAMPLES_PER_FRAME` | 16 | Max samples per WebSocket frame |
| `SAMPLE_SIZE` | 128 | Bytes per sample |
| `DEFAULT_DRAIN_RATE_HZ` | 10 | Default transmission rate |

### Python Configuration

**Environment Variables:**

| Variable | Default | Description |
|----------|---------|-------------|
| `LWOS_HOST` | lightwaveos.local | ESP32 hostname/IP |
| `LWOS_WS_PORT` | 80 | WebSocket port |
| `LWOS_DB_PATH` | ~/.lwos_benchmark.db | SQLite database path |

**CLI Options:**

```bash
# Collection options
lwos-bench collect [OPTIONS]
  --host TEXT           ESP32 hostname [default: lightwaveos.local]
  --port INTEGER        WebSocket port [default: 80]
  --run-name TEXT       Name for this test run [required]
  --duration FLOAT      Collection duration in seconds
  --max-samples INTEGER Maximum samples to collect
  --database PATH       Database file path

# Comparison options
lwos-bench compare RUN1_ID RUN2_ID [OPTIONS]
  --metric TEXT         Metric to compare [default: all]
  --output PATH         Export report file
  --format [json|csv]   Output format

# Dashboard options
lwos-bench serve [OPTIONS]
  --port INTEGER        Dashboard port [default: 8050]
  --debug              Enable debug mode
```

---

## Integration with Existing Systems

### Audio Pipeline Integration

The validation harness integrates with the existing audio pipeline through the ControlBus:

```
AudioActor::processHop()
    |
    v
ControlBus::UpdateFromHop()
    |
    +---> SnapshotBuffer<ControlBusFrame> (existing)
    |
    +---> EffectValidationRing (new, when enabled)
```

### Effect Instrumentation

Effects can be instrumented using the validation macros:

```cpp
#include "validation/EffectValidationMacros.h"

void MyEffect::render(plugins::EffectContext& ctx) {
    #ifdef FEATURE_EFFECT_VALIDATION
    VALIDATION_INIT(EFFECT_ID);
    #endif

    // ... effect rendering code ...

    #ifdef FEATURE_EFFECT_VALIDATION
    VALIDATION_PHASE(m_phase, phaseDelta);
    VALIDATION_SPEED(speedTarget, m_speedScaleSmooth);
    VALIDATION_AUDIO(m_dominantBin, m_energyAvg, m_energyDelta);
    VALIDATION_REVERSAL_CHECK(prevDelta, currDelta);
    VALIDATION_SUBMIT(&g_validationRing);
    #endif
}
```

### WebSocket Protocol Integration

The validation stream uses the same WebSocket endpoint as other LightwaveOS services:

- **Endpoint:** `ws://lightwaveos.local/ws`
- **Message Types:**
  - `validation.subscribe` - Start receiving validation frames
  - `validation.unsubscribe` - Stop receiving validation frames
- **Binary Messages:** Validation frames (identified by magic number)
- **Text Messages:** JSON control/status messages

---

## Troubleshooting

### Common Issues

**1. No validation data received:**
- Verify `FEATURE_EFFECT_VALIDATION=1` is set in build flags
- Check that the effect being tested has instrumentation macros
- Confirm WebSocket subscription was successful
- Check serial output for validation-related errors

**2. High frame loss:**
- Reduce transmission rate: `setDrainRate(5)`
- Check WiFi signal strength
- Verify no other high-bandwidth WebSocket streams active

**3. Ring buffer overflow:**
- Increase drain rate
- Increase ring buffer size: `-D VALIDATION_RING_SIZE=256`
- Reduce effect render rate if possible

**4. Incorrect metrics:**
- Verify instrumentation macros are placed correctly in effect
- Check that phase/speed variables are being updated before logging
- Confirm effect ID matches expected value

### Debug Output

Enable verbose logging in the serial monitor:

```cpp
// In platformio.ini
-D CORE_DEBUG_LEVEL=4  // Debug level
-D CONFIG_LOG_DEFAULT_LEVEL=4
```

### Validation Health Check

The firmware exposes a health endpoint:

```bash
curl http://lightwaveos.local/api/v1/validation/status
```

Response:
```json
{
    "enabled": true,
    "ring_available": 42,
    "ring_capacity": 127,
    "drain_rate_hz": 10,
    "subscribers": 1,
    "frames_sent": 1234,
    "samples_captured": 56789
}
```

---

## Performance Impact

### Memory Usage

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| EffectValidationRing | 16KB | 128 x 128-byte samples |
| ValidationFrameEncoder | 2KB | Frame buffer |
| Total | ~18KB | When enabled |

### CPU Overhead

| Phase | Time | % of Frame Budget |
|-------|------|-------------------|
| Sample creation | ~5us | 0.03% |
| Ring buffer push | ~2us | 0.01% |
| Frame encoding | ~50us | 0.3% (10 Hz) |
| WebSocket send | ~100us | 0.6% (10 Hz) |
| **Total** | ~157us | **<1%** |

### Network Bandwidth

| Scenario | Bandwidth | Notes |
|----------|-----------|-------|
| Full frame (16 samples) | ~20 KB/s | At 10 Hz |
| Typical (8 samples) | ~10 KB/s | Average |
| Minimal (1 sample) | ~1.3 KB/s | Low activity |

---

## Related Documentation

- [TEST_SCENARIOS.md](./TEST_SCENARIOS.md) - Detailed test procedures
- [METRICS_REFERENCE.md](./METRICS_REFERENCE.md) - Metric calculations and formulas
- [AUDIO_LGP_EXPECTED_OUTCOMES.md](../AUDIO_LGP_EXPECTED_OUTCOMES.md) - Expected behaviors
- [AUDIO_CONTROL_API.md](../AUDIO_CONTROL_API.md) - Audio tuning API reference

---

*End of Audio Test Harness Documentation*
