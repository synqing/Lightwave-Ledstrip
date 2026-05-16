---
abstract: "Source-anchored first-pass map for BACKLOG WB-2: K1v2 ESP32-S3 LED transport from LedDriver_S3 through FastLED RMT4 and the project wire-time fence."
---

# WB-2 FastLED/RMT Transport Map - 2026-05-16

## RBDO Label

GROUNDED.

## Scope

This is the first read-only source map for `BACKLOG.md` WB-2. It maps the active K1v2 ESP32-S3 LED output path far enough to separate project-owned preparation, FastLED RMT4 dispatch, RMT transmission, and the project-owned wire-time fence.

This is not a claim that FastLED is broken. It does not change LED output behaviour, weaken the fence, alter FastLED build flags, or propose a driver rewrite.

## Active Transport Path

```text
RendererActor output buffers
  -> LedDriver_S3::m_strip1 / m_strip2
  -> LedDriver_S3::syncBuffersToFastLED()
  -> m_txStrip1 / m_txStrip2
  -> FastLED.addLeds<WS2812, GPIO, GRB>()
  -> FastLED.show()
  -> FastLED ESP32 RMT4 ClocklessController
  -> ESP32RMTController::showPixels()
  -> RMT register start / RMT ISR refill
  -> ESP32RMTController::doneOnChannel()
  -> project-owned post-show kWireTimeUs delay
```

## Source Map

| Layer | Current source reality | Blocking / return semantics visible in source | Source anchors |
|---|---|---|---|
| HAL driver selection | ESP32-S3 aliases `hal::LedDriver` to `LedDriver_S3`. | Compile-time selection, no runtime transport switch. | `HalFactory.h:19-33`; `ILedDriver.h:43-49`. |
| Production K1v2 FastLED flags | The production S3 build uses FastLED custom RMT driver, not ESP-IDF builtin RMT driver; it caps FastLED's internal GTX semaphore wait and exposes four RMT channel indices for two strips with two memory blocks. | `FASTLED_RMT_BUILTIN_DRIVER=0`; `FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM=100`; `FASTLED_RMT_MAX_CHANNELS=4`. | `platformio.ini:71-87`; `platformio.ini:1318-1323`; `platformio.ini:1357-1361`. |
| Project TX buffers | `LedDriver_S3` owns authoring buffers `m_strip1/m_strip2` and FastLED TX buffers `m_txStrip1/m_txStrip2`. | Project copies into TX buffers before calling FastLED, so buffer-reuse safety depends on the project fence after TX starts. | `LedDriver_S3.h:59-71`; `LedDriver_S3.cpp:130-143`. |
| Project pre-show guards | The driver uses a 2 ms mutex take; failure increments `showSkips` and `ledShowFailures`. It also enforces a 250 us minimum gap from last show end. | Mutex failure drops the frame before FastLED/RMT dispatch. Minimum gap waits, not drops. | `LedDriver_S3.cpp:147-160`; `ILedDriver.h:31-39`. |
| Core-affinity assertion | `FastLED.show()` is asserted to run only on Core 1. | Cross-core calls are treated as fatal misuse, not a recoverable transport condition. | `LedDriver_S3.cpp:163-165`. |
| FastLED controller registration | Single strip registers one controller; dual strip registers two WS2812/GRB controllers against the TX buffers. | Controllers are created by `FastLED.addLeds`; the project does not directly call ESP-IDF RMT APIs in the S3 path. | `LedDriver_S3.cpp:48-54`; `LedDriver_S3.cpp:89-97`. |
| FastLED RMT4 overlay selection | FastLED's ESP32 clockless path includes RMT4 when `FASTLED_RMT5` is false. RMT4's default is custom direct-register driver unless `FASTLED_RMT_BUILTIN_DRIVER` is defined true. | Current build flags force the custom/direct-register branch. | `clockless_rmt_esp32.h:35-40`; `idf4_clockless_rmt_esp32.h:47-50`; `idf4_clockless_rmt_esp32.h:73-106`. |
| FastLED RMT global completion semaphore | FastLED RMT4 has global `gTX_sem`; comments state it is not given until all data has been sent. | The source intends `gTX_sem` to represent all controllers completed, but K1 source still fences because hardware showed return/tearing risk. | `idf4_rmt_impl.cpp:208-217`; `idf4_rmt_impl.cpp:399-404`; `idf4_rmt_impl.cpp:791-811`. |
| FastLED RMT dispatch | Last controller call takes `gTX_sem`, waits the FastLED minimum wait, starts available RMT channels, and returns from the custom-driver path after starting transmission rather than after a visible project wait in that function. | The actual TX complete path is ISR-driven through `doneOnChannel`; project source does not rely on that alone. | `idf4_rmt_impl.cpp:422-460`; `idf4_rmt_impl.cpp:520-545`; `idf4_rmt_impl.cpp:548-664`; `idf4_rmt_impl.cpp:757-811`. |
| Project wire-time fence | `LedDriver_S3::show()` records `m_lastShowStartUs`, calls `FastLED.show()`, then delays `kWireTimeUs` (`5600 us`). `syncBuffersToFastLED()` also delays until `kWireTimeUs` since the previous show start before reusing TX buffers. | Project-owned fence is the safety boundary protecting TX buffer reuse and visible output integrity. Do not remove without hardware instrumentation. | `LedDriver_S3.cpp:171-184`; `LedDriver_S3.cpp:130-143`; `LedDriver_S3.h:70-73`. |
| `isShowInProgress()` meaning | Interface says S3 patched RMT4 wire time may continue after this reads false. `LedDriver_S3` clears it after `FastLED.show()` returns and the project fence has elapsed. | It is a rough software-scope indicator, not a hardware TX-complete proof. | `ILedDriver.h:173-179`; `LedDriver_S3.cpp:169-185`. |

