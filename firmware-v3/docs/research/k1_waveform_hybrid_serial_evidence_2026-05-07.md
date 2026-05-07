---
abstract: "Captain visual judgement, Hybrid-only colour-budget tuning, and serial evidence for K1 Waveform Hybrid 0x1313."
---

# K1 Waveform Hybrid Serial Evidence - 2026-05-07

**Visual characterisation anchor:** `firmware-v3/docs/research/k1_visual_characterisation_database.md`.

## RBDO Label

GROUNDED.

## Scope

This note records Captain visual judgement, Waveform-family speed-floor tuning, a Hybrid colour-budget tuning pass, and serial evidence for `0x1313 K1 Waveform Hybrid` during the Phase 5 visual-quality lane.

This is not a Captain visual sign-off, not a PASS claim, and not a ship-gate claim.

## Decision

Do not patch failed fixtures or global VP defaults as the first move.

The intended smallest candidate change for `0x1302` was a local chroma colour lift plus SensoryBridge-style colour handling. Current source truth shows that `0x1313 K1 Waveform Hybrid` already exists as a separate registered effect and already implements that candidate class:

- `1.5x` brightness boost after contrast squaring.
- chroma-bin share scaling with a K1 LGP-local colour budget.
- dt-correct temporal RGB smoothing.
- same K1 dot/trail/mirror topology as `0x1302`.

Therefore the correct path is to treat `0x1313` as the stronger waveform-family candidate, tune it directly from Captain's visual judgement, and keep any shared Waveform-family motion fix effect-local to `0x1302` and `0x1313`.

## Captain Visual Judgement

Captain confirmed that `0x1313 K1 Waveform Hybrid` is visually superior to `0x1302 K1 Waveform` in musical response. The important observed traits:

- Hybrid triggers more quickly.
- Hybrid fades out more quickly.
- Both traits materially improve the user's perception of effect capability.
- Both standard and Hybrid feel most locked to music around speed `27`.
- Hybrid is significantly darker than standard Waveform and usually needs brightness `255` to avoid dim/desaturated colours.
- After the first Hybrid colour-budget lift, Captain observed that colour saturation appears to max out around brightness `200`, while anything below roughly `150` remains visually unacceptable.

This is a visual judgement, not a ship/pass sign-off. The effect-local tuning target is Hybrid's native colour budget at non-max PHOTONS brightness.

## Firmware Change

Waveform-family speed floor:

```text
files:
  firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.cpp
  firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1WaveformHybridEffect.cpp
change: ctx.speed values below 27 use an effect-local effective speed of 27 for scroll-rate calculation
scope: 0x1302 K1 Waveform and 0x1313 K1 Waveform Hybrid only
```

Hybrid colour budget lift:

```text
file: firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1WaveformHybridEffect.cpp
change: kLedShare 1/12 -> 1/4, PHOTONS compensation -> 1.30x capped at 1.0
scope: 0x1313 K1 Waveform Hybrid only
```

No global VP defaults, WiFi mode, colour correction, gamma, EdgeMixer, silence policy, or render topology were changed.

The colour change keeps proportional chroma mixing but raises the per-bin native colour budget so Hybrid remains readable below maximum PHOTONS brightness while preserving the standard Waveform colour path.

## Source Anchors

- `firmware-v3/src/config/effect_ids.h`: `EID_SB_K1_WAVEFORM_HYBRID = 0x1313`.
- `firmware-v3/src/effects/CoreEffects.cpp`: registers `EID_SB_K1_WAVEFORM_HYBRID`.
- `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1WaveformHybridEffect.cpp`: implements the SB colour-character corrections and keeps the K1 Waveform render topology.
- `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.cpp`: current `0x1302` baseline.

## K1v2 Serial Baseline

Device: K1v2 on `/dev/cu.usbmodem2101`.

Starting effect:

