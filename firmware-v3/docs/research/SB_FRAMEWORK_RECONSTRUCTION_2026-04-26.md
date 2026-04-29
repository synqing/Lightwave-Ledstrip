---
abstract: "Track D Phase 1 research output. Reconstructs the canonical Sensory Bridge 4.1 audio-reactive light-show framework from the source-of-truth at K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h, cross-validated against 8 prior LightwaveOS memory observations. 12 LOAD-BEARING properties identified (single-stage smoothing, asymmetric max follower, squaring contrast curve, centre-origin geometry, chromagram colour domain, silence-as-first-class-state, post-mode pipeline order, dt-correct smoothing, etc) with K1 lineage preservation table. Captain's Track D directive: research-first, then Captain decides policy on LOAD-BEARING vs INCIDENTAL classification before standards-doc authoring. Read when authoring EFFECT_FRAMEWORK_STANDARD.md or auditing existing effects against canonical SB grammar."
---

# Sensory Bridge 4.1 Framework Reconstruction

**Track D Phase 1 — research-output preservation, NOT policy.**

This document captures the canonical SB 4.1 framework as reconstructed from source. The next phase (Captain-authorised) is to ratify which properties are LOAD-BEARING (must be enforced across all effects) vs INCIDENTAL (variation acceptable), then author `EFFECT_FRAMEWORK_STANDARD.md` from the ratified rule set.

## Sources

| Source | Path | Used for |
|---|---|---|
| SB 4.1 lightshow modes | `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.0/SENSORY_BRIDGE_FIRMWARE/lightshow_modes.h` | Mode-by-mode brightness composition, smoothing topology |
| SB 4.1 globals | `K1.node1/.../globals.h` | CONFIG defaults (PRISM_COUNT, BULB_OPACITY, MOOD, etc.) |
| SB 4.1 main loop | `K1.node1/.../SENSORY_BRIDGE_FIRMWARE.ino` | Two-stage thread structure, post-mode pipeline order |
| SB 4.1 audio | `K1.node1/.../i2s_audio.h` | Asymmetric max-follower constants (waveform peak) |
| SB 4.1 GDFT | `K1.node1/.../GDFT.h` | MOOD scaling, chromagram derivation |
| SB 4.1 LED utilities | `K1.node1/.../led_utilities.h` | Brightness application, silent_scale, dithering |
| K1 SbK1Base port | `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h` | Lineage — what was preserved, what diverged, what was made dt-correct |
| LightwaveOS memory | observations #38029, #38092, #38148, #38150, #38199, #38247, #38248, #40407, #40411, #45875 | Cross-validation of prior framework analyses |

## The 12 LOAD-BEARING properties

### 1. Two-stage thread separation

Audio analysis (loop()) and LED rendering (led_thread()) run on separate cores/tasks. The LED thread calls `get_smooth_spectrogram()` and `make_smooth_chromagram()` at the start of each frame BEFORE dispatching to any mode function.

**Source:** `SENSORY_BRIDGE_FIRMWARE.ino:107, 186-188`

### 2. Single-stage post-mode smoothing on the spectrogram (symmetric EMA)

`get_smooth_spectrogram()` applies one smoothing pass with a single alpha per bin: attack `distance * 0.75` when rising, decay `distance * 0.75` when falling. This is a symmetric 75% EMA on the distance, applied ONCE before every mode call, to `spectrogram_smooth[]`. Mode functions read `spectrogram_smooth[]`, NOT raw `spectrogram[]`. Any mode that applies additional smoothing layers on top of this adds a second stage and violates the pattern.

**Source:** `lightshow_modes.h:1-16`

**Anti-pattern reference:** the 5L-AR triple-smoothing bug (LightwaveOS commit `aed805bb`, fixed 2026-03-24, hardware-verified 2026-03-23) is the canonical example of this rule violated. See `feedback_5lar_fix_pattern` memory.

