# K1 Medium Phase 0/1B Serial Evidence — 0x0204 LGP Wave Collision
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
- `2026-05-12T04:53:09` setEffect {"effectId":516} (attempts=1 responses=3)
- `2026-05-12T04:53:10` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:53:10` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:53:11` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:53:11` setSaturation {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:53:15` setComplexity {"value":128} (attempts=2 responses=2)
- `2026-05-12T04:53:15` setVariation {"value":0} (attempts=1 responses=1)
- `2026-05-12T04:53:16` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:53:16` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=1 responses=1)
- `2026-05-12T04:53:25` vp stack (attempts=1 responses=27)
- `2026-05-12T04:53:25` s (attempts=1 responses=37)
- `2026-05-12T04:53:25` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:53:26` # (attempts=1 responses=8)
- `2026-05-12T04:53:26` adbg status (attempts=1 responses=4)
- `2026-05-12T04:53:26` dbg status (attempts=1 responses=4)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:53:25, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x0204 LGP Wave Collision
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=128 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=4521
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
  target_fps=120 frames=4864 drops=3310 fps=67 avg_us=14926 min_us=8246 max_us=32947 cpu=100%
  timing: effect_render last_us=8207 avg_us=7859 colour_correction last_us=65 avg_us=47
  timing: show_leds last_us=6259 avg_us=6309 pre_pacing_work last_us=15221 avg_us=14926
  timing: output_prep last_us=95 avg_us=92 led_driver_show avg_us=6174
  led_show: frames=4865 last_us=6129 avg_us=6174 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:53:25, cmd=s)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 51249 ms
Active actors: 3
Total messages: 491
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 516 (LGP Wave Collision)
Brightness: 160
Speed: 27
FPS: 67 (target: 120)
CPU: 100%
Frames: 4885, Drops: 3331
Frame time: avg=14852, min=8246, max=32947 us
LED show: avg=6168, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 491, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Wave Collision - Colliding wave fronts creating standing nodes
```

### dbg memory (ts=2026-05-12T04:53:25, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:53:26, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```

### adbg status (ts=2026-05-12T04:53:26, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.705  Flux: 0.425
  BPM: 121.0  Conf: 0.741  BeatTick: 0
  Onset: in=0.00714 floor=0.00224 act=1.000 gate[abs=0 act=0 prev=0 warm=0] flux=39.995 env=0.000 evt=0.000 k/s/h=0/0/0 us=1910
```

### dbg status (ts=2026-05-12T04:53:26, cmd=dbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.814  Flux: 0.198
  BPM: 121.0  Conf: 0.745  BeatTick: 0
  Onset: in=0.00985 floor=0.00224 act=1.000 gate[abs=0 act=0 prev=0 warm=0] flux=31.571 env=0.000 evt=0.000 k/s/h=0/0/0 us=2025
```

## Baseline state summary
- effect/control: `0x0204 LGP Wave Collision`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=67 frame_avg_us=14852 frame_min_us=8246 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=4521
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.
## Captain visual verdict
[blank]