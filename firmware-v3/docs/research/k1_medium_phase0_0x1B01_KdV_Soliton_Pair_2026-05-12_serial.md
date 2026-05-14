# K1 Medium Phase 0/1B Serial Evidence — 0x1B01 KdV Soliton Pair
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
- `2026-05-12T04:56:42` setEffect {"effectId":6913} (attempts=1 responses=2)
- `2026-05-12T04:56:43` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:56:43` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:56:44` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:56:44` setSaturation {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:56:44` setComplexity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:56:45` setVariation {"value":0} (attempts=1 responses=1)
- `2026-05-12T04:56:45` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:56:46` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=1 responses=1)
- `2026-05-12T04:56:54` vp stack (attempts=1 responses=27)
- `2026-05-12T04:56:54` s (attempts=1 responses=37)
- `2026-05-12T04:56:55` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:56:55` # (attempts=1 responses=8)
- `2026-05-12T04:56:55` adbg status (attempts=1 responses=4)
- `2026-05-12T04:56:55` dbg status (attempts=1 responses=4)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:56:54, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x1B01 LGP KdV Soliton Pair
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=210 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=24059
  3 tone_map: bypassed
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.994 audio=true
  6 edge_mixer: mode=mirror spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=24402 drops=15794 fps=105 avg_us=9649 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=2384 avg_us=2154 colour_correction last_us=209 avg_us=249
  timing: show_leds last_us=6411 avg_us=6380 pre_pacing_work last_us=10079 avg_us=9649
  timing: output_prep last_us=191 avg_us=167 led_driver_show avg_us=6175
  led_show: frames=24403 last_us=6185 avg_us=6175 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:56:54, cmd=s)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 260746 ms
Active actors: 3
Total messages: 2445
Heap: 8093871 / min 8046083 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 6913 (LGP KdV Soliton Pair)
Brightness: 160
Speed: 27
FPS: 105 (target: 120)
CPU: 100%
Frames: 24429, Drops: 15821
Frame time: avg=9635, min=8245, max=32947 us
LED show: avg=6173, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 2445, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP KdV Soliton Pair - Two sech^2 solitons pass through each other unchanged -- KdV physics
```

### dbg memory (ts=2026-05-12T04:56:55, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:56:55, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```

### adbg status (ts=2026-05-12T04:56:55, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.045
  BPM: 125.0  Conf: 0.777  BeatTick: 0
  Onset: in=0.00239 floor=0.00448 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=23.966 env=0.000 evt=0.000 k/s/h=0/0/0 us=1620
```

### dbg status (ts=2026-05-12T04:56:55, cmd=dbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.093
  BPM: 125.0  Conf: 0.780  BeatTick: 0
  Onset: in=0.00303 floor=0.00444 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=23.562 env=0.000 evt=0.000 k/s/h=0/0/0 us=1562
```

## Baseline state summary
- effect/control: `0x1B01 LGP KdV Soliton Pair`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=105 frame_avg_us=9635 frame_min_us=8245 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=24059
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.
## Captain visual verdict
[blank]