# K1v2 Liquid Light Captain Palette Pass - 2026-05-09

Status: GROUNDED hardware execution, Captain-selected palette sequence.

## Reason

Captain rejected the earlier agent-selected Liquid Light palette passes and supplied the forward palette sequence directly.

## Captain-Selected Sequence

1. `31` Red Magenta Yellow
2. `28` Autumn 19
3. `24` Fire
4. `22` Emerald Dragon
5. `19` Vintage 57
6. `16` GR64 Hult
7. `10` Vintage 01

`firmware-v3/tools/liquid_light_palette_sequence.py` now uses this order.

## Command

```bash
~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py \
  --port /dev/cu.usbmodem2101 \
  --loops 1 \
  --brightness 181 \
  --speed 14 \
  --dwell 7
```

The runner retained the corrected operational controls from the previous recut:

- force `0x0201` re-init via `0x0200 -> 0x0201`
- direct SerialJSON `setPalette`, `setBrightness`, and `setSpeed`
- no quick-key stepping through unrelated intermediate palettes

## Hardware Result

The K1v2 completed the pass and was left on:

- effect: `0x0201 LGP Holographic`
- palette: `10 Vintage 01`
- brightness: `181`
- speed: `14`

## Post-Run Evidence

Post-run `vp stack`:

```text
effect: 0x0201 LGP Holographic
palette: 10 Vintage 01
controls: brightness=181 speed=14 intensity=128 saturation=253 complexity=128 variation=0 hue=198 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.462 audio=true
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=3653554 drops=934760 fps=115 avg_us=8658 min_us=8244 max_us=32950 cpu=100%
timing: effect_render last_us=1539 avg_us=1455 colour_correction last_us=40 avg_us=44
timing: show_leds last_us=6456 avg_us=6477 pre_pacing_work last_us=8643 avg_us=8654
timing: output_prep last_us=283 avg_us=250 led_driver_show avg_us=6189
led_show: frames=3653555 last_us=6141 avg_us=6189 max_us=7504 brightness=181
```

Post-run `s`:

```text
Effect: 513 (LGP Holographic)
Brightness: 181
Speed: 14
FPS: 115 (target: 120)
CPU: 100%
Frames: 3653607, Drops: 934809
Frame time: avg=8690, min=8244, max=32950 us
LED show: avg=6202, max=7504 us, skips=0
Heap: 8093899 / min 8087659 bytes
SPIRAM free: 8066351 bytes
Renderer stack watermark: 10432 words
```

Post-run `dbg memory`:

```text
Free heap: 27548 bytes
Min free heap: 26016 bytes
Max alloc heap: 18420 bytes
```

## Interpretation

GROUNDED:

- The Captain-selected sequence ran on K1v2 through the corrected runner.
- Post-run effect render average was under the 2.0 ms effect-code ceiling.
- LED show timing stayed in the expected WS2812 range.
- Post-run evidence reported no show skips, failures, RMT errors, or underruns.

DEGRADED-MODE:

- This is still hardware execution evidence, not a captured LP hero asset or final visual sign-off.
- The known `0x0201` long-run phase/timing risk still exists; the runner's forced re-init is the operational capture guard until firmware is changed.
