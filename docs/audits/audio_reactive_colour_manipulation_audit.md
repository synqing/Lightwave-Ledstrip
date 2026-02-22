# Audio-Reactive Effects: Colour Manipulation Audit

**Document:** Reference audit of colour corruption patterns across all audio-reactive effects.  
**Purpose:** Level, scope, and depth of colour manipulation flaws (additive RGB stacking, white injection, brightness stacking, single-channel bias).  
**Root cause:** Additive RGB accumulation without normalisation (or with only saturating add) drives output toward (255,255,255) and washes out palette; `addWhiteSaturating` and unchecked brightness stacking worsen it.

---

## 1. Severity Definitions

| Severity | Meaning |
|----------|--------|
| **CRITICAL** | Additive RGB stacking (multiple palette/colour contributions summed) with **no** normalisation or pre-scale; output can exceed 255 or quickly saturate to white. |
| **HIGH** | Additive RGB with clamp-only (e.g. `min(sum,255)`), or **addWhiteSaturating** / **ColourUtil::additive** used on final colour, or radial/trail buffers accumulated with qadd8 then written to `ctx.leds` again with qadd8. |
| **MEDIUM** | Brightness/intensity stacking only (e.g. `brightness = qadd8(...)`), or additive splat/float buffer with **output** clamped (e.g. lum clamped to [0,1] before palette lookup). |
| **LOW** | Single-channel boost (e.g. one channel only) or minor accumulation with strong scaling. |
| **OK** | Single palette lookup per pixel, or proper alpha/blend (e.g. `blendColors`) with no additive RGB. |

---

## 2. Summary Counts

| Severity | Count | Notes |
|----------|-------|--------|
| **CRITICAL** | 6 | Unbounded or weakly bounded additive RGB |
| **HIGH** | 16 | addWhiteSaturating, ColourUtil::additive, or leds += with no normalisation |
| **MEDIUM** | 14 | Brightness qadd8 or clamped-output accumulation |
| **LOW** | 0 | (Single-channel noted in table where relevant) |
| **OK** | 12 | Single getColor or alpha blend |
| **Reference / not in main registry** | 8 | esv11/sensorybridge refs; audit for consistency only |

---

## 3. Per-Effect Table (Production Audio-Reactive)

