---
abstract: "Per-effect synthesis mapping K1's 4 spazzing effects (ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision) to canonical SB / Emotiscope motion cousins with verbatim upstream snippets, port-ready C++ pseudo-code using the K1 EffectContext audio API, identity-preservation rules (centre origin 79/80, dual-strip mirror, kMaxZones, fadeToBlackByDt), and risk/A-B test plan. Read when redesigning the four broken motion effects from the canonical baseline rather than from current K1 implementations."
---

# Canonical Per-Effect Port — K1 Spazz Redesign (2026-04-30)

## Methodology

I read the four K1 effect implementations (ChevronWavesEffect, ChevronWavesEffectEnhanced, SnapwaveLinearEffect, LGPWaveCollisionEffect) end-to-end, then read the canonical motion sources cited in the K1 file headers and comments: SB 3.1.0 `lightshow_modes.h` (`light_mode_bloom`, `light_mode_waveform`, `light_mode_vu_dot`), SB 4.1.1 `lightshow_modes.h` (`light_mode_bloom` rewrite using `draw_sprite`), and Emotiscope 1.2 `bloom.h`, `beta/waveform.h`, `beat_tunnel.h`, `metronome.h`, `spectronome.h`, `perlin.h`, `hype.h` plus `leds.h` (`draw_sprite`). The K1 audio surface (`EffectContext.h::AudioContext`) was verified directly. For each broken K1 effect I identify the closest canonical cousin by motion topology (advection vs oscillation vs interference), copy the verbatim upstream code that produces the desired motion, diff what K1 added that the canonical does not have (the spazz drivers), and write port-ready C++ that uses the K1 audio API but keeps the canonical structure. K1-specific identity (centre origin at LED 79/80, dual-strip mirror, per-zone state, palette, `fadeToBlackByDt`) is layered on top of the canonical motion, not woven through it.

A critical finding up-front: **none of the four K1 effects has a 1:1 cousin in the canonical sources**. SB does not have a "snapwave" mode (the K1 SnapwaveLinear file header lies — `light_mode_snapwave` does not exist in SB 3.1.0 / 4.1.0 / 4.1.1). Emotiscope 1.2 does not have a "chevron" or "wave-collision" mode. The closest canonical cousins are motion-topology matches, not name matches. The canonical pattern is uniform across all four: **the source is positional advection of a CRGBF buffer via `draw_sprite()` with a slow alpha decay; the audio drives a single scalar (`vu_level`, `chromagram_smooth`, `tempi_magnitude`) and the spatial structure emerges from buffer history, not from `sin(k*x - phase)` per-pixel computation**. This is the architectural recommendation that propagates through every section below.

---

## ChevronWavesEffect (K1 EID 18)

### 1. Closest canonical cousin

**Emotiscope 1.2 `bloom.h::draw_bloom()` (lines 3–39)** combined with **SB 4.1.1 `lightshow_modes.h::light_mode_bloom()` (lines 398–499)**. There is no canonical V-shaped chevron. The visual that K1 actually produces (sharpened periodic peaks travelling outward from centre) is a misimplementation; the canonical that delivers the *intended* "outward propagation from centre" gestalt is bloom with `mirror_mode == true`, where audio energy is injected at the centre seam and `draw_sprite` advects the previous frame outward each call.

### 2. Verbatim canonical motion code (Emotiscope 1.2 `bloom.h:1-39`)

```c
float novelty_image_prev[NUM_LEDS] = { 0.0 };

void draw_bloom() {
    float novelty_image[NUM_LEDS] = { 0.0 };

    float spread_speed = 0.125 + 0.875*configuration.speed.value.f32;
    draw_sprite(novelty_image, novelty_image_prev, NUM_LEDS, NUM_LEDS, spread_speed, 0.99);

    novelty_image[0] = (vu_level);
    novelty_image[0] = min( 1.0f, novelty_image[0] );

    if(configuration.mirror_mode.value.u32 == true){
        for(uint16_t i = 0; i < NUM_LEDS>>1; i++){
            float progress = num_leds_float_lookup[i<<1];
            float novelty_pixel = clip_float(novelty_image[i]*1.0);
            CRGBF color = hsv(get_color_range_hue(progress),
                              configuration.saturation.value.f32, novelty_pixel);
            leds[ (NUM_LEDS>>1)    + i] = color;   // outward right of centre
            leds[((NUM_LEDS>>1)-1) - i] = color;   // outward left of centre
        }
    }
    memcpy(novelty_image_prev, novelty_image, sizeof(float)*NUM_LEDS);
}
```

`draw_sprite` (Emotiscope `leds.h:89-108`):
```c
void draw_sprite(float dest[], float sprite[], uint32_t dest_length, uint32_t sprite_length,
                 float position, float alpha) {
    int16_t position_whole = floor(position);
    float position_fract = fabsf(fabsf(position) - fabsf(position_whole));
    for (int16_t i = 0; i < sprite_length; i++) {
        int16_t pos_left  = i + position_whole;
        int16_t pos_right = i + position_whole + 1;
        float mix_right = position_fract;
        float mix_left  = 1.0 - mix_right;
        if (pos_left  >= 0 && pos_left  < dest_length) dest[pos_left]  += sprite[i] * mix_left  * alpha;
        if (pos_right >= 0 && pos_right < dest_length) dest[pos_right] += sprite[i] * mix_right * alpha;
    }
}
```

### 3. What the canonical does that K1 does not

- **Buffer advection, not per-pixel oscillator.** Canonical bloom maintains a `novelty_image_prev[NUM_LEDS]` and shifts it by `spread_speed` LEDs each frame using `draw_sprite`. The travelling pattern is an emergent property of the persistence buffer, not of `sinf(k*dist - phase)`.
- **Audio drives a single scalar source pixel** (`novelty_image[0] = vu_level`). Energy is *injected* at one position and the buffer carries it outward. K1 evaluates audio inside the per-pixel loop (`tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth))`) which couples noise into every LED simultaneously.
- **Alpha fade is global and constant** (0.99). One smoothing path. K1 has six concurrent smoothing paths (chroma history rolling sum, AsymmetricFollower energy, AsymmetricFollower delta, Spring speed, EMA chroma hue, snare sharpness decay), each of which can chatter independently.
- **Position interpolation is sub-pixel-accurate** via `mix_left` / `mix_right`. K1 has no spatial smoothing — it samples `sinf` at integer LED positions, so any phase-rate jitter aliases visibly.

### 4. What K1 does that the canonical does not (the spazz-causing additions)

- **Per-pixel `sinf(distFromCenter * 0.25f - m_chevronPos)` evaluated each frame.** Phase is shared across 160 LEDs, so any phase-velocity perturbation moves all pixels simultaneously — exactly what produces "spazz".
- **`tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth))`** — energy modulates *edge sharpness*, so when bass hits, every chevron edge snaps sharper at once. This is not motion, this is global brightness chatter.
- **Hue computed per-pixel from `m_chevronPos`** (`fmodf(m_chevronPos * 0.5f, 256.0f)`). When phase rate hiccups, the entire palette index shifts as a unit.
- **Six concurrent audio inputs to the renderer** (chroma sum, energyAvg, energyDelta, heavyBand 1+2 average, isSnareHit, gHue). Canonical bloom uses ONE.

### 5. Port-ready C++ pseudo-code

