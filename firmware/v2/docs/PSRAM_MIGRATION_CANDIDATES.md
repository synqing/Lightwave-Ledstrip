# PSRAM Migration Candidates

Candidates for moving from internal SRAM to PSRAM to free internal RAM for WiFi/lwIP/FreeRTOS. **Do not change behaviour or protocol.** Allocations must use `heap_caps_malloc`/`heap_caps_calloc` with `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` and have a fallback or graceful degradation if PSRAM fails.

**Already in PSRAM (no action):** LogStreamBroadcaster ring buffer and slots, ESV11 DSP buffers, ZoneComposer, TransitionEngine, RendererActor capture, AudioMappingRegistry, effect large buffers per MEMORY_ALLOCATION.md §3.5.

---

## 1. WsGateway binary assembly buffers — **~33 KB internal → PSRAM**

**Where:** `WsGateway.h` — `m_binaryAssembly[CLIENT_IP_MAP_SLOTS]` (16 slots). Each `BinaryAssemblyEntry` has `uint8_t buffer[MAX_BINARY_FRAME_BUFFER]` (2048 B). Total ≈ 16 × 2048 = **32 KB** internal SRAM.

**Use:** Assembles chunked binary WebSocket frames (e.g. render.stream) per client. Not DMA; not in render path.

**Change:** Replace inline `buffer[2048]` with `uint8_t* buffer`; in ctor (or first use) allocate per slot with `heap_caps_calloc(1, MAX_BINARY_FRAME_BUFFER, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. Null-check; if PSRAM fails, either disable binary assembly for that slot or fallback to internal (document the choice). Free in dtor.

**Savings:** ~32 KB internal.

---

## 2. LedStreamBroadcaster frame buffer — **~1 KB internal → PSRAM**

**Where:** `LedStreamBroadcaster.h` — `uint8_t m_frameBuffer[LedStreamConfig::FRAME_SIZE]` (966 B). Single instance.

**Use:** Encodes LED frame for WebSocket binary broadcast. Not DMA.

**Change:** Replace member array with `uint8_t* m_frameBuffer`. In ctor allocate with `heap_caps_calloc(1, LedStreamConfig::FRAME_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. Fallback to internal or disable stream if alloc fails. Free in dtor.

**Savings:** ~1 KB internal.

---

## 3. AudioStreamBroadcaster frame buffer — **~0.5 KB internal → PSRAM**

**Where:** `AudioStreamBroadcaster.h` — `uint8_t m_frameBuffer[AudioStreamConfig::FRAME_SIZE]` (464 B). Single instance.

**Change:** Same pattern as LedStreamBroadcaster (pointer + alloc in ctor, free in dtor).

**Savings:** ~0.5 KB internal.

---

## 4. UdpStreamer LED and audio buffers — **~1.4 KB internal → PSRAM**

**Where:** `UdpStreamer.h` — `uint8_t m_ledBuffer[LedStreamConfig::FRAME_SIZE]` (966 B), `uint8_t m_audioBuffer[AudioStreamConfig::FRAME_SIZE]` (464 B).

**Change:** Replace with pointers; allocate both in ctor from PSRAM (with fallback); free in dtor.

**Savings:** ~1.4 KB internal.

---

## 5. ValidationFrameEncoder frame and sample buffers — **~2.4 KB internal → PSRAM**

**Where:** `ValidationFrameEncoder.h` — `m_frameBuffer[MAX_FRAME_SIZE]` (2052 B), `m_sampleBuffer[MAX_SAMPLES_PER_FRAME]` (16 × `EffectValidationSample`). Instance is `new ValidationFrameEncoder()` (internal heap).

**Use:** Validation stream encoding; debug/diagnostics. Not in render path.

**Change:** Replace arrays with pointers; allocate in ctor from PSRAM; free in dtor. Or allocate the whole `ValidationFrameEncoder` in PSRAM (single block) — more invasive.

**Savings:** ~2–2.5 KB internal (depending on `EffectValidationSample` size).

---

## 6. ColorCorrectionEngine LUTs — **~0.5 KB internal → PSRAM or PROGMEM**

**Where:** `ColorCorrectionEngine.h` — `static uint8_t s_gammaLUT[256]`, `static uint8_t s_srgbLinearLUT[256]` (512 B total). Initialised once, then read-only in render path.

**Change:**  
- **PSRAM:** Allocate with `heap_caps_malloc(512, MALLOC_CAP_SPIRAM)` (or two 256-byte blocks), init in `initLUTs()`, store in static pointers. Slight latency in hot path.  
- **PROGMEM:** If LUTs can live in flash, declare `static const uint8_t s_gammaLUT[256] PROGMEM` and read with `pgm_read_byte()`. No internal SRAM, no PSRAM.

**Savings:** 512 B internal.

---

## Summary (approximate)

| Candidate                         | Internal saved | Effort |
|----------------------------------|----------------|--------|
| WsGateway binary assembly        | ~32 KB         | Medium |
| LedStreamBroadcaster             | ~1 KB          | Low    |
| AudioStreamBroadcaster           | ~0.5 KB        | Low    |
| UdpStreamer                      | ~1.4 KB        | Low    |
| ValidationFrameEncoder           | ~2.4 KB        | Low    |
| ColorCorrectionEngine LUTs       | ~0.5 KB        | Low    |
| **Total**                        | **~38 KB**     |        |

Largest single win is WsGateway binary assembly (~32 KB). Streamer and validation buffers are small but straightforward. LUTs are optional (512 B) and PROGMEM is an alternative to PSRAM.

---

*Do not move to PSRAM:* LED DMA buffers (`m_leds`, FastLED/RMT), I2S DMA, FreeRTOS stacks, WiFi/lwIP/AsyncTCP buffers, anything in an ISR or DMA path. See MEMORY_ALLOCATION.md and DMA_AND_PSRAM.md.
