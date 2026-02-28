# ESP32-S3 RMT Show Path Fix (2026-02-28)

## Summary

The persistent ~83 FPS ceiling was caused by RMT channel serialisation in the active
FastLED **RMT4** path (Arduino ESP32 core uses ESP-IDF 4.4.7 on this target), not the
RMT5 path.

## Symptom

- `showLeds()` dominated frame time.
- Typical logs before fix:
  - `[SHOW] pre≈67 us driver≈11159 us`
  - `[PERF] total≈12579 us` (~79-83 FPS raw)

## Root Cause

FastLED RMT4 uses:

- `FASTLED_RMT_MEM_BLOCKS=2` (default)
- Channel iteration step = `gMemBlocks` (`2`)

With `FASTLED_RMT_MAX_CHANNELS=2`, the scheduler only exposes channel index `0`
(loop `i = 0; i < 2; i += 2`), so all controllers are queued on one channel and
transmit mostly sequentially.

## Fix

Set:

```ini
-D FASTLED_RMT_MAX_CHANNELS=4
```

This allows channel indices `0` and `2`, restoring parallel transmission for the
two main 160-LED strips.

## Validation

After rebuild and flash (`esp32dev_audio_pipelinecore`):

- Typical logs after fix:
  - `[SHOW] pre≈66-74 us driver≈6303-6327 us`
  - `[PERF] total≈7705-7749 us` (below 8.33 ms frame budget)

Measured driver time improvement: ~11.16 ms -> ~6.31 ms (about 4.85 ms saved).

## Notes

- IDF5-only instrumentation in `rmt_5` sources is non-operative on this firmware
  target and should not be used for diagnosis unless the toolchain is upgraded to
  ESP-IDF 5.x.
- Status strip (30 LEDs) is a third FastLED controller and contributes to wire-time
  overhead; this is expected.