```cpp
// ChevronWavesEffect.cpp — canonical-bloom port
// The "V-shape from centre" gestalt comes from mirror_mode advection of a
// 1D persistence buffer driven by a single audio scalar. No per-pixel sinf().

#include "ChevronWavesEffect.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include "effects/PersistenceHelpers.h"
#include <cstring>

using lightwaveos::effects::persistence::fadeToBlackByDt;

namespace lightwaveos { namespace effects { namespace ieffect {

bool ChevronWavesEffect::init(plugins::EffectContext& ctx) {
    // PSRAM-allocated persistence buffer (per-zone for ZoneComposer)
    if (!m_ps) {
        m_ps = static_cast<ChevronPsram*>(
            heap_caps_malloc(sizeof(ChevronPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(ChevronPsram));
    for (uint8_t z = 0; z < kMaxZones; ++z) {
        m_vuFollower[z] = enhancement::AsymmetricFollower{0.0f, 0.020f, 0.150f};
    }
    return true;
}

void ChevronWavesEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = ctx.getSafeDeltaSeconds();
    constexpr uint16_t kHalf = STRIP_LENGTH / 2;  // 80 — outward radius

    // ---- Single audio scalar drives the source pixel (canonical: vu_level) ----
    float vu = 0.0f;
    if (ctx.audio.available) {
        // Use heavyBass for sub-bass body. heavyChroma summed gives tonal weight.
        vu = ctx.audio.heavyBass();
        if (vu > 1.0f) vu = 1.0f;
    }
    float vuSmooth = m_vuFollower[z].update(vu, dt);  // single smoother, fast attack

    // ---- Spread speed: gentle baseline, modest audio modulation ----
    // Canonical: 0.125 + 0.875 * config.speed. We map ctx.speed (1..100) -> [0,1]
    // and scale by a calm 0.6..1.2 LED/frame so the wavefront travels rather than races.
    float speedNorm = ctx.speed / 100.0f;
    float spreadLedsPerFrame = (0.6f + 0.6f * speedNorm);
    // Subtle audio "push" — envelope-shaped, not per-frame chatter:
    spreadLedsPerFrame *= (0.85f + 0.30f * vuSmooth);

    // ---- Advect previous radial buffer outward by spreadLedsPerFrame ----
    // m_ps->radialPrev[z][r] is the canonical "previous frame" in radial coordinates,
    // r = 0..kHalf-1 (centre to edge). draw_sprite shifts r outward by `position`.
    float radialNew[kHalf] = {0.0f};
    drawSpriteFloat(radialNew, m_ps->radialPrev[z], kHalf, kHalf,
                    spreadLedsPerFrame, /*alpha=*/0.985f);

    // ---- Inject audio energy at the centre seam (radius 0) ----
    radialNew[0] += vuSmooth;
    if (radialNew[0] > 1.0f) radialNew[0] = 1.0f;

    // ---- Persist for next frame ----
    memcpy(m_ps->radialPrev[z], radialNew, sizeof(radialNew));

    // ---- fadeToBlackByDt clears trail across full strip (K1 identity) ----
    fadeToBlackByDt(ctx.leds, ctx.ledCount, ctx.fadeAmount, dt);

    // ---- Mirror radial buffer outward from centre 79/80 to both halves of strip 1 ----
    for (uint16_t r = 0; r < kHalf; ++r) {
        float lum = radialNew[r];
        if (lum < 0.005f) continue;
        // Hue: chromagram-derived (NOT phase-derived — kills the rainbow chatter)
        uint8_t chromaHue = effects::chroma::circularChromaHueSmoothed(
            ctx.audio.heavyChroma(), m_chromaAngle[z], dt, 0.20f);
        uint8_t hue = (uint8_t)(ctx.gHue + chromaHue + r);  // mild radial gradient
        uint8_t bri = (uint8_t)(lum * 255.0f * (ctx.brightness / 255.0f));
        CRGB col = ctx.palette.getColor(hue, bri);

        // Centre origin: r=0 hits LEDs 79 and 80; r=kHalf-1 hits LEDs 0 and 159.
        const uint16_t left  = (kHalf - 1) - r;            // 79..0
        const uint16_t right = kHalf + r;                  // 80..159
        if (left  < ctx.ledCount) ctx.leds[left]  += col;
        if (right < ctx.ledCount) ctx.leds[right] += col;

        // Strip 2 mirror (K1 dual-strip identity)
        if (right + STRIP_LENGTH < ctx.ledCount) {
            // Slight hue offset preserves the K1 dual-strip parallax look.
            CRGB col2 = ctx.palette.getColor((uint8_t)(hue + 90), bri);
            const uint16_t left2  = STRIP_LENGTH + left;
            const uint16_t right2 = STRIP_LENGTH + right;
            if (left2  < ctx.ledCount) ctx.leds[left2]  += col2;
            if (right2 < ctx.ledCount) ctx.leds[right2] += col2;
        }
    }
}

// drawSpriteFloat: direct port of Emotiscope draw_sprite (float overload).
// Linear sub-pixel interpolation. NOT a global helper yet — see "Shared K1 helpers".
}}}
```

Header additions (sketch):
```cpp
struct ChevronPsram { float radialPrev[kMaxZones][80]; };  // 80 = STRIP_LENGTH/2
ChevronPsram* m_ps = nullptr;
enhancement::AsymmetricFollower m_vuFollower[kMaxZones];
float m_chromaAngle[kMaxZones] = {0};
```

### 6. Identity preservation

- **Centre origin 79/80**: explicit — radial buffer `r=0` writes to LEDs 79 and 80. Edge `r=79` writes to LEDs 0 and 159. No `getDistanceFromCenter()` needed; the indexing IS centre-origin.
- **Dual-strip mirror**: explicit branch writes to `STRIP_LENGTH + left` / `STRIP_LENGTH + right` with hue offset `+90` (matches existing K1 chevron palette twist).
- **kMaxZones**: `radialPrev[kMaxZones][80]` and `m_vuFollower[kMaxZones]` per-zone; ZoneComposer-safe.
- **Palette**: `ctx.palette.getColor(hue, bri)` only — no fixed CHSV.
- **fadeToBlackByDt**: applied with `ctx.fadeAmount` (user-adjustable) — gives the trailing decay K1 expects.
- **PSRAM**: per CLAUDE.md `MEMORY_ALLOCATION.md` rule, `radialPrev[4][80] * sizeof(float) = 1280 B` is over the 64 B threshold; allocate from PSRAM with `MALLOC_CAP_SPIRAM`.
- **No heap in render()**: zero. `radialNew[80]` is stack-local and small.
- **British English**: comments use "centre", "colour" already in pseudo-code where applicable.

### 7. Risk assessment

- **Alpha=0.985 may bloom too long** at 120 FPS (decay constant ~67 frames ≈ 560 ms). The Emotiscope canonical runs at ~100 FPS. A/B test alpha {0.97, 0.985, 0.992} on hardware. Faster alpha = punchier, less smear.
- **`spreadLedsPerFrame` interaction with `ctx.speed`**: speed=100 gives ~1.2 LED/frame, speed=1 gives ~0.6. Both should look sane; verify there's no integer-only behaviour at speed≈0.5 LED/frame (sub-pixel mix should handle it; that's what `mix_left`/`mix_right` are for).
- **Sub-pixel `draw_sprite` accumulation**: because each call adds to dest with `alpha=0.985`, there's no upper bound without per-frame clamp. The canonical bloom relies on `clip_float(novelty_image[i]*2.0)` at the read site. Add a per-LED clamp to `[0, 1]` at write time (already in pseudo-code via `> 1.0f` check).
- **Spazz regression**: the only audio-driven temporal variable now is `vuSmooth` (one AsymmetricFollower). The current effect has six. This is the deliberate simplification. **A/B test must specifically include a quiet-passage-into-loud-passage transition** (e.g. song intro into drop) — that's where the old chatter was loudest. Compare on hardware: if the new version pulses too gently on the drop, raise the `+ 0.30f * vuSmooth` modulation coefficient; do NOT add a second smoother.
- **Hardware A/B**: also test pure tonal content (sustained chord) where chroma smoothing dominates. Verify the rainbow drift is gentle, not chattery.

---

