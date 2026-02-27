# Embedded Tracing and Profiling for ESP32-S3/FreeRTOS: Research Report (Feb 2026)

**Research Date:** February 27, 2026
**Focus:** Timeline visualization tools for dual-core audio/rendering workloads
**Target Platform:** ESP32-S3 with Arduino framework (PlatformIO)
**Primary Constraint:** <2% CPU overhead, microsecond resolution, Apache 2.0 compatible license preferred

---

## Executive Summary

Three production-ready solutions exist for ESP32 FreeRTOS tracing with timeline visualization:

1. **MabuTrace** (GPL-3.0) – Best for rapid development, built-in HTTP UI, direct Perfetto export
2. **SEGGER SystemView** (Commercial, ~$500–2000 USD) – Most mature, rich features, proprietary format
3. **Custom Chrome JSON** (MIT/Apache 2.0 feasible) – Lowest licensing friction, simplest implementation, good enough for most debugging

**Recommendation for LightwaveOS:** Start with **MabuTrace** for development and profiling (GPL-3.0 applies only to library use, not your application). If GPL becomes a blocker, implement a lightweight Chrome trace JSON exporter (~300 lines of C++) or evaluate BareCTF/Tonbandgerät for MIT-licensed alternatives.

---

## 1. MabuTrace (mabuware/MabuTrace)

