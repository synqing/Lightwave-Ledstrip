# K1 Medium Phase 0/1B Serial Evidence — 0x0901 LGP Chromatic Pulse
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
- `2026-05-12T04:55:15` setEffect {"effectId":2305} (attempts=1 responses=2)
- `2026-05-12T04:55:16` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:55:16` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:55:17` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:17` setSaturation {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:17` setComplexity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:18` setVariation {"value":0} (attempts=1 responses=1)
- `2026-05-12T04:55:18` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:55:19` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=1 responses=1)
- `2026-05-12T04:55:27` vp stack (attempts=1 responses=27)
- `2026-05-12T04:55:27` s (attempts=1 responses=37)
- `2026-05-12T04:55:28` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:55:28` # (attempts=1 responses=8)
- `2026-05-12T04:55:28` adbg status (attempts=1 responses=4)
- `2026-05-12T04:55:29` dbg status (attempts=1 responses=4)
- `2026-05-12T04:55:32` setSaturation 253 (attempts=2 responses=3)
- `2026-05-12T04:55:32` setSaturation 253 (attempts=1 responses=3)
- `2026-05-12T04:55:40` vp stack (attempts=1 responses=27)
- `2026-05-12T04:55:41` s (attempts=1 responses=37)
- `2026-05-12T04:55:41` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:55:41` # (attempts=1 responses=8)
- `2026-05-12T04:55:42` adbg status (attempts=1 responses=4)
- `2026-05-12T04:55:42` dbg status (attempts=1 responses=4)
- `2026-05-12T04:55:42` setSaturation 128 (attempts=1 responses=1)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:55:27, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x0901 LGP Chromatic Pulse
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=146 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=16315
  3 tone_map: bypassed
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.950 audio=true
  6 edge_mixer: mode=mirror spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=16658 drops=10841 fps=91 avg_us=10979 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=3981 avg_us=3749 colour_correction last_us=100 avg_us=115
  timing: show_leds last_us=6402 avg_us=6382 pre_pacing_work last_us=11230 avg_us=10979
  timing: output_prep last_us=147 avg_us=152 led_driver_show avg_us=6189
  led_show: frames=16659 last_us=6203 avg_us=6189 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:55:27, cmd=s)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 173773 ms
Active actors: 3
Total messages: 1671
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2305 (LGP Chromatic Pulse)
Brightness: 160
Speed: 27
FPS: 90 (target: 120)
CPU: 100%
Frames: 16684, Drops: 10867
Frame time: avg=10976, min=8245, max=32947 us
LED show: avg=6190, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1671, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Pulse - Pulsing dispersion wave
```

### dbg memory (ts=2026-05-12T04:55:28, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:55:28, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```

### adbg status (ts=2026-05-12T04:55:28, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.676  Flux: 0.124
  BPM: 133.0  Conf: 0.486  BeatTick: 0
  Onset: in=0.00649 floor=0.00318 act=0.361 gate[abs=0 act=0 prev=0 warm=0] flux=47.286 env=0.000 evt=0.000 k/s/h=0/0/0 us=1772
```

### dbg status (ts=2026-05-12T04:55:29, cmd=dbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 1.000  Flux: 0.120
  BPM: 133.0  Conf: 0.502  BeatTick: 0
  Onset: in=0.00815 floor=0.00320 act=0.695 gate[abs=0 act=0 prev=0 warm=0] flux=42.447 env=0.000 evt=0.000 k/s/h=0/0/0 us=1564
```

## Baseline state summary
- effect/control: `0x0901 LGP Chromatic Pulse`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=90 frame_avg_us=10976 frame_min_us=8245 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=16315
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.

## Optional saturation comparison (saturation=253)
- command: setSaturation 253 (requestId=cmpSat-0x0901-2026-05-12T04:55:29)
- brightness held at 160
### vp stack (ts=2026-05-12T04:55:40)
```text

=== VP Stack Introspection ===
effect: 0x0901 LGP Chromatic Pulse
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=253 complexity=128 variation=0 hue=208 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=17401
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
  target_fps=120 frames=17744 drops=11927 fps=91 avg_us=10898 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=4165 avg_us=3715 colour_correction last_us=107 avg_us=117
  timing: show_leds last_us=6330 avg_us=6327 pre_pacing_work last_us=11349 avg_us=10898
  timing: output_prep last_us=72 avg_us=92 led_driver_show avg_us=6199
  led_show: frames=17745 last_us=6222 avg_us=6199 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:55:41)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 187171 ms
Active actors: 3
Total messages: 1779
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2305 (LGP Chromatic Pulse)
Brightness: 160
Speed: 27
FPS: 90 (target: 120)
CPU: 100%
Frames: 17771, Drops: 11954
Frame time: avg=11012, min=8245, max=32947 us
LED show: avg=6162, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1780, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Pulse - Pulsing dispersion wave
```
### dbg memory (ts=2026-05-12T04:55:41)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```
### edge mixer (ts=2026-05-12T04:55:41)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```
### adbg status (ts=2026-05-12T04:55:42)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.775  Flux: 0.508
  BPM: 133.0  Conf: 0.811  BeatTick: 0
  Onset: in=0.00606 floor=0.00541 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=33.506 env=0.000 evt=0.000 k/s/h=0/0/0 us=1880
```
### dbg status (ts=2026-05-12T04:55:42)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.583  Flux: 0.035
  BPM: 133.0  Conf: 0.813  BeatTick: 0
  Onset: in=0.00895 floor=0.00543 act=0.098 gate[abs=0 act=0 prev=0 warm=0] flux=46.309 env=0.000 evt=0.000 k/s/h=0/0/0 us=1586
```

- comparison state summary: effect=0x0901 saturation=253
- comparison show/skips/failures/rmt_errors/underruns: 0/0/0/0
- comparison timing/load: fps=90 frame_avg_us=11012 frame_min_us=8245 frame_max_us=32947 

## Captain visual verdict
[blank]