# TRACE Instrumentation Spec — Section 5: Memory/Heap + Power/Thermal Health Surface

**Status:** Draft
**Target subsystems:** Heap fragmentation, PSRAM availability, stack high-water marks, die temperature
**Sampling frequency:** 1 Hz (low-frequency background task ONLY)
**Confidence level:** High (infrastructure exists; thermal sensor API requires IDF version check)

---

## Critical Safety Constraint

**From `feedback_no_heap_scans_in_high_freq_paths.md`:**

> `heap_caps_get_largest_free_block()` and similar functions contend on the heap mutex with Core 1 renderer. Caused d14 Step 3 stutter+corruption.

**ENFORCEMENT:**
- NEVER call heap query functions from the render path (Core 1, high-frequency)
- NEVER call them from the audio hop path (Core 0)
- ONLY call from the 1 Hz health/watchdog task (identified below)
- All memory/thermal counters in this surface fire exclusively from background task context
- Document this constraint prominently in any instrumentation PR

---

## Where the 1 Hz Sampling Task Lives

### Existing Infrastructure: `main.cpp` + `StackMonitor::checkAllTasks()`

**File:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/main.cpp`
**Line:** ~450–454 (periodic system health checks in Arduino `loop()`)

```cpp
// Periodic system health checks every 10 seconds
if ((loopCount++ % 50) == 0) {  // 50 iterations of 200 ms loop = 10 s
    lightwaveos::core::system::StackMonitor::checkAllTasks();
    // ... other periodic checks
    esp_task_wdt_reset();  // Feed watchdog
}
```

**Current behavior:**
- Main Arduino `loop()` runs on the low-priority `loopTask` (not Core 0 or Core 1 renderer)
- Already calls `StackMonitor::checkAllTasks()` at ~10 s intervals
- Safe location for heap sampling (no mutex contention with render path)

**Recommendation:**
- Add a sub-task within the 10 s health check to sample heap every second via a local 1 Hz counter
- Alternative: Create a dedicated `FreeRTOS` task at priority 1–2 (below renderer, above idle) if finer timing control is needed
- Current approach is preferred: reuse existing 10 s loop with 1 s sub-counter

**Task names in actor system (for stack HWM sampling):**
- `loopTask` (Arduino, Core 0, low priority)
- `rendererActor` (Core 1, high priority, do NOT sample from here)
- `audioActor` (Core 0, do NOT sample from here)
- `ShowDirectorActor` (referenced in main.cpp, background priority)
- Additional actors identified in `core/actors/` directory

---

## ESP-IDF Compatibility for Temperature Sensor

### Current Build Configuration
- **Platform:** espressif32@6.9.0
- **Board:** ESP32-S3 N16R8 (DevKit C1)
- **Framework:** Arduino (framework-arduinoespressif32 v3.20017.241212 per implicit platformio.ini)
- **ESP-IDF implied version:** ~5.2.x (shipped with Espressif32 6.9.0)

### Temperature Sensor API Status

**Function:** `temp_sensor_read_celsius(float* celsius)`

**Include path:** `<driver/temperature_sensor.h>` (ESP-IDF v5.0+)

**Availability:**
- ✅ Present in ESP-IDF 5.2.x (current build)
- ✅ Available in Arduino-ESP32 v3.x integration layer
- Requires: `#include <driver/temperature_sensor.h>`

**Initialization requirement:**
```cpp
// Inside system startup (SystemInit.cpp or HeapMonitor.init()):
// Modern ESP-IDF requires a call to initialize the temperature sensor
// Check if temp_sensor_init() is exposed; older IDF versions may use
// nvs_flash + calibration data automatically.
```

**Known issues for ESP32-S3:**
- ESP32-S3 includes an integrated die temperature sensor (single-point measurement)
- Accuracy: ~±1 °C typical, adequate for thermal throttle warnings
- Slow response: ~50–100 ms characteristic time constant (coarse for real-time throttle detection)
- **No** external thermal sensor interface in this design

**API stability:**
- Function signature stable since ESP-IDF 5.0
- Arduino-ESP32 3.x wraps it without modification
- Safe to use for 1 Hz sampling

---

## Instrumentation Points

### Tier 1 — Always-On Heap Counters (Sampled at 1 Hz)

