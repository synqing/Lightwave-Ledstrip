# FastLED 3.10.0 RMT4 overlay (ESP32)

## Purpose

Upstream `ESP32RMTController::showPixels()` busy-waits until all RMT channels
finish (~5 ms for dual WS2812 × 160). LightwaveOS overlaps that wire time with
the next frame’s CPU work by returning after channels are started and moving
completion bookkeeping into `doneOnChannel()`.

## Application

`scripts/apply_fastled_rmt4_patch.py` runs as a PlatformIO **pre** extra script
(see `platformio.ini`) and copies `idf4_rmt_impl.cpp` over the package file in
`.pio/libdeps/<env>/FastLED/...` after dependencies resolve.

## Constraints

- **K1 dual strip:** two FastLED controllers start in the first channel batch;
  the risky `startNext()` path from inside `doneOnChannel()` (ISR context) is
  not exercised. Configurations with more controllers than first-batch capacity
  need a task-deferred `startNext` before relying on this overlay in production.

- Rebase this file when bumping `fastled/FastLED` in `lib_deps`, then re-apply
  the behavioural diff onto the new upstream source.
