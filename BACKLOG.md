# LightwaveOS Backlog

Prioritised engineering backlog. Items are tagged by category and roughly ordered by impact.

---

## Performance

### [P0] RendererActor vTaskDelay(1) costs 10 ms per frame
- **File:** `firmware-v3/src/core/actors/RendererActor.cpp:653`
- **Problem:** `vTaskDelay(1)` before `showLeds()` yields for one FreeRTOS tick (10 ms at `configTICK_RATE_HZ=100`). This alone exceeds the 8.33 ms frame budget for 120 FPS, making the target physically unreachable.
- **Root cause:** Added to prevent IDLE1 task starvation and watchdog timeout. Same pattern as the AudioActor fix (see MEMORY.md), but the renderer has different constraints.
- **Investigation needed:**
  - Can `taskYIELD()` work here? (It only yields to equal/higher priority tasks -- may not feed IDLE1)
  - Can `configTICK_RATE_HZ` be raised to 1000? (1 ms ticks instead of 10 ms)
  - Can the yield be moved AFTER `FastLED.show()` since show() itself blocks for ~2 ms (natural preemption)?
  - Event-driven architecture: wake on RMT DMA complete interrupt instead of polling
- **Impact:** Fixing this could double effective frame rate from ~60 FPS to true 120 FPS.
- **Discovered:** 2026-02-27 via MabuTrace instrumentation analysis

---

## Observability

### [P1] Activate MabuTrace -- currently dead code
- **Problem:** `TRACE_INIT()` is never called anywhere in the firmware. The circular buffer is never allocated, making every `TRACE_*` macro a silent no-op. The `trace_dump` serial command referenced in `platformio.ini` comments does not exist. The integration was wired up but never activated.
- **Phase 0 fix (1-2 hours):**
  1. Add `TRACE_INIT(64)` to `main.cpp` after serial init (gated by `FEATURE_MABUTRACE`)
  2. Add a `trace_dump` serial command that calls `TRACE_FLUSH()` then streams JSON via `get_json_trace_chunked()`
  3. Alternatively, call `mabutrace_start_server(81)` after WiFi is up for web-based capture
- **Phase 1: Audio pipeline** -- add 5 missing spans (DC/AGC, RMS/flux, ControlBus build, snapshot publish, tempo update)
- **Phase 2: Renderer pipeline** -- add 6 spans (render_frame_total, effect_render, zone_compose, color_correction, pre_show_yield, fastled_show) + FPS/frame_drop counters
- **Phase 3: Cross-core flow** -- add `TRACE_FLOW_OUT/IN` to draw Perfetto arrows from audio publish (Core 0) to renderer read (Core 1)
- **Total overhead:** ~8-12 us per frame (0.1% of budget)
- **Reference:** `firmware-v3/docs/research/EMBEDDED_TRACING_RESEARCH_2026.md`

### [P2] Create MabuTrace capture guide
- **Problem:** `docs/debugging/MABUTRACE_GUIDE.md` is referenced in code comments but never existed.
- **Action:** Write the guide covering: build with trace env, serial capture workflow, Perfetto UI import, interpreting dual-core timelines.
- **WiFi constraint:** The built-in MabuTrace web server requires WiFi. Dev constraint says "NEVER connect to ESP32 WiFi AP from dev machine." Serial dump is the primary capture path.

---

## Content

### [P3] Hero photo/GIF for README
- **Status:** Placeholder in README. Owner needs to provide a photo or GIF of the Light Guide Plate in action.

---

## Future (no urgency)

### Investigate Perfetto-compatible tracing alternatives
- MabuTrace is GPL-3.0 (dev-only, never ships -- acceptable but not ideal)
- Alternatives researched:
  - Custom Chrome JSON tracer (~300 LOC, Apache-2.0 compatible)
  - Tonbandgeraet (MIT, native Perfetto protobuf output)
  - ESP-IDF `esp_app_trace` (Apache-2.0, but outputs SystemView format, NOT Perfetto)
  - SEGGER SystemView and Percepio Tracealyzer are NOT Perfetto-compatible
- If GPL-3.0 becomes a concern, the Chrome JSON approach is simplest (~300 lines of C++)
- Reference: `firmware-v3/docs/research/EMBEDDED_TRACING_RESEARCH_2026.md`

### ControlBusFrame hot/cold split
- The ~2 KB ControlBusFrame is copied atomically across cores via SnapshotBuffer
- If cross-core contention becomes measurable, split into hot (~100 B: RMS, flux, bands) and cold (~1.9 KB: full spectrum, waveform) sub-structs with independent update rates

### MabuTrace library risk
- 7 GitHub stars, 1 fork, single maintainer (mabuware/Matthias Buhlmann)
- Core is only ~15 KB of C -- could fork or reimplement under Apache-2.0 if abandoned
- Library is feature-complete and stable for current needs
