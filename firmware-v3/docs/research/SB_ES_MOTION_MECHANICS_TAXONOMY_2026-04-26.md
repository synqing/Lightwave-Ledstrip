---
abstract: "Canonical cross-version motion-mechanic taxonomy reconstructed from 5 Sensory Bridge releases (3.0.0, 3.1.0, 3.2.0, 4.0.0, 4.1.1) and 4 Emotiscope releases (1.0, 1.1, 1.2, 2.0). Source-of-truth: K1.node1/references/. Identifies every spatial-transport, temporal-persistence, motion-driver, and frame-coupling primitive used in lightshow modes; tracks SB lineage trajectory (custom edge-out scrolls → centre-mirror sprite scroll), ES lineage trajectory (single-trail framebuffer LPF → dual-buffer sprite self-feedback + tempo-phase swarm), and the narrow set of primitives shared by both. Includes K1 SbK1* preservation gap-analysis and 6 bug-shaped findings flagged in original sources. Read alongside SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md when authoring EFFECT_FRAMEWORK_STANDARD.md or assessing whether a candidate primitive has lineage support."
---

# Sensory Bridge × Emotiscope Motion-Mechanic Taxonomy

**Track D companion to `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md`. Research preservation, NOT policy.**

This document distils 9 forensic SSA passes (one per release) into a single cross-version map of every motion mechanic used in SB and ES light-show modes. The source-of-truth files (one `lightshow_modes.h` per SB release; one `light_modes/` directory tree per ES release) live under `K1.node1/references/`. Per-mode parameter detail and source-line citations are in the SSA outputs cached at `/private/tmp/.../tasks/`; this file is the synthesis layer.

## Sources

| Project | Release | Source path | Modes |
|---|---|---|---|
| Sensory Bridge | 3.0.0 | `Sensorybridge.sourcecode/SensoryBridge-3.0.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h` | gdft, gdft_chromagram, bloom (slow+fast), waveform, vu, vu_dot |
| Sensory Bridge | 3.1.0 | `Sensorybridge.sourcecode/SensoryBridge-3.1.0/.../lightshow_modes.h` | (same as 3.0.0; bloom adds frame-skip + ping-pong, vu adds sub-pixel AA) |
| Sensory Bridge | 3.2.0 | `Sensorybridge.sourcecode/SensoryBridge-3.2.0/.../lightshow_modes.h` | gdft, gdft_chromagram, bloom (slow+fast), vu, vu_dot, **kaleidoscope** (NEW); waveform REMOVED |
| Sensory Bridge | 4.0.0 | `Sensorybridge.sourcecode/SensoryBridge-4.0.0/.../lightshow_modes.h` | test_mode, gdft, chromagram_gradient, chromagram_dots, vu_dot, kaleidoscope, bloom (sprite-based, but `draw_sprite` UNDEFINED in tree); vu (bar) REMOVED |
| Sensory Bridge | 4.1.1 | `Sensorybridge.sourcecode/SensoryBridge-4.1.1/.../lightshow_modes.h` | (same as 4.0.0 active set; bloom now functional via supplied `draw_sprite`; large legacy commented blocks remain) |
| Emotiscope | 1.0 | `Emotiscope.sourcecode/Emotiscope-1.0/src/lightshow_modes/` (12 flat .h) | analog, bloom, debug, hype, metronome, neurons, neutral, octave, plot, spectronome, spectrum, waveform |
| Emotiscope | 1.1 | `Emotiscope.sourcecode/Emotiscope-1.1/src/light_modes/` (active+beta+inactive+system) | active(7): analog, bloom, hype, metronome, octave, spectronome, spectrum; beta(4): debug, neurons, plot, waveform |
| Emotiscope | 1.2 | `Emotiscope.sourcecode/Emotiscope-1.2/src/light_modes/` | active(11) adds **beat_tunnel, fft, perlin, tempiscope**; beta(4) unchanged |
| Emotiscope | 2.0 | `Emotiscope.sourcecode/Emotiscope-2.0/main/light_modes/` | active(10): analog, spectrum, octave, metronome, spectronome, hype, bloom, fft, beat_tunnel, **pitch** (NEW); perlin moved to inactive; beta(3): debug, **temp** (NEW), tempiscope |