```text
effect: 0x1302 K1 Waveform
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
show_skips=0 failures=0 rmt_errors=0 underruns=0
target_fps=120 frames=104818 drops=45698 fps=119 avg_us=8374 min_us=8244 max_us=32924 cpu=100%
led_show: frames=104820 last_us=6286 avg_us=6228 max_us=7503 brightness=149
silent_scale=0.000 audio=true
```

Candidate effect after `effect 0x1313`:

```text
effect: 0x1313 K1 Waveform Hybrid
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
show_skips=0 failures=0 rmt_errors=0 underruns=0
target_fps=120 frames=106407 drops=45960 fps=119 avg_us=8379 min_us=8244 max_us=32924 cpu=100%
led_show: frames=106408 last_us=6217 avg_us=6200 max_us=7503 brightness=149
```

Short stability sample on `0x1313`:

```text
Effect: 4883 (K1 Waveform Hybrid)
FPS: 119 (target: 120)
Frames: 109254, Drops: 46469
Frame time: avg=8386, min=8244, max=32924 us
LED show: avg=6198, max=7503 us, skips=0
Stack watermark: 10432 words
Heap: 8089007 / min 8087831 bytes
SPIRAM free: 8061747 bytes
Free heap: 27260 bytes
Min free heap: 26164 bytes
Max alloc heap: 18420 bytes
```

Second `vp stack` sample on `0x1313`:

```text
effect: 0x1313 K1 Waveform Hybrid
palette: 3 RGI 15
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=74 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
colour_correction: active toggle=on skipped_by_effect=false
tone_map: active
silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.000 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=109280 drops=46475 fps=119 avg_us=8371 min_us=8244 max_us=32924 cpu=100%
led_show: frames=109282 last_us=6159 avg_us=6192 max_us=7503 brightness=149
```

The device was restored to `0x1302 K1 Waveform` before closing the monitor:

```text
Effect 0x1302: K1 Waveform
Renderer: Effect changed: 0x1313 (K1 Waveform Hybrid) -> 0x1302 (K1 Waveform)
```

## Findings

- `0x1313` is present, registered, and switchable over serial.
- `0x1313` uses unified topology and has no authored/correction/output surface mismatch.
- `0x1313` produced no show skips, RMT errors, failures, or underruns in the captured serial window.
- Timing is tight but comparable to `0x1302`: average frame time sits around the 120 FPS period, while LED wire/show time remains sane for dual 160-LED strips.
- Serial evidence cannot answer visual quality. It only proves that the candidate is operational and safe enough to put in front of Captain for a focused visual judgement.

## Validation After Hybrid Colour-Budget Lift

Native RED/GREEN:

```text
RED: pio test -e native_test_sbk1_waveform_hybrid
failure: Expected 42 to be greater than or equal to 80.

GREEN: pio test -e native_test_sbk1_waveform_hybrid
result: 1 test case succeeded in 00:00:01.312
```

clangd diagnostics:

```text
file: SbK1WaveformHybridEffect.cpp
result: MCP diagnostics completed.
remaining diagnostics: unused include warnings for CoreEffects.h and features.h only.
```

Canonical build:

```text
pio run -e esp32dev_audio_esv11_k1v2_32khz
result: SUCCESS in 00:01:33.151
RAM: 38.4% (125748 / 327680 bytes)
Flash: 33.5% (2460365 / 7340032 bytes)
```

MAC verification before flash:

```text
port: /dev/cu.usbmodem2101
chip: ESP32-S3
MAC: b4:3a:45:a5:87:f8
```

Flash:

```text
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101
result: SUCCESS in 00:01:00.716
target MAC during upload: b4:3a:45:a5:87:f8
```

Post-flash serial evidence on `0x1313`:

```text
effect: 0x1313 K1 Waveform Hybrid
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=76 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
colour_correction: active toggle=on skipped_by_effect=false
tone_map: active
silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=4074 drops=2048 fps=118 avg_us=8441 min_us=8244 max_us=32918 cpu=100%
led_show: frames=4075 last_us=6218 avg_us=6181 max_us=7500 brightness=149
```

