# K1v2 Liquid Light Palette Run - 2026-05-08

Status: GROUNDED hardware execution, REJECTED palette selection.

Supersession: Captain rejected this palette sequence on 2026-05-09. Use `lp_hero_liquid_light_palette_recut_2026-05-09.md` for the forward Liquid Light palette path.

## Purpose

This records the first controlled K1v2 audition of the landing-page Liquid Light source direction from `lp_hero_liquid_light_holographic_direction_2026-05-08.md`.

The run deliberately reused existing firmware behaviour:

- effect: `0x0201` / `LGP Holographic`
- interface: SerialCLI only
- helper: `firmware-v3/tools/liquid_light_palette_sequence.py`
- no firmware effect code changed

## Target

- Device path: `/dev/cu.usbmodem2101`
- Hardware ID observed by PlatformIO: `USB VID:PID=303A:1001 SER=B4:3A:45:A5:87:F8`
- Intended unit: K1v2

## Command

```bash
~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py \
  --port /dev/cu.usbmodem2101 \
  --loops 1 \
  --brightness 208 \
  --speed 14 \
  --echo
```

The brightness target is best-effort because the current SerialCLI brightness control steps in increments of 16. The device settled at `brightness=213`.

## Palette Sequence

The executed palette sequence was:

1. `62` Abyss - deep blue base
2. `64` Ocean - broader blue field
3. `2` Ocean Breeze 036 - richer cool blue
4. `8` Ocean Breeze 068 - teal shift
5. `66` Seafloor - marine blue-green
6. `1` Rivendell - soft green resolve

The unit was left on the final stop:

- effect: `0x0201 LGP Holographic`
- palette: `1 Rivendell`
- brightness: `213`
- speed: `14`

## Post-Run Evidence

Post-run `vp stack` reported:

```text
effect: 0x0201 LGP Holographic
palette: 1 Rivendell
controls: brightness=213 speed=14 intensity=128 saturation=253 complexity=128 variation=0 hue=28 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.068 audio=true
led_show: frames=3570697 last_us=6176 avg_us=6190 max_us=7504 brightness=213 dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
timing: effect_render last_us=1669 avg_us=1489 colour_correction last_us=46 avg_us=43
timing: show_leds last_us=6414 avg_us=6417 pre_pacing_work last_us=8799 avg_us=8631
```

Post-run `s` reported:

```text
Effect: 513 (LGP Holographic)
Brightness: 213
Speed: 14
FPS: 114
CPU: 100%
Frames: 3570742
Drops: 855130
Frame Time: avg=8692us
LED Show: avg=6180us max=7504us skips=0
Heap: 8093995 bytes free / 8087795 bytes min
SPIRAM free: 8066351 bytes
Renderer stack watermark: 10432
```

Post-run `dbg memory` reported:

```text
Free heap: 27644 bytes
Min free heap: 26152 bytes
Max alloc heap: 18420 bytes
```

## Interpretation

GROUNDED:

- The K1v2 accepted the controlled `0x0201` run on `/dev/cu.usbmodem2101`.
- Serial evidence stayed free of LED show skips, show failures, RMT errors, and underruns during the post-run check.
- The effect render average was below the 2.0 ms effect-code ceiling in the post-run `vp stack` sample.
- The observed LED driver show average stayed in the expected multi-millisecond WS2812 range, not the unsafe near-1 ms regime.

DEGRADED-MODE:

- This is not a visual sign-off. No camera capture or Captain visual judgement was recorded in this run.
- FPS was below 120 in the sampled status output (`114`), with historical frame drops already present on the running unit. That does not invalidate the palette audition, but it remains timing evidence to consider before production capture.
- The final visual candidate still needs human review against the LP hero brief: liquid, dimensional, blue/teal-green, restrained, and not rainbow-like.

## Return Path

If Captain approves the visual direction, the next non-invasive step is to capture the same sequence as LP hero source material and keep any landing-page encode in candidate-only output until explicitly promoted.