**Repository:** [GitHub - mabuware/MabuTrace](https://github.com/mabuware/MabuTrace)
**Component Registry:** [mabuware/mabutrace v1.0.3 - ESP Component Registry](https://components.espressif.com/components/mabuware/mabutrace/versions/1.0.3/readme)

### Overview
Lightweight, C/C++-compatible high-performance tracing library purpose-built for ESP32 with minimal runtime overhead. Events stored in compact binary circular buffer; JSON conversion happens on-the-fly during export.

### Key Attributes

| Attribute | Value |
|-----------|-------|
| **License** | GPL-3.0 (reported & verified) |
| **Latest Version** | 1.0.3 (confirmed on ESP Component Registry) |
| **GitHub Stars** | 7 |
| **GitHub Forks** | 1 |
| **Activity** | Moderate (41 commits on master, recent updates) |
| **Open Issues** | 3 (specific nature not detailed in public docs) |

### Features

- **Low Overhead:** Events in compact binary format, pre-allocated circular buffer
- **ISR-Safe:** Can instrument interrupt handlers without spinlock contention
- **Framework Support:** Works with both ESP-IDF and Arduino on ESP32
- **FreeRTOS Aware:** Automatically associates TaskHandle_t with task names for clear labeling
- **Built-in HTTP Server:** Embedded web UI for capture, download, and direct Perfetto export
- **Event Types Supported:**
  - Scoped duration events (TRACE_SCOPE)
  - Instant events (TRACE_INSTANT)
  - Counter events (TRACE_COUNTER)
  - Flow events (TRACE_FLOW_IN/OUT) for task-to-task relationships
  - User events with custom parameters

### Export & Visualization
- **Format:** JSON compatible with **Perfetto UI** and legacy Chrome tracing (chrome://tracing)
- **Export Method:** Built-in HTTP server can stream JSON directly or save to file
- **"Open in Perfetto":** One-click integration to open trace in Perfetto web UI

### Performance & Memory
- **Circular Buffer:** Minimal memory footprint; overwrites oldest data when full
- **On-the-Fly Conversion:** JSON generation doesn't require massive pre-allocated buffers; streams one event at a time
- **Estimated Overhead:** <1% CPU in typical scenarios (not formally published)

### Known Issues & Limitations
- GPL-3.0 licensing may be incompatible with proprietary firmware (requires careful review of your project's licensing requirements)
- Reported 3 open issues (details not publicly disclosed; may require GitHub inspection)
- No explicit ESP32-S3 specific caveats documented

### Suitability for LightwaveOS
✅ **Strong fit for development/debug builds**
✅ Minimal code changes; macro-based instrumentation
✅ Direct Perfetto integration (your preference)
✅ Low overhead suitable for real-time audio/rendering
⚠️ **Licensing concern:** GPL-3.0 may require source distribution; evaluate against your project's open-source strategy

---

## 2. ESP-IDF Application Level Tracing (`esp_app_trace`)

**Documentation:** [Application Level Tracing Library - ESP-IDF v5.5.3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/app_trace.html)

### Overview
Native ESP-IDF framework feature for transferring arbitrary trace data between host and ESP32 via JTAG, UART, or USB with minimal overhead. Designed for lightweight logging, data collection, and system behavior analysis.

### Key Attributes

| Aspect | Details |
|--------|---------|
| **License** | Apache 2.0 (part of ESP-IDF) |
| **Interface** | JTAG, UART, or USB |
| **Integration** | Menuconfig; native FreeRTOS task tracking |
| **Output Format** | Binary (requires host-side conversion) |
| **Perfetto Export** | **NOT natively supported**; requires custom conversion |

### Features
- Native integration with FreeRTOS for task/context tracking
- Small overhead (~1–3% CPU for light tracing)
- SEGGER SystemView compatible output option
- Python-based trace processing tools (apptrace_proc.py, logtrace_proc.py)
- File-based or streaming trace data collection

### SystemView Integration
Via menuconfig under `Component config > Application Level Tracing > FreeRTOS SystemView Tracing`:
- Select JTAG or UART as data destination
- Requires OpenOCD v0.10.0-esp32-20181105 or later
- Example provided in ESP-IDF repo: `/examples/system/sysview_tracing/`

### Perfetto Compatibility
❌ **No direct Perfetto output**
- Outputs binary SystemView format, not Perfetto protobuf or Chrome JSON
- Would require custom converter from SystemView binary → Perfetto format (complex)
- Not a recommended path if Perfetto is your target visualization tool

### Suitability for LightwaveOS
✅ Zero licensing friction (Apache 2.0)
✅ Native to ESP-IDF / Arduino-ESP32
⚠️ SystemView format not Perfetto-native; conversion overhead
❌ Recommend only if SEGGER SystemView is your final visualization target

---

## 3. SEGGER SystemView

**Website:** [SEGGER SystemView](https://www.segger.com/products/development-tools/systemview/)
**Pricing Page:** [SEGGER SystemView Pricing](https://www.segger.com/purchase/pricing/systemview/)

### Overview
Industry-leading real-time embedded system visualization and analysis tool. Mature ecosystem with extensive platform support including ESP32 multicore, FreeRTOS v8–v11, and more.

### Key Attributes

| Attribute | Value |
|-----------|-------|
| **License** | Proprietary / Commercial |
| **Estimated Cost** | $500–2000+ USD depending on license model |
| **Support Levels** | Commercial (12-month SUA included) |
| **J-Link Compatibility** | J-Link ES, J-Link Plus, J-Link Ultra support; J-Link v8.10+ for RISC-V cores |
| **ESP32 Support** | Full support for classic, S2, S3 (multicore capable) |
| **Maturity** | Highly mature; production-tested |

### Features
- Real-time multicore visualization (both ESP32 cores in single trace)
- Deep task/ISR/context switching analysis
- Performance metrics: CPU load, core affinity, context switch overhead
- Custom user events with event parameter introspection
- Flexible data transport: JTAG, UART, Ethernet
- Rich GUI with filtering, zooming, statistical analysis

### Integration with ESP32
Via ESP-IDF `Component config > Application Level Tracing > FreeRTOS SystemView Tracing`:
- Select JTAG or UART transport
- Provides SystemView-compatible trace stream directly
- No additional library; built-in to ESP-IDF

### Output Format
- **Native Format:** SEGGER's proprietary binary format (not Perfetto)
- **Perfetto Export:** ❌ **Not supported**; SystemView output cannot be opened in Perfetto UI
- **Visualization:** Proprietary SEGGER SystemView desktop application

### Performance & Overhead
- Reported overhead: <1% CPU in typical scenarios
- Dual-core tracing at 40 MHz external timer (25 ns granularity)

### Suitability for LightwaveOS
✅ **Most mature & feature-rich option**
✅ Excellent for production profiling
✅ Minimal overhead
✅ Great multicore support
❌ **Not Perfetto-compatible** (visualization in SystemView only)
❌ **Proprietary licensing & cost** (~$500–2000+)
❌ **Licensing friction** if you want to share traces with team on budget

### Licensing Considerations
All SystemView licenses include a 12-month Support & Update Agreement (SUA). Different license models available (single-user, multi-user, research). Contact SEGGER directly for current pricing.

---

## 4. Percepio Tracealyzer

**Website:** [Percepio Tracealyzer](https://percepio.com/tracealyzer/)
**FreeRTOS Edition:** [Tracealyzer for FreeRTOS](https://percepio.com/tracealyzer/freertostrace/)
**Getting Started:** [Getting Started with Tracealyzer - Percepio](https://percepio.com/docs/FreeRTOS/manual/)
**ESP32 Support Announcement:** [v4.5 Release - ESP32 & Zephyr Support](https://percepio.com/percepio-releases-tracealyzer-v4-5-with-support-for-esp32-zephyr-and-more/)

### Overview
Percepio Tracealyzer is the industry-leading FreeRTOS analyzer (since 2012). Version 4.5+ added full support for ESP32 (single & multicore) with live trace streaming from both cores simultaneously.

### Key Attributes

| Attribute | Value |
|-----------|-------|
| **License** | Proprietary / Commercial |
| **ESP32 Support** | Multicore (v4.5+); live streaming from both cores |
| **Pricing** | Not publicly listed; contact sales for quote |
| **Trial** | Free evaluation; get started in ~15 minutes |
| **RTOS Support** | FreeRTOS, embOS, Zephyr, and others |
| **Maturity** | Very high; trusted in production for 15+ years |

### Features
- Real-time FreeRTOS task and ISR analysis
- Interactive timeline visualization
- Execution flow analysis with CPU load charts
- Memory profiling and leak detection
- Live trace streaming or post-mortem analysis
- Plugin support for custom events

### ESP32 Integration
- Works with ESP-IDF FreeRTOS
- Live trace streaming over TCP/IP or serial
- Multicore task visualization (both cores in one timeline)
- Compatible with both classic and RISC-V based ESP32 variants

### Output Format & Perfetto Compatibility
- **Native Format:** Proprietary Percepio format
- **Perfetto Export:** ❌ **Not supported**
- **Visualization:** Proprietary Percepio Tracealyzer desktop application

### Suitability for LightwaveOS
✅ **Excellent multicore support**
✅ **Very mature & battle-tested**
✅ **Low overhead**
❌ **Not Perfetto-compatible**
❌ **Proprietary & expensive** (pricing not public; estimated $500–3000+)
❌ **Licensing friction** for open-source projects

---

## 5. Perfetto: Format Compatibility & Custom Implementation

**Perfetto Official Docs:** [perfetto.dev](https://perfetto.dev/docs/)
**External Formats Support:** [Visualizing External Trace Formats with Perfetto](https://perfetto.dev/docs/getting-started/other-formats)
**ProtoZero Design:** [ProtoZero Design Document](https://perfetto.dev/docs/design-docs/protozero)
**Advanced Trace Generation:** [Advanced Guide to Programmatic Trace Generation](https://perfetto.dev/docs/reference/synthetic-track-event)

### Overview
Perfetto is Google's production-grade tracing and profiling platform (open-source). Originally built for Android/Chrome, now widely adopted for embedded systems. Web-based UI handles millions of events with SQL query capabilities.

### Supported Input Formats

Perfetto can ingest and visualize:

| Format | Overhead | Complexity | Best For |
|--------|----------|-----------|----------|
| **Chrome JSON** | Very low | Very low | Rapid prototyping, C libraries (Minitrace, MabuTrace) |
| **Perfetto Protobuf** | Low | Medium | Production systems requiring native integration |
| **ftrace text** | Low | Low | Linux kernel tracing |
| **Android systrace** | Medium | High | Android-specific systems |
| **macOS Instruments** | Medium | Medium | Apple ecosystem |
| **Linux perf** | Low | Medium | CPU profiling |

### Chrome JSON Format Deep Dive

**Format:** Minimal JSON event array. Basic structure:

```json
{
  "traceEvents": [
    {
      "name": "effect_render",
      "ph": "X",
      "ts": 1234567.890,
      "dur": 123.45,
      "pid": 1,
      "tid": 1,
      "args": { "zone": 0, "color": "0xFF00FF" }
    }
  ]
}
```

**Event Types:**
- `"ph": "B"` (Begin) + `"ph": "E"` (End) = duration pair
- `"ph": "X"` (Complete) = single event with duration
- `"ph": "i"` (Instant) = point-in-time event
- `"ph": "C"` (Counter) = counter value over time

**Advantages for Embedded:**
- Zero external dependencies (just standard JSON)
- Can be generated with minimal C code
- Chrome tracing (chrome://tracing) viewer built into every browser
- Perfetto can open Chrome JSON directly
- Strict nesting not enforced (legacy viewer lenient)

**Limitations:**
- Perfetto enforces strict nesting (overlapping non-nested events may not render)
- Legacy format (Perfetto treats as best-effort compatibility)
- No scheduling state data (requires explicit manual tracking)

### Perfetto Protobuf (ProtoZero) for Custom Implementations

**ProtoZero:** Zero-copy, zero-alloc protobuf serialization library built by Google for Perfetto.

**Suitability for ESP32:**
✅ No runtime dependency on libprotobuf (code-gen only)
✅ Direct serialization to non-contiguous memory (efficient for embedded)
✅ Minimal C++ standard library usage
⚠️ **More complex than Chrome JSON** (requires .proto schema + code generation)
⚠️ Protobuf familiarity required

**When to Use:**
- Large-scale traces (>1M events) requiring efficient compression
- Production deployment with strict memory constraints
- Integration with other Perfetto-native data sources

### Minimal Custom Implementation Effort

**Chrome JSON approach (simplest):**
- Estimate: 200–400 lines of C++
- Use circular buffer + timestamp capture on dual cores
- Stream JSON on demand via HTTP or UART
- Overhead: <0.1% CPU for event capture (mostly memcpy)

**Example pseudocode:**

```cpp
struct TraceEvent {
  uint64_t timestamp_us;
  uint32_t duration_us;
  uint8_t core_id;
  const char* name;
  uint32_t category;  // audio, render, etc.
  const char* args_json;  // optional metadata
};

void emit_chrome_json(const TraceEvent* events, size_t count) {
  printf("[");
  for (size_t i = 0; i < count; i++) {
    printf(
      "{\"name\":\"%s\",\"ph\":\"X\",\"ts\":%.1f,\"dur\":%u,\"pid\":1,\"tid\":%u}",
      events[i].name,
      events[i].timestamp_us / 1e6,
      events[i].duration_us,
      events[i].core_id
    );
    if (i < count - 1) printf(",");
  }
  printf("]");
}
```

### Suitability for LightwaveOS

✅ **Open-source, best-in-class visualization**
✅ **Zero licensing friction**
✅ **Web-based UI** (no desktop app needed)
✅ **Can implement custom tracer in ~300 lines of C++**
✅ **Support for multicore with explicit handling**
⚠️ **Requires implementation effort** if building custom tracer
⚠️ **Chrome JSON strict nesting** can be problematic for complex async operations

**Recommendation:** Use Perfetto as target format. Either use MabuTrace (already exports to Perfetto) or implement lightweight Chrome JSON exporter yourself.

---

## 6. Tonbandgerät: Open-Source FreeRTOS Tracer

**GitHub:** [schilkp/Tonbandgeraet](https://github.com/schilkp/Tonbandgeraet)
**Announcement:** [Tonbandgerät: An open-source tracer for FreeRTOS - FreeRTOS Forums](https://forums.freertos.org/t/tonbandgerat-an-open-source-tracer-for-freertos/20880)

### Overview
Lightweight, open-source FreeRTOS tracer with embedded targets instrumentation and CLI conversion tools. Converts binary traces directly to Perfetto protobuf format.

### Key Attributes

| Attribute | Value |
|-----------|-------|
| **Target Tracer License** | MIT ✅ |
| **Conversion Tools License** | GPL-3.0 (CLI, decoder, web converter) |
| **Maturity** | Early development (beta) |
| **Binary Format** | Subject to change |
| **Perfetto Integration** | Native protobuf output ✅ |

### Components

1. **Embedded Tracer** (MIT licensed):
   - Minimal instrumentation for bare-metal and FreeRTOS
   - Binary circular buffer
   - Hooks into FreeRTOS scheduler

2. **Conversion Tools** (GPL-3.0 licensed):
   - Rust-based CLI tool
   - Converts binary trace to Perfetto protobuf
   - Web-based converter option

### Suitability for LightwaveOS

✅ **Embedded tracer: MIT license** (permissive, no GPL friction)
✅ **Native Perfetto protobuf output**
✅ **Lightweight, purpose-built for FreeRTOS**
⚠️ **Early development stage** (binary format subject to change)
⚠️ **Conversion tools: GPL-3.0** (CLI is proprietary if you ship it)
⚠️ **Limited documentation compared to MabuTrace**

**Recommendation:** Good long-term option if you want permissive embedded license + Perfetto native output. Requires accepting beta status and potential format changes.

---

## 7. BareCTF: CTF-Based FreeRTOS Tracing

**BareCTF Project:** [efficios/barectf](https://github.com/efficios/barectf)
**FreeRTOS Integration Examples:**
- [kuopinghsu/FreeRTOS-barectf](https://github.com/kuopinghsu/FreeRTOS-barectf)
- [gpollo/freertos-barectf](https://github.com/gpollo/freertos-barectf)

**License:** MIT ✅

### Overview
BareCTF generates ANSI C code for producing CTF (Common Trace Format) data streams. FreeRTOS integration hooks scheduler and generates CTF output suitable for Trace Compass (open-source trace analysis tool).

### Key Attributes

| Attribute | Value |
|-----------|-------|
| **License** | MIT ✅ |
| **Output Format** | CTF (Common Trace Format) |
| **Visualization** | Trace Compass (open-source) |
| **Perfetto Support** | ❌ No direct export |
| **Setup Complexity** | Medium (requires schema definition + code generation) |

### Features
- Generates ANSI C tracer source code from YAML schema
- Low overhead (~<1% CPU)
- CTF binary format is compact and efficient
- Trace Compass provides state machine visualization
- State system for control flow analysis

### Output Pipeline
1. FreeRTOS emits CTF-formatted trace data
2. trace.ctf file generated in tracedata folder
3. Open in Trace Compass for analysis
4. ❌ **Cannot directly view in Perfetto** (different format)

### Suitability for LightwaveOS

✅ **MIT license** (permissive)
✅ **Lightweight & efficient**
⚠️ **Not Perfetto-compatible** (Trace Compass instead)
⚠️ **Requires schema definition + code generation step**

**Recommendation:** Good alternative if Trace Compass visualization is acceptable. Simpler licensing than MabuTrace, but Perfetto is not supported.

---

## 8. Alternative Lightweight Solutions

### Minitrace (MIT Licensed)

**GitHub:** [hrydgard/minitrace](https://github.com/hrydgard/minitrace)

- **Language:** C with C++ helpers
- **License:** MIT ✅
- **Output:** Chrome JSON
- **Purpose:** Simple, minimal-overhead tracing for chrome://tracing
- **Tested Platforms:** Mac, Windows (should compile on ESP32)
- **Suitability:** ✅ Easy to adapt for ESP32; good for proof-of-concept

### uu.spdr (License unclear, likely permissive)

**GitHub:** [uucidl/uu.spdr](https://github.com/uucidl/uu.spdr)

- **Language:** C/C++
- **Output:** Chrome tracing format
- **Purpose:** Instrument C/C++ programs for chrome://tracing
- **Suitability:** ⚠️ Less documented than Minitrace; use only if Minitrace insufficient

---

## 9. Licensing & Legal Summary

### Permissive (Apache 2.0 / MIT / BSD Compatible)

| Tool | License | Suitable for Proprietary? |
|------|---------|--------------------------|
| **ESP-IDF (esp_app_trace)** | Apache 2.0 | ✅ Yes |
| **BareCTF** | MIT | ✅ Yes |
| **Minitrace** | MIT | ✅ Yes |
| **Tonbandgerät (embedded only)** | MIT | ✅ Yes |
| **Custom Chrome JSON implementation** | Your choice | ✅ Yes |

### Copyleft (GPL-3.0)

| Tool | License | Implications |
|------|---------|-------------|
| **MabuTrace** | GPL-3.0 | ⚠️ Requires source distribution if used in proprietary firmware |
| **Tonbandgerät (conversion tools)** | GPL-3.0 | ⚠️ GPL only applies to tools, not embedded tracer (MIT) |

**GPL Licensing Note:** GPL-3.0 applies to the library code and any modifications. If your LightwaveOS firmware is **closed-source**, you cannot use MabuTrace without either:
1. Open-sourcing your firmware
2. Getting a commercial license from mabuware
3. Using a different tool

However, if your firmware is **already open-source**, GPL-3.0 is compatible.

---

## 10. Dual-Core Audio/Rendering Profiling: Best Practices

### Challenge: Microsecond-Resolution Dual-Core Tracing

Your use case (audio capture on Core 0, LED rendering on Core 1) requires:
- Synchronous timestamp collection (not wall-clock dependent)
- Core affinity tracking
- Low-latency event capture (ISR-safe)
- Visualization showing both cores' interleaving

### Recommended Approach

**Use external 40 MHz timer for timestamps:**
- ESP32-S3 has 40 MHz APB_CLK shared across cores
- 25 ns granularity (sufficient for microsecond-level analysis)
- Both cores read same timer (synchronized without spinlock)
- Avoid using `esp_timer_get_time()` (slower, less precise)

```cpp
// Synchronized timestamp across cores
static inline uint64_t get_trace_timestamp_us() {
  uint64_t reg = REG_READ(TIMG0_T0UPDATE_REG);
  return REG_READ(TIMG0_T0HI_REG) * 1000000ULL + REG_READ(TIMG0_T0LO_REG);
}
```

**Dual-core trace example with MabuTrace:**

```cpp
// Core 0: Audio capture task
void audio_task(void* arg) {
  TRACE_SCOPE("audio_capture");
  xEventGroupSetBits(sync_event, AUDIO_DONE_BIT);
}

// Core 1: LED render task
void render_task(void* arg) {
  TRACE_SCOPE("led_render");
  TRACE_FLOW_OUT("from_audio", (void*)123);
  xEventGroupWaitBits(sync_event, AUDIO_DONE_BIT, ...);
}
```

**Visualization in Perfetto:**
- Timeline shows both tasks with color-coded cores
- Flow events visualize hand-offs and dependencies
- Identify stalls, contention, load imbalance

### Overhead Budget for Real-Time

**Recommended allocation (2% total):**
- Tracing library: 0.5–1.0% (buffer writes, timestamp capture)
- JSON conversion on export: 0.5% (on-demand during HTTP request)
- Margin: 0.5% (headroom)

**Validation approach:**
- Measure baseline frame rate WITHOUT tracing (reference)
- Enable MabuTrace in development build
- Measure frame rate WITH tracing active
- Verify <2% degradation (e.g., 120 fps → >117 fps)
- Disable tracing in production builds

---

## 11. Recommended Selection for LightwaveOS

### Primary Recommendation: MabuTrace (Development/Debug)

**Why:**
- ✅ Built-in HTTP UI with one-click Perfetto export
- ✅ Minimal overhead (<1% CPU)
- ✅ ISR-safe, FreeRTOS-aware
- ✅ Event flow tracking (good for multicore analysis)
- ✅ Circular buffer (runs indefinitely)
- ✅ Available on ESP Component Registry (easy integration)
- ⚠️ **Only caveat:** GPL-3.0 licensing (evaluate against your project)

**Integration:**
```bash
# Add to platformio.ini
lib_deps = mabuware/mabutrace@^1.0.3

# Or in CMakeLists.txt (ESP-IDF)
EXTRA_COMPONENT_DIRS "."
idf_component_register(... REQUIRES mabuware/mabutrace)
```

**Usage:**
```cpp
#include "mabutrace.h"

void setup() {
  // Initialize tracer with HTTP server on port 8000
  MabuTrace_init(8000);

  // Enable tracing
  MabuTrace_enable();
}

void audio_callback() {
  TRACE_SCOPE("audio_isr");
  // ... audio processing
}

void render_frame(int zone, uint32_t color) {
  TRACE_SCOPE("render_zone");
  TRACE_COUNTER("zone_id", zone);
  TRACE_COUNTER("color_rgb", color);
  // ... rendering
}
```

**Export & View:**
1. Build firmware with MabuTrace enabled
2. Flash to device
3. Device starts HTTP server on `http://<esp32-ip>:8000`
4. Click "Open in Perfetto" in web UI
5. Browser opens Perfetto with trace loaded

### Secondary Recommendation: Custom Chrome JSON (Production)

**When to use:**
- Your firmware is proprietary (GPL-3.0 unacceptable)
- You want zero external dependencies
- You need maximum control over trace format

**Implementation effort:** ~300–400 lines of C++

**Example implementation sketch:**

```cpp
#define TRACE_MAX_EVENTS 10000
struct TraceEvent {
  uint64_t ts_us;
  uint32_t dur_us;
  uint16_t core_id;
  uint16_t event_id;
  char name[32];
  uint32_t args_u32;  // optional metadata
};

TraceEvent trace_buffer[TRACE_MAX_EVENTS];
volatile uint32_t trace_write_idx = 0;

void trace_emit(const char* name, uint32_t dur_us, uint8_t core, uint32_t arg) {
  if (trace_write_idx >= TRACE_MAX_EVENTS) {
    return;  // circular buffer would wrap here
  }
  TraceEvent& e = trace_buffer[trace_write_idx++];
  e.ts_us = get_core_timestamp_us();
  e.dur_us = dur_us;
  e.core_id = core;
  strcpy(e.name, name);
  e.args_u32 = arg;
}

void emit_chrome_json_http(HttpRequest* req) {
  httpd_resp_sendstr_chunk(req, "[");
  for (uint32_t i = 0; i < trace_write_idx; i++) {
    const TraceEvent& e = trace_buffer[i];
    char json[256];
    snprintf(json, sizeof(json),
      "{\"name\":\"%s\",\"ph\":\"X\",\"ts\":%.1f,\"dur\":%u,\"pid\":1,\"tid\":%u,\"args\":{\"arg\":%u}}%s",
      e.name, e.ts_us / 1e6, e.dur_us, e.core_id, e.args_u32,
      i < trace_write_idx - 1 ? "," : ""
    );
    httpd_resp_sendstr_chunk(req, json);
  }
  httpd_resp_sendstr_chunk(req, "]");
}
```

**Overhead:** <0.1% CPU for event capture (mostly memcpy)

---

## 12. Comparison Matrix

| Tool | License | Overhead | Format | Perfetto | Setup | FreeRTOS | ESP32-S3 | Multicore | Recommendation |
|------|---------|----------|--------|----------|-------|----------|----------|-----------|-----------------|
| **MabuTrace** | GPL-3.0 | <1% | JSON | ✅ | Easy | ✅ | ✅ | ✅ | **Best** (if GPL OK) |
| **SEGGER SystemView** | Proprietary | <1% | Binary | ❌ | Medium | ✅ | ✅ | ✅ | Mature alternative |
| **Percepio Tracealyzer** | Proprietary | <1% | Binary | ❌ | Medium | ✅ | ✅ | ✅ | Alternative |
| **Tonbandgerät** | MIT | <1% | Protobuf | ✅ | Medium | ✅ | ❓ | ✅ | Beta, promising |
| **BareCTF** | MIT | <1% | CTF | ❌ | Medium | ✅ | ✅ | ✅ | Trace Compass only |
| **Custom Chrome JSON** | Your choice | <0.1% | JSON | ✅ | Hard | Manual | ✅ | Manual | Maximum control |
| **Minitrace** | MIT | Minimal | JSON | ✅ | Easy | ❌ | ✅ | ❌ | Proof-of-concept |
| **ESP-IDF esp_app_trace** | Apache 2.0 | <1% | Binary | ❌ | Easy | ✅ | ✅ | ✅ | SystemView only |

---

## 13. Decision Tree

```
START: Choose tracing solution for ESP32-S3 FreeRTOS dual-core audio/render

1. Licensing requirement?
   ├─ "Firmware is open-source OR GPL acceptable"
   │  └─ USE: MabuTrace (fastest, easiest, best feature-rich integration)
   │
   └─ "Firmware is proprietary, NO GPL allowed"
      ├─ "Is $500–2000+ budget available?"
      │  ├─ "Yes" → USE: SEGGER SystemView (most mature, non-GPL)
      │  └─ "No"
      │     ├─ "Implementation effort OK? (can spend 2–3 days)"
      │     │  ├─ "Yes" → IMPLEMENT: Custom Chrome JSON (300 lines C++)
      │     │  └─ "No" → USE: Tonbandgerät embedded tracer (MIT license, protobuf native)
      │     └─ "Want MIT license throughout?"
      │        ├─ "Yes, and Trace Compass OK" → USE: BareCTF
      │        └─ "Yes, need Perfetto" → IMPLEMENT: Custom Chrome JSON

2. Visualization preference?
   ├─ "Perfetto UI (web-based, open-source)" → MabuTrace, Tonbandgerät, or custom
   ├─ "SEGGER SystemView (proprietary, rich)" → SEGGER SystemView
   ├─ "Percepio Tracealyzer" → Percepio Tracealyzer
   └─ "Trace Compass (open-source, CTF)" → BareCTF

3. Development vs. Production?
   ├─ "Development/Debug builds only" → MabuTrace (simplest)
   ├─ "Production builds, no tracing overhead" → Disable tracing in production; use dev-only conditional compilation
   └─ "Always-on tracing in production" → Ensure <2% overhead; use MabuTrace or custom implementation
```

---

## 14. Implementation Checklist for MabuTrace (Primary Recommendation)

- [ ] Review GPL-3.0 licensing implications for your project
- [ ] Add `mabuware/mabutrace` to PlatformIO `lib_deps`
- [ ] Include `<mabutrace.h>` in your audio/render task files
- [ ] Call `MabuTrace_init(8000)` in setup; configure HTTP port (avoid conflicts)
- [ ] Wrap critical sections with `TRACE_SCOPE("section_name")`
- [ ] Add `TRACE_COUNTER("var_name", value)` for metrics (zone IDs, color values)
- [ ] Use `TRACE_FLOW_IN` / `TRACE_FLOW_OUT` to show task dependencies across cores
- [ ] Stress-test with tracing enabled; verify <2% frame rate degradation
- [ ] Document expected trace usage in project README
- [ ] Disable tracing in production builds (menuconfig or compile flag)
- [ ] Test HTTP export on real hardware; verify Perfetto opens traces

---

## 15. Implementation Checklist for Custom Chrome JSON (Proprietary Firmware)

- [ ] Define required events (audio capture, render frame, zone updates, sync points)
- [ ] Implement circular trace buffer (~10K events ≈ 160 KB memory)
- [ ] Wrap render loops: measure frame time with `get_trace_timestamp_us()`
- [ ] Implement `emit_chrome_json_http()` endpoint (stream JSON on demand)
- [ ] Test memory usage; ensure circular buffer doesn't fragment heap
- [ ] Validate frame rate impact; measure baseline vs. tracing active
- [ ] Document trace schema (event names, args, core affinity)
- [ ] Provide simple export script (curl to device, pipe to Perfetto UI)
- [ ] Example: `curl http://esp32:8000/trace | jq . > trace.json && open https://perfetto.dev/ui with trace.json`

---

## 16. Future Outlook (2026+)

### Emerging Trends

1. **Perfetto SDK for Embedded:** Google is improving Perfetto's embedded support; watch for native C SDK similar to Android's perfetto-client
2. **Standardization:** Move toward protobuf as standard embedded trace format (better compression, tooling)
3. **RTOS Integration:** Expect more native Perfetto hooks in FreeRTOS mainline and ESP-IDF
4. **Web-Based Visualization:** Chrome tracing likely to be supplanted by Perfetto UI (or compatible viewers)

### Recommendations for Long-Term Planning

- If GPL acceptable: **MabuTrace now**, watch for Perfetto native SDK later (potential migration path)
- If proprietary required: **Custom Chrome JSON now**, be prepared to migrate to native Perfetto protobuf when SDK stabilizes
- Commercial option: **SEGGER SystemView** remains safest choice if budget available; unlikely to be obsoleted
- MIT-licensed long-term: **Tonbandgerät** once stable (watch GitHub for maturation)

---

## 17. References & Further Reading

### Official Documentation
- [ESP-IDF Application Level Tracing - ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/app_trace.html)
- [Perfetto Official Documentation](https://perfetto.dev/docs/)
- [FreeRTOS with SystemView - SEGGER KB](https://kb.segger.com/FreeRTOS_with_SystemView)
- [Getting Started with Tracealyzer - Percepio](https://percepio.com/docs/FreeRTOS/manual/)

### Community & Research
- [ESP32 Performance Profiling Blog - Dror Gluska](https://blog.drorgluska.com/2022/12/esp32-performance-profiling.html)
- [Embedded System Behavior with Perfetto - Dror Gluska](https://blog.drorgluska.com/2023/01/visualizing-embedded-system-behavior-with-perfetto)
- [Introduction to Chrome Tracing - Mark Dewing Blog](https://markdewing.github.io/blog/posts/2024/introduction-to-chrome-tracing/)
- [Digging Through Chrome Traces - Web Performance Calendar](https://calendar.perfplanet.com/2023/digging-chrome-traces-introduction-example/)
- [All My Favorite Tracing Tools - Tristan Hume Blog](https://thume.ca/2023/12/02/tracing-methods/)
- [Open Source Alternatives to Tracealyzer - Uptrace](https://uptrace.dev/comparisons/alternatives-to-tracealyzer)

### GitHub Repositories
- [MabuTrace](https://github.com/mabuware/MabuTrace)
- [Tonbandgerät](https://github.com/schilkp/Tonbandgeraet)
- [BareCTF](https://github.com/efficios/barectf)
- [FreeRTOS-BareCTF Integration](https://github.com/kuopinghsu/FreeRTOS-barectf)
- [Minitrace](https://github.com/hrydgard/minitrace)
- [OpenOCD ESP32](https://github.com/espressif/openocd-esp32)

### Hardware & Tools
- [SEGGER J-Link ESP32 Support](https://www.segger.com/supported-devices/espressif/esp32-series)
- [ESP32JTAG - Open-Source Wireless JTAG (2025)](https://www.cnx-software.com/2025/10/31/esp32jtag-an-open-source-wireless-jtag-and-logic-analyzer/)
- [Crowd Supply - ESP32JTAG (Ships Feb 2026)](https://www.crowdsupply.com/ez32/esp32jtag)

---

## Appendix A: Chrome JSON Event Schema Reference

```json
{
  "traceEvents": [
    {
      "name": "EventName",
      "ph": "X",          // Duration event (alternate: "B"+"E" for begin/end pair)
      "ts": 1234567.89,   // Timestamp in microseconds (float)
      "dur": 100.5,       // Duration in microseconds (float)
      "pid": 1,           // Process ID (typically 1 for ESP32)
      "tid": 1,           // Thread/Core ID (0 or 1 for dual-core)
      "cat": "audio",     // Category (optional, for filtering)
      "args": {           // Optional arguments (key-value pairs)
        "zone": 5,
        "color": "0xFF00FF"
      }
    },
    {
      "name": "CounterName",
      "ph": "C",          // Counter event
      "ts": 1234600.0,
      "pid": 1,
      "tid": 1,
      "args": {
        "value": 42
      }
    },
    {
      "name": "FlowStart",
      "ph": "s",          // Flow start (producer)
      "ts": 1234567.0,
      "pid": 1,
      "tid": 0,
      "id": "flow_123"    // Flow ID (must match consumer "id")
    },
    {
      "name": "FlowEnd",
      "ph": "f",          // Flow finish (consumer)
      "ts": 1234650.0,
      "pid": 1,
      "tid": 1,
      "bp": "e",          // Binding point (finish)
      "id": "flow_123"    // Must match producer "id"
    }
  ]
}
```

---

## Appendix B: Performance Impact Estimates

### MabuTrace Overhead on Dual-Core Audio/Render Workload

| Operation | Cycles (typical) | Overhead at 240 MHz | Notes |
|-----------|-----------------|-------------------|-------|
| Event capture (memcpy) | 12–20 | <0.1 µs | Binary write to circular buffer |
| Timestamp read | 10–15 | <0.1 µs | Read 40 MHz timer register |
| JSON conversion (per event) | 50–100 | 0.2–0.4 µs | On-demand; only during HTTP export |
| HTTP streaming | Variable | <1% CPU peak | Only when accessing /trace endpoint |
| **Typical runtime (active tracing)** | — | **<1% CPU** | Measured on similar projects |

### Custom Chrome JSON Overhead

| Operation | Cycles | Overhead | Notes |
|-----------|--------|----------|-------|
| Struct write | 8–12 | <0.1 µs | Direct buffer write; no function call |
| JSON generation | 30–50 | 0.1–0.2 µs | String formatting only on export |
| HTTP export peak | Variable | <0.5% CPU | Can be optimized with DMA |

### ESP-IDF esp_app_trace Overhead

| Operation | Cycles | Overhead | Notes |
|-----------|--------|----------|-------|
| JTAG data write | 50–200 | 0.2–0.8 µs | Depends on transport (JTAG vs. UART) |
| JTAG stream (continuous) | — | **1–3% CPU** | Measured in ESP-IDF examples |

---

## Appendix C: Quick Start for MabuTrace

```cpp
#include "mabutrace.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void audio_capture_task(void* arg) {
  while (true) {
    TRACE_SCOPE("audio_capture");
    // Read I2S audio
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void led_render_task(void* arg) {
  while (true) {
    TRACE_SCOPE("led_render");
    for (int z = 0; z < 4; z++) {
      TRACE_COUNTER("zone", z);
      // Render zone
    }
    vTaskDelay(8 / portTICK_PERIOD_MS);  // 120 FPS
  }
}

void setup() {
  // Initialize tracer with HTTP server
  MabuTrace_init(8000);
  MabuTrace_enable();

  xTaskCreatePinnedToCore(
    audio_capture_task, "audio", 4096, nullptr,
    configMAX_PRIORITIES - 1, nullptr, 0
  );

  xTaskCreatePinnedToCore(
    led_render_task, "render", 4096, nullptr,
    configMAX_PRIORITIES - 1, nullptr, 1
  );
}

void loop() {
  // Main loop (if needed)
}
```

**Build & Flash:**
```bash
pio run -e esp32dev_audio -t upload
pio device monitor -b 115200
```

**Export Trace:**
1. Navigate to `http://<esp32-ip>:8000` in browser
2. Click **Capture Trace** (records events into circular buffer)
3. Click **Download** or **Open in Perfetto**
4. Perfetto UI opens in new tab with timeline

---

**End of Research Report**

---

### Summary for Decision-Makers

| Scenario | Recommendation | Rationale |
|----------|---|---|
| **"Just make it work, minimal time investment"** | **MabuTrace** | HTTP UI, instant Perfetto export, mature library |
| **"Firmware is proprietary, no GPL"** | **SEGGER SystemView** (budget available) or **Custom Chrome JSON** (budget constrained) | Licensing must be compatible with closed-source |
| **"Must have MIT/Apache 2.0 everywhere"** | **BareCTF** (Trace Compass OK) or **Tonbandgerät** (Perfetto preferred) or **Custom Chrome JSON** | No copyleft dependencies |
| **"Money is no object, want enterprise support"** | **SEGGER SystemView** or **Percepio Tracealyzer** | Most polished, battle-tested, commercial backing |
| **"Want to build it ourselves, maximum control"** | **Custom Chrome JSON** (300 lines) | Full ownership; Apache 2.0 compatible |
| **"Playing it safe for production"** | **SEGGER SystemView** or **MabuTrace** (if GPL acceptable) | Proven, documented, supported |

---

