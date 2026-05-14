# K1 Medium Phase 0/1B Serial Evidence — 0x0900 LGP Chromatic Lens
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
- `2026-05-12T04:54:43` setEffect {"effectId":2304} (attempts=1 responses=3)
- `2026-05-12T04:54:43` setBrightness {"value":160} (attempts=1 responses=1)
- `2026-05-12T04:54:44` setSpeed {"value":27} (attempts=1 responses=1)
- `2026-05-12T04:54:44` setIntensity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:54:48` setSaturation {"value":128} (attempts=2 responses=2)
- `2026-05-12T04:54:48` setComplexity {"value":128} (attempts=1 responses=1)
- `2026-05-12T04:54:49` setVariation {"value":0} (attempts=1 responses=1)
- `2026-05-12T04:54:49` setPalette {"paletteId":10} (attempts=1 responses=1)
- `2026-05-12T04:54:53` setEdgeMixer {"mode":0,"spread":30,"strength":255,"spatial":0,"temporal":1} (attempts=2 responses=2)
- `2026-05-12T04:55:01` vp stack (attempts=1 responses=27)
- `2026-05-12T04:55:01` s (attempts=1 responses=37)
- `2026-05-12T04:55:02` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:55:02` # (attempts=1 responses=9)
- `2026-05-12T04:55:02` adbg status (attempts=1 responses=3)
- `2026-05-12T04:55:03` dbg status (attempts=1 responses=5)
- `2026-05-12T04:55:03` setSaturation 253 (attempts=1 responses=1)
- `2026-05-12T04:55:03` setSaturation 253 (attempts=1 responses=1)
- `2026-05-12T04:55:11` vp stack (attempts=1 responses=27)
- `2026-05-12T04:55:12` s (attempts=1 responses=37)
- `2026-05-12T04:55:12` dbg memory (attempts=1 responses=6)
- `2026-05-12T04:55:12` # (attempts=1 responses=8)
- `2026-05-12T04:55:12` adbg status (attempts=1 responses=4)
- `2026-05-12T04:55:13` dbg status (attempts=1 responses=4)
- `2026-05-12T04:55:13` setSaturation 128 (attempts=1 responses=1)

## Baseline serial captures
### vp stack (ts=2026-05-12T04:55:01, cmd=vp stack)
```text

=== VP Stack Introspection ===
effect: 0x0900 LGP Chromatic Lens
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0 hue=84 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=14205
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
  target_fps=120 frames=14548 drops=8731 fps=91 avg_us=10916 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=4032 avg_us=3744 colour_correction last_us=102 avg_us=118
  timing: show_leds last_us=6315 avg_us=6333 pre_pacing_work last_us=11127 avg_us=10916
  timing: output_prep last_us=85 avg_us=91 led_driver_show avg_us=6204
  led_show: frames=14549 last_us=6190 avg_us=6204 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:55:01, cmd=s)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 147751 ms
Active actors: 3
Total messages: 1460
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2304 (LGP Chromatic Lens)
Brightness: 160
Speed: 27
FPS: 91 (target: 120)
CPU: 100%
Frames: 14574, Drops: 8757
Frame time: avg=10895, min=8245, max=32947 us
LED show: avg=6150, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1460, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Lens - Simulated lens dispersion
```

### dbg memory (ts=2026-05-12T04:55:02, cmd=dbg memory)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```

### edge mixer (ts=2026-05-12T04:55:02, cmd=#)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

[150188][INFO][WiFi] AP Mode - SSID: 'LightwaveOS-AP', IP: 192.168.4.1, Clients: 0
```

### adbg status (ts=2026-05-12T04:55:02, cmd=adbg status)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.666  Flux: 0.274
  BPM: 133.0  Conf: 0.755  BeatTick: 0
```

### dbg status (ts=2026-05-12T04:55:03, cmd=dbg status)
```text
  Onset: in=0.01168 floor=0.00792 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=20.629 env=0.000 evt=0.000 k/s/h=0/0/0 us=1613
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.785  Flux: 0.626
  BPM: 133.0  Conf: 0.759  BeatTick: 0
  Onset: in=0.00751 floor=0.00796 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=22.887 env=0.000 evt=0.000 k/s/h=0/0/0 us=1615
```

## Baseline state summary
- effect/control: `0x0900 LGP Chromatic Lens`
- controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
- palette: 10 Vintage 01
- vp topology: authored=m_leds correction_surface=none output=physical_strips mismatch=false
- EdgeMixer: mode=mirror spread=30 strength=255 spatial=uniform temporal=rms_gate
- silence/audio: `audio` status from vp stack + `adbg status`/`dbg status` snapshots included below
- timing/load: fps=91 frame_avg_us=10895 frame_min_us=8245 frame_max_us=32947
- show/skips/failures/rmt_errors/underruns: 0/0/0/0
- color correction status: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=14205
- tone map status: bypassed

