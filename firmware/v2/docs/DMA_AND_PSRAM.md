# DMA, RMT, and PSRAM Rules (ESP32-S3)

The ESP32-S3 has external PSRAM, but not all peripherals can access it. As a rule: **anything that a peripheral reads via DMA must live in internal SRAM**, not PSRAM.

This project uses PSRAM for large, non-DMA data (tables, history buffers, caches) to preserve internal SRAM for WiFi/lwIP/timers.

## MUST stay in internal SRAM

These buffers must remain in internal SRAM to avoid hard-to-debug corruption and crashes:

- **Renderer output buffer** used for LED driving
  - `RendererActor::m_leds[]`
- **FastLED / RMT driver buffers**
  - Per-strip buffers returned by the LED driver (`m_strip1`, `m_strip2`)
  - Any RMT/DMA staging buffers owned by the driver
- **I2S / microphone capture DMA buffers** (if used)
  - Any buffers passed to I2S drivers or DMA descriptors

## Safe candidates for PSRAM

These are safe to allocate from PSRAM (and are intentionally moved there where possible):

- Lookup tables and DSP history buffers
- Mapping registries / configuration tables
- ZoneComposer persistent render buffers
- TransitionEngine internal scratch buffers
- Debug-only capture buffers (allocated lazily)

## Performance note

PSRAM is slower than internal SRAM. It is suitable for:

- infrequently accessed tables
- sequential access patterns
- debug features

Avoid placing per-pixel inner-loop data in PSRAM unless profiling proves it is safe at 120 FPS.