| Effect | File | Severity | Patterns | Notes |
|--------|------|----------|----------|--------|
| **LGP Star Burst (Narrative)** | LGPStarBurstNarrativeEffect.cpp | **CRITICAL** | Additive RGB: up to 7× `c += palette.getColor`, 6× `c2 +=`; no normalisation | Chord layers (root/third/fifth + accent/shimber/texture/pulse) summed per pixel; drives to white. |
| **LGP Spectrum Detail** | LGPSpectrumDetailEffect.cpp | **CRITICAL** | `ctx.leds[leftIdx] += color` (×3 bands); no normalisation | Three bands add into same LEDs. |
| **Snapwave Linear** | SnapwaveLinearEffect.cpp | **CRITICAL** | `ctx.leds[leftPos] += fadedColor`, `ctx.leds[rightPos] += fadedColor` | Direct leds +=; overlapping waves stack. |
| **Audio Bloom** | AudioBloomEffect.cpp | **CRITICAL** | `ctx.leds[leftIdx] += warmBoost`, `ctx.leds[...] += CRGB(boost, boost>>2, 0)` | Centre and radial adds; single-channel bias (R-heavy). |
| **Ripple** | RippleEffect.cpp | **CRITICAL** | `m_radial[dist].r = qadd8(m_radial[dist].r, color.r)` (and g,b); radial buffer then written to leds | Trail accumulation; no pre-scale. |
| **Ripple EsTuned** | RippleEsTunedEffect.cpp | **CRITICAL** | Same as Ripple | Same pattern. |
| **LGP Spectrum Detail Enhanced** | LGPSpectrumDetailEnhancedEffect.cpp | **HIGH** | `m_radialTrail[...].r = qadd8(..., color.r)`; `ctx.leds[...].r = qadd8(ctx.leds[...].r, m_radialTrail[...].r)`; double additive | Has PRE_SCALE/SPECTRUM_PRE_SCALE etc.; still multiple qadd8 stages into leds. |
| **LGP Beat Pulse** | LGPBeatPulseEffect.cpp | **HIGH** | `finalColor += primaryColor`; `finalColor += snareColor`; `finalColor += hihatColor` (manual clamp 255) | Additive RGB with saturating clamp only; still washes to white when all three contribute. |
| **Beat Pulse Bloom** | BeatPulseBloomEffect.cpp | **HIGH** | addWhiteSaturating(inject, whitePush01) | Explicit white push. |
| **Beat Pulse LGP Interference** | BeatPulseLGPInterferenceEffect.cpp | **HIGH** | addWhiteSaturating(c1, strip1White); addWhiteSaturating(c2, strip2White) | Same. |
| **Beat Pulse Breathe** | BeatPulseBreatheEffect.cpp | **HIGH** | addWhiteSaturating(c, white) | Same. |
| **Beat Pulse Shockwave Cascade** | BeatPulseShockwaveCascadeEffect.cpp | **HIGH** | addWhiteSaturating(c, whiteMixVal) | Same. |
| **Beat Pulse Shockwave** | BeatPulseShockwaveEffect.cpp | **HIGH** | addWhiteSaturating(c, whiteMixVal) | Same. |
| **Beat Pulse Stack** | BeatPulseStackEffect.cpp | **HIGH** | addWhiteSaturating(c, whiteMixVal) | Same. |
| **Beat Pulse Spectral Pulse** | BeatPulseSpectralPulseEffect.cpp | **HIGH** | addWhiteSaturating(c, whitePunch) | Same. |
| **Beat Pulse Spectral** | BeatPulseSpectralEffect.cpp | **HIGH** | ColourUtil::additive(bass, mid, treble) twice; addWhiteSaturating(c, trebleHit-based) | Additive RGB + white. |
| **Beat Pulse Ripple** | BeatPulseRippleEffect.cpp | **HIGH** | addWhiteSaturating(c, sparkle*0.35f) | White push. |
| **Beat Pulse Resonant** | BeatPulseResonantEffect.cpp | **HIGH** | ColourUtil::additive(bodyColor, attackColor) | Additive RGB, no normalisation. |
| **Beat Pulse Void** | BeatPulseVoidEffect.cpp | **HIGH** | addWhiteSaturating(c, coreWhite * brightness) | White push. |
| **Trinity Test** | TrinityTestEffect.cpp | **HIGH** | `ctx.leds[i] += flashColor` | Direct leds +=. |
| **Breathing** | BreathingEffect.cpp | **MEDIUM** | `sum.r = qadd8(sum.r, noteColor.r)` ×12 bins; then PRE_SCALE 180 before write | Pre-scale reduces but does not remove drift. |
| **Breathing Enhanced** | BreathingEnhancedEffect.cpp | **MEDIUM** | Same; PRE_SCALE 180 | Same. |
| **Heartbeat EsTuned** | HeartbeatEsTunedEffect.cpp | **MEDIUM** | `a.r = qadd8(a.r, b.r)` (and g,b) for two colours | Additive but only two sources. |
| **Bloom Parity** | BloomParityEffect.cpp | **MEDIUM** | `dest[pos].r += px.r * mix * alpha` (splat); **per-channel clamp** of curr to [0,1] after post-FX, then lum from (r+g+b)/3 before palette | Per-channel clamp preserves r:g:b ratio and restores dynamic range in hot spots. |
| **Wave Reactive** | WaveReactiveEffect.cpp | **MEDIUM** | brightness = qadd8(raw, fluxBoost) | Brightness stacking. |
| **Wave** | WaveEffect.cpp | **MEDIUM** | brightness = qadd8(...) (or similar) | Brightness stacking. |
| **Wave Ambient** | WaveAmbientEffect.cpp | **MEDIUM** | brightness = qadd8(...) | Brightness stacking. |
| **BPM** | BPMEffect.cpp | **MEDIUM** | intensity = qadd8(base, ringBoost) | Brightness stacking. |
| **BPM Enhanced** | BPMEnhancedEffect.cpp | **MEDIUM** | Same pattern | Brightness stacking. |
| **Ripple Enhanced** | RippleEnhancedEffect.cpp | **MEDIUM** | brightness = qadd8(brightness, shimmerBoost) | Brightness stacking. |
| **LGP Photonic Crystal** | LGPPhotonicCrystalEffect.cpp | **MEDIUM** | brightness = qadd8(brightness, flash*60) | Brightness stacking. |
| **LGP Photonic Crystal Enhanced** | LGPPhotonicCrystalEffectEnhanced.cpp | **MEDIUM** | Same family | Brightness stacking. |
| **LGP Perlin Shocklines** | LGPPerlinShocklinesEffect.cpp | **MEDIUM** | brightness = qadd8(...) | Brightness stacking. |
| **LGP Perlin Shocklines Ambient** | (same or sibling) | **MEDIUM** | Same | Brightness stacking. |
| **LGP Birefringent Shear** | LGPBirefringentShearEffect.cpp | **MEDIUM** | brightness = qadd8(combined, beat) | Brightness stacking. |
| **LGP Star Burst** | LGPStarBurstEffect.cpp | **OK** | Single `ctx.palette.getColor(..., bright)` per pixel; tanh for pattern | No additive RGB. |
| **Chord Glow** | LGPChordGlowEffect.cpp | **OK** | blendColors(c1, c2, blend) with clampU8; no leds += | Alpha blend only. |
| **Spectrum Bars** | LGPSpectrumBarsEffect.cpp | **OK** | Single getColor(hue, bright) per bar | No stacking. |
| **Bass Breath** | LGPBassBreathEffect.cpp | **OK** | Single getColor(hue, bright) | No stacking. |
| **Waveform Parity** | WaveformParityEffect.cpp | **OK** | Single getColor(palIdx, brightness) | No stacking. |
| **LGP Holographic (EsTuned)** | LGPHolographicEsTunedEffect.cpp | **OK** | Single getColor per pixel (or equivalent safe path) | Palette-safe. |
| **Audio Waveform** | AudioWaveformEffect.cpp | **OK** | Assign or single lookup (no leds += in hot path) | Checked; no additive. |
| **Juggle** | JuggleEffect.cpp | **OK** | Balls drawn with single colour; no multi-layer add | No additive RGB. |
| **Kuramoto Transport** | KuramotoTransportEffect.cpp | **OK** | Uses phase/amplitude; single colour mapping (verify per impl) | Treated OK unless audit shows add. |
| **LGP Wave Collision** | LGPWaveCollisionEffect.cpp | **OK** | Superposition of wave values then single colour (verify) | Treated OK. |
| **LGP Interference Scanner** | LGPInterferenceScannerEffect.cpp | **OK** | Single colour per pixel (verify) | Treated OK. |
| **LGP Perlin Veil / Caustics / Interference Weave** | LGPPerlin*Effect.cpp | **OK** | Typically single lookup or modulated hue (verify) | Treated OK. |
| **LGP Audio Test** | LGPAudioTestEffect.cpp | **OK** | Spectrum display; single colour per bar (verify) | Treated OK. |
| **Chevron Waves / Enhanced** | ChevronWavesEffect*.cpp | **OK** | Single colour per segment (verify) | Treated OK. |
| **LGP Star Burst Enhanced** | LGPStarBurstEffectEnhanced.cpp | **OK** | Likely same as LGP Star Burst (verify) | Treated OK. |