Status sample:

```text
Effect: 4883 (K1 Waveform Hybrid)
FPS: 118 (target: 120)
Frames: 4063, Drops: 2044
Frame time: avg=8440, min=8244, max=32918 us
LED show: avg=6181, max=7500 us, skips=0
Heap: 8089395 / min 8087811 bytes
SPIRAM free: 8061747 bytes
Stack watermark: 10432 words
```

At this earlier validation point, K1v2 was left on `0x1313 K1 Waveform Hybrid` for Captain visual inspection.

## Validation After Speed-27 Floor And Second Hybrid Lift

Captain later locked Waveform-family native speed at `27` for both `0x1302` and `0x1313`. The implementation keeps this effect-local: `ctx.speed` values below `27` use effective speed `27` for the two waveform scroll-rate calculations only.

Native RED/GREEN:

```text
RED: pio test -e native_test_sbk1_waveform_hybrid
failure: Standard Waveform speed 25 reached index 133 while speed 27 reached index 137.
failure: Hybrid Waveform speed 25 reached index 133 while speed 27 reached index 137.

GREEN: pio test -e native_test_sbk1_waveform_hybrid
result: 3 test cases succeeded in 00:00:00.716
```

The speed-floor change made the prior Hybrid brightness guard fail at native speed:

```text
failure after speed floor: Expected 68 to be greater than or equal to 104.
after kLedShare 1/4: Expected 102 to be greater than or equal to 104.
final after PHOTONS compensation 1.30x: PASS.
```

clangd diagnostics:

```text
file: SbK1WaveformEffect.cpp
result: MCP diagnostics completed.
remaining diagnostics: unused include warnings for CoreEffects.h and features.h only.

file: SbK1WaveformHybridEffect.cpp
result: MCP diagnostics completed.
remaining diagnostics: unused include warnings for CoreEffects.h and features.h only.
```

Static/diff hygiene:

```text
git diff --check: clean
render-path heap scan: only existing init-time allocations in init()/native-build paths; no renderEffect() heap operations added.
```

Canonical K1v2 build:

```text
pio run -e esp32dev_audio_esv11_k1v2_32khz
result: SUCCESS in 00:01:25.682
RAM: 38.4% (125748 / 327680 bytes)
Flash: 33.5% (2460393 / 7340032 bytes)
```

K1v2 follow-up after port release:

```text
pio device list:
  /dev/cu.usbmodem2101
  SER=B4:3A:45:A5:87:F8

pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101
result: SUCCESS in 00:00:58.460
target MAC during upload: b4:3a:45:a5:87:f8
```

Post-flash `vp stack` on `0x1313`:

```text
effect: 0x1313 K1 Waveform Hybrid
palette: 3 RGI 15
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=77 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
colour_correction: active toggle=on skipped_by_effect=false apply_count=5254 skip_count=0
tone_map: active
silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.084 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=5254 drops=2749 fps=117 avg_us=8805 min_us=8246 max_us=32921 cpu=100%
led_show: frames=5255 last_us=6199 avg_us=6159 max_us=7493 brightness=149
```

Post-flash `s` sample:

```text
Effect: 4883 (K1 Waveform Hybrid)
Brightness: 149
Speed: 25
FPS: 113 (target: 120)
Frames: 6527, Drops: 3332
Frame time: avg=8801, min=8246, max=32921 us
LED show: avg=6187, max=7493 us, skips=0
Heap: 8089403 / min 8087831 bytes
SPIRAM free: 8061747 bytes
Stack watermark: 10432 words
```

Post-flash `dbg memory`:

```text
Free heap: 27656 bytes
Min free heap: 26148 bytes
Max alloc heap: 18420 bytes
```

The control plane still reported `speed=25` in this sample. That does not mean the speed-floor code path was absent: the floor is effect-local inside the `0x1302`/`0x1313` scroll-rate calculation, so values below `27` render with effective speed `27` without changing global control defaults.

