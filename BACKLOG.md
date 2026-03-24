# LightwaveOS Backlog

Prioritised engineering backlog. Items are tagged by category and roughly ordered by impact.

---

## Performance

### [DONE] ~~RendererActor vTaskDelay(1) costs 10 ms per frame~~ — resolved in d943101a
- Original `vTaskDelay(1)` before `showLeds()` replaced with `vTaskDelay(0)` (equivalent to `taskYIELD()`)
- Pre-show delay removed entirely; renderer now self-clocked at 120 FPS via `esp_rom_delay_us` frame pacer
- FastLED.show() yields naturally via `xSemaphoreTake(gTX_sem)` during ~4.8ms RMT DMA transmission
- Watchdog fed explicitly via `esp_task_wdt_reset()` every 10 frames (no IDLE1 dependency)
- **Discovered:** 2026-02-27 | **Resolved:** d943101a (2026-02-27, stable-effect-ids integration)

### [DONE] ~~Frame pacer uses esp_rom_delay_us busy-wait~~ — resolved in 9a055687
- Replaced `esp_rom_delay_us()` CPU spin with `esp_timer` one-shot + `ulTaskNotifyTake()`
- Zero-overhead event-driven wait, falls back to `taskYIELD()` for <100us remainders
- Saves 0-2ms CPU spin per frame, yielded to IDLE1 instead
- **Discovered:** 2026-03-21 | **Resolved:** 9a055687 (2026-03-24)

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