| Name | Type | Tier | File:Line | Args | Cost | Hypothesis | Sanity |
|------|------|------|-----------|------|------|-----------|--------|
| `heap_free_internal_kb` | Gauge | 1 | `HeapMonitor.cpp:~450` | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024` | 2–5 μs (mutex-guarded) | Heap exhaustion over long soak; effect transitions cause spikes | Poll at 1 Hz from background task only |
| `heap_free_psram_kb` | Gauge | 1 | `HeapMonitor.cpp:~450` | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024` | 2–5 μs | PSRAM pressure under plugin load; correlation with plugin manager transitions | Verify PSRAM availability on board (8 MB N16R8) |
| `heap_largest_internal_kb` | Gauge | 1 | `HeapMonitor.cpp:~450` | `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024` | 3–7 μs | Fragmentation tracking; identifies when largest contiguous block shrinks | Critical for long-running systems; compare with total free for frag % |
| `heap_largest_psram_kb` | Gauge | 1 | `HeapMonitor.cpp:~450` | `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024` | 3–7 μs | PSRAM fragmentation; can starve large audio buffers if fragmented | Use with `heap_free_psram_kb` to compute PSRAM fragmentation % |

**Implementation location:** Within the 10 s health check block in `main.cpp:~450`, wrap a 1 Hz sub-counter:

```cpp
// In main.cpp loop():
static uint32_t heap_sample_counter = 0;
if ((loopCount++ % 50) == 0) {  // 10 s check
    if ((heap_sample_counter++ % 50) == 0) {  // 1 s sub-sample
        // Heap sampling here (safe from render mutex)
        uint32_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
        // ... emit TRACE_* macro ...
    }
    StackMonitor::checkAllTasks();
    esp_task_wdt_reset();
}
```

---

### Tier 1 — Stack High-Water Marks (Sampled at 1 Hz)

| Task Name | Type | Tier | File:Line | Args | Cost | Hypothesis | Sanity |
|-----------|------|------|-----------|------|------|-----------|--------|
| `task_stack_hwm_loop` | Gauge | 1 | `main.cpp:~450` | `uxTaskGetStackHighWaterMark(xTaskGetCurrentTaskHandle())` | <1 μs (no kernel call) | Arduino loop stack creep; identify runaway stack allocations | Called from loopTask itself; safe |
| `task_stack_hwm_renderer` | Gauge | 1 | `main.cpp:~450` | `uxTaskGetStackHighWaterMark(g_rendererTaskHandle)` | <1 μs | Effect recursion or effect-specific stack bloat; detect early before overflow hook fires | Store task handle in global from rendererActor init |
| `task_stack_hwm_audio` | Gauge | 1 | `main.cpp:~450` | `uxTaskGetStackHighWaterMark(g_audioTaskHandle)` | <1 μs | Audio pipeline DSP stack depth; correlation with beat tracking complexity | Store task handle in global from audio init |
| `task_stack_hwm_show_director` | Gauge | 1 | `main.cpp:~450` | `uxTaskGetStackHighWaterMark(g_showDirectorHandle)` | <1 μs | Transition animation recursion or plugin loading state machine | Store task handle from ShowDirectorActor::init() |

**Obtaining task handles:**
- **loopTask:** `xTaskGetCurrentTaskHandle()` when called from `loop()`
- **rendererActor, audioActor, ShowDirectorActor:** Store `TaskHandle_t` as module-level globals in each actor's init function; expose via getter or direct inspection in `main.cpp`

**Example pattern:**
```cpp
// In RendererActor.cpp:
static TaskHandle_t g_rendererTaskHandle = nullptr;
void RendererActor::init() {
    g_rendererTaskHandle = xTaskGetCurrentTaskHandle();
    // ... rest of init
}

// In main.cpp (via extern):
extern TaskHandle_t g_rendererTaskHandle;
if ((heap_sample_counter++ % 50) == 0) {
    uint32_t hwm = uxTaskGetStackHighWaterMark(g_rendererTaskHandle);
    // emit TRACE_* macro
}
```

---

### Tier 1 — Temperature Sensor (Sampled at 1 Hz)

| Name | Type | Tier | File:Line | Args | Cost | Hypothesis | Sanity |
|------|------|------|-----------|------|------|-----------|--------|
| `temp_celsius_x10` | Gauge | 1 | `main.cpp:~450` | `temp_sensor_read_celsius(&temp_c) * 10.0f` (fixed-point as uint16_t) | 50–100 μs (coarse sensor response) | Thermal throttle detection; correlation with sustained render load | 1 Hz sampling is adequate for ~100 ms sensor response time; throttle events are slow |

