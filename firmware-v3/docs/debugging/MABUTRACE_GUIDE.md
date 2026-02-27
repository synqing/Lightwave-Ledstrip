<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2025-2026 SpectraSynq -->

# MabuTrace Capture Guide

Microsecond-resolution timeline profiling for LightwaveOS firmware, visualised
in the Perfetto UI.

---

## 1. Overview

MabuTrace records timestamped trace events (spans, counters, instants) into a
64 KB circular buffer on the ESP32-S3. Events are exported as **Chrome Trace
Format JSON**, which you paste directly into [Perfetto UI](https://ui.perfetto.dev)
for interactive exploration.

### What Perfetto shows you

| Concept | What you see |
|---------|-------------|
| **Dual-core timeline** | Core 0 (audio DSP) and Core 1 (render at 120 FPS) side by side |
| **Nested spans** | `audio_pipeline` wrapping `i2s_dma_read`, `goertzel_analyze`, etc. |
| **Cross-core handoffs** | `snapshot_publish` on Core 0 followed by `audio_snapshot_read` on Core 1 |
| **Counter graphs** | FPS, frame time, CPU load plotted over time |
| **Instant markers** | `frame_drop`, `effect_change`, `FALSE_TRIGGER` events as flags |

### When to use it

- Debugging frame drops (is the effect too slow, or is RMT blocking?)
- Diagnosing audio stalls (I2S DMA timeouts, Goertzel overruns)
- Measuring cross-core latency (audio publish to render read)
- Profiling individual effects (`effect_render` span duration)
- Identifying `pre_show_yield` dominance (known P0 issue, see `BACKLOG.md`)

---

## 2. Quick Start

### Step 1 -- Build with tracing enabled

```bash
cd firmware-v3
pio run -e esp32dev_audio_trace
```

This extends the standard `esp32dev_audio` environment with:
- `-D FEATURE_MABUTRACE=1`
- `mabuware/mabutrace` library dependency

### Step 2 -- Flash the firmware

```bash
pio run -e esp32dev_audio_trace -t upload
```

### Step 3 -- Open the serial monitor

```bash
pio device monitor -b 115200
```

You should see `MabuTrace: INITIALISED (64 KB buffer)` in the boot log.

### Step 4 -- Let the system run for 2-3 seconds

The 64 KB buffer holds approximately 2,700-4,000 events, which covers roughly
2.5 seconds of dual-core activity. Let the firmware stabilise before capturing.

### Step 5 -- Type `trace` in the serial monitor

```
trace
```

The firmware flushes the buffer and streams the JSON between markers:

```
=== MabuTrace Dump ===
Flushing trace buffer...
{"traceEvents":[{"ph":"X","name":"render_frame","ts":12345678,...}, ...]}
=== End Trace ===
Copy the JSON between markers and open at https://ui.perfetto.dev
```

### Step 6 -- Copy the JSON

Select everything between `=== MabuTrace Dump ===` and `=== End Trace ===`
(excluding those marker lines). Copy it to your clipboard.

### Step 7 -- View in Perfetto

1. Open [https://ui.perfetto.dev](https://ui.perfetto.dev)
2. Click **Open trace file** (or drag-drop)
3. Paste the JSON into a `.json` file and open it, or use **Open with copy-paste**

You should see a dual-track timeline with audio spans on one track and render
spans on another.

---

## 3. Instrumented Spans

Every span listed below exists in the current firmware. All are wrapped by the
`TRACE_*` macros defined in `src/config/Trace.h`, which delegate to MabuTrace
when `FEATURE_MABUTRACE=1` and compile to zero-cost no-ops otherwise.

### Audio pipeline (Core 0)

Source: `src/audio/AudioActor.cpp`

| Span | Type | What it measures |
|------|------|-----------------|
| `audio_pipeline` | `TRACE_SCOPE` | Full `processHop()` cycle (~20 ms hop at 12.8 kHz) |
| `i2s_dma_read` | `TRACE_BEGIN/END` | I2S DMA capture -- blocks until the DMA buffer fills |
| `dc_agc_loop` | `TRACE_BEGIN/END` | DC offset removal + automatic gain control per sample |
| `rms_flux` | `TRACE_BEGIN/END` | RMS level computation + spectral flux estimation |
| `goertzel_analyze` | `TRACE_BEGIN/END` | 8-band Goertzel frequency analysis |
| `goertzel64_fold` | `TRACE_BEGIN/END` | 64-bin Goertzel fold (conditional -- fires when 64-bin window completes) |
| `tempo_update` | `TRACE_BEGIN/END` | Beat/tempo tracking (novelty + interleaved Goertzel tempo) |
| `chroma_analyze` | `TRACE_BEGIN/END` | Chromagram analysis (12 pitch classes) |
| `controlbus_build` | `TRACE_BEGIN/END` | ControlBus frame assembly (smoothing, silence gate, style) |
| `snapshot_publish` | `TRACE_BEGIN/END` | Cross-core data publish via lock-free `SnapshotBuffer` |

### Render pipeline (Core 1)

Source: `src/core/actors/RendererActor.cpp`

| Span | Type | What it measures |
|------|------|-----------------|
| `render_frame` | `TRACE_SCOPE` | Full frame cycle (target: 8.33 ms for 120 FPS) |
| `audio_snapshot_read` | `TRACE_SCOPE` | Cross-core `ControlBusFrame` read from `SnapshotBuffer` |
| `effect_render` | `TRACE_SCOPE` | `IEffect::render()` call for the active effect |
| `zone_compose` | `TRACE_SCOPE` | Multi-zone composition (blending zone buffers) |
| `color_correction` | `TRACE_SCOPE` | LGP colour correction pass (skipped for sensitive effects) |
| `pre_show_yield` | `TRACE_SCOPE` | `vTaskDelay(1)` yield before `FastLED.show()` blocks |
| `show_leds` | `TRACE_SCOPE` | Complete LED output call (includes `showLeds()`) |

### LED driver (Core 1)

Source: `src/hal/esp32s3/LedDriver_S3.cpp`

| Span | Type | What it measures |
|------|------|-----------------|
| `fastled_rmt_show` | `TRACE_SCOPE` | RMT DMA transfer of pixel data to WS2812 strips |

### Counters

| Counter | Source | Description |
|---------|--------|------------|
| `fps` | `RendererActor.cpp:707` | Current frame rate (integer) |
| `frame_us` | `RendererActor.cpp:708` | Raw frame time in microseconds (pre-throttle) |
| `cpu_load` | `AudioActor.cpp:1552` | Audio CPU usage as integer percentage x100 (requires `FEATURE_AUDIO_BENCHMARK`) |

### Instant events

| Event | Source | When it fires |
|-------|--------|--------------|
| `FALSE_TRIGGER` | `AudioActor.cpp:1131` | Activity gate says "signal present" but all bands have near-zero energy |
| `frame_drop` | `RendererActor.cpp:1354` | Frame time exceeded 8.33 ms budget |
| `effect_change` | `RendererActor.cpp:1438` | Effect ID changed (user or auto-cycle selection) |

---

## 4. Reading the Timeline

### Identifying cores

Perfetto groups events by thread ID. On the ESP32-S3:

- **Core 0** thread: carries `audio_pipeline` and all its child spans
- **Core 1** thread: carries `render_frame` and all its child spans

Look for `audio_pipeline` to find Core 0 and `render_frame` to find Core 1.

### What a healthy timeline looks like

```
Core 0:  |--audio_pipeline--|  |--audio_pipeline--|  |--audio_pipeline--|
            ~20 ms apart           ~20 ms apart           ~20 ms apart

Core 1:  |render|render|render|render|render|render|render|render|render|
           ~8.3 ms each (120 FPS)
```

- Audio hops appear roughly every 20 ms (256 samples at 12,800 Hz).
- Render frames appear roughly every 8.33 ms (120 FPS target).
- `snapshot_publish` on Core 0 should be closely followed by
  `audio_snapshot_read` on Core 1 within 1-2 render frames.

### Common pathologies

#### Frame drops: `render_frame` exceeds 8.33 ms

Look inside the long frame to find which child span expanded:

- `effect_render` is large: the active effect's `render()` is too slow.
  Check the effect ID in the preceding `effect_change` instant.
- `color_correction` is large: the colour correction engine is thrashing.
  May indicate too many LEDs or an unoptimised correction table.
- `show_leds` / `fastled_rmt_show` is large: RMT contention or DMA
  stall. Check if another peripheral is competing for DMA channels.

#### Audio stalls: gaps between `audio_pipeline` spans

If the gap between consecutive `audio_pipeline` spans is significantly
greater than 20 ms, the audio task is being starved. Possible causes:

- Higher-priority task is monopolising Core 0 (check WiFi or network tasks).
- I2S DMA overrun (`i2s_dma_read` span is abnormally long).

#### RMT contention: `fastled_rmt_show` balloons

Normal `fastled_rmt_show` duration is approximately 9.6 ms for 320 WS2812
LEDs (30 us per LED). If it suddenly jumps to 15-20 ms, check:

- `FASTLED_RMT_MAX_CHANNELS` setting (should be 2 for dual-strip).
- Other peripherals using the RMT peripheral.

#### Cross-core latency: gap between publish and read

Measure the wall-clock gap between the end of `snapshot_publish` (Core 0) and
the start of `audio_snapshot_read` (Core 1). This should be under one render
frame (8.33 ms). If it exceeds two frames, the renderer may be running stale
audio data.

#### vTaskDelay dominance: `pre_show_yield` shows 10 ms

This is a **known issue** documented in `BACKLOG.md` as P0. The `vTaskDelay(1)`
before `FastLED.show()` yields for a full FreeRTOS tick (10 ms at default
`configTICK_RATE_HZ = 100`). This is necessary to prevent watchdog resets on
IDLE1, but it consumes most of the frame budget. The trace will show
`pre_show_yield` as the widest span inside `render_frame`.

---

## 5. Buffer Management

| Parameter | Value |
|-----------|-------|
| Default buffer size | 64 KB |
| Approximate event capacity | 2,700-4,000 events |
| Approximate time coverage | ~2.5 seconds of dual-core activity |
| Buffer type | Circular (ring buffer) |
| Overflow behaviour | Oldest events silently overwritten |

### Initialisation

The buffer is allocated in `main.cpp` during `setup()`, gated by
`FEATURE_MABUTRACE`:

```cpp
#if FEATURE_MABUTRACE
    TRACE_INIT(64);  // 64 KB circular buffer
    LW_LOGI("MabuTrace: INITIALISED (64 KB buffer)");
#endif
```

### Increasing buffer size

For longer captures, edit the `TRACE_INIT` call in
`firmware-v3/src/main.cpp:128`:

```cpp
TRACE_INIT(128);  // 128 KB -- approximately 5 seconds
```

> **Note:** The ESP32-S3 has 512 KB of SRAM. A 128 KB trace buffer consumes
> 25% of total SRAM. Monitor free heap at boot (`Boot memory: heap XXXXX bytes
> free`) to ensure sufficient headroom for the audio pipeline and effect
> rendering.

### Capture workflow

1. The buffer fills continuously from the moment `TRACE_INIT` is called.
2. Typing `trace` calls `TRACE_FLUSH()` to finalise pending events, then
   streams the buffer contents as JSON via `mabutrace_get_json_trace_chunked()`.
3. The buffer continues recording after the dump.

---

## 6. Overhead

All timings measured on ESP32-S3 at 240 MHz.

| Operation | Cost | Notes |
|-----------|------|-------|
| `TRACE_SCOPE` / `TRACE_BEGIN`+`TRACE_END` | ~0.5 us | RAII scope guard using `esp_timer_get_time()` |
| `TRACE_COUNTER` | ~0.3 us | Single timestamp + value write to ring buffer |
| `TRACE_INSTANT` | ~0.3 us | Single timestamp write to ring buffer |
| Total per audio hop (~10 spans) | ~5 us | 0.025% of 20 ms hop budget |
| Total per render frame (~7 spans + 2 counters) | ~4 us | 0.05% of 8.33 ms frame budget |
| Combined per frame | ~8-12 us | <0.15% of frame budget |

### Production builds

When `FEATURE_MABUTRACE=0` (the default for all non-trace build environments),
every `TRACE_*` macro expands to `do {} while(0)` or equivalent. The compiler
eliminates them entirely. **Zero runtime overhead. Zero binary size impact.**

---

## 7. Licence Note

MabuTrace is licensed under **GPL-3.0**. This has specific implications for how
it may be used in the project.

### Isolation strategy

| Build environment | MabuTrace included? | Distributable? |
|-------------------|---------------------|----------------|
| `esp32dev_audio` (production) | No | Yes (Apache 2.0) |
| `esp32dev_audio_trace` (dev only) | Yes (GPL-3.0) | **No** |

- MabuTrace is **only** pulled in by the `esp32dev_audio_trace` PlatformIO
  environment via `lib_deps = mabuware/mabutrace`.
- The production build (`esp32dev_audio`) never includes MabuTrace. All
  `TRACE_*` macros compile to no-ops.
- **Never distribute firmware binaries built with `esp32dev_audio_trace`.** If
  a trace-enabled binary were distributed, the entire binary would need to
  comply with GPL-3.0.

See `docs/DEPENDENCY_LICENSES.md` section "GPL-3.0 Red Flag: MabuTrace" for the
full licence audit.

---

## 8. Troubleshooting

### "No trace data" / empty output from `trace` command

| Symptom | Cause | Fix |
|---------|-------|-----|
| `MabuTrace not enabled. Build with: pio run -e esp32dev_audio_trace` | Wrong build environment | Rebuild with `-e esp32dev_audio_trace` |
| Boot log missing `MabuTrace: INITIALISED` | `TRACE_INIT` not called | Verify `FEATURE_MABUTRACE=1` in build flags |
| JSON output is `{"traceEvents":[]}` | Buffer wrapped before any events | Ensure the system has been running for at least 1 second before dumping |

### Buffer wrapped -- missing early events

The 64 KB buffer covers ~2.5 seconds. If you wait too long after boot, the
earliest events (including boot-time initialisation) will be overwritten.

**Fix:** Dump within 2 seconds of the events you want to capture. For boot
profiling, type `trace` immediately after seeing the boot log complete.

### Missing renderer spans

If you see audio spans but no `render_frame` spans:

1. Verify that `RendererActor.cpp` includes the trace header:
   ```cpp
   #include "../../audio/AudioBenchmarkTrace.h"
   ```
   (This redirects to `config/Trace.h`.)
2. Ensure the `RendererActor` task is actually running (check for `Actor
   System: RUNNING` in boot log).

### Serial output garbled or truncated

The JSON output can be large (tens of kilobytes). Ensure:

- `monitor_raw = yes` is set in `platformio.ini` (prevents PlatformIO from
  interpreting escape sequences or line-buffering the output).
- Your terminal emulator does not truncate long lines. PlatformIO's built-in
  monitor works; if using `screen` or `minicom`, increase the scrollback
  buffer.
- Baud rate matches: `115200` in both firmware (`Serial.begin(115200)`) and
  monitor (`-b 115200`).

### `cpu_load` counter shows zero

The `cpu_load` counter requires `FEATURE_AUDIO_BENCHMARK=1` in addition to
`FEATURE_MABUTRACE=1`. The `esp32dev_audio_trace` environment does not enable
benchmarking by default.

**Fix:** Add `-D FEATURE_AUDIO_BENCHMARK=1` to the trace environment's
`build_flags` in `platformio.ini`, or create a combined environment:

```ini
[env:esp32dev_audio_trace_bench]
extends = env:esp32dev_audio_trace
build_flags =
    ${env:esp32dev_audio_trace.build_flags}
    -D FEATURE_AUDIO_BENCHMARK=1
```

### Perfetto cannot parse the JSON

- Ensure you copied **only** the JSON between the `=== MabuTrace Dump ===` and
  `=== End Trace ===` markers. Do not include the marker lines or the
  instruction text.
- If the JSON appears truncated (missing closing `]}`), the serial buffer may
  have overflowed. Try reducing the trace buffer size (`TRACE_INIT(32)`) for a
  smaller dump, or increase the serial TX buffer.

---

## 9. Reference

### Source files

| File | Purpose |
|------|---------|
| `firmware-v3/src/config/Trace.h` | System-wide trace macro definitions |
| `firmware-v3/src/audio/AudioBenchmarkTrace.h` | Legacy include path (redirects to `Trace.h`) |
| `firmware-v3/src/config/features.h:234-236` | `FEATURE_MABUTRACE` default (0) |
| `firmware-v3/src/main.cpp:127-130` | `TRACE_INIT(64)` call |
| `firmware-v3/src/main.cpp:1365-1379` | `trace` serial command handler |
| `firmware-v3/platformio.ini:111-118` | `esp32dev_audio_trace` build environment |
| `firmware-v3/src/audio/AudioActor.cpp` | Audio pipeline instrumentation |
| `firmware-v3/src/core/actors/RendererActor.cpp` | Render pipeline instrumentation |
| `firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp` | `fastled_rmt_show` span |

### Adding new trace spans

To instrument a new code section:

1. Include the trace header:
   ```cpp
   #include "config/Trace.h"
   ```

2. For scoped spans (automatic end-of-scope):
   ```cpp
   void myFunction() {
       TRACE_SCOPE("my_function");
       // ... work ...
   }  // span ends here
   ```

3. For manual spans (when scope boundaries do not match):
   ```cpp
   TRACE_BEGIN("my_phase");
   // ... work ...
   TRACE_END();
   ```

4. For counters (numeric values plotted as graphs):
   ```cpp
   TRACE_COUNTER("my_metric", integerValue);
   ```

5. For instant events (point-in-time markers):
   ```cpp
   TRACE_INSTANT("my_event");
   ```

All macros are zero-cost no-ops when `FEATURE_MABUTRACE=0`.

### External resources

- [Perfetto UI](https://ui.perfetto.dev) -- trace viewer
- [Chrome Trace Format specification](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview)
- [MabuTrace repository](https://github.com/mabuware/MabuTrace) (GPL-3.0)
