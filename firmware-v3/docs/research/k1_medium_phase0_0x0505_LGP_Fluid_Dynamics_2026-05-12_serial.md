# K1 Medium Phase 0/1B Serial Evidence — 0x0505 LGP Fluid Dynamics
Date: 2026-05-12
Device: K1v2 confirmed by MAC and runtime sync identity.
Scope: evidence-only runtime serial pass. No implementation changes made.

## Baseline controls
- brightness=160
- speed=27
- intensity=128
- saturation=128
- complexity=128
- variation=0
- palette=10 / Vintage 01
- EdgeMixer=mode:MIRROR spread=30 strength=255 spatial=0 temporal=1
- capture mode: baseline

## Command sequence and timestamps
- `2026-05-12T04:54:04` setEffect {"effectId":1285} (attempts=1 responses=2)
- `2026-05-12T04:54:04` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:54:05` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:54:05` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:54:09` setSaturation {"value":128} (attempts=2 responses=2)
- `2026-05-12T04:54:09` setComplexity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:54:13` setVariation {"value":0} (attempts=2 responses=2)
- `2026-05-12T04:54:13` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:54:14` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=1 responses=1)
- `2026-05-12T04:54:22` vp stack (attempts=1 responses=27)
- `2026-05-12T04:54:23` s (attempts=1 responses=38)
- `2026-05-12T04:54:23` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:54:23` # (attempts=1 responses=8)
- `2026-05-12T04:54:23` adbg status (attempts=1 responses=4)
- `2026-05-12T04:54:24` dbg status (attempts=1 responses=4)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:54:22, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x0505 LGP Fluid Dynamics
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=103 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=10640
  3 tone_map: bypassed
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
  6 edge_mixer: mode=mirror spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=10983 drops=5274 fps=114 avg_us=8727 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=1817 avg_us=1628 colour_correction last_us=88 avg_us=78
  timing: show_leds last_us=6273 avg_us=6306 pre_pacing_work last_us=9069 avg_us=8727
  timing: output_prep last_us=52 avg_us=73 led_driver_show avg_us=6195
  led_show: frames=10984 last_us=6190 avg_us=6195 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:54:23, cmd=s)
```text


=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 108902 ms
Active actors: 3
Total messages: 1106
Heap: 8092575 / min 8087239 bytes
SPIRAM free: 8065055 bytes

--- Renderer ---
Effect: 1285 (LGP Fluid Dynamics)
Brightness: 160
Speed: 27
FPS: 114 (target: 120)
CPU: 100%
Frames: 11036, Drops: 5326
Frame time: avg=8764, min=8245, max=32947 us
LED show: avg=6163, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1106, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Fluid Dynamics - Fluid flow simulation
```

### dbg memory (ts=2026-05-12T04:54:23, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:54:23, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```

### adbg status (ts=2026-05-12T04:54:23, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.783  Flux: 0.096
  BPM: 114.0  Conf: 0.162  BeatTick: 0
  Onset: in=0.00608 floor=0.00762 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=49.278 env=0.000 evt=0.000 k/s/h=0/0/0 us=2014
```

### dbg status (ts=2026-05-12T04:54:24, cmd=dbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.851  Flux: 0.170
  BPM: 114.0  Conf: 0.172  BeatTick: 0
  Onset: in=0.00882 floor=0.00761 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=54.854 env=0.000 evt=1.000 k/s/h=0/1/0 us=2013
```

## Baseline state summary
- effect/control: `0x0505 LGP Fluid Dynamics`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=114 frame_avg_us=8764 frame_min_us=8245 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=10640
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.
## Captain visual verdict
[blank]