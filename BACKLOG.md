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

### [DONE] ~~Activate MabuTrace~~ -- completed 2026-02-27
- Phase 0: `TRACE_INIT(64)` in `main.cpp`, `trace` serial command, `esp32dev_audio_trace` build env
- Phase 1: 6 audio spans (i2s_dma_read, dc_agc_loop, rms_flux, tempo_update, controlbus_build, snapshot_publish)
- Phase 2: 12 renderer spans (render_frame, effect_render, zone_compose, colour_correction, pre_show_yield, show_leds, fastled_rmt_show, audio_snapshot_read + fps/frame_us counters + frame_drop/effect_change instants)
- System-wide `config/Trace.h` header, `AudioBenchmarkTrace.h` is now a redirect
- **Remaining:** Phase 3 (cross-core `TRACE_FLOW_OUT/IN` arrows) deferred to Future section

### [DONE] ~~MabuTrace capture guide~~ -- completed 2026-02-27
- 454-line guide at `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md`
- Covers: build, capture, Perfetto import, span reference, troubleshooting, licence isolation

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