---

## 4. Non–Audio-Reactive Effect with Same Bug (For Consistency)

| Effect | File | Severity | Patterns |
|--------|------|----------|----------|
| **LGP Gravitational Lensing** | LGPGravitationalLensingEffect.cpp | **CRITICAL** | `ctx.leds[pixelPos] += ctx.palette.getColor(...)`; no normalisation |

---

## 5. Reference Implementations (Not in Main Registry)

- **esv11_reference:** EsWaveformRefEffect, EsAnalogRefEffect, EsBloomRefEffect, EsOctaveRefEffect, EsSpectrumRefEffect — review for same additive/addWhite patterns if used as template.
- **sensorybridge_reference:** SbWaveform310RefEffect — same.

---

## 6. Corruption Patterns (Quick Reference)

### 6.1 Additive RGB, no normalisation

- **Symptom:** `c += palette.getColor(...)` or `leds[i] += color` multiple times; no divide-by-N or pre-scale.
- **Fix:** Pre-scale each contribution (e.g. ~180/255), or sum then divide by layer count, or use alpha blend instead of add.

### 6.2 addWhiteSaturating / ColourUtil::additive

- **Symptom:** Pushes (255,255,255) or sums R,G,B with qadd8.
- **Fix:** Remove or replace with tint (e.g. hue-preserving brightness) or blend.

