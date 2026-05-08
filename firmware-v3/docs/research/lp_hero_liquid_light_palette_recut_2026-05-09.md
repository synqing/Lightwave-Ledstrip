# K1v2 Liquid Light Palette Recut - 2026-05-09

Status: GROUNDED hardware execution, DEGRADED-MODE visual judgement pending.

## Reason

Captain rejected the first Liquid Light palette sequence on 2026-05-09. That sequence is archived only as evidence and must not be treated as the forward LP hero palette direction.

Rejected sequence:

1. `62` Abyss
2. `64` Ocean
3. `2` Ocean Breeze 036
4. `8` Ocean Breeze 068
5. `66` Seafloor
6. `1` Rivendell

The replacement sequence intentionally moves away from soft/fantasy blue-green palettes and towards harder, candidate camera-facing holographic contrast.

## Corrected Sequence

1. `65` Nighttime - black-violet depth
2. `63` Bathy - deep cyan water column
3. `70` Cool - cyan-magenta holographic hit
4. `29` Blue Magenta White - black/blue/magenta caustic contrast
5. `32` Blue Cyan Yellow - electric cyan with controlled gold highlight
6. `15` GR65 Hult - magenta/blue/teal liquid finish

## Runner Fixes

`firmware-v3/tools/liquid_light_palette_sequence.py` now does three things differently:

- Forces a real `0x0201` re-init by switching through `0x0200` first.
- Uses direct SerialJSON `setPalette`, `setBrightness`, and `setSpeed` by default.
- Keeps legacy quick-key palette stepping only behind `--quick-keys`.

The forced re-init matters because `RendererActor::handleSetEffect()` only calls an effect's `init()` when `m_currentEffect != effectId`. Re-sending `effect 0x0201` while `0x0201` is already active is therefore a no-op for `LGPHolographicEffect::init()`.

Source anchors:

- `firmware-v3/src/core/actors/RendererActor.cpp`: `handleSetEffect()` gates cleanup/init behind `if (m_currentEffect != effectId)`.
- `firmware-v3/src/effects/ieffect/LGPHolographicEffect.cpp`: `init()` resets `m_phase1..3`; `render()` increments those phases and feeds them into `sinf()`.

## Failed Intermediate Finding

Before the forced re-init fix, the revised palette pass left `0x0201` at:

```text
effect: 0x0201 LGP Holographic
palette: 15 GR65 Hult
controls: brightness=181 speed=14
frame: target_fps=120 fps=52 avg_us=18890
timing: effect_render last_us=11514 avg_us=11683
led_show: show_skips=0 failures=0 rmt_errors=0 underruns=0
```

That was not accepted as a valid capture state because it violated the 2.0 ms effect-code budget. The issue reproduced across multiple palettes until the effect was switched away and back, so the immediate operational fix is re-init discipline, not palette-specific blame.

## Corrected Hardware Run

Command:

```bash
~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py \
  --port /dev/cu.usbmodem2101 \
  --loops 1 \
  --brightness 181 \
  --speed 14 \
  --dwell 7
```

Device:

- Port: `/dev/cu.usbmodem2101`
- K1v2 serial observed previously on this port: `B4:3A:45:A5:87:F8`

The corrected run completed and left the unit on:

- effect: `0x0201 LGP Holographic`
- palette: `15 GR65 Hult`
- brightness: `181`
- speed: `14`

## Post-Run Evidence

Post-run `vp stack`:

```text
effect: 0x0201 LGP Holographic
palette: 15 GR65 Hult
controls: brightness=181 speed=14 intensity=128 saturation=253 complexity=128 variation=0 hue=8 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=3630836 drops=913265 fps=115 avg_us=8697 min_us=8244 max_us=32950 cpu=100%
timing: effect_render last_us=1783 avg_us=1544 colour_correction last_us=42 avg_us=42
timing: show_leds last_us=6397 avg_us=6408 pre_pacing_work last_us=9049 avg_us=8686
timing: output_prep last_us=169 avg_us=170 led_driver_show avg_us=6198
led_show: frames=3630837 last_us=6194 avg_us=6198 max_us=7504 brightness=181
```

Post-run `s`:

```text
Effect: 513 (LGP Holographic)
Brightness: 181
Speed: 14
FPS: 115 (target: 120)
CPU: 100%
Frames: 3630884, Drops: 913304
Frame time: avg=8697, min=8244, max=32950 us
LED show: avg=6203, max=7504 us, skips=0
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

- The first palette sequence is rejected and superseded.
- The corrected runner produced a clean K1v2 serial run with no show skips, failures, RMT errors, or underruns in the post-run sample.
- The corrected post-run effect render average was back under the 2.0 ms effect-code ceiling.
- LED show timing stayed in the expected WS2812 wire-time range.

DEGRADED-MODE:

- This is still not a visual sign-off. Captain has rejected the first palette set, but has not yet accepted the replacement set as the final LP hero colour story.
- `0x0201` still has an underlying long-run phase/timing risk because its render path advances phases by frame increments and does not wrap or use delta time. The runner reset is an operational capture workaround, not a firmware fix.
