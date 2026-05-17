# WB-2 LED Transport Hardware Smoke - 2026-05-17

## Scope

WB-2 regression smoke for commit `bbbd6e19` on K1v2 after adding LED transport timing boundary counters. This evidence covers upload, serial status, and VP stack telemetry only. It does not prove physical TX-start/TX-complete/reset-latch boundaries with external instrumentation.

## Identity And Upload

- SOURCE: repo HEAD before smoke was `bbbd6e19`.
- USB_SERIAL: `pio device list` reported `/dev/cu.usbmodem2101` with USB descriptor serial `B4:3A:45:A5:87:F8`.
- USB_SERIAL: `python -m esptool` was unavailable because `python` is not on PATH.
- USB_SERIAL: `/Users/spectrasynq/.platformio/penv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2101 read_mac` previously failed with `Failed to connect to ESP32-S3: No serial data received`.
- USB_SERIAL: PlatformIO upload later connected to the same port and reported `MAC: b4:3a:45:a5:87:f8`.
- BUILD: `/Users/spectrasynq/.platformio/penv/bin/pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101` succeeded.
- BUILD: uploaded image size was RAM `127076 / 327680` bytes (`38.8%`) and flash `2518221 / 7340032` bytes (`34.3%`).

## Serial Status Sample 1

USB_SERIAL evidence from serial command `s`:

```text
Uptime: 15550 ms
Heap: 8084247 / min 8082287 bytes
SPIRAM free: 8061627 bytes
FPS: 114 (target: 120)
Frame budget: 100%
Frames: 1507, OverBudget: 1451 (96.3%)
Frame time: avg=8768, min=8257, max=33076 us
LED show: avg=6200, max=9084 us, skips=0, failures=0
LED transport: fastled_avg=547 us, rmt_fence_avg=5601 us, latch_wait_avg=0 us
Stack watermark: 11356 words
```

## VP Stack Sample

USB_SERIAL evidence from serial command `vp stack`:

```text
7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame:
  target_fps=120 frames=2244 over_budget=2112 fps=115 avg_us=8623 min_us=8249 max_us=33076 frame_budget=100%
  timing: effect_render last_us=405 avg_us=414 colour_correction last_us=826 avg_us=777
  timing: show_leds last_us=6472 avg_us=6445 pre_pacing_work last_us=8624 avg_us=8621
  timing: output_prep last_us=212 avg_us=197 led_driver_show avg_us=6200
  led_show: frames=2245 last_us=6214 avg_us=6200 max_us=9084 brightness=149
  led_transport: output_prep last_us=42 avg_us=43 fastled_call last_us=561 avg_us=544 rmt_fence last_us=5608 avg_us=5602 latch_wait last_us=0 avg_us=0
```

## Serial Status Sample 2

USB_SERIAL evidence from serial command `s`:

```text
Uptime: 27890 ms
Heap: 8083975 / min 8082263 bytes
SPIRAM free: 8061627 bytes
FPS: 114 (target: 120)
Frame budget: 100%
Frames: 2740, OverBudget: 2538 (92.6%)
Frame time: avg=8824, min=8249, max=33076 us
LED show: avg=6191, max=9084 us, skips=0, failures=0
LED transport: fastled_avg=537 us, rmt_fence_avg=5601 us, latch_wait_avg=0 us
Stack watermark: 11356 words
```

## Result

- USB_SERIAL: no panic or exception text appeared during the captured monitor window.
- USB_SERIAL: `showSkips=0`, `failures=0`, `rmt_errors=0`, and `underruns=0` in status/VP output.
- USB_SERIAL: LED show average stayed around `6.19-6.20 ms`, consistent with the existing full wire-fence behaviour for this build path rather than a premature `~1 ms` return.
- USB_SERIAL: new transport counters separated FastLED call-return (`~0.54 ms`) from fixed RMT fence (`~5.60 ms`) and latch wait (`0 us`) without changing the existing fence.
- USB_SERIAL: heap stayed stable across samples: free heap `8084247 -> 8083975` bytes and min heap `8082287 -> 8082263` bytes.

## Remaining Evidence Gap

DEGRADED-MODE:

- Unresolved assumption: source and serial timing counters are not external physical TX-start/TX-complete/reset-latch proof.
- Risk if wrong: the firmware could report sane software timing while the physical waveform has a boundary issue not visible to serial telemetry.
- Fallback: preserve the existing fixed `kWireTimeUs` fence and keep WB-2 physical-boundary proof open.
- Revisit trigger: logic analyser or equivalent hardware instrumentation on data pins plus diagnostic markers.
- Debt count / affected outputs: 1 WB-2 output class remains externally unproven.