K1v2 hardware health is acceptable for committing this effect-local tuning: MAC was verified, flash succeeded, the render path stayed unified with no surface mismatch, and serial evidence showed `showSkips=0`, `failures=0`, `rmt_errors=0`, and `underruns=0`. This is still not a visual PASS or ship-gate claim. Frame timing remains tight/degraded in the captured samples (`fps=113-117`, average frame time around `8800 us`), so future Waveform work must keep timing pressure visible.

Supplementary K1v1 evidence:

```text
port: /dev/tty.usbmodem1101
MAC: b4:3a:45:a5:89:b4
env: esp32dev_audio_esv11_32khz
build: SUCCESS in 00:01:28.932
RAM: 40.5% (132668 / 327680 bytes)
Flash: 33.6% (2467237 / 7340032 bytes)
upload: SUCCESS in 00:01:03.077
```

K1v1 `0x1313` serial sample:

```text
effect: 0x1313 K1 Waveform Hybrid
controls: brightness=255 speed=27 intensity=128 saturation=128 complexity=128 variation=128
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
colour_correction: active toggle=on skipped_by_effect=false
tone_map: active
silence_policy: global_active=true bypassed=false hard_gate_effect=false silent_scale=0.987 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=20 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=4942 drops=4334 fps=116 avg_us=8588 min_us=8245 max_us=32940 cpu=100%
led_show: frames=4943 last_us=6196 avg_us=6161 max_us=7697 brightness=255
```

K1v1 `0x1302` serial sample:

```text
effect: 0x1302 K1 Waveform
controls: brightness=255 speed=27 intensity=128 saturation=128 complexity=128 variation=128
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=11623 drops=9159 fps=116 avg_us=8537 min_us=8244 max_us=32940 cpu=100%
led_show: frames=11624 last_us=6198 avg_us=6212 max_us=7697 brightness=255
```

K1v1 was returned to `0x1313 K1 Waveform Hybrid` before closing serial monitor. This remains useful supplementary runtime evidence; the required focused K1v2 flash/run is now also recorded above.

## Waveform Loiter State

Captain identified a second important visual state in both `0x1302` and `0x1313`: during cold-start acquisition and during trailing release below the effect's apparent response floor, the effect can lose its galloping sprite trails and become a uniform single-colour/single-brightness sheet across the full strip or partial sections of it.

This is not classified as a fail yet. The working classification is `Waveform Loiter`: an intermediate state where the audio frame is still available and the confidence/silence gates are still open, but the waveform-motion signal has fallen below its local floor.

Source-grounded mechanism:

- `SbK1BaseEffect::baseProcessAudio()` copies ESV11 `ControlBusFrame::chroma[]` directly into `m_chromaSmooth[]` and then updates the local waveform peak from `ctx.audio.waveform()`.
- `SbK1BaseEffect::updateWaveformPeak()` subtracts a fixed `750` waveform floor before updating `m_wfPeakScaled` and `m_wfPeakLast`. Below that floor, the position signal collapses toward centre rather than making strong galloping excursions.
- `SbK1WaveformEffect::renderEffect()` and `SbK1WaveformHybridEffect::renderEffect()` still synthesise a dot colour from chroma, multiply it by `audioConfidence * silentScale`, scroll the trail buffer at the effect-local speed, mirror the right half to the left half, and output the result.
- Therefore, when chroma/confidence/silence remain non-zero while waveform peak is below floor, the render path can keep injecting a centre-origin dot every frame. The scrolling/fade path then turns that repeated centre injection into a sheet-like loiter band rather than a clean fade to black.
- Hybrid is expected to make this state easier to see because it processes all chroma bins, applies temporal RGB smoothing, and now has a wider K1 LGP-local colour budget.

Native characterisation:

```text
test: standard waveform loiter state is real below waveform floor
condition: waveform all zero, chroma non-zero, audioConfidence=1.0, silentScale=1.0
result: primary strip remains visibly lit across more than half the strip

control: same below-floor waveform/chroma condition with audioConfidence=0.0, silentScale=0.0
result: primary strip stays dark

test: hybrid waveform loiter state is real below waveform floor
condition/control: same as standard
result: same behaviour
```