## ChevronWavesEffectEnhanced (K1 EID 90)

### 1. Closest canonical cousin

Same as ChevronWavesEffect (Emotiscope `bloom.h::draw_bloom()`), with the **PLL-style beat lock** of Emotiscope `metronome.h::draw_metronome()` (lines 1–55) layered on. There is no canonical "enhanced bloom"; the Emotiscope enhancement vector for tempo-synced visuals is `metronome` (uses `tempi[i].phase` directly to position content) and `beat_tunnel.h`.

### 2. Verbatim canonical motion code

The bloom advection is identical to ChevronWavesEffect (above). Beat-locked positioning from Emotiscope `metronome.h:6-43`:

```c
for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
    float progress = float(tempo_bin) / NUM_TEMPI;
    float tempi_magnitude = tempi_smooth[tempo_bin];
    float contribution = (tempi_magnitude / tempi_power_sum) * tempi_magnitude;
    if (contribution >= 0.00001) {
        float sine = sin( tempi[tempo_bin].phase + (PI*0.5) );
        sine *= 1.5;
        if (sine > 1.0) sine = 1.0; else if (sine < -1.0) sine = -1.0;
        float metronome_width = (mirror_mode) ? 0.5 : 1.0;
        float dot_pos = clip_float( sine * (0.5*sqrt(contribution) * metronome_width) + 0.5 );
        float opacity = clip_float(contribution*1.0);
        // ... draw_dot at dot_pos with opacity
    }
}
```

Key lessons from the canonical: **the beat phase drives a position scalar (`dot_pos`), not a per-pixel argument**. Confidence (`contribution`) drives *opacity*, not phase. When confidence is low, the dot fades; phase is never wrestled into a free-running oscillator.

### 3. What the canonical does that K1 does not

- **Tempo confidence gates opacity, not phase rate.** When confidence drops, the visual fades instead of de-syncing. K1 Enhanced uses confidence to switch a Schmitt trigger (`m_tempoLocked`) and either applies or skips PLL correction — the underlying oscillator (`m_chevronPos`) keeps free-running with `+= speedNorm * 240.0f * smoothedSpeed * dt` regardless. When confidence wobbles around 0.5, K1 jitters between locked and free-run states.
- **Beat phase is read directly** — `sin(tempi[bin].phase + PI/2)`. No phase-error wrapping, no shortest-path arithmetic, no `correctionAlpha = 1 - exp(-dt/tau)` math layered on top.
- **The advection stays the same with or without tempo lock.** Tempo info adds a *second* dot via `draw_dot`; it does not modulate the bloom's spread speed.

### 4. What K1 does that the canonical does not (spazz drivers, beyond the inherited ChevronWaves drivers)

- **PLL P-correction layered onto a free-running phase** (`m_chevronPos += phaseError * correctionAlpha`). The correction tau is 100 ms, but `m_chevronPos` is also being incremented by `speedNorm * 240 * smoothedSpeed * dt`. The two interact whenever `tempoConfidence` crosses 0.4 / 0.6, producing visible phase pops at lock acquisition.
- **`m_snareSharpness *= powf(0.90f, rawDt * 60.0f)`**, then `tanhScale = 2.0 + m_snareSharpness * 3.0` per pixel. Snare hits jab the entire strip's edge sharpness simultaneously. Two snares 80 ms apart double-stack the sharpness on one frame.
- **`m_chromaTargets[12]` updated only on `newHop` but `m_chromaSmoothed[12]` updated every frame.** This is correct in principle but pumps 12 followers per frame for a single colour readout — wasted CPU and an additional smoothing path.
- **`m_subBassFollower` fed by `m_targetSubBass` which is never assigned anywhere in `render()`**. This is dead code (verified: `m_targetSubBass` is only set to 0.0 in `init()`). The "enhancement" is wired but disconnected.
- **`PHASE_DOMAIN = 628.3f` (100·2π) wraparound** vs sin's natural 2π — the wrap arithmetic itself is fine, but the 100x scale means small phase deltas have to traverse a long range to reach a beat-aligned target, amplifying jitter.

### 5. Port-ready C++ pseudo-code

```cpp
// ChevronWavesEffectEnhanced.cpp — canonical-bloom + canonical-metronome overlay.
// Inherits the bloom advection from ChevronWavesEffect; adds a centre-origin
// "tempo dot" that pulses ON-BEAT only when confidence is high. Tempo never
// modulates the bloom's spread speed — that decoupling is what kills the spazz.

void ChevronWavesEnhancedEffect::render(plugins::EffectContext& ctx) {
    // ---- Layer 1: canonical bloom (identical to ChevronWavesEffect port) ----
    renderBloomLayer(ctx);   // Same buffer-advection code as base ChevronWaves port.

    // ---- Layer 2: tempo-locked centre pulse (canonical metronome pattern) ----
    if (!ctx.audio.available) return;

    float tempoConf = ctx.audio.tempoConfidence();
    if (tempoConf < 0.35f) return;   // Below confidence floor, no pulse — no PLL chatter.

    // Canonical metronome reads phase directly. K1 audio API exposes beatPhase()
    // as 0..1 (already wrapped). Convert to sin domain.
    float phase01 = ctx.audio.beatPhase();
    float sinePulse = sinf(phase01 * 2.0f * PI + (PI * 0.5f));   // peaks on the beat
    if (sinePulse > 1.0f) sinePulse = 1.0f;
    if (sinePulse < 0.0f) sinePulse = 0.0f;

    // Confidence drives OPACITY, not phase rate (canonical rule).
    float opacity = sinePulse * tempoConf * tempoConf;   // squared softens low-conf

    // Snare adds an instant brightness lift at the centre — single-frame, decays naturally.
    float snareKick = ctx.audio.isSnareHit() ? 0.6f : 0.0f;
    opacity += snareKick;
    if (opacity > 1.0f) opacity = 1.0f;

    // Place a sub-pixel dot at radius 0 (LEDs 79.5 — already centred).
    uint8_t chromaHue = effects::chroma::circularChromaHueSmoothed(
        ctx.audio.heavyChroma(), m_chromaAngle[z], dt, 0.20f);
    uint8_t hue = (uint8_t)(ctx.gHue + chromaHue);
    uint8_t bri = (uint8_t)(opacity * 255.0f * (ctx.brightness / 255.0f));
    CRGB pulseCol = ctx.palette.getColor(hue, bri);

    enhancement::SubpixelRenderer::renderPoint(
        ctx.leds, STRIP_LENGTH, 79.5f, pulseCol, bri);
    if (STRIP_LENGTH * 2 <= ctx.ledCount) {
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds + STRIP_LENGTH, STRIP_LENGTH, 79.5f,
            ctx.palette.getColor((uint8_t)(hue + 90), bri), bri);
    }
}
```

Notes:
- **No PLL.** The bloom's spread speed has no tempo input. The tempo overlay is a dot, not a phase correction. This is the canonical decoupling.
- **`beatPhase()` consumed directly** in 0..1 form — no `* 628.3f` scaling, no error wrapping.
- **Confidence gates visibility, not behaviour.** Below 0.35 the pulse vanishes; above 0.35 it ramps in proportionally. No Schmitt trigger, no lock states.

### 6. Identity preservation

- All ChevronWavesEffect identity (centre origin, dual-strip, kMaxZones, palette, fadeToBlackByDt, PSRAM advection buffer) inherited from Layer 1.
- **Snare-driven brightness lift retained** (the K1-specific feel) but as a clean opacity addition, not a per-pixel sharpness multiplier.
- **Tempo lock visible only when meaningful** — the canonical "confidence drives visibility" rule means the user sees a clean pulse on beat-tracking-friendly material (EDM, rock) and a clean bloom on confidence-poor material (jazz, vocals).
- **HeavyChroma palette**: retained — kills bin-flip rainbow chatter.