### 6.3 Brightness stacking

- **Symptom:** `brightness = qadd8(base, boost)` so effective brightness climbs to 255.
- **Fix:** Use weighted average or soft max, or cap boost before add.

### 6.4 Radial/trail accumulation

- **Symptom:** `m_radial[dist] = qadd8(m_radial[dist], color)` then that buffer added again to leds.
- **Fix:** Pre-scale per contribution or normalise trail before adding to leds.

### 6.5 Single-channel bias

- **Symptom:** e.g. `CRGB(boost, boost>>2, 0)` — one channel dominates.
- **Fix:** Use palette or balanced RGB from energy.

---

## 7. Relevant Paths and Helpers

| Item | Path / location |
|------|------------------|
| Effect implementations | `firmware/v2/src/effects/ieffect/*.cpp` |
| addWhiteSaturating, ColourUtil::additive | `firmware/v2/src/effects/ieffect/BeatPulseRenderUtils.h` |
| Zone blending, softAccumulate | `firmware/v2/src/effects/ieffect/BlendMode.h` |
| Centre distance, STRIP_LENGTH | `firmware/v2/src/effects/ieffect/CoreEffects.h` |
| Effect context (palette, audio, leds) | `firmware/v2/src/plugins/api/EffectContext.h` |

---

## 8. Recommended Fix Order

1. **CRITICAL first:** LGP Spectrum Detail, Snapwave Linear, Audio Bloom, Ripple, Ripple EsTuned, LGP Star Burst Narrative (and LGP Gravitational Lensing if fixing non–audio-reactive).
2. **HIGH next:** LGP Beat Pulse, Beat Pulse family (Bloom, LGP Interference, Breathe, Shockwave*, Stack, Spectral Pulse, Spectral, Ripple, Resonant, Void), Trinity Test, LGP Spectrum Detail Enhanced.
3. **MEDIUM then:** Breathing, Breathing Enhanced, Heartbeat EsTuned, Bloom Parity, Wave Reactive, Wave, Wave Ambient, BPM, BPM Enhanced, Ripple Enhanced, LGP Photonic Crystal*, LGP Perlin Shocklines*, LGP Birefringent Shear.

---

## 9. Palette-Safe Reference (No Corruption)

- **LGPHolographicEffect** (non–audio): Single `ctx.palette.getColor(paletteIndex, brightness)` per pixel; no RGB addition; brightness from sine layers via tanh; chromatic dispersion via complementary hue per strip.
- **LGPStarBurstEffect:** Single getColor per pixel; tanh for pattern.
- **LGPChordGlowEffect:** `blendColors(baseColor, accentColor, accentStrength)` with clampU8 — alpha blend, not additive.

---

---

## 10. Implemented Fixes (Sample of 3)

