# K1 Medium Phase 0/1B Serial Evidence — 0x0902 Chromatic Interference
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
- `2026-05-12T04:55:44` setEffect {"effectId":2306} (attempts=1 responses=2)
- `2026-05-12T04:55:45` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:55:45` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:55:46` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:46` setSaturation {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:47` setComplexity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:55:47` setVariation {"value":0} (attempts=1 responses=1)
- `2026-05-12T04:55:48` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:55:48` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=1 responses=1)
- `2026-05-12T04:55:56` vp stack (attempts=1 responses=27)
- `2026-05-12T04:55:57` s (attempts=1 responses=37)
- `2026-05-12T04:55:57` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:55:57` # (attempts=1 responses=8)
- `2026-05-12T04:55:58` adbg status (attempts=1 responses=4)
- `2026-05-12T04:55:58` dbg status (attempts=1 responses=4)
- `2026-05-12T04:56:01` setSaturation 253 (attempts=2 responses=2)
- `2026-05-12T04:56:01` setSaturation 253 (attempts=1 responses=2)
- `2026-05-12T04:56:10` vp stack (attempts=1 responses=27)
- `2026-05-12T04:56:10` s (attempts=1 responses=37)
- `2026-05-12T04:56:10` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:56:11` # (attempts=1 responses=8)
- `2026-05-12T04:56:11` adbg status (attempts=1 responses=4)
- `2026-05-12T04:56:11` dbg status (attempts=1 responses=4)
- `2026-05-12T04:56:11` setSaturation 128 (attempts=1 responses=1)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:55:56, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x0902 LGP Chromatic Interference
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=106 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=18579
  3 tone_map: bypassed
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.028 audio=true
  6 edge_mixer: mode=mirror spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=18922 drops=13105 fps=78 avg_us=12568 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=5253 avg_us=5361 colour_correction last_us=102 avg_us=118
  timing: show_leds last_us=6330 avg_us=6343 pre_pacing_work last_us=12470 avg_us=12568
  timing: output_prep last_us=140 avg_us=137 led_driver_show avg_us=6164
  led_show: frames=18923 last_us=6157 avg_us=6164 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:55:57, cmd=s)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 203134 ms
Active actors: 3
Total messages: 1897
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2306 (LGP Chromatic Interference)
Brightness: 160
Speed: 27
FPS: 78 (target: 120)
CPU: 100%
Frames: 18944, Drops: 13127
Frame time: avg=12596, min=8245, max=32947 us
LED show: avg=6161, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1897, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Interference - Interfering dispersion patterns
```

### dbg memory (ts=2026-05-12T04:55:57, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:55:57, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```

### adbg status (ts=2026-05-12T04:55:58, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.394  Flux: 0.008
  BPM: 109.0  Conf: 0.794  BeatTick: 0
  Onset: in=0.00114 floor=0.00396 act=0.000 gate[abs=1 act=0 prev=0 warm=0] flux=0.000 env=0.000 evt=0.000 k/s/h=0/0/0 us=14
```

### dbg status (ts=2026-05-12T04:55:58, cmd=dbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.967  Flux: 0.272
  BPM: 109.0  Conf: 0.796  BeatTick: 0
  Onset: in=0.00525 floor=0.00389 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=35.277 env=0.000 evt=0.000 k/s/h=0/0/0 us=1868
```

## Baseline state summary
- effect/control: `0x0902 LGP Chromatic Interference`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=78 frame_avg_us=12596 frame_min_us=8245 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=18579
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.

## Optional saturation comparison (saturation=253)
- command: setSaturation 253 (requestId=cmpSat-0x0902-2026-05-12T04:55:58)
- brightness held at 160
### vp stack (ts=2026-05-12T04:56:10)
```text

=== VP Stack Introspection ===
effect: 0x0902 LGP Chromatic Interference
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=253 complexity=128 variation=0 hue=22 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=19519
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
  target_fps=120 frames=19862 drops=14045 fps=79 avg_us=12481 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=5138 avg_us=5340 colour_correction last_us=104 avg_us=108
  timing: show_leds last_us=6334 avg_us=6295 pre_pacing_work last_us=12169 avg_us=12481
  timing: output_prep last_us=96 avg_us=86 led_driver_show avg_us=6169
  led_show: frames=19863 last_us=6202 avg_us=6169 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:56:10)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 216343 ms
Active actors: 3
Total messages: 1991
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2306 (LGP Chromatic Interference)
Brightness: 160
Speed: 27
FPS: 79 (target: 120)
CPU: 100%
Frames: 19886, Drops: 14069
Frame time: avg=12526, min=8245, max=32947 us
LED show: avg=6166, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1991, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Interference - Interfering dispersion patterns
```
### dbg memory (ts=2026-05-12T04:56:10)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```
### edge mixer (ts=2026-05-12T04:56:11)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```
### adbg status (ts=2026-05-12T04:56:11)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.865  Flux: 0.092
  BPM: 134.0  Conf: 0.719  BeatTick: 1
  Onset: in=0.00386 floor=0.00569 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=37.450 env=0.000 evt=0.000 k/s/h=0/0/0 us=1610
```
### dbg status (ts=2026-05-12T04:56:11)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.870  Flux: 0.153
  BPM: 134.0  Conf: 0.755  BeatTick: 0
  Onset: in=0.00618 floor=0.00570 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=36.640 env=0.000 evt=1.000 k/s/h=0/0/1 us=1821
```

- comparison state summary: effect=0x0902 saturation=253
- comparison show/skips/failures/rmt_errors/underruns: 0/0/0/0
- comparison timing/load: fps=79 frame_avg_us=12526 frame_min_us=8245 frame_max_us=32947 

## Captain visual verdict
[blank]