### 7. Risk assessment

- **Beat phase quality is now the single point of failure.** If `ctx.audio.beatPhase()` itself is jittery, the centre dot will jitter even at high confidence. Prerequisite hardware A/B: log `beatPhase()` and `tempoConfidence()` to serial during the test track and verify monotonic phase progression with confidence > 0.6.
- **Confidence threshold 0.35**: if too low, the dot appears on confidence-poor material and looks wrong; if too high, it never appears. Test {0.30, 0.35, 0.45} on EDM (high conf), rock (medium), and vocal (low) material.
- **No fallback motion when audio drops.** The Layer 2 pulse vanishes — that's intentional. Layer 1 (bloom) keeps running on `ctx.audio.heavyBass()` so the visual doesn't go dead.
- **Hardware A/B**: the original Enhanced was supposed to be "tempo-aware". Captain must confirm whether the new gentle-pulse-when-confident behaviour matches that intent, or whether they actually want phase-locked wave fronts (in which case the answer is to reject this design and instead use Emotiscope `beat_tunnel.h` as the cousin — which IS phase-driven advection. That is a more invasive port; flag for Captain decision.).

---

## SnapwaveLinearEffect (K1 EID 98)

### 1. Closest canonical cousin

**SB 3.1.0 `lightshow_modes.h::light_mode_vu_dot()` (lines 334–399)** — a single dot whose position is driven by `waveform_peak_scaled` (a single audio scalar), with sub-pixel interpolation, plus **SB 3.1.0 `light_mode_waveform()` (lines 160–260)** for the chromagram-derived colour. There is **no canonical light_mode_snapwave** — the K1 file header citation is a hallucination. The closest motion topology is "single-dot positional with trail", which is canonically `vu_dot`.

### 2. Verbatim canonical motion code (SB 3.1.0 `light_mode_vu_dot()`, lines 334–399)

```c
void light_mode_vu_dot() {
    const float led_share = 255 / float(12);
    static float sum_color_last[3] = {0, 0, 0};
    static float led_pos_last = 0;
    float smoothing = (0.025 + CONFIG.MOOD * 0.975) * 0.25;
    float led_pos = waveform_peak_scaled * (NATIVE_RESOLUTION - 1);
    static float led_pos_smooth = 0.0;
    led_pos_smooth = led_pos * (smoothing) + led_pos_smooth * (1.0 - smoothing);
    if (led_pos_smooth > NATIVE_RESOLUTION - 2) led_pos_smooth = NATIVE_RESOLUTION - 2;
    else if (led_pos_smooth < 0) led_pos_smooth = 0;

    CRGB sum_color = CRGB(0, 0, 0);
    if (chromatic_mode == true) {
        for (uint8_t c = 0; c < 12; c++) {
            float prog = c / float(12);
            float bin = note_chromagram[c] * (1.0 / chromagram_max_val);
            CRGB out_col;
            hsv2rgb_spectrum(CHSV(255 * prog, 255, led_share * bin), out_col);
            sum_color += out_col;
        }
    }
    // ... colour smoothing 0.05/0.95 EMA ...

    fadeToBlackBy(leds, NATIVE_RESOLUTION, 255);   // FULL CLEAR — no trail in source

    if (led_pos_last < led_pos_smooth) {
        for (uint8_t i = led_pos_last; i <= led_pos_smooth; i++) {
            leds[i] = CRGB(...);
            leds[i + 1] = CRGB(...);
        }
    }
    // (else block paints in the other direction)
    led_pos_last = led_pos_smooth;
}
```

The trail in `vu_dot` is implicit: it paints the entire range from `led_pos_last` to `led_pos_smooth` solid, so a fast-moving dot leaves a "swept" line. K1's history-buffer trail (40 frames) is a separate K1 invention, not from SB.

### 3. What the canonical does that K1 does not

- **Position is driven by a single scalar (`waveform_peak_scaled`), not by a sum of 12 per-note oscillators.** The K1 oscillator formula `sum(chromagram[i] * sinf(timeMs * BASE_FREQ * (1 + 0.5*i)))` is a custom compound oscillator with 12 phase-incoherent inputs — it does not exist in any canonical SB source.
- **Smoothing is a single first-order EMA on position** (`led_pos_smooth = led_pos * smoothing + ...`). One pole. K1 has an AsymmetricFollower on `peakSmoothed`, an AsymmetricFollower on `smoothRms`, plus `tanhf(oscillation * 3.0)` snap, plus the per-frame oscillation recompute.
- **Colour is one EMA-smoothed RGB triple** (alpha 0.05). Frame to frame, colour drifts smoothly. K1 recomputes `computeChromaColor()` fresh every frame from the latest 12 chroma bins with no persistence.
- **No history buffer.** The trail is implicit in the line-paint between previous and current position. K1 maintains a 40-entry ring buffer in PSRAM and renders all 40 entries with quadratic age fade — this is K1-specific, not canonical.

### 4. What K1 does that the canonical does not (the spazz drivers — this is the most structural)

- **Compound time-based oscillation** `oscillation += chromaVal * sinf(timeMs * 0.001 * (1 + 0.5*i))`. Twelve phase-incoherent sines summed each frame. When chroma values shift (which they do every audio hop), the sum changes character abruptly. `tanhf(oscillation * 3.0)` then maps that into a hard-snap [-1, +1] range, so any chroma fluctuation is amplified.
- **Energy gate `rms < 0.05f → return 0`** as a hard-zero return. The dot snaps to centre instantaneously when energy drops below threshold. No exit hysteresis, no smooth glide-back. Below-threshold transient drops cause the dot to teleport.
- **Dynamic fade `fadeAmount = 20 + 40 * (1 - smoothRms)`**. Loud → fade=20 (long trail), quiet → fade=60 (short trail). When energy oscillates near silence, fade swings between 20 and 60 frame to frame, modulating trail length stroboscopically.
- **Two AsymmetricFollowers stacked on the same audio scalar** (`m_peakFollower` for `currentPeak`, `m_rmsFollower` for `rmsEnergy`, both reading `ctx.audio.rms()`). Two smoothers on the same input is one too many.
- **History buffer with quadratic age fade `ageFactor² * 255`** — produces a sharp falloff, but combined with the dynamic fade above, the trail never reaches steady state.
- **Strip 2 separately rendered** with an independent loop over the same history buffer (lines 287–321). Should be a memcpy mirror; instead it's a re-render. Code duplication, but not a spazz driver per se.

### 5. Port-ready C++ pseudo-code (full structural rewrite)