Conclusion: Captain's visual assessment matches a real under-the-hood state. It is not the same as `audio.available=false`; it is an effect-local uncertainty state produced by a mismatch between surviving chroma/confidence and collapsed waveform-position energy.

## Next-Phase Runtime Baseline After 555f841e

After the Waveform tuning commit and Codex clangd routing commit, K1v2 was checked again on `/dev/cu.usbmodem2101`.

Repository state:

```text
HEAD: 555f841e docs(repo): clarify codex clangd transport recovery
working tree: clean
K1v2 port: /dev/cu.usbmodem2101
K1v2 serial: B4:3A:45:A5:87:F8
```

Important boundary:

```text
clangd diagnostics smoke on SbK1WaveformEffect.cpp:
  result: Transport closed
```

Per the Codex clangd routing rule, this live Codex session is poisoned for C++ symbol/reference/definition work. The following observations are serial runtime evidence only. They do not promote any new C++ source-mechanism claim.

`0x1313 K1 Waveform Hybrid` baseline:

```text
effect: 0x1313 K1 Waveform Hybrid
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=194 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=438011 drops=286986 fps=117 avg_us=8500 min_us=8244 max_us=32921 cpu=100%
led_show: frames=438012 last_us=6198 avg_us=6182 max_us=7493 brightness=149
```

`0x1313` status/memory:

```text
Effect: 4883 (K1 Waveform Hybrid)
FPS: 119 (target: 120)
Frames: 438808, Drops: 287416
Frame time: avg=8548, min=8244, max=32921 us
LED show: avg=6181, max=7493 us, skips=0
Heap: 8089403 / min 8087831 bytes
SPIRAM free: 8061747 bytes
Stack watermark: 10432 words
Free heap: 27656 bytes
Min free heap: 26148 bytes
Max alloc heap: 18420 bytes
```

`0x1302 K1 Waveform` baseline:

```text
effect: 0x1302 K1 Waveform
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0 hue=167 mood=255
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
silence_policy: global_active=false bypassed=false hard_gate_effect=false silent_scale=1.000 audio=true
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=441312 drops=289397 fps=117 avg_us=8546 min_us=8244 max_us=32921 cpu=100%
led_show: frames=441313 last_us=6277 avg_us=6172 max_us=7493 brightness=149
```

`0x1302` status/memory:

```text
Effect: 4866 (K1 Waveform)
FPS: 117 (target: 120)
Frames: 442061, Drops: 289861
Frame time: avg=8515, min=8244, max=32921 us
LED show: avg=6206, max=7493 us, skips=0
Heap: 8089403 / min 8087831 bytes
SPIRAM free: 8061747 bytes
Stack watermark: 10432 words
Free heap: 27656 bytes
Min free heap: 26148 bytes
Max alloc heap: 18420 bytes
```

K1v2 was returned to `0x1313 K1 Waveform Hybrid` before the serial monitor was closed.

Runtime finding:

- Both Waveform-family effects use the same clean global VP stack and show no LED output faults in the captured window.
- Both remain under visible timing pressure at current runtime settings: `fps=117-119`, average frame time around `8500 us`, high accumulated frame drops, and `CPU=100%`.
- This is a technical runtime trait, not a Captain visual trait yet. It is captured in the ledger as `Waveform Runtime Timing Pressure` with unknown visual linkage.

## Waveform Runtime Timing Pressure - Source Mechanism

This source pass was performed only after a fresh Codex session returned a successful clangd diagnostics smoke on `SbK1WaveformEffect.cpp`. The only diagnostics were unused-include warnings for `CoreEffects.h` and `features.h`.

The timing-pressure mechanism is now source-anchored as a frame-budget and observability issue, not as an LED-output fault:

- `RendererActor.h:105-115` sets `TOTAL_LEDS=320`, `TARGET_FPS=120`, and `FRAME_TIME_US=8333`.
- `RendererActor.cpp:932-1059` measures frame time from the start of `onTick()` through `renderFrame()`, colour correction, `showLeds()`, and pacing/stat update. Drops increment when `rawFrameTimeUs > FRAME_TIME_US`.
- `SerialCLI.cpp:204-219` prints the same render stats and LED-show stats separately in `vp stack`.
- `LedDriver_S3.h:70-72` sets the protective WS2812 wire-time fence to `5600 us`; `LedDriver_S3.cpp:171-177` calls `FastLED.show()` and then waits that full wire time.
- `SbK1WaveformEffect.cpp:156-299` and `SbK1WaveformHybridEffect.cpp:162-327` each perform multiple strip-length passes: chroma synthesis, trail fade over `160`, scroll shifts over the `160`-pixel trail buffer, centre mirroring, output to the first strip, and copy to the second strip. Hybrid adds all-bin colour processing and temporal RGB smoothing at `SbK1WaveformHybridEffect.cpp:162-219`.

The serial arithmetic matches that source shape:

```text
0x1313: frame avg 8500-8548 us, LED-show avg 6181-6182 us
0x1302: frame avg 8515-8546 us, LED-show avg 6172-6206 us
120 FPS target: 8333 us
wire-time fence: 5600 us
```

Therefore, the current evidence says:

- LED transport is healthy: no `showSkips`, RMT errors, failures, or underruns.
- The protected LED show consumes most of the 120 FPS frame period by design.
- The remaining headroom for effect render + colour correction + tone map/split/silence/EdgeMixer + scheduler overhead is narrow.
- The Waveform-family effects are near that total frame budget, but current evidence does not prove that `0x1313` is meaningfully worse than `0x1302`; their captured frame and LED-show averages are close.
- The existing `render_frame_deadline_miss` trace check at `RendererActor.cpp:1050-1058` is not an effect-code-only measurement: `rawFrameTimeUs` includes `showLeds()`. Treat it as total pre-pacing frame work unless a narrower `effect_render_us` or per-layer trace is captured.

Runtime characterisation attempt:

```text
command: ~/.platformio/penv/bin/python3 firmware-v3/tools/capture_trace.py --port /dev/cu.usbmodem2101 --effect 0x1313 --soak 4 --timeout 20 --output /private/tmp/k1_waveform_hybrid_0x1313_timing_trace_20260507.json --quiet
result: failed; no [TRACE] Done marker and no MabuTrace markers captured.
likely reason: current firmware image is not a trace build or FEATURE_MABUTRACE is off.
```

Next evidence needed before optimisation:

- Capture a trace-enabled build or add/read an existing per-layer serial timing surface that separates `effect_render`, colour correction, EdgeMixer, and LED show.
- Do not optimise Waveform render loops or global VP stages from total frame averages alone.

## Per-Layer VP Stack Timing Instrumentation

A read-only `vp stack` timing patch was added to expose:

- `effect_render last_us/avg_us`;
- `colour_correction last_us/avg_us`;
- `show_leds last_us/avg_us`;
- `pre_pacing_work last_us/avg_us`.

Implementation boundaries:

- fixed-width integer counters only;
- no heap allocation added to render paths;
- no global VP defaults changed;
- no colour correction, EdgeMixer, silence, gamma, WiFi, or effect visual behaviour changed.

Validation:

```text
pio run -e esp32dev_audio_esv11_k1v2_32khz
result: SUCCESS in 00:01:27.406
RAM: 38.4% (125748 / 327680 bytes)
Flash: 33.5% (2460945 / 7340032 bytes)

pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101
result: SUCCESS in 00:01:03.843
target MAC during upload: b4:3a:45:a5:87:f8
```

K1v2 `0x1313 K1 Waveform Hybrid` per-layer sample:

```text
effect: 0x1313 K1 Waveform Hybrid
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=3011 drops=2564 fps=116 avg_us=8531 min_us=8247 max_us=32933 cpu=100%
timing: effect_render last_us=524 avg_us=478 colour_correction last_us=765 avg_us=801
timing: show_leds last_us=6413 avg_us=6390 pre_pacing_work last_us=8457 avg_us=8464
led_show: frames=3012 last_us=6190 avg_us=6164 max_us=10002 brightness=149
```

K1v2 `0x1302 K1 Waveform` per-layer sample:

```text
effect: 0x1302 K1 Waveform
topology: mode=unified vp=unified authored=m_leds correction_surface=m_leds output=physical_strips mismatch=false
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=6451 drops=5188 fps=111 avg_us=8879 min_us=8245 max_us=32933 cpu=100%
timing: effect_render last_us=529 avg_us=449 colour_correction last_us=1137 avg_us=1177
timing: show_leds last_us=6299 avg_us=6477 pre_pacing_work last_us=8767 avg_us=8879
led_show: frames=6452 last_us=6071 avg_us=6173 max_us=10002 brightness=149
```

K1v2 status after the `0x1313` sample:

```text
Effect: 4883 (K1 Waveform Hybrid)
FPS: 117 (target: 120)
Frames: 3988, Drops: 3239
Frame time: avg=8534, min=8246, max=32933 us
LED show: avg=6160, max=10002 us, skips=0
Heap: 8089387 / min 8087843 bytes
SPIRAM free: 8061715 bytes
Stack watermark: 10432 words
Free heap: 27672 bytes
Min free heap: 26192 bytes
Max alloc heap: 18420 bytes
```

Finding:

- In these samples, Waveform-family `effect_render` is under `0.6 ms` and the rolling average is under `0.5 ms`.
- The total frame pressure is dominated by the protected show path and shared VP work, especially LED show plus colour correction.
- `0x1313` is not proven to be slower than `0x1302`; both are close enough that this should be treated as global/per-layer budget pressure until more controlled per-layer samples exist.
- The useful next optimisation target is not a blind Waveform render rewrite. It is controlled per-layer evidence around colour correction, EdgeMixer/show path, and any future trace-enabled build.
- K1v2 was returned to `0x1313 K1 Waveform Hybrid` before the serial monitor was closed.

## Controlled Runtime Layer Isolation

This pass used K1v2 `/dev/cu.usbmodem2101`, MAC `b4:3a:45:a5:87:f8`, on commit `e3a696ef`. All controls were runtime-only and restored immediately. No persistent save command was used.

Baseline lock:

```text
effect: 0x1313 K1 Waveform Hybrid
controls: brightness=149 speed=25 intensity=128 saturation=253 complexity=128 variation=0
colour: mode=both auto_exposure=off gamma=on value=2.200 brown_guardrail=off
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
memory: free_heap=27672 min_free_heap=26192 max_alloc_heap=18420
```

`0x1313 K1 Waveform Hybrid` baseline:

```text
frame: target_fps=120 frames=83901 drops=51977 fps=118 avg_us=8440 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=500 avg_us=449 colour_correction last_us=777 avg_us=707
timing: show_leds last_us=6407 avg_us=6379 pre_pacing_work last_us=8697 avg_us=8316
led_show: frames=83902 last_us=6177 avg_us=6161 max_us=10002 brightness=149
```

`0x1313`, dither temporarily off, then restored:

```text
command: dither off
frame: target_fps=120 frames=93385 drops=57367 fps=118 avg_us=8452 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=451 avg_us=451 colour_correction last_us=831 avg_us=755
timing: show_leds last_us=6366 avg_us=6384 pre_pacing_work last_us=8434 avg_us=8338
led_show: frames=93386 last_us=6100 avg_us=6162 max_us=10002 brightness=149
restore: dither on
```

`0x1313`, colour correction mode temporarily off, then restored:

```text
command: cc 0
colour: mode=off
frame: target_fps=120 frames=96156 drops=59352 fps=117 avg_us=8432 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=420 avg_us=458 colour_correction last_us=463 avg_us=531
timing: show_leds last_us=6378 avg_us=6379 pre_pacing_work last_us=8338 avg_us=8187
led_show: frames=96158 last_us=6176 avg_us=6163 max_us=10002 brightness=149
restore: cc 3, confirmed Mode 3 BOTH
```

`0x1313`, EdgeMixer strength temporarily zero, then restored:

```text
command: {"type":"setEdgeMixer","strength":0}
result: success, mode=tetradic spread=30 strength=0 spatial=uniform temporal=rms_gate
frame: target_fps=120 frames=99690 drops=61309 fps=117 avg_us=8437 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=537 avg_us=459 colour_correction last_us=838 avg_us=784
timing: show_leds last_us=6349 avg_us=6356 pre_pacing_work last_us=8795 avg_us=8296
led_show: frames=99691 last_us=6171 avg_us=6179 max_us=10002 brightness=149
restore: {"type":"setEdgeMixer","strength":255}, confirmed tetradic strength=255
```

`0x1302 K1 Waveform` comparison baseline:

```text
effect: 0x1302 K1 Waveform
frame: target_fps=120 frames=103404 drops=63321 fps=119 avg_us=8472 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=491 avg_us=451 colour_correction last_us=704 avg_us=687
timing: show_leds last_us=6532 avg_us=6388 pre_pacing_work last_us=8503 avg_us=8347
led_show: frames=103405 last_us=6244 avg_us=6165 max_us=10002 brightness=149
```

`0x1302`, colour correction mode temporarily off, then restored:

```text
command: cc 0
colour: mode=off
frame: target_fps=120 frames=105538 drops=64416 fps=119 avg_us=8440 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=457 avg_us=439 colour_correction last_us=722 avg_us=699
timing: show_leds last_us=6410 avg_us=6393 pre_pacing_work last_us=8431 avg_us=8313
led_show: frames=105539 last_us=6176 avg_us=6169 max_us=10002 brightness=149
restore: cc 3, confirmed Mode 3 BOTH
```

Final restored state:

```text
effect: 0x1313 K1 Waveform Hybrid
colour: mode=both auto_exposure=off gamma=on value=2.200 brown_guardrail=off
edge_mixer: mode=tetradic spatial=uniform temporal=rms_gate spread=30 strength=255
led_show: dither=on wire_fence=true expected_wire_us=5600 show_skips=0 failures=0 rmt_errors=0 underruns=0
frame: target_fps=120 frames=108548 drops=66187 fps=118 avg_us=8426 min_us=8244 max_us=32933 cpu=100%
timing: effect_render last_us=409 avg_us=447 colour_correction last_us=601 avg_us=603
timing: show_leds last_us=6388 avg_us=6377 pre_pacing_work last_us=8042 avg_us=8231
led_show: frames=108550 last_us=6170 avg_us=6171 max_us=10002 brightness=149
memory: free_heap=27608 min_free_heap=26192 max_alloc_heap=18420 stack_watermark=10432 words
```

Finding:

- Dither state does not materially explain the timing pressure in this short sample.
- Temporarily disabling colour correction reduced the `0x1313` colour-correction timing surface, but did not eliminate the pre-pacing budget pressure.
- Temporarily neutralising EdgeMixer strength did not materially reduce `show_leds` or pre-pacing pressure.
- `0x1302` and `0x1313` continue to look similar on timing: the effect render component stays well under the `2.0 ms` effect-code ceiling, while total frame pressure remains dominated by the protected output path and shared VP stages.
- No LED output faults were observed: `show_skips=0`, `failures=0`, `rmt_errors=0`, and `underruns=0` throughout this pass.
- This is still not a visual PASS or ship-gate signal; it is a source/serial characterisation of runtime pressure.

## Next Step

Continue Waveform-family characterisation with per-layer timing evidence. No firmware behaviour change is justified by this note alone. The next useful source question is whether the current `show_leds` timing surface can be split further into wrapper overhead versus driver/wire-fence time without changing output behaviour.