## Preliminary agent verdict
Captured with fixed controls and runtime-only setters; no command-level transport errors observed during baseline capture window.

## Optional saturation comparison (saturation=253)
- command: setSaturation 253 (requestId=cmpSat-0x0900-2026-05-12T04:55:03)
- brightness held at 160
### vp stack (ts=2026-05-12T04:55:11)
```text

=== VP Stack Introspection ===
effect: 0x0900 LGP Chromatic Lens
palette: 10 Vintage 01
controls: brightness=160 speed=27 intensity=128 saturation=253 complexity=128 variation=0 hue=142 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=none output=physical_strips mismatch=false
layers:
  1 effect_render: active surface=m_leds
  2 colour_correction: bypassed toggle=on skipped_by_effect=true apply_count=343 skip_count=15031
  3 tone_map: bypassed
  4 split/converge: m_leds -> physical_strips
  5 silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.777 audio=true
  6 edge_mixer: mode=mirror spatial=uniform temporal=rms_gate spread=30 strength=255
  7 led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
colour:
  mode=both hsv_min_sat=120 rgb_white_threshold=150 rgb_target_min=100 saturation_boost=25
  auto_exposure=off target=110 brown_guardrail=off v_clamp=on max_brightness=255
  gamma=on value=2.200 lut_gen=1 samples=[0,3,12,56,137,255]
frame:
  target_fps=120 frames=15374 drops=9557 fps=90 avg_us=11072 min_us=8245 max_us=32947 cpu=100%
  timing: effect_render last_us=3886 avg_us=3842 colour_correction last_us=94 avg_us=110
  timing: show_leds last_us=6358 avg_us=6349 pre_pacing_work last_us=11021 avg_us=11072
  timing: output_prep last_us=132 avg_us=124 led_driver_show avg_us=6187
  led_show: frames=15375 last_us=6187 avg_us=6187 max_us=7485 brightness=160
capture:
  enabled=false tap_mask=0x00 last_effect=0xFFFF last_palette=0 frame=0 timestamp_us=0
==============================
```
### s (ts=2026-05-12T04:55:12)
```text

=== LightwaveOS v2 Actor System ===
State: 2
Uptime: 157921 ms
Active actors: 3
Total messages: 1542
Heap: 8093871 / min 8087239 bytes
SPIRAM free: 8066351 bytes

--- Renderer ---
Effect: 2304 (LGP Chromatic Lens)
Brightness: 160
Speed: 27
FPS: 90 (target: 120)
CPU: 100%
Frames: 15401, Drops: 9584
Frame time: avg=11119, min=8245, max=32947 us
LED show: avg=6172, max=7485 us, skips=0
Stack watermark: 10432 words

--- ShowDirector ---
Has show: NO

--- MessageBus ---

=== MessageBus Subscriptions ===
Active entries: 2/32
Published: 1543, Delivered: 2, Failed: 0

Type 0x83: 1 subscribers [ Renderer ]
Type 0xD3: 1 subscribers [ ShowDirector ]
================================

===================================

  Current effect type: IEffect (native)
  Metadata: LGP Chromatic Lens - Simulated lens dispersion
```
### dbg memory (ts=2026-05-12T04:55:12)
```text

=== Memory Status ===
  Free heap: 27520 bytes
  Min free heap: 25572 bytes
  Max alloc heap: 17396 bytes

```
### edge mixer (ts=2026-05-12T04:55:12)
```text

=== EdgeMixer Status ===
  Mode:     mirror
  Spread:   30
  Strength: 255
  Spatial:  uniform
  Temporal: rms_gate

```
### adbg status (ts=2026-05-12T04:55:12)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.054
  BPM: 141.0  Conf: 0.200  BeatTick: 0
  Onset: in=0.00339 floor=0.00641 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=46.183 env=0.000 evt=0.000 k/s/h=0/0/0 us=1654
```
### dbg status (ts=2026-05-12T04:55:13)
```text
=== Audio Status (ES v1.1 backend) ===
  RMS: 0.000  Flux: 0.056
  BPM: 141.0  Conf: 0.241  BeatTick: 0
  Onset: in=0.00328 floor=0.00623 act=0.000 gate[abs=0 act=1 prev=0 warm=0] flux=35.841 env=0.000 evt=0.000 k/s/h=0/0/0 us=1589
```

- comparison state summary: effect=0x0900 saturation=253
- comparison show/skips/failures/rmt_errors/underruns: 0/0/0/0
- comparison timing/load: fps=90 frame_avg_us=11119 frame_min_us=8245 frame_max_us=32947 

## Captain visual verdict
[blank]