### 3. Asymmetric max follower for waveform peak normalisation

`max_waveform_val_follower` uses fast attack `delta * 0.25` and extremely slow decay `delta * 0.005`. This is the canonical follower topology. `waveform_peak_scaled` is then derived from `max_waveform_val / max_waveform_val_follower` with its own 0.25/0.25 symmetric smoothing pass.

**Source:** `i2s_audio.h:106-127`

### 4. Audio-to-brightness via squaring (SQUARE_ITER), not raw linear

Every active mode applies brightness shaping via squaring: `bin = (bin * bin) * 0.65 + bin * 0.35` (or equivalent `bin *= bin` repeated `SQUARE_ITER` times). This is the K1/SB contrast curve — it compresses low values and boosts high values, making the response perceptually punchy. The base helper `applyContrast(bin, squareIter)` is the K1 port.

**Source:** `lightshow_modes.h:75-77` (gdft), `lightshow_modes.h:305-310` (kaleidoscope), `lightshow_modes.h:348-350` (chromagram_gradient)

### 5. Centre-origin symmetric geometry — all modes mirror

`shift_leds_up(leds_16, 64)` moves image to upper half, then `mirror_image_downwards(leds_16)` folds it to lower half. This makes all spectrogram modes symmetric from the centre outward. Bloom and VU dot use `set_dot_position()` with paired symmetric positions. Centre-origin is universal across all active modes.

**Source:** `lightshow_modes.h:94-96` (gdft), `lightshow_modes.h:212-213` (vu_dot), `lightshow_modes.h:339` (kaleidoscope), `lightshow_modes.h:496-498` (bloom)

### 6. Palette / hue driven by chromagram musical domain (12 bins), not 64 frequency bins

Colour always derives from `chromagram_smooth[12]` or `note_colors[12]` (one hue per pitch class), not from raw frequency bin index. In non-chromatic mode, `chroma_val + hue_position` provides the current palette position. In chromatic mode, `note_colors[i % 12]` maps each pixel to its musical pitch class.

**Source:** `lightshow_modes.h:85-91` (gdft), `lightshow_modes.h:353-356` (chromagram_gradient), `lightshow_modes.h:378-381` (chromagram_dots)

### 7. SATURATION gates chromatic vs monochromatic mode

All modes branch on `chromatic_mode` (a global bool), not locally. Chromatic mode assigns distinct hues per pitch class. Non-chromatic mode forces all pixels to a single hue derived from `chroma_val + hue_position`.

**Source:** `lightshow_modes.h:33, 72, 85, 328, 353, 378, 476`

### 8. Global brightness pipeline: `PHOTONS² × silent_scale` applied AFTER all mode rendering

`apply_brightness()` in `show_leds()` multiplies all pixels by `MASTER_BRIGHTNESS * (CONFIG.PHOTONS * CONFIG.PHOTONS) * silent_scale`. Mode functions write normalised [0,1] values; the global scale is applied once at output. **No mode function should hard-set absolute brightness.**

**Source:** `led_utilities.h:211`

### 9. Silence gating via `silent_scale` — first-class audio presence state

Silence is detected 10 s after amplitude drops below `SWEET_SPOT_MIN_LEVEL * 1.10`. `silent_scale` EMA-tracks the binary silence state (coefficient 0.1 new / 0.9 last). This is applied at output time (brightness pipeline) and also to `base_coat_width_scaled`. **Effects are not responsible for silence gating internally — it is a global post-process.**

**Source:** `i2s_audio.h:157-172`, `led_utilities.h:129, 667`

### 10. Post-mode global effects in fixed order

PRISM → BULB → incandescent → BASE_COAT → scale → dither. Defaults: `PRISM_COUNT=0`, `BULB_OPACITY=0.00`, `INCANDESCENT_FILTER=0.50`, `BASE_COAT=true`.

**Source:** `SENSORY_BRIDGE_FIRMWARE.ino:206-227`, `globals.h:76-79`

**Memory correction:** `reference_sb41_defaults` previously stated PRISM_COUNT defaults to 1.0 at 0.25 opacity. SSA cross-check 2026-04-25 confirmed `globals.h:78` factory default is **PRISM_COUNT=0**. The 1.0/0.25 figures may apply to a specific preset, not factory defaults. Verify which context applies before quoting.

### 11. MOOD knob controls responsiveness, not what responds

MOOD (0.0–1.0, default 0.05) affects scroll speed, follower mix rates (`mood_scale()`), and animation velocity — not the audio-to-visual mapping itself. In Bloom mode `MOOD_VAL` is forced to 1.0 for GDFT processing.

**Source:** `GDFT.h:60-63`, `lightshow_modes.h:185-186, 273, 279`

### 12. Rate-independent smoothing via tau constants in K1 port

All K1-side smoothing uses `alpha = 1 - exp(-dt / tau)` with explicit tau values derived from the original 120 FPS frame-coupled alphas. Frame-coupled constants in derived effects are a violation.

**Source:** `SbK1BaseEffect.h:164-173`

## INCIDENTAL properties (variation acceptable)

1. **Specific SQUARE_ITER count** — default is 1 iteration; chromatic mode adds +1 extra in GDFT. Tunable.
2. **Hue shift from brightness** — `(sqrt(bin) * 0.05) + (prog * 0.10) * hue_shifting_mix` adds a small brightness-correlated hue offset in non-chromatic mode. Incidental colour texture.
3. **Trail persistence mechanism** — Bloom uses spatial transport (fractional-pixel scroll + edge fade); Waveform uses exponential temporal decay; chromagram_gradient has no trail. Per-mode choice.
4. **Geometry within mode** — GDFT: horizontal spectrum. Chromagram dots: dot pairs. Bloom: scroll outward from centre. Kaleidoscope: full-strip Perlin. VU dot: amplitude-proportional dot position. Per-mode aesthetic.
5. **PRISM_COUNT and BULB_OPACITY values** — post-process decorators with defaults of 0. Per-preset variation permitted.
6. **`BASE_COAT` baseline glow** — default `true`; draws dim `1/256` level centre line scaled by `silent_scale`. Optional.
7. **Temporal dithering** — optional 4-step dither table at output quantisation. Default `true`.
8. **MOOD forced to 1.0 in Bloom** — Bloom overrides `MOOD_VAL = 1.0` for GDFT processing. Mode-specific override.

## SB 4.1 lightshow modes inventory

| Mode # | Name | Brightness composition | Smoothing | Palette use | Centre origin? |
|--------|------|------------------------|-----------|-------------|----------------|
| 0 | `light_mode_gdft` | `spectrogram_smooth[bin]` → squared `(x²×0.65 + x×0.35)` × SQUARE_ITER → `hsv(hue, SAT, bin)` | Single-stage 75% EMA pre-call + contrast squaring | `note_colors[i%12]` (chromatic) or `chroma_val+hue_pos` (mono) | Yes — `shift_leds_up(64)` + `mirror_image_downwards` |
| 1 | `light_mode_chromagram_gradient` | `chromagram_smooth[12]` interpolated to 64 bins × `0.9+0.1` → squared × SQUARE_ITER → `note_magnitude²` as V | Single-stage 75% EMA on chromagram_smooth | `interpolate(prog, note_colors, 12)` or `chroma_val+hue_pos` | Yes — writes `leds_16[64+i]` and `leds_16[63-i]` |
| 2 | `light_mode_chromagram_dots` | `chromagram_smooth[i]²` per 12 chroma bins → dot pair symmetric around centre | `low_pass_array_fixed(chromagram_smooth, last, 12, LED_FPS, mood_scale(3.5,1.5))` separate slow low-pass | `note_colors[i]` (chromatic) or `chroma_val+hue_pos` | Yes — paired dots |
| 3 | `light_mode_bloom` | `chromagram_smooth[i]²` → sum 12 bins with hue spread → scroll via `draw_sprite` with `0.250+1.750×MOOD` scale, 0.99 persistence | `draw_sprite` provides implicit temporal persistence | Chromagram additive sum with `hsv(prog, SAT, bin²×share)` | Yes — `leds_16[63]` and `leds_16[64]` as centre |
| 4 | `light_mode_vu_dot` | `audio_vu_level_average` → EMA → max follower → `sqrt(dot_pos_smooth)` as brightness | Two-stage: EMA (10/5%) then position EMA (25/24%) | `chroma_val + hue_position` (no chromatic branch) | Yes — `0.5±dot_pos_smooth×0.5` |
| 5 | `light_mode_kaleidoscope` | 3 band sums (low/mid/high, bins 0-20/20-40/40-60) → `bin×0.5 + bin²×0.5` → Perlin noise modulated by per-band brightness, squared × SQUARE_ITER | Per-band max followers `dist*0.1` attack, `*0.99` decay; pre-smoothed spectrogram | Chromatic: direct RGB from Perlin; non-chromatic: `chroma_val+hue_pos+sqrt(brightness)×0.05` | Yes — `leds_16[i] = leds_16[NATIVE_RESOLUTION-1-i]` |

## K1 lineage preservation table

| Property | SB 4.1 original | K1 SbK1* port | Status |
|----------|-----------------|---------------|--------|
| Single-stage pre-mode spectrogram smoothing (75% EMA) | `get_smooth_spectrogram()` called before every mode | `SbK1BaseEffect::smoothSpectrogram()` with asymmetric tau (kTauSpecAttack=4.4ms, kTauSpecDecay=9.1ms) | **Preserved** — converted to dt-correct tau; asymmetric replaces symmetric |
| Asymmetric max follower (attack 0.25, decay 0.005) | `i2s_audio.h:106-113` | `SbK1BaseEffect::updateWaveformPeak()` — kTauWfFollowerAttack=6.9ms, kTauWfFollowerDecay=99.9ms + kTauWfPeakLast=23.4ms | **Preserved** — dt-correct conversion |
| Squaring contrast curve `(x²×0.65 + x×0.35)` | `lightshow_modes.h:76` | `SbK1BaseEffect::applyContrast(bin, squareIter)` identical formula | **Preserved** — exact formula ported |
| Centre-origin symmetric geometry | `shift_leds_up` + `mirror_image_downwards` | All SbK1* effects write to centre indices and mirror; LightwaveOS `EFFECT_DEVELOPMENT_STANDARD` mandates centre-origin | **Preserved** — made mandatory policy |
| Chromagram 12-bin musical domain driving colour | `note_colors[12]` / `chromagram_smooth[12]` | `SbK1BaseEffect::buildChromagram()` folds 96 semitone bins to 12 chroma; `m_chromaSmooth[12]` | **Preserved** — extended 96→12 vs SB 64→12 |
| Audio-to-visual via `spectrogram_smooth[]` (not raw) | `spectrogram_smooth[bin]` in all mode render loops | K1 uses `m_ps->specSmooth[96]` built by `reconstructSpectrum96()` + `smoothSpectrogram()` | **Preserved** — same principle, higher resolution |
| SATURATION governs chromatic vs mono | `chromatic_mode` global bool | Per-effect `ctx.palette.saturation >= 128` or equivalent | **Preserved** — gating mechanism changed but semantic identical |
| Global brightness = `PHOTONS² × silent_scale` post-mode | `apply_brightness()` in `show_leds()` | LightwaveOS RendererActor handles global brightness; effects write normalised [0,1] | **Preserved** |
| Silence gating as first-class state | `silent_scale` EMA + 10s hysteresis | `ctx.audio.available` = false triggers trail fade | **Preserved** — per memory #38150 |
| MOOD = responsiveness not mapping | `mood_scale()` affects mix rates and scroll speeds | `ctx.speed` maps to scroll rate; audio mapping is fixed | **Preserved** |
| Single-stage smoothing only (no triple smoothing) | `get_smooth_spectrogram()` = one pass, modes do one additional squaring | 5L-AR initially shipped triple smoothing → broken → fixed `aed805bb` 2026-03-24 | **Fixed** — was deviated, corrected |
| Post-mode global pipeline | `SENSORY_BRIDGE_FIRMWARE.ino:206-227` | LightwaveOS RendererActor applies equivalent post-processing | **Preserved** |
| Palette-free chromagram colour summation | `calc_chromagram_color()` direct CRGB accumulation | `synthesizeChromaColor()` additive CRGB_F accumulation with `1.0/peak` normalisation | **Preserved** |
| Rate-independent smoothing | SB: frame-coupled alphas at ~120 FPS | K1: tau constants with `alpha = 1 - exp(-dt/tau)` | **Preserved** — converted correctly |
| No local silence attenuation in effects | Global `silent_scale` handles dimming | Per remediation plan, no local silence gating in effect code | **Preserved** |
| NATIVE_RESOLUTION 128 LEDs, NUM_FREQS 64 bins | `constants.h:8-9` | K1 uses 160 LEDs; SbK1* uses 96 semitone bins not 64 GDFT bins | **Deviated** — resolution increased; bin count changed |
| Bloom persistence via `draw_sprite` scroll (spatial transport) | `draw_sprite(leds_16, leds_16_prev, 128, 128, 0.250+1.750×MOOD, 0.99)` | `SbK1BloomEffect` dual PSRAM buffers with fractional-pixel scroll + sqrt spatial distortion | **Preserved** — equivalent mechanism, different implementation |

## Open questions for Captain (Track D Phase 2: policy)

Before authoring `EFFECT_FRAMEWORK_STANDARD.md`:

1. **Which of the 12 LOAD-BEARING properties become MUST in the standard, vs SHOULD?** All 12 were preserved in K1 lineage, but the standard could enforce them with different rigour. E.g. centre-origin is already a MUST per `EFFECT_DEVELOPMENT_STANDARD.md`, but rate-independence might be SHOULD (with violations flagged but not gated).

2. **Are the 8 INCIDENTAL properties truly free, or are some "INCIDENTAL but commonly violated"?** E.g. trail persistence mechanism is per-effect choice — but should the standard provide canonical reference implementations of each pattern (spatial / temporal / none) so effects don't reinvent?

3. **Resolution divergence** — K1 uses 160 LEDs and 96 semitone bins instead of SB's 128 LEDs / 64 GDFT bins. Is the 96-bin chromagram path a permanent K1 choice, or should new effects target the 64-bin SB-parity path for cross-platform fidelity?

4. **Existing-effect audit scope after standard lands** — should every existing effect be audited against the new standard, or only new effects from a cutoff date? Centre-origin audit hit 9 violations; a full 12-rule audit will likely find more.

## Next steps

1. Captain reads this doc, ratifies which properties are MUST vs SHOULD vs INCIDENTAL.
2. Agent authors `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` from ratified rule set.
3. Agent dispatches audit SSA(s) to score existing effects against new standard.
4. Captain decides remediation backlog (in-tree refactors vs `isExperimental` tagging vs deregistration).

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:opus-4.7-1M | Created — Track D Phase 1 research preservation. Reconstructed SB 4.1 framework from `K1.node1/references/Sensorybridge.sourcecode/` source-of-truth. 12 LOAD-BEARING + 8 INCIDENTAL properties identified, K1 lineage preservation table built. Awaits Captain Phase 2 policy ratification before `EFFECT_FRAMEWORK_STANDARD.md` authoring. |