NATIVE strip: SB = 128 LEDs (mirror seam 63|64). ES 1.x/2.0 = 128 LEDs (mirror seam 63|64) at REFERENCE_FPS=100. K1 = 160 LEDs (centre 79|80) at 120 FPS — geometry and pacing both differ.

## SB lineage trajectory (3.0.0 → 4.1.1)

**Phase 1 — Custom-loop scrolls + frame-coupled EMAs (3.0.0–3.1.0):** Bloom is the sole spatial-transport mode, hand-rolled as an inline reverse-copy ring shift on `leds_temp` from `leds_last` (1 or 2 px/frame), followed by `distort_logarithmic` (`sqrt(prog)` non-linear remap) and `fade_top_half` (linear spatial taper). VU/VU-Dot use scalar-position EMA with MOOD-modulated alpha; VU-Dot fills the swept span between previous and current position each frame as a velocity-length comet. 3.1.0 adds two refinements: (a) every-other-frame gate `bitRead(iter, 0)` on bloom (half-rate animation), (b) `leds_aux` ping-pong replay on odd frames, (c) sub-pixel `fract` head-pixel anti-aliasing on the VU bar. Helpers `low_pass_array`, `draw_sprite`, `set_dot_position` DO NOT EXIST in 3.0/3.1 (grep-confirmed).

**Phase 2 — Compositor enrichment + first Perlin/velocity mode (3.2.0):** `waveform` is REMOVED, `kaleidoscope` is ADDED — first appearance of (i) 2-D Perlin per RGB channel sampled at cubic-shifted spatial index `(i+18)³`, (ii) per-band max-follower envelopes (attack 0.1, decay 0.99, separate ops) driving (iii) Y-axis position drift `pos += sum * shift_speed` where `shift_speed = 100 + 500*MOOD`, with `calc_punch` spectral-flux as the band driver. VU-Dot gains a halo composite: `scale_image_to_half` → `scale_half_to_full` lerp + RGB channel rotation + two `blend_buffers(BLEND_ADD)` rounds. `apply_prism_effect` (post-pipeline mirror primitive) appears in `led_utilities.h`.

**Phase 3 — Goertzel/chromagram reset + symbolic geometry (4.0.0):** Mode set rebuilt around 12-bin chromagram and 64-bin Goertzel spectrogram. New canonical centre-origin geometry primitive: `shift_leds_up(leds_16, 64)` + `mirror_image_downwards(leds_16)` (gdft pattern). `chromagram_gradient` performs symmetric in-loop write to BOTH halves directly (no shift+mirror needed). `chromagram_dots` introduces the FIRST and ONLY dt-correct mode in any SB release: `low_pass_array_fixed(chromagram_smooth, last, 12, LED_FPS, mood_scale(3.5, 1.5))` — Hz-domain cutoff via `α = 1 - exp(-2π·fc/LED_FPS)`. `vu_dot` becomes the most elaborate dot mode: dual cascaded EMA (mood-scaled) + asymmetric AGC max-follower (1.1× headroom, 0.1 attack, 0.9999 decay, 0.0025 floor) + `sqrt` brightness curve. `bloom` calls `draw_sprite(leds_16, leds_16_prev, 128, 128, 0.250 + 1.750*MOOD, 0.99)` — but **`draw_sprite` is UNDEFINED in the 4.0.0 source tree** (grep-confirmed across the entire firmware), so 4.0.0 bloom would not link. Helper supplied externally / in 4.1.x.

**Phase 4 — Bloom completion + parameter retunes (4.1.1):** No new modes vs 4.0.0. `draw_sprite` now linkable. Parameter-level deltas worth canonising: (a) bloom `share = 1/6.0` per chroma slot (NOT 1/12 — deliberate 2× over-summation), (b) bloom centre injection at BOTH `leds_16[63]` AND `[64]` (centre-pair), (c) bloom `prog²` quadratic end-fade over last 32 LEDs, (d) bloom canvas `memset` each frame (only `leds_16_prev` carries history), (e) chromagram_gradient `*0.9 + 0.1` additive floor + extra final squaring, (f) chromagram_dots excursion ±0.45 (NOT ±0.5; 10% strip-end dead zone), (g) kaleidoscope `speed_limit` declared but unused — DEAD CODE since at least 4.0.0, (h) gdft contrast formula is `(b²)*0.65 + b*0.35` with +1 extra iter when `chromatic_mode`, (i) `spectrogram_smooth` is **symmetric 0.75/0.75 EMA**, NOT asymmetric — corrects the framework reconstruction's asymmetric-EMA claim. Two large legacy commented blocks (old shift-register bloom; old gdft_chromagram) remain in source.