## Timing Boundary

```text
CPU output prep:
  m_strip* -> m_txStrip* memcpy

FastLED/RMT start:
  FastLED.show()
  FastLED RMT4 starts controller(s), drives RMT register tx_start, and completes via ISR callback.

Project safety fence:
  esp_rom_delay_us(kWireTimeUs) after FastLED.show()
  syncBuffersToFastLED() also refuses TX-buffer reuse until kWireTimeUs has elapsed since m_lastShowStartUs.

Observed status fields:
  led_show avg/max includes project guard time as measured by LedDriver_S3::show().
  showSkips counts mutex-timeout skips before FastLED/RMT dispatch.
```

## Unproven / Needs Hardware Instrumentation

The source map does not prove exact physical TX-start/TX-complete timing on K1v2. The remaining proof requires hardware instrumentation, for example:

1. GPIO markers around `syncBuffersToFastLED()`, immediately before `FastLED.show()`, immediately after `FastLED.show()`, and after the `kWireTimeUs` fence.
2. Logic-analyser capture of both WS2812 data pins plus marker GPIOs.
3. A run that compares marker deltas to expected dual 160-LED wire time and the reset/latch low period.
4. A failure-mode run only if authorised: temporarily shortening the fence in an isolated diagnostic build to reproduce or refute tearing, never in production firmware.

## Decision Matrix For Future WB-2 Work

| Option | Evidence required before decision | Risk |
|---|---|---|
| Keep upstream FastLED + project fence | Logic-analyser evidence shows fence safely covers TX and reset/latch windows with `showSkips=0`. | Lowest code risk, keeps current proven behaviour. |
| Vendor-fork FastLED RMT4 | Source-level patch target and hardware evidence that upstream return/completion semantics cause measurable risk not solved by the fence. | Medium maintenance risk; still tied to FastLED internals. |
| Project-owned S3 transport wrapper | Hardware evidence that project needs explicit TX-complete ownership while preserving FastLED colour utilities. | Higher implementation risk; requires test harness and soak. |
| Full in-house S3 LED driver | Strong evidence that FastLED integration is the limiting failure point and a replacement can meet timing, reset/latch, dual-strip, and safety requirements. | Highest risk; not justified by current source evidence alone. |

## Verification

Read-only source and documentation checks used:

```text
find firmware-v3/src -iname '*LedDriver*' -o -iname '*FastLed*' -o -iname '*RMT*'
find firmware-v3/.pio/libdeps -path '*FastLED*' -type f \( -iname '*rmt*' -o -iname '*esp32*' -o -iname '*clockless*' -o -iname '*led_strip*' \)
grep -RInE "FastLED\\.show|addLeds|RMT|gTX_sem|xSemaphoreTake|esp_rom_delay_us|kWireTimeUs|kMinShowGapUs" --include='*.cpp' --include='*.h' firmware-v3/src/hal firmware-v3/src/core firmware-v3/src/main.cpp
grep -RInE "FASTLED_RMT|rmt_config|rmt_driver_install|tx_start|gTX_sem|xSemaphoreGive" firmware-v3/.pio/libdeps/esp32dev_audio_esv11_k1v2_32khz/FastLED/src/platforms/esp/32/rmt_4
grep -RInE "FASTLED_RMT_BUILTIN_DRIVER|FASTLED_RMT_MAX_CHANNELS|FASTLED_RMT_MEM_BLOCKS|FASTLED_RMT" --include='*.ini' --include='*.h' --include='*.cpp' firmware-v3/src firmware-v3/platformio.ini platformio.ini
```

`rg` was unavailable in the active shell, so the text-search fallback was `grep`. This document is a source-text transport map, not a clangd-backed call hierarchy.

## Changelog

| Date | Agent | Note |
|---|---|---|
| 2026-05-16 | codex:gpt-5.5 | Created first-pass WB-2 source map from current K1v2 S3 driver, production build flags, FastLED 3.10.0 RMT4 overlay, and VP timing docs. |