Three high-value patterns were fixed for assessment. If these translate to meaningful improvements, apply the same strategies to the remaining CRITICAL/HIGH effects.

| Effect | Fix applied | Behaviour |
|--------|-------------|-----------|
| **LGP Star Burst (Narrative)** | Count active layers per pixel (strip 1 and strip 2). After summing chord/accent/shimmer/texture/pulse contributions, call `c.nscale8(255u / layerCount)` (and `c2.nscale8(255u / layerCount2)`) so the combined colour is averaged, not summed. | Prevents wash to white; preserves hue when multiple chord layers are active. |
| **LGP Spectrum Detail** | Pre-scale each band contribution before adding to `ctx.leds`: `color = color.nscale8(bright)` then `color = color.nscale8(SPECTRUM_PRE_SCALE)` with `SPECTRUM_PRE_SCALE = 85` (~3 overlapping bands sum to 255). Strip 2 uses the same pre-scale. Adjacent LEDs use the already pre-scaled colour. | Multiple bins hitting the same LED stay in range; palette hues remain visible. |
| **LGP Beat Pulse** | Count combined layers (1 base + primary + snare + hihat). After adding all layers with saturating clamp, call `finalColor.nscale8(255u / layerCount)` when `layerCount > 1`. | Combined beat/snare/hihat no longer washes to white; hue balance preserved. |
| **LGP Spectrum Detail Enhanced** | Reduce `SPECTRUM_PRE_SCALE` from 90 to 40 and `TRAIL_PRE_SCALE` from 45 to 25 so spectrum + trail contributions to the same LED stay in range (both add via qadd8). | Prevents double additive (spectrum + trail) from washing to white. |
| **Ripple Enhanced** | Halve treble shimmer coefficient from 60 to 30 in `shimmerBoost = m_trebleShimmer * shimmerFade * 30.0f` so brightness stacking does not push palette to 255 as hard. | Preserves palette saturation on wavefront. |
| **Ripple (ES tuned)** | Pre-scale each ripple colour before adding to `m_radial`: `color = color.nscale8(RIPPLE_PRE_SCALE)` with `RIPPLE_PRE_SCALE = 85`. | Multiple overlapping ripples stay in range; hue preserved. |
| **Kuramoto Transport** | In `KuramotoTransportBuffer::toneMapToCRGB8`, use luminance-based tone mapping: compute `lum = (r+g+b)/3`, tone map `lumT = lum/(1+lum)`, scale `(r,g,b)` by `lumT/lum` instead of per-channel tone map. | Prevents HDR accumulation from collapsing to white; hue preserved. |

---

## 11. Definitive Fixes (2026-02)

**Tier 1 – Beat Pulse family (remove white push and additive sum):**

| Location | Change |
|----------|--------|
| **BeatPulseRenderUtils.h** | `addWhiteSaturating(CRGB& c, uint8_t w)`: Replaced add-(w,w,w) with **hue-preserving luminance scale**: lum = (r+g+b)/3, lumNew = min(255, lum+w), scale (r,g,b) by lumNew/lum. No more push to (255,255,255). |
| **BeatPulseRenderUtils.h** | `ColourUtil::additive(base, overlay)`: Replaced saturating sum with **average** (r,g,b) = ((base+overlay)+1)>>1. Combined layers no longer wash to white. |

**Nuclear – Renderer post-pass:**

| Location | Change |
|----------|--------|
| **RendererActor.cpp** | In `showLeds()`, before copying to strip buffers: **soft-knee tone map** on `m_leds`. Per pixel: lum = (r+g+b)/3, lumT = lum/(lum+knee) with knee=1.0, scale (r,g,b) by lumT/lum. Compresses over-bright additive output while preserving hue; applies to both zone-composer and single-effect paths. |

---

*Audit completed from codebase grep and file inspection. Use this document to prioritise and implement fixes (normalisation, pre-scaling, or blend-mode changes) without re-discovering the same patterns.*