## ES lineage trajectory (1.0 → 2.0)

**ES 1.0 — Three engine primitives + global framebuffer LPF as universal trail:**
- `draw_dot()` with persistent `fx_dots[slot].position` cache → `draw_line(prev, current)` motion blur per dot per frame, brightness scaled by `1 / |Δposition|`.
- `draw_sprite(dest, src, dest_len, src_len, position, alpha)` — fractional-pixel buffer translate (linear interp between adjacent integer offsets). Used by bloom as self-feedback transport.
- `tempi[].phase` — bank of 64 Goertzels over 64–192 BPM; each bin exposes instantaneous phase (radians, `atan2`-derived, `delta`-stepped, dt-correct). Modes composite ALL 64 bin phases simultaneously — never a single dominant tempo.
- **Universal trail**: `apply_image_lpf` in `gpu_core.h:87` — frame-rate-aware temporal EMA on the framebuffer (`α = 1 - exp(-2π·fc/FPS_GPU)`) with cutoff `0.5 + (1 - √softness)·14.5 Hz` driven by the softness knob. ALSO doubles as cross-fade transition via `lpf_drag *= 0.995`. Most modes have NO own persistence — they redraw fresh every frame and let the global LPF supply the trail.

ES 1.0 active mode set: 7 modes (analog, bloom, hype, metronome, octave, spectronome, spectrum). Only **bloom** has its own per-mode persistence (`novelty_image_prev` + `draw_sprite` self-feedback at α=0.99). Only **plot** does sub-pixel line drawing internally (mode-local `draw_line`, distinct from the engine's). Tempo-phase modes (hype, metronome, spectronome) inherit dt-correctness from `delta`-stepped phase; everything else is frame-coupled.

**ES 1.1 — Directory restructure, no primitive change:** Modes split into `active/`, `beta/`, `inactive/`, `system/`. Same engine primitives. `auto_color_cycle` (in `leds.h:445`) advances `configuration.color` from novelty — a global hue-motion mechanism that all modes inherit when toggled.

**ES 1.2 — Spatial-transport diversification:** Adds `beat_tunnel` (sprite self-feedback at α=0.965, sub-pixel scroll position `sin(angle)` with `angle += 0.001` free-running phase, tempo-phase-gated injection at narrow window `|phase - 0.65| < 0.02`), `perlin` (2-D Perlin walk with momentum-decay `momentum *= 0.99` fed by `vu_level⁴`), `tempiscope` (per-tempo-bin pixel raster), `fft` (sole hardware-FFT consumer; everything else uses Goertzel-derived buffers). Bloom α=0.99, beat_tunnel α=0.965, perlin momentum=0.99 — sprite self-feedback at α∈[0.95, 0.99] is the canonical trail family.

**ES 2.0 — IDF-native rewrite + SIMD pacing primitives:** Drops `perlin` to inactive; adds `pitch` (auto-correlation buffer mapped along strip + 8-frame box average via `dsps_add_f32` / `dsps_mulc_f32` ESP-DSP SIMD ops), `temp` (colour-temperature placeholder), `tempiscope` retained in beta. New global `apply_frame_blending()` in `leds.h:800` — knob-driven temporal EMA across the whole strip, mode-agnostic. ESP-DSP SIMD buffer ops (`dsps_memset_aes3`, `dsps_memcpy_aes3`) become the workhorse for every dual-buffer mode. **`fx_dots[]` global motion-blur cache** is now an explicit shared resource across modes. Tempo-phase-gated sprite injection (beat_tunnel kernel) and beat-parity dots (hype: odd vs even tempo bins as binary colour-axis) are crystallised. Still NO dt-correction in any mode — pure per-frame coefficients.

## Canonical motion-mechanic catalogue

Spatial-transport family — every primitive observed across 9 releases, with provenance:

| Primitive | First seen | Last seen | Notes |
|---|---|---|---|
| Inline reverse-copy ring shift (1 or 2 px/frame) | SB 3.0.0 (bloom) | SB 3.2.0 (bloom) | Custom loop on `leds_temp`/`leds_last`. Replaced by `draw_sprite` from SB 4.0.0+. |
| `distort_logarithmic` (`sqrt(prog)` non-linear remap) | SB 3.0.0 (bloom) | SB 3.2.0 (bloom) | Sibling of `lerp_led`. Removed in SB 4.x. |
| `fade_top_half(MIRROR_ENABLED)` (linear spatial taper) | SB 3.0.0 (bloom) | SB 3.2.0 (bloom) | Removed in SB 4.x. Replaced by `prog²` end-fade in 4.1.1 bloom. |
| `shift_leds_up(N)` + `mirror_image_downwards` | SB 4.0.0 (gdft) | SB 4.1.1 (gdft) | The CANONICAL SB centre-origin geometry primitive. memcpy + zero + reverse-copy. |
| Symmetric in-loop write `leds[64+i]` AND `leds[63-i]` | SB 4.0.0 (chromagram_gradient) | SB 4.1.1 (chromagram_gradient) | Cleanest centre-origin pattern — no intermediate buffer. |
| `set_dot_position` + `draw_dot` (motion-blur via `draw_line(last→current)` with `1/|Δ|` brightness) | SB 4.0.0 (chromagram_dots, vu_dot, test_mode) | SB 4.1.1 + ES 1.0–2.0 | The fractional-pixel dot primitive. ES exposes `fx_dots[]` cache as a first-class shared resource (ES 2.0). |
| `interpolate(prog, array, N)` (linear resample to NUM_LEDS) | SB 3.1.0 (gdft_chromagram) | SB 4.1.1 + ES 1.0–2.0 | Sub-pixel data-vector→strip resample. |
| Fractional-pixel sprite scroll `draw_sprite(dest, prev, ..., position, alpha)` | SB 4.0.0 (bloom) — UNDEFINED in 4.0.0 tree, completed in 4.1.1 | SB 4.1.1 + ES 1.0 (bloom), 1.2 (beat_tunnel), 2.0 (bloom, beat_tunnel) | The CANONICAL trail primitive in SB 4.x and ES. SB uses `0.250 + 1.750*MOOD`/0.99; ES uses `0.125 + 0.875*speed`/0.99 (bloom) or sin-driven swing/0.965 (beat_tunnel). |
| Centre-pair injection `leds_16[63]=leds_16[64]=colour` | SB 4.1.1 (bloom) | SB 4.1.1 only | New energy at BOTH centre LEDs simultaneously. |
| `prog²` quadratic end-fade over last 32 LEDs | SB 4.1.1 (bloom) | SB 4.1.1 only | Replaces the old `fade_top_half`. |
| RGB channel rotation `CRGB(c.g, c.b, c.r)` halo | SB 3.2.0 (vu_dot) | SB 3.2.0 only | Per-frame static channel swap, not hue rotation. |
| `scale_image_to_half` + `scale_half_to_full` lerp halo composite | SB 3.2.0 (vu_dot) | SB 3.2.0 only | Two-pass downsample/upsample chroma-rotated halo via `blend_buffers(BLEND_ADD, 32)`. |
| 2-D `inoise16` Perlin per RGB at cubic-shifted index `(i+18)³` | SB 3.2.0 (kaleidoscope) | SB 4.1.1 (kaleidoscope) | Three independent virtual cameras at 0.5/1.0/1.5 spatial scale. |
| 2-D Perlin walk with `vu_level⁴` momentum (`momentum *= 0.99`) | ES 1.2 (perlin) | ES 1.2 only | Active in 1.2; moved to inactive in 2.0. |
| Auto-correlation buffer mapped along strip | ES 2.0 (pitch) | ES 2.0 only | Pitch-period histogram. NEW geometric primitive vs SB lineage. |
| Sample-history-as-spatial-axis (time-as-x scope) | ES 2.0 (debug) | ES 2.0 only | Direct readout of `novelty_curve[hist_len-NUM_LEDS+i]`. |
| Sub-pixel custom `draw_line` rasteriser (mode-local) | ES 1.0 (plot) | ES 1.0–2.0 (plot) | Distinct from the engine's `draw_line`. 1000-iteration coverage walk with sub-pixel endpoint weighting. |
| 8-frame ring-buffer box-average via SIMD `dsps_add_f32`/`dsps_mulc_f32` | ES 2.0 (pitch) | ES 2.0 only | Frame-aligned 1-D temporal smoothing. **Bug-shaped: `average_index` declared local; ring may reset every frame.** |

Temporal-persistence family:

| Primitive | First seen | Notes |
|---|---|---|
| Per-pixel temporal EMA on a private buffer | SB 3.0.0 (waveform: `waveform_last[i]`, α scaled by MOOD) | Removed when SB 4.0.0 dropped waveform. |
| Scalar-position EMA on a single moving index | SB 3.0.0 (vu, vu_dot) | Smoothing IS the motion. K1 SbK1 ports use this for centre-mirror dot pairs. |
| Asymmetric max-follower / AGC envelope (fast attack, slow decay) | SB 3.0.0 (`max_waveform_val_follower`: 0.25 attack, 0.005 decay) | SB 4.0.0 vu_dot AGC: 1.1× headroom, 0.1 attack, 0.9999 decay, 0.0025 floor. |
| Per-band max-follower with separate rise/decay ops (rise 0.1, decay 0.99) | SB 3.2.0 (kaleidoscope) | Bands split low/mid/high (bins 0–19/20–39/40–59). Drives Perlin Y-axis position (NOT brightness). |
| Sprite self-feedback temporal+spatial trail (α∈[0.95, 0.99]) | SB 4.x (bloom), ES 1.0–2.0 (bloom, beat_tunnel) | The DOMINANT trail primitive in modern lineage. Combines transport and decay in a single pass. |
| Frame-skip cadence `bitRead(iter, 0)` (half-rate animation) | SB 3.1.0 (bloom) | Removed in SB 4.x sprite bloom. |
| Ping-pong replay buffer (`leds_aux` re-display on alternate frames) | SB 3.1.0 (bloom) | Removed in SB 4.x sprite bloom. |
| `low_pass_array_fixed(array, last, N, sample_rate, cutoff_hz)` — DT-CORRECT | SB 4.0.0 (chromagram_dots) | The SOLE dt-correct primitive in any SB release. Cutoff `mood_scale(3.5, 1.5)` Hz. |
| Global framebuffer LPF (`apply_image_lpf`, knob-driven, dt-correct) | ES 1.0 (`gpu_core.h:87`) | Universal ES trail mechanism. Cutoff `0.5 + (1 - √softness)·14.5 Hz`. Dual-purpose as `lpf_drag` cross-fade. |
| Global frame blending (`apply_frame_blending`, knob-driven) | ES 2.0 (`leds.h:800`) | Per-frame temporal EMA across the whole strip. |
| 8-frame integer ring boxcar via SIMD | ES 2.0 (pitch) | See bug-shaped finding above. |

Motion-driver family:

| Driver | First seen | Used by |
|---|---|---|
| Raw waveform / `sample_history` | SB 3.0.0 (waveform) + ES 1.0 (plot, waveform) | Time-domain audio per LED. |
| `vu_level` / `audio_vu_level_average` | SB 3.0.0 (vu, vu_dot), ES 1.0 (analog, bloom) | RMS-style envelope. |
| Bin energy (`bins[]`, `spectrogram_smooth[]`) | SB 3.0.0 (gdft, bloom), ES 1.0 (spectrum, octave) | Frequency-domain bands. |
| Chromagram (12-pitch class) | SB 3.1.0 (gdft_chromagram) | Pitch-class energy folded from spectrum. |
| Per-band spectral flux (`calc_punch(low, high)`) | SB 3.2.0 (kaleidoscope) | Drives scroll velocity per band. |
| **Tempo bank phases (`tempi[i].phase` × NUM_TEMPI bins)** | ES 1.0 (hype, metronome) | Bank-of-Goertzels over 64–192 BPM. ES SIGNATURE — no SB equivalent. |
| Per-bin beat envelope `tempi[i].beat` | ES 1.0 (hype) | Sample of phase-magnitude product. |
| `tempo_confidence` (scalar 0–1) | ES 1.0 (spectronome) | Drives layer-composite gain. |
| `novelty_curve[]` history | ES 1.0 (debug, internally hype/metronome via tempo) | Onset-detection proxy. |
| Neural-network tensor outputs | ES 1.0 (neurons) | `hidden_neuron_*_values`, `output_neuron_values`. ES-unique. |
| `MOOD` knob (SB) / `speed` knob (ES) | SB 3.0.0+, ES 1.0+ | Global "responsiveness" knob. Modulates EMA rates / scroll speeds, NEVER what responds. |

Frame-coupling status (dt-correct vs frame-rate-coupled):

- **Dt-correct primitives observed:**
  - SB 4.x `low_pass_array_fixed(..., LED_FPS, ...)` — used ONLY by chromagram_dots
  - ES 1.0+ `apply_image_lpf` — global framebuffer trail, frame-rate-aware
  - ES 1.0+ `update_tempi_phase(delta)` — tempo phase advance (modes inherit when consuming `tempi[].phase`)
  - K1 `SbK1BaseEffect::smoothSpectrogram()` and `updateWaveformPeak()` — explicit tau via `α = 1 - exp(-dt/tau)`
- **Everything else is frame-coupled.** Constants are per-frame multipliers (0.99, 0.05, 0.25, etc). Apparent speed scales with frame rate.

## Mode-set evolution at a glance

```
SB 3.0.0:  gdft  gdft_chromagram  bloom_slow  bloom_fast  waveform  vu  vu_dot
SB 3.1.0:  (same)  +half-rate gate +ping-pong  +sub-pixel VU AA
SB 3.2.0:  gdft  gdft_chromagram  bloom_slow  bloom_fast            vu  vu_dot  +kaleidoscope (-waveform)
SB 4.0.0:  gdft  chromagram_gradient  chromagram_dots  vu_dot  kaleidoscope  bloom*  test_mode
            (*draw_sprite undefined; -vu bar; -gdft_chromagram retired)
SB 4.1.1:  (same as 4.0.0)  *draw_sprite supplied externally; bloom now functional

ES 1.0:    analog  bloom  hype  metronome  octave  spectronome  spectrum  +beta(debug, neurons, plot, waveform, neutral)
ES 1.1:    (same active 7)  +directory restructure
ES 1.2:    +beat_tunnel  +fft  +perlin  +tempiscope  (active 11)  +beta(4)
ES 2.0:    -perlin (→inactive)  +pitch  +temp  (active 10 + beta 3)  +SIMD pacing  +apply_frame_blending
```

Persistent across all 9 releases: a `bloom`-class spatial-transport mode (always sprite-based from SB 4.x / ES 1.0 onward), a chromagram/spectrum data-map mode (`gdft` / `octave`), a VU-driven dot mode (`vu_dot` / `analog`).

## What ES has that SB doesn't (and vice versa)

ES-unique primitives with NO SB analogue:

1. **Bank-of-Goertzels tempo phase rendered as N independent pendulum dots** (`metronome` × NUM_TEMPI ∈ {64, 128} bins). SB has at most one tempo phase; ES has dozens to hundreds.
2. **Tempo-phase-gated sprite injection** (`beat_tunnel`: only deposit colour into the scroll buffer when `|phase - 0.65| < 0.02`).
3. **Tempo-confidence layer compositing** (`spectronome`: `(1 - sqrt(sqrt(tempo_confidence)))*0.85 + 0.15` darkening between layers).
4. **Beat parity dots** (`hype`: odd vs even tempo bins as binary colour-axis).
5. **Global framebuffer LPF as universal trail mechanism** (`apply_image_lpf` with knob-driven dt-correct cutoff).
6. **`lpf_drag` cross-fade transitions** (mode-change override on the same global LPF).
7. **`fx_dots[]` global motion-blur cache** as an explicit shared resource (ES 2.0).
8. **`auto_color_cycle` global hue-motion driven by novelty momentum** (lives outside any mode).
9. **History-buffer-as-spatial-axis** (ES 2.0 debug: time as x-axis along strip).
10. **Auto-correlation pitch-period strip raster** (ES 2.0 pitch).
11. **Neural-network tensor visualisation** (ES 1.0+ neurons, currently disabled in production).
12. **ESP-DSP SIMD buffer ops** as the pacing primitive for dual-buffer modes (ES 2.0).
13. **Free-running internal phase accumulator decoupled from tempo** (ES 1.2/2.0 beat_tunnel: `angle += 0.001` per frame).

SB-unique primitives with NO ES analogue:

1. **Centre-origin geometry via `shift_leds_up(64) + mirror_image_downwards`** (SB 4.x signature). ES uses left-edge or symmetric in-loop write; never the SB shift+mirror pattern.
2. **`distort_logarithmic` `sqrt(prog)` non-linear spatial remap** (SB 3.x bloom). Removed in 4.x.
3. **`fade_top_half` linear spatial taper** (SB 3.x). Replaced by `prog²` end-fade in 4.1.1.
4. **VU-Dot velocity-length comet** (sweep-fill between previous and current position each frame) — SB 3.x mechanic; superseded by sprite trails in 4.x.
5. **Halo composite via `scale_image_to_half` + `scale_half_to_full` + RGB channel rotation + `blend_buffers(BLEND_ADD)`** (SB 3.2 vu_dot only; appeared once, never again).
6. **Cubic-shifted Perlin spatial input `(i+18)³`** (SB 3.2/4.x kaleidoscope). ES Perlin walks the y-axis in 1.2 but does not use cubic spatial scaling.
7. **Per-band attack-only follower with separate decay step** (SB 3.2/4.x kaleidoscope: `if (bin > br) br += dist*0.1; br *= 0.99` as two ops). ES uses sprite α decay or global LPF instead.
8. **`PRISM_COUNT` / `BULB_OPACITY` post-mode global compositor** (`apply_prism_effect` + `apply_bulb_cover` after every mode in `SENSORY_BRIDGE_FIRMWARE.ino:206-227`). ES has separate global passes (`apply_background`, `apply_image_lpf`, `apply_blue_light_filter`, white-balance LUT, dither) but no PRISM/BULB equivalents.

Shared primitives (in both lineages, often with parameter drift):

- Fractional-pixel sprite scroll with persistence α (SB 4.x `draw_sprite` 0.250+1.750×MOOD/0.99; ES `draw_sprite_float` 0.125+0.875×speed/0.99).
- Sub-pixel `draw_dot` with prev-position motion blur (SB `draw_dot`/`draw_line` `1/|Δ|`; ES `draw_dot` `1/(sqrt(d)*NUM_LEDS)`).
- Linear data-vector→strip resample via `interpolate`.
- Mirror-mode toggle producing centre-symmetric writes (parameter-level convention; no shared helper).

## K1 lineage gap analysis

Reference: `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h` (per `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` table).

What K1 SbK1* DOES inherit:
- `applyContrast(bin, squareIter)` ← SB 4.x `(b²)*0.65 + b*0.35` (preserved exactly, with corrected formula now confirmed via 4.1.1).
- `smoothSpectrogram()` ← SB 4.x `get_smooth_spectrogram()` — but K1 uses asymmetric tau (`kTauSpecAttack=4.4ms`, `kTauSpecDecay=9.1ms`) where SB 4.x is symmetric 0.75/0.75. **DIVERGENCE confirmed by SB 4.1.1 SSA — `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` claim of "asymmetric EMA" is incorrect; SB is symmetric.** K1's asymmetric choice may be deliberate, but it should be documented as a deviation, not a preservation.
- `updateWaveformPeak()` ← SB 4.x asymmetric max-follower (preserved with dt-correct conversion).
- `buildChromagram()` ← SB 4.x `note_colors[12]` / chromagram_smooth (extended from 64→12 to 96→12 bins).
- Centre-origin policy ← SB 4.x `shift_leds_up + mirror_image_downwards` (made mandatory across all K1 effects).
- Sprite-based bloom transport ← SB 4.1.1 `draw_sprite` (K1 `SbK1BloomEffect` uses dual PSRAM buffers + fractional-pixel scroll + `sqrt` spatial distortion — equivalent semantics, different impl).

What K1 has NOT inherited (gap candidates):
1. **ES tempo bank** — K1 has no equivalent of ES `tempi[NUM_TEMPI].phase` / `.beat`. K1's audio chain has tempo tracking but not as a phase-bank visible to effects.
2. **`apply_image_lpf` global framebuffer trail** — K1 does not have a global, knob-driven, dt-correct framebuffer EMA. Each effect manages its own persistence (or doesn't).
3. **`auto_color_cycle` global novelty-driven hue drift** — K1 effects use palette directly; no global novelty hue advance.
4. **`fx_dots[]` motion-blur cache** — K1 effects implement dot motion individually; no shared prev-position cache.
5. **`set_dot_position` + `draw_dot(motion_blur_via_line)` engine helpers** — K1 has no equivalent canonical sub-pixel dot primitive.
6. **`low_pass_array_fixed` dt-correct array LPF helper** — K1 has dt-correct EMA helpers but no array variant with explicit Hz cutoff.
7. **`tempo_confidence` layer compositing** — no K1 effect uses confidence-gated layer dimming.
8. **PRISM / BULB post-mode compositor** — K1 RendererActor has equivalent post-processing but no `PRISM_COUNT` knob exposing ES-style multi-mirror reflection.

## Bug-shaped findings worth flagging

Discovered by SSAs during forensic reads — none requires immediate K1 action, but each is worth caching for future research:

1. **SB 4.0.0 `draw_sprite` is undefined in the source tree** (grep-confirmed across the entire firmware). SB 4.0.0 bloom would not link as-shipped. Helper supplied externally / in 4.1.x.
2. **SB 4.1.1 chromagram_dots `memcpy(chromagram_last, chromagram_smooth, sizeof(float)*12)`** uses `sizeof(float)` for what is declared as a `SQ15x16` array. Accidentally correct on ESP32 (both 4 bytes), but a type-mismatch bug-shape.
3. **SB 4.1.1 kaleidoscope `speed_limit = 2000 + 2000*MOOD`** is computed but never applied. Dead code since at least 4.0.0.
4. **ES 2.0 pitch.h `average_index` declared local** rather than static — the 8-frame ring buffer may reset every call. Worth verifying on hardware.
5. **ES 2.0 tempiscope.h** computes `phase = 1.0 - ((tempi[i].phase + π) / (2π))` and never reads it. Dead / WIP.
6. **ES 1.2 waveform.h `1.0 /*-configuration.bass*/`** — bass knob commented out. Either intentional removal or in-progress refactor.

## SB asymmetric-EMA correction (callout)

`SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` Property #2 states: *"`get_smooth_spectrogram()` applies one smoothing pass with a single alpha per bin: attack `distance * 0.75` when rising, decay `distance * 0.75` when falling. This is a symmetric 75% EMA on the distance"*. The body text says **symmetric**; an earlier sentence in the same property says **asymmetric**. Both 4.0.0 and 4.1.1 SSAs independently confirmed: **the EMA is symmetric 0.75/0.75 — `spectrogram_smooth += distance * 0.75` on both rise and fall**. Recommend correcting the framework reconstruction document.

## Open questions for Captain (Phase 2 policy input)

1. **Should K1 import the ES tempo phase bank?** It is the single largest motion vocabulary ES has and SB does not. A K1 `TempoBank` actor exposing `tempi[N].phase` per-frame would unlock multiple ES-class modes (metronome pendulum swarm, hype beat-parity, beat_tunnel sprite gating). Cost: substantial DSP work; benefit: high. Not in scope for this research doc — flagged for separate decision.
2. **Should K1 add a global framebuffer trail equivalent to `apply_image_lpf`?** It would eliminate per-effect persistence boilerplate and supply a uniform softness knob. Cost: one new RendererActor pass; needs dt-correct Hz cutoff. Risk: every existing effect with its own trail mechanism would composite with the global, potentially producing double-trails. Captain decision.
3. **Cubic-spatial Perlin `(i+18)³` and per-band max-follower-driving-velocity** are unused outside SB kaleidoscope. Are they motion patterns the K1 effect catalogue should adopt as canonical "spatial-noise-with-audio-velocity" exemplars, or are they too niche?
4. **`draw_sprite` semantics differ between SB and ES** — SB uses additive accumulation onto destination; ES uses additive sub-pixel translation with self-feedback. Should K1's bloom port standardise on one or remain dual?
5. **The 6 bug-shaped findings** above — should any be reported upstream to the SB / ES maintainers, or are they out-of-scope for K1?

## Next steps

1. Captain reads this taxonomy in the context of `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md`.
2. Captain decides which (if any) ES-unique primitives are import candidates for K1 effect framework (separate planning track).
3. Agent corrects the asymmetric-EMA claim in `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` (one-line edit to Property #2 first sentence).
4. Agent assesses whether `EFFECT_FRAMEWORK_STANDARD.md` authoring should reference SB-only patterns or include ES vocabulary as an extension catalogue.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:opus-4.7-1M (orchestrator + 9 SSAs) | Created — synthesis of 9 parallel forensic SSA passes across SB 3.0.0 / 3.1.0 / 3.2.0 / 4.0.0 / 4.1.1 and ES 1.0 / 1.1 / 1.2 / 2.0. Catalogued every motion-mechanic primitive with provenance. Identified asymmetric-EMA error in SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md (SB EMA is symmetric 0.75/0.75). 6 bug-shaped findings flagged. K1 lineage gap analysis included. |