**Implementation:**
```cpp
#include <driver/temperature_sensor.h>

// In main.cpp loop():
float die_temp_c = 0.0f;
if (temp_sensor_read_celsius(&die_temp_c) == ESP_OK) {
    uint16_t temp_x10 = (uint16_t)(die_temp_c * 10.0f);  // Fixed-point: 75.3 °C -> 753
    // emit TRACE_* macro with temp_x10
}
```

**Initialization (one-time in SystemInit.cpp or HeapMonitor::init):**
```cpp
// Modern ESP-IDF 5.2+ may auto-initialize; verify by attempting a read.
// If ESP_ERR_INVALID_STATE is returned, manual init may be required.
// Check framework/cores/esp32/esp_temperatureRead.cpp in Arduino-ESP32.
```

---

### Tier 4 — Threshold/Regime Change Events

| Name | Type | Tier | File:Line | Args | Threshold | Hypothesis | Sanity |
|------|------|------|-----------|------|-----------|-----------|---------|
| `heap_alloc_failure_internal` | Event (instant) | 4 | `HeapMonitor.cpp:160` | `requested_size_bytes` | On malloc failure | Gap: Code may not wrap all allocations with error checking | Document via code audit |
| `heap_alloc_failure_psram` | Event (instant) | 4 | `HeapMonitor.cpp:160` | `requested_size_bytes` | On malloc failure | Gap: PSRAM allocation failures not currently instrumented | Recommend adding SPIRAM-specific wrapper |
| `oom_warning` | Event (instant) | 4 | `main.cpp:~455` | None | `heap_free_internal < 20 KB` | Internal heap pressure; may precede malloc failures by 100–200 ms | Emit from 1 Hz check when free < threshold; configure threshold per board |
| `psram_pressure_warn` | Event (instant) | 4 | `main.cpp:~455` | None | `heap_free_psram < 256 KB` | PSRAM fragmentation or plugin load exceeds available space | Emit from 1 Hz check; adjust threshold based on effect workloads |
| `thermal_throttle_warn` | Event (instant) | 4 | `main.cpp:~455` | `temp_celsius_x10` | `temp_c >= 80.0` | System thermal limit approached; may trigger OS frequency scaling | Emit from 1 Hz check; ESP32-S3 typically throttles at 85–90 °C |

**Implementation location:** Same 1 Hz loop block in `main.cpp:~450`:
```cpp
if ((heap_sample_counter++ % 50) == 0) {
    // Thresholds
    const uint32_t OOM_THRESHOLD_KB = 20;
    const uint32_t PSRAM_THRESHOLD_KB = 256;
    const float THERMAL_THRESHOLD_C = 80.0f;

    uint32_t free_int_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    if (free_int_kb < OOM_THRESHOLD_KB) {
        // TRACE_OOM_WARNING(free_int_kb);  // Emit instant event
    }

    float temp_c = 0.0f;
    if (temp_sensor_read_celsius(&temp_c) == ESP_OK) {
        if (temp_c >= THERMAL_THRESHOLD_C) {
            // TRACE_THERMAL_THROTTLE_WARN((uint16_t)(temp_c * 10.0f));
        }
    }
}
```

---

## Integration Checklist

- [ ] Verify `HeapMonitor::getFreeHeap()` calls use `MALLOC_CAP_INTERNAL` + `MALLOC_CAP_SPIRAM` separately
- [ ] Confirm `StackMonitor` stores task handles for renderer, audio, show_director at init time
- [ ] Add temperature sensor header include to SystemInit or main.cpp; verify ESP-IDF 5.2 availability
- [ ] Add 1 Hz sub-counter to the 10 s health check loop in `main.cpp`
- [ ] Define threshold constants for OOM, PSRAM pressure, thermal warnings
- [ ] Document that all heap/thermal sampling is gated to 1 Hz background task (never render path)
- [ ] Add unit test for heap-sampling code path (heap queries do not trigger memory exceptions)
- [ ] Update TRACE_* macro definitions to include memory/thermal counter types

---

## References

- **HeapMonitor implementation:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/system/HeapMonitor.h/cpp`
- **StackMonitor implementation:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/core/system/StackMonitor.h`
- **Main loop health checks:** `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3/src/main.cpp:~450–481`
- **Feedback on heap contention:** `feedback_no_heap_scans_in_high_freq_paths.md`
- **ESP-IDF temperature sensor:** `driver/temperature_sensor.h` (ESP-IDF 5.2.x)

---

**Document version:** 05-draft-1
**Last updated:** 2026-04-27
**Token budget used:** ~18K