```cpp
// SnapwaveLinearEffect.cpp — canonical vu_dot port.
// CRITICAL: This is a structural rewrite. The compound 12-sine oscillator and
// hard energy gate are deleted. The history buffer is replaced with the canonical
// "paint between last and current position" sweep. The result is a single dot
// whose distance from centre tracks a single audio scalar smoothly.

bool SnapwaveLinearEffect::init(plugins::EffectContext& ctx) {
    for (uint8_t z = 0; z < kMaxZones; ++z) {
        // Single position smoother (canonical: 0.025 + MOOD*0.975 * 0.25)
        // Translated to a single first-order EMA. Mood maps to the rate.
        m_lastPos[z]      = 0.0f;
        m_smoothedPos[z]  = 0.0f;
        m_colorEMA_R[z]   = 0.0f;
        m_colorEMA_G[z]   = 0.0f;
        m_colorEMA_B[z]   = 0.0f;
    }
    // PSRAM no longer needed for history — the canonical doesn't use one.
    return true;
}

void SnapwaveLinearEffect::render(plugins::EffectContext& ctx) {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = ctx.getSafeDeltaSeconds();
    constexpr uint16_t kHalf = STRIP_LENGTH / 2;  // 80

    // ---- Mood-driven smoothing rate (canonical: 0.025 + MOOD*0.975 * 0.25) ----
    // MOOD=0 (reactive)  -> alpha ≈ 0.006 per frame  -> snappy
    // MOOD=1 (smooth)    -> alpha ≈ 0.250 per frame  -> dreamy
    // Convert frame-rate-tied alpha to dt-corrected lambda for stability.
    float moodNorm   = ctx.getMoodNormalized();
    float frameAlpha = (0.025f + 0.975f * moodNorm) * 0.25f;
    // Convert per-frame alpha to per-second lambda (frame-rate independent):
    //   alpha_per_frame ≈ 1 - exp(-lambda * dt_at_120fps)
    // Solve for lambda at canonical 120 FPS (dt = 0.00833):
    float lambda    = -logf(1.0f - frameAlpha) / 0.00833f;
    float dtAlpha   = 1.0f - expf(-lambda * dt);

    // ---- Single audio scalar drives position (canonical: waveform_peak_scaled) ----
    // K1 has SB-parity waveform peak. Use it; fall back to RMS.
    float peak = 0.0f;
    if (ctx.audio.available) {
        peak = ctx.audio.hasSbWaveform() ? ctx.audio.sbWaveformPeakScaled() : ctx.audio.rms();
        if (peak > 1.0f) peak = 1.0f;
    }

    // Target distance from centre: 0..kHalf-1.
    float targetDist = peak * (kHalf - 1);

    // ---- Single-pole EMA on position (canonical) — NO compound oscillator ----
    m_smoothedPos[z] += (targetDist - m_smoothedPos[z]) * dtAlpha;
    if (m_smoothedPos[z] > kHalf - 1) m_smoothedPos[z] = kHalf - 1;
    if (m_smoothedPos[z] < 0.0f)      m_smoothedPos[z] = 0.0f;

    // ---- Chromagram-derived colour, EMA-smoothed (canonical 0.05/0.95) ----
    float sumR = 0, sumG = 0, sumB = 0;
    if (ctx.audio.available) {
        for (uint8_t c = 0; c < 12; ++c) {
            float bin = ctx.audio.getHeavyChroma(c);
            uint8_t hue = (uint8_t)((c / 12.0f) * 255.0f + ctx.gHue);
            CRGB n = ctx.palette.getColor(hue, 255);
            sumR += n.r * bin;
            sumG += n.g * bin;
            sumB += n.b * bin;
        }
        // Per-frame normalisation to avoid white-out on dense chords:
        float total = sumR + sumG + sumB;
        if (total > 0.001f) {
            float scale = 255.0f / fmaxf(255.0f, total / 3.0f);
            sumR *= scale; sumG *= scale; sumB *= scale;
        }
    } else {
        CRGB fb = ctx.palette.getColor(ctx.gHue, 255);
        sumR = fb.r; sumG = fb.g; sumB = fb.b;
    }
    // Frame-rate independent EMA (alpha=0.05 at 120 FPS canonical → lambda ≈ 6.16/s)
    const float colorLambda = 6.16f;
    float cAlpha = 1.0f - expf(-colorLambda * dt);
    m_colorEMA_R[z] += (sumR - m_colorEMA_R[z]) * cAlpha;
    m_colorEMA_G[z] += (sumG - m_colorEMA_G[z]) * cAlpha;
    m_colorEMA_B[z] += (sumB - m_colorEMA_B[z]) * cAlpha;

    float briScale = ctx.brightness / 255.0f;
    CRGB dotColor = CRGB(
        (uint8_t)fminf(m_colorEMA_R[z] * briScale, 255.0f),
        (uint8_t)fminf(m_colorEMA_G[z] * briScale, 255.0f),
        (uint8_t)fminf(m_colorEMA_B[z] * briScale, 255.0f));

    // ---- Static fadeToBlackByDt (canonical: full clear OR fixed fade — pick fixed) ----
    // K1 identity prefers a trail. Static fade=ctx.fadeAmount, NOT a dynamic
    // fade tied to RMS (the dynamic fade is a K1 spazz driver — removed).
    fadeToBlackByDt(ctx.leds, ctx.ledCount, ctx.fadeAmount, dt);

    // ---- Centre-origin sub-pixel dot at distance m_smoothedPos[z] ----
    // distance 0 → LEDs 79 and 80; distance kHalf-1 → LEDs 0 and 159.
    float distF = m_smoothedPos[z];
    float leftPosF  = (kHalf - 1) - distF;            // 79..0
    float rightPosF = (float)kHalf + distF;           // 80..159

    enhancement::SubpixelRenderer::renderPoint(ctx.leds, STRIP_LENGTH, leftPosF,  dotColor, 255);
    enhancement::SubpixelRenderer::renderPoint(ctx.leds, STRIP_LENGTH, rightPosF, dotColor, 255);

    // ---- "Sweep paint" between last and current position (canonical line-fill) ----
    // This produces the implicit trail vu_dot is famous for. Range is small
    // (typical frame-to-frame motion < 5 LEDs at 120 FPS).
    float lastDist = m_lastPos[z];
    int   lo = (int)fminf(lastDist, distF);
    int   hi = (int)fmaxf(lastDist, distF);
    if (hi - lo > 1) {
        for (int d = lo; d <= hi; ++d) {
            uint16_t L = (kHalf - 1) - d;
            uint16_t R = kHalf + d;
            if (L < ctx.ledCount) ctx.leds[L] += dotColor;
            if (R < ctx.ledCount) ctx.leds[R] += dotColor;
        }
    }
    m_lastPos[z] = distF;

    // ---- Strip 2 mirror (K1 dual-strip identity) ----
    if (ctx.ledCount > STRIP_LENGTH) {
        CRGB col2 = CRGB(dotColor.r, dotColor.g, dotColor.b);  // same colour, no shift
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds + STRIP_LENGTH, STRIP_LENGTH, leftPosF,  col2, 255);
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds + STRIP_LENGTH, STRIP_LENGTH, rightPosF, col2, 255);
        if (hi - lo > 1) {
            for (int d = lo; d <= hi; ++d) {
                uint16_t L2 = STRIP_LENGTH + (kHalf - 1) - d;
                uint16_t R2 = STRIP_LENGTH + kHalf + d;
                if (L2 < ctx.ledCount) ctx.leds[L2] += col2;
                if (R2 < ctx.ledCount) ctx.leds[R2] += col2;
            }
        }
    }
}
```

### 6. Identity preservation

- **Centre origin 79/80**: explicit — `distF=0` paints LEDs 79 and 80 (sub-pixel so 79.5 actually).
- **Dual-strip mirror**: explicit second SubpixelRenderer call against `ctx.leds + STRIP_LENGTH`.
- **kMaxZones**: per-zone state `m_lastPos`, `m_smoothedPos`, `m_colorEMA_*` indexed by `z`.
- **Palette**: chroma colour built from `ctx.palette.getColor(hue, 255)` — palette-driven.
- **fadeToBlackByDt**: applied with `ctx.fadeAmount` (NOT a dynamic RMS-driven fade). User controls trail length.
- **PSRAM no longer required** — the 40-frame history buffer (`distanceHistory[4][40] + colorHistory[4][40] = 640 B + 1920 B = 2560 B`) is deleted. Net memory saved.
- **British English**: comments use "centre", "colour".
- **Mood knob**: respected — `getMoodNormalized()` controls smoothing rate (canonical SB pattern).

### 7. Risk assessment

- **Loss of long history trail**: K1's 40-frame quadratic-fade trail is gone. The new effect's trail comes from `fadeToBlackByDt` + the canonical line-sweep. **Hardware A/B must compare trail-length perception** — Captain may want the longer trail. If so, the cleanest add-back is to render past sub-pixel positions from a small ring buffer (5–10 entries, NOT 40) with a single age-fade pass; do not reintroduce the dynamic-fade-by-RMS chatter.
- **`sbWaveformPeakScaled` availability**: `ctx.audio.hasSbWaveform()` may be false on some configurations. Fallback to `rms()` is safe but feels different (RMS is energy-like, peak is amplitude-like). If `hasSbWaveform()` is consistently false on K1 V2 32 kHz, hardcode RMS and remove the branch.
- **Mood-rate conversion**: the `lambda = -log(1 - frameAlpha) / 0.00833` math assumes 120 FPS reference. If frame timing wanders (drops to 60 FPS during heavy rendering), the perceived smoothing rate stays correct (that's the point of dt-corrected EMA), but verify with `esp_timer_get_time()` profiling.
- **Stripped energy gate**: K1 had a hard `rms < 0.05 → 0` gate. Removed. The dot will now glide smoothly to centre on quiet passages instead of teleporting. Captain must confirm this is the intended feel — if they specifically wanted "silence = stillness", reintroduce as a smooth multiplier `peak *= smoothstep(0.03, 0.07, rms)`, not as a hard return.
- **Chord colour normalisation**: the per-frame divide-by-total is approximate; on dense chords colour drifts toward the dominant note. Canonical SB does the same. Verify on chord-rich material (jazz, full-band rock).
- **Hardware A/B**: must include silent-to-loud transitions, sustained tonal content, and percussive content. The previous SnapwaveLinear "spazz" was loudest on percussive content with chord activity — that's the must-pass test.

---

## LGPWaveCollisionEffect (K1 EID 17)

### 1. Closest canonical cousin

**Emotiscope 1.2 `beat_tunnel.h::draw_beat_tunnel()` (lines 1–49)**. This is the canonical "wave collision at centre" topology: a CRGBF buffer is advected by a position derived from `sin(angle)`, and the source samples are placed at positions derived from `tempi[i].phase`. The collision-at-centre look comes from `mirror_mode == true` mirroring at index `NUM_LEDS>>1`. There is **no canonical `light_mode_wave_collision`**.

The K1 effect's two-counter-propagating-sines-summed-to-form-standing-waves model (`sin(k*x - phase) + sin(k*x + phase)`) is mathematically real (it produces standing waves) but is not how any canonical SB / Emotiscope visual achieves the look. The canonical achieves "collision" via mirror-mode advection + centre-aligned source.

### 2. Verbatim canonical motion code (Emotiscope `beat_tunnel.h:5-46`)

```c
void draw_beat_tunnel(){
    memset(tunnel_image, 0, sizeof(CRGBF)*NUM_LEDS);

    angle += 0.001;
    float position = (0.125 + 0.875*configuration.speed.value.f32)*(sin(angle)) * 0.5;
    draw_sprite(tunnel_image, tunnel_image_prev, NUM_LEDS, NUM_LEDS, position, 0.965);

    for(uint16_t i = 0; i < NUM_TEMPI; i++){
        float phase = 1.0 - ((tempi[i].phase + PI) / (2.0*PI));
        float mag = 0.0;
        if( fabs(phase - 0.65) < 0.02 ){
            mag = clip_float(tempi_smooth[i]);
        }
        CRGBF tempi_color = hsv(get_color_range_hue(num_tempi_float_lookup[i]),
                                configuration.saturation.value.f32, mag);
        tunnel_image[i].r += tempi_color.r;
        tunnel_image[i].g += tempi_color.g;
        tunnel_image[i].b += tempi_color.b;
    }

    if(configuration.mirror_mode.value.u32 == true){
        for(uint16_t i = 0; i < NUM_TEMPI-2; i++){
            leds[ (NUM_LEDS>>1)    + ((i+2)>>1)] = tunnel_image[i];
            leds[((NUM_LEDS>>1)-1) - ((i+2)>>1)] = tunnel_image[i];
        }
    }
    memcpy(tunnel_image_prev, tunnel_image, sizeof(CRGBF)*NUM_LEDS);
}
```

The defining canonical features:
- **Position is `sin(angle) * 0.5`** — bounded oscillation, so the buffer slides back and forth around centre. Audio modulates the position offset, not the rate.
- **Source pixels are gated by phase** (`fabs(phase - 0.65) < 0.02`) — only fire when each tempo's phase passes a specific point. This is the "collision event" pattern.
- **Single advection alpha = 0.965**.

### 3. What the canonical does that K1 does not

- **One advected buffer, mirror-mode rendered.** Canonical produces "collision" via mirroring a single buffer at the centre; K1 sums two analytic sin waves per pixel and calls that interference.
- **Position-bounded oscillation.** `sin(angle)*0.5` means the buffer never travels more than half a strip's worth in either direction. K1's `m_phase += 240 * smoothedSpeed * dt` is monotonically increasing (modulo `628.3f`), wrapping arithmetic notwithstanding.
- **Audio drives event firing, not phase rate.** `tempi[i].phase` passing a threshold fires a colour pixel. K1 modulates `m_phase` rate by `bassEnergy` and `m_speedTarget`, then `m_collisionBoost` from snare hits on top.

### 4. What K1 does that the canonical does not (the spazz drivers)

- **Counter-propagating sin sum evaluated per pixel** (`sinf(d * 0.15 - phase) + sinf(d * 0.15 + phase)`). This is `2 * sin(k*d) * cos(phase)` analytically — i.e. a *static* spatial pattern (`sin(k*d)`) modulated by a *temporal* envelope (`cos(phase)`). It does not produce travelling collision events; it produces a spatial sin-wave whose amplitude wobbles. The "collision" the user perceives is `m_collisionBoost * exp(-d * 0.12)` overlaid separately, NOT the wave sum.
- **Hi-hat speed boost `m_speedTarget = 1.6f` then decays at 0.95 per frame.** Hi-hats fire often. Each one yanks the phase rate. The phase rate change ripples through the analytic sin sum, making the "wobble" (cos(phase) envelope) chatter every hi-hat hit.
- **`tanhf(interference * 2.0) * 0.5 + 0.5` per pixel** — same edge-sharpening problem as ChevronWaves.
- **`nblend(ctx.leds[i], newColor, 180)` at 70/30 mix** — overrides the `fadeToBlackByDt` decay, so the trail is partially the previous frame's nblend residue. This is incoherent persistence: half-fade, half-nblend.
- **`m_collisionBoost += energyDeltaSmooth * 0.4f`** when no snare — silent accumulator. Energy delta is noisy. Boost grows from noise even when no real onsets occurred.
- **Per-frame 4-input audio mix in the per-pixel loop**: `0.4 + 0.5*energyAvgSmooth + 0.4*energyDeltaSmooth` for `audioIntensity`, plus `m_collisionBoost`, plus `chromaHue`. Four temporal signals, each with its own smoother, all writing into the same brightness equation per pixel.

### 5. Port-ready C++ pseudo-code

```cpp
// LGPWaveCollisionEffect.cpp — canonical beat_tunnel port.
// CRITICAL: counter-propagating-sin model is deleted. Replaced with
// a single advected radial buffer (centre-out) where audio onset events
// (snare, kick) inject pixels at radial positions chosen by chroma.
// Mirror-mode rendering produces the "collision at centre" gestalt.

bool LGPWaveCollisionEffect::init(plugins::EffectContext& ctx) {
    if (!m_ps) {
        m_ps = static_cast<CollisionPsram*>(
            heap_caps_malloc(sizeof(CollisionPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(CollisionPsram));
    m_angle = 0.0f;
    return true;
}

void LGPWaveCollisionEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
    const float dt = ctx.getSafeDeltaSeconds();
    constexpr uint16_t kHalf = STRIP_LENGTH / 2;  // 80 — radial extent

    // ---- Bounded oscillator angle (canonical: angle += 0.001 per frame) ----
    // K1 dt-correct: 0.001 per frame at 120 FPS = 0.12 rad/s.
    m_angle += 0.12f * dt;
    if (m_angle > 2.0f * PI) m_angle -= 2.0f * PI;

    // ---- Position offset for sprite advection (canonical: sin(angle) * 0.5) ----
    // Bounded oscillation, NOT monotonic phase advance. Audio softens.
    float speedNorm = ctx.speed / 100.0f;
    float audioPush = ctx.audio.available ? ctx.audio.heavyBass() : 0.0f;
    float position  = (0.125f + 0.875f * speedNorm) * sinf(m_angle) * 0.5f;
    position *= (0.7f + 0.4f * audioPush);   // very mild audio coupling

    // ---- Advect previous radial buffer (canonical alpha = 0.965) ----
    float radialNew[kHalf] = {0.0f};
    drawSpriteFloat(radialNew, m_ps->radialPrev[z], kHalf, kHalf, position, 0.965f);

    // ---- Inject onset events at chroma-chosen radii (canonical: phase gate) ----
    // Kick → injection at small radius (near centre, "deep"). Snare → mid radius.
    // Hi-hat → far radius (edge "shimmer"). Each event is one-frame; the buffer
    // advection carries it. NO smoothed accumulator (kills the noise build-up).
    if (ctx.audio.available) {
        if (ctx.audio.isKickHit()) {
            // Inject at radius 2-4 (near centre)
            for (uint8_t r = 2; r <= 4; ++r) {
                radialNew[r] += 1.0f * ctx.audio.kickLevel();
            }
        }
        if (ctx.audio.isSnareHit()) {
            // Inject at radius 25-30 (mid)
            for (uint8_t r = 25; r <= 30; ++r) {
                radialNew[r] += 1.0f * ctx.audio.snare();
            }
        }
        if (ctx.audio.isHihatHit()) {
            // Inject at radius 55-60 (far)
            for (uint8_t r = 55; r <= 60; ++r) {
                radialNew[r] += 0.7f * ctx.audio.hihat();
            }
        }
    }

    // Clamp once per cell (canonical: clip_float).
    for (uint16_t r = 0; r < kHalf; ++r) {
        if (radialNew[r] > 1.0f) radialNew[r] = 1.0f;
    }

    // Persist for next frame.
    memcpy(m_ps->radialPrev[z], radialNew, sizeof(radialNew));

    // ---- fadeToBlackByDt (K1 identity — single coherent decay path) ----
    fadeToBlackByDt(ctx.leds, ctx.ledCount, ctx.fadeAmount, dt);

    // ---- Centre-origin mirror render (canonical mirror_mode) ----
    uint8_t chromaHue = effects::chroma::circularChromaHueSmoothed(
        ctx.audio.heavyChroma(), m_chromaAngle, dt, 0.20f);

    for (uint16_t r = 0; r < kHalf; ++r) {
        float lum = radialNew[r];
        if (lum < 0.005f) continue;
        // Hue from chroma + radial position (mild gradient, NOT phase-driven).
        uint8_t hue = (uint8_t)(ctx.gHue + chromaHue + r * 2);
        uint8_t bri = (uint8_t)(lum * 255.0f * (ctx.brightness / 255.0f));
        CRGB col   = ctx.palette.getColor(hue, bri);

        const uint16_t left  = (kHalf - 1) - r;
        const uint16_t right = kHalf + r;
        if (left  < ctx.ledCount) ctx.leds[left]  += col;
        if (right < ctx.ledCount) ctx.leds[right] += col;

        // Strip 2 mirror with K1 hue offset (+90, matches family pattern).
        if (right + STRIP_LENGTH < ctx.ledCount) {
            CRGB col2 = ctx.palette.getColor((uint8_t)(hue + 90), bri);
            const uint16_t left2  = STRIP_LENGTH + left;
            const uint16_t right2 = STRIP_LENGTH + right;
            if (left2  < ctx.ledCount) ctx.leds[left2]  += col2;
            if (right2 < ctx.ledCount) ctx.leds[right2] += col2;
        }
    }
}
```

Header additions (sketch):
```cpp
struct CollisionPsram { float radialPrev[kMaxZones][80]; };
CollisionPsram* m_ps = nullptr;
float m_angle = 0.0f;
float m_chromaAngle = 0.0f;
static constexpr uint8_t kMaxZones = 4;
```

### 6. Identity preservation

- **Centre origin 79/80**: radial buffer indexing makes centre the first cell; no `centerPairDistance` indirection.
- **Dual-strip mirror**: explicit second-strip render with hue offset `+90`.
- **kMaxZones**: per-zone radial buffer.
- **Palette + chromaHue**: retained; kills the per-pixel `nblend(180)` colour-mixing chatter.
- **fadeToBlackByDt**: SOLE persistence path. No nblend overlay — this fixes the "incoherent persistence" issue.
- **PSRAM**: `radialPrev[4][80]*4 = 1280 B`, allocated SPIRAM.
- **No heap in render()**: `radialNew[80]` stack-local.
- **Standing-wave / collision gestalt**: now produced by the canonical mechanism (mirror-mode advection of a centre-injected event buffer), not by analytic sin sums. The collision flash on snare hits remains visible — it's the ring at radius 25–30 expanding outward and inward simultaneously.

### 7. Risk assessment

- **"Wave collision" perception**: the K1 effect's perceptual signature was the centre flash on snare. The new design retains that (snare injects at radius 25–30 which moves outward via sprite advection AND inward via mirror, meeting at centre on the next sweep cycle). However, the new motion is "rings of energy" rather than "two waves colliding". Captain must confirm this matches intent. If not, add a stronger centre-burst on simultaneous kick+snare.
- **No more `m_collisionBoost` accumulator**: the silent-accumulation-from-noise bug is gone. Hi-hat speed-burst is gone (kept ONLY as injection event). If Captain wanted the speed-burst feel, it can be added back as a small, dt-correct `position` modulation gated by `isHihatHit()` — but only as a single-frame event, not a smoothed target.
- **Onset event quality**: same caveat as ChevronWavesEnhanced — the new design depends on `isKickHit()`, `isSnareHit()`, `isHihatHit()` being reliable. If onset detection is noisy, the rings will fire spuriously. Pre-test by logging onset triggers to serial during test material.
- **Buffer "back-fill" on edges**: with `position = sin(angle) * 0.5`, the sprite oscillates back and forth across the buffer. When it reverses, edge cells may starve. This is the canonical behaviour and produces the "tunnel breathing" feel; if too pronounced, lower the position amplitude (multiply `* 0.3f` instead of `* 0.5f`).
- **Hardware A/B**: must include high-percussion content (drum solo, EDM drop) where the previous spazz was worst. Verify rings emerge cleanly per onset; verify no "noise rings" during sustained tonal content.

---

## Implementation order

| Order | Effect | Rationale |
|------|--------|-----------|
| 1 | **SnapwaveLinearEffect** | Most structural rewrite (compound oscillator deleted, history buffer deleted). Largest risk, simplest dependency graph (no shared helper changes). Doing this first locks in the canonical "single audio scalar drives one position" doctrine for the whole batch. |
| 2 | **ChevronWavesEffect** | Introduces the `drawSpriteFloat` shared helper (see below). Once SnapwaveLinear is hardware-validated, the bloom-advection pattern is established and its A/B coverage informs ChevronWaves tuning. |
| 3 | **LGPWaveCollisionEffect** | Reuses `drawSpriteFloat` from step 2. The onset-event injection pattern is the most aggressive change to perceptual identity — leaving it for third lets steps 1-2 pre-validate the canonical-bloom motion model. |
| 4 | **ChevronWavesEffectEnhanced** | Pure overlay on ChevronWavesEffect (Layer 1 = base bloom, Layer 2 = canonical metronome dot). Cannot land before step 2. Smallest risk because the bloom layer is already validated. |

Do NOT port all four in parallel. Each port must be hardware-A/B'd before the next starts; the canonical motion model is unfamiliar to K1 and needs perceptual calibration before the next variant is layered on.

---

## Shared K1 helpers needed

1. **`drawSpriteFloat()`** — direct port of Emotiscope `leds.h:89-108` (the `float[]` overload of `draw_sprite`). Linear sub-pixel interpolation, alpha-multiplied accumulate. Should live at `firmware-v3/src/effects/utils/SpriteAdvection.h` as a `static inline` helper.

   ```cpp
   static inline void drawSpriteFloat(float* dest, const float* sprite,
                                       uint16_t destLen, uint16_t spriteLen,
                                       float position, float alpha) {
       int16_t posWhole = (int16_t)floorf(position);
       float   posFract = fabsf(fabsf(position) - fabsf((float)posWhole));
       for (int16_t i = 0; i < (int16_t)spriteLen; ++i) {
           int16_t pL = i + posWhole;
           int16_t pR = pL + 1;
           float mR = posFract;
           float mL = 1.0f - mR;
           if (pL >= 0 && pL < destLen) dest[pL] += sprite[i] * mL * alpha;
           if (pR >= 0 && pR < destLen) dest[pR] += sprite[i] * mR * alpha;
       }
   }
   ```

   No PSRAM, no allocations, no FastLED dependency. Pure float math. Reusable by future canonical-bloom-style ports.

2. **PSRAM-backed radial buffer pattern** — three of the four ports need `float radialPrev[kMaxZones][80]` allocated from `MALLOC_CAP_SPIRAM`. Consider a header-only template `template<uint16_t Radii> struct RadialPersist { ... allocate_psram(); ... advect(); ... }` if a fourth ports needs it; for four sites, copy-paste is fine.

3. **`circularChromaHueSmoothed()`** — already exists at `firmware-v3/src/effects/ieffect/ChromaUtils.h:94`. No changes needed.

4. **`enhancement::SubpixelRenderer::renderPoint()`** — already exists in `SmoothingEngine.h`. Used by SnapwaveLinear and ChevronWavesEnhanced ports.

5. **NO new audio API** — every K1 audio call in the pseudo-code (`heavyBass()`, `heavyChroma()`, `isKickHit()`, `isSnareHit()`, `isHihatHit()`, `kickLevel()`, `snare()`, `hihat()`, `beatPhase()`, `tempoConfidence()`, `hasSbWaveform()`, `sbWaveformPeakScaled()`, `rms()`) was verified against `EffectContext.h`. No additions to AudioContext required.

---

### Update — 2026-04-30: relaunch SSA convergence + implementation-order disagreement

A second SSA reading (relaunched after Anthropic usage cap) independently converged on the same canonical-cousin choice for three of the four effects (bloom topology for ChevronWavesEnhanced and LGPWaveCollision, derived single-scalar motion for Snapwave) and identified `light_mode_kaleidoscope` (SB 4.1.0 lines 224–341) as the closer cousin for vanilla `ChevronWavesEffect` because that mode's three-band attack-only `brightness_low/_mid/_high` followers + scroll-position-driven-by-band-SUM pattern is the canonical "bands drive intensity, hue drifts smoothly, no phase accumulator" template. The relaunch also confirmed the spazz diagnoses verbatim: phase-velocity bound to bass + tanh saturation gain bound to smoothed energy + chord-coupled compound oscillator + per-pixel SQUARE_ITER contrast.

**Implementation-order disagreement to escalate.** This document recommends `SnapwaveLinear → ChevronWaves → LGPWaveCollision → ChevronWavesEnhanced` (rationale: lock canonical doctrine on the highest-risk rewrite first). The Captain's relaunch prompt cited `PORT_PLAN.md` as recommending `LGPWaveCollision` first (rationale: bloom-cousin lowest perceptual divergence, validates the persistence-buffer pattern before reuse). Both orderings are defensible:

| Ordering rationale | Strengths | Weaknesses |
|---|---|---|
| `SnapwaveLinear → ChevronWaves → LGPWaveCollision → ChevronWavesEnhanced` (this doc) | Locks single-audio-scalar doctrine on the structural rewrite first; smaller dependency graph; failure on first port informs all three remaining | Snapwave's beat-tracker dependency is the biggest unknown; if K1 `tempoBeatConfidence()` is unreliable on quiet music, the whole rewrite stalls before any port lands |
| `LGPWaveCollision → ChevronWavesEnhanced → ChevronWaves → SnapwaveLinear` (PORT_PLAN.md) | Bloom topology is the most familiar visual; LGPWaveCollision keeps the snare-flash identity; ChevronWavesEnhanced reuses the same persistence helper; defers Snapwave's beat-tracker risk to last | If the first port reveals canonical doctrine is wrong for K1 perception (e.g. centre origin disagrees with bloom's edge-injection origin), three ports must be reworked |

This is an upstream-fact-gated decision: the right ordering depends on (F1) measured K1 `tempoBeatConfidence()` quality on the corpus benchmark, (F2) Captain's perceptual tolerance for "bands drive intensity" vs "phase accumulator" visual character, (F3) whether `PORT_PLAN.md` was authored with these tradeoffs documented or as a default ordering. Captain to resolve.

The relaunch SSA also recommended hoisting the bloom-cousin mechanic to a shared header (`firmware-v3/src/effects/CanonicalBloomHelpers.h`) so LGPWaveCollision and ChevronWavesEnhanced share `buildChordSeedColor()`, `scrollPersistenceBuffer()`, `applyEdgeFade()` rather than copy-paste. This is consistent with shared-helper item #2 above (PSRAM-backed radial buffer pattern) — same idea, different slice.

No new files were created; this update lives in the canonical doc per project policy.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:Claude (claude-opus-4-7-1m) | Created. Synthesised four K1 broken effects against canonical SB 3.1.0 / 4.1.1 and Emotiscope 1.2 sources. Identified `light_mode_snapwave` as a non-existent citation in the K1 SnapwaveLinearEffect file header. Recommended canonical-bloom buffer-advection pattern (Emotiscope `bloom.h::draw_bloom`) as the unifying motion model for all four ports, with metronome-dot overlay (Emotiscope `metronome.h`) for tempo-aware variants and onset-event injection for collision. Each port keeps centre-origin 79/80, dual-strip mirror, kMaxZones, palette, fadeToBlackByDt, PSRAM, and no-heap-in-render identity. Included implementation order (SnapwaveLinear → ChevronWaves → LGPWaveCollision → ChevronWavesEnhanced), shared `drawSpriteFloat` helper, and per-effect hardware A/B test plan focused on quiet-to-loud transitions and percussive content where the original spazz was worst. |
| 2026-04-30 | agent:deep-technical-analyst (relaunch after usage cap) | Appended convergence + disagreement section. Independent re-reading converged on bloom topology for ChevronWavesEnhanced/LGPWaveCollision/Snapwave-derivation, and added SB 4.1.0 `light_mode_kaleidoscope` lines 224–341 as the closer cousin for vanilla ChevronWavesEffect (three-band attack-only follower + scroll-driven-by-band-SUM, NOT phase accumulator). Surfaced implementation-order disagreement vs PORT_PLAN.md as upstream-fact-gated Captain decision (depends on K1 tempo-confidence quality, perceptual tolerance, and whether PORT_PLAN.md tradeoffs are documented). Recommended a shared `CanonicalBloomHelpers.h` header for `buildChordSeedColor` / `scrollPersistenceBuffer` / `applyEdgeFade` — consistent with the existing shared-helper guidance in the document. No new files; appended in-place per project policy. |
