# Audio-Reactive Patterns Master Reference

**Version**: 1.0  
**Last Updated**: 2026-01-11  
**Firmware Path**: `firmware/v2/src/effects/ieffect/`

---

## Introduction

This document provides a comprehensive technical breakdown of the audio-reactive pattern system in LightwaveOS. It covers:

1. **Motion "logic seeds"** — the mathematical and algorithmic primitives that generate movement
2. **Colour choreography** — how patterns dance with the Light Guide Plate (LGP) medium
3. **Taxonomic framework** — a TouchDesigner-friendly categorisation system for pattern synthesis and recombination

### The LGP Context

The hardware is not merely 320 LEDs. It consists of:
- **Two PCBs**, each with 160 WS2812 LEDs
- **One 330mm Light Guide Plates** (film stacks)
- **Centre-origin topology**: LEDs 79/80 form the centre pair, effects propagate outward to edges (0/159 per strip)
- **Photon interference medium**: The LGP transforms discrete LED pulses into fluid, organic, or geometric visual forms through light scattering and interference

This physical medium is critical: a simple radial pulse becomes a breathing interference pattern; a sharp spectral bar becomes a glowing column that bleeds and diffracts into neighbouring space.

---

## Part 1: Motion "Logic Seeds" — Algorithmic Generators

At the highest level, **every audio-reactive pattern is a time-evolving scalar/vector field over a 1D radial coordinate** `d = 0..79` (distance from centre), mirrored via `SET_CENTER_PAIR(...)` to create centre-origin symmetry on the LGP.

### Seed A: Radial Travelling "Ring" Impulses (Event → Expanding Radius)

**Core equation**:  
r(t) grows outward from 0, brightness is a band-limited "ring" around r(t).

**Mathematical form**:
\`\`\`
position(t) = position(t-1) + velocity * dt
brightness(d) = gaussian(d - position(t), sigma) * amplitude
\`\`\`

**Examples**:
- **LGPBeatPulseEffect**: Kick/snare trigger discrete pulses; each pulse has \`m_pulsePosition\` that increments ~2.5 pixels/frame, with Gaussian falloff (σ ≈ 8–12 pixels).
- **WaveReactiveEffect**: Continuous RMS-driven "wave" that flows outward; position increments based on audio flux, with slower decay.

**Characteristics**:
- **Discrete events** (beats) → sharp, fast-moving rings
- **Continuous audio** (RMS/flux) → smoother, sustained waves
- **Decay**: Exponential amplitude fade (e.g., \`m_pulseAmplitude *= 0.92\` per frame)

**Composability**:
- Multiple concurrent pulses (primary/secondary)
- Variable velocity based on audio feature (e.g., spectral centroid → speed)
- Colour per pulse (kick = red, snare = blue)

---

### Seed B: Standing "Bar" Fields (Spatial Frequency Decomposition)

**Core equation**:  
brightness(d, f) = amplitude(f) · window(d, f)

Where \`f\` is a frequency bin/band, \`amplitude(f)\` comes from FFT/Mel bands, and \`window(d, f)\` maps spatial position to frequency.

**Mathematical form**:
\`\`\`
For each bin i in 0..N:
  startPixel = (i * stripLength) / N
  endPixel = ((i+1) * stripLength) / N
  brightness[startPixel:endPixel] = binAmplitude[i]
\`\`\`

**Examples**:
- **SpectrumAnalyzerEffect**: 64 FFT bins → 64 spatial "bars"; each bar's brightness = \`bins64[i]\` with logarithmic dB scaling.
- **LGPSpectrumBarsEffect**: 8 Mel bands → 8 wider bars; brightness = \`bands[i]\`, with smoothing and anti-flicker.

**Characteristics**:
- **Static spatial mapping** (each bin owns a fixed pixel range)
- **Per-frame update** (no temporal propagation; brightness tracks instantaneous FFT)
- **Anti-flicker**: Temporal smoothing via exponential moving average or minimum brightness floor

**Composability**:
- Variable bin count (8/16/32/64)
- Logarithmic vs linear frequency mapping
- Hue rotation per bin (spectral → chromatic gradient)

---

### Seed C: Diffuse "Glow" Fields (Smooth Spatial Envelopes)

**Core equation**:  
brightness(d) = Σ weight_k(d) · feature_k

Where \`weight_k(d)\` is a smooth spatial basis function (Gaussian, cosine, polynomial), and \`feature_k\` is an audio descriptor (RMS, spectral flux, saliency, chord stability).

**Mathematical form**:
\`\`\`
brightness(d) = baseGlow * RMS + centerPeak * exp(-d^2 / sigma^2) * saliency
\`\`\`

**Examples**:
- **LGPChordGlowEffect**: Centre-weighted Gaussian glow modulated by \`chordStability\`; hue = \`chordRoot\` (C=0°, G=210°, etc.).
- **SaliencyAwareEffect**: Dual-layer glow (base + harmonic peaks); brightness tracks \`audioPeakSaliency\`, with slower attack/decay (τ ≈ 150ms).

**Characteristics**:
- **No discrete events** (continuous envelope tracking)
- **Soft spatial gradients** (no sharp edges)
- **Slow temporal response** (attack/release filtering to avoid flicker)

**Composability**:
- Multiple overlapping Gaussians (multi-source glow)
- Hue/saturation modulation by harmonic content
- Spatial basis library (radial cosine, polynomial decay, etc.)

---

### Seed D: Adaptive "Morph" Fields (State Machine + Interpolation)

**Core equation**:  
pattern(t) = α(t) · pattern_A + (1 - α(t)) · pattern_B

Where \`α(t)\` is driven by high-level audio features (musical style, tempo, energy).

**Mathematical form**:
\`\`\`
if (style == AGGRESSIVE):
  alpha = energyHigh * 0.7 + tempo_norm * 0.3
else if (style == CALM):
  alpha = (1 - energyHigh) * 0.8
patternOutput = lerp(patternA, patternB, alpha)
\`\`\`

**Examples**:
- **StyleAdaptiveEffect**: Switches between "calm diffuse glow" and "aggressive fast pulses" based on \`musicStyle\` and \`energyLevel\`; cross-fade over ~500ms.

**Characteristics**:
- **High-level semantic control** (not raw FFT)
- **Smooth transitions** (avoids jarring jumps)
- **Context-aware** (patterns respond to musical "mood")

**Composability**:
- N-way morphs (not just binary A/B)
- Fuzzy logic blending (multiple style features)
- Parameter space interpolation (speed, colour, spatial scale all morph together)

---

### Seed E: Diagnostic "Test" Patterns (Direct Audio Feature Visualisation)

**Core equation**:  
pixel[i] = directMap(audioFeature[i])

No abstraction; raw 1:1 mapping for debugging and calibration.

**Examples**:
- **LGPAudioTestEffect**: 8 regions showing RMS, flux, beatConfidence, saliency, etc.; brightness = feature value, hue = cycleHue.

**Characteristics**:
- **No artistic intent** (pure data visualisation)
- **Useful for tuning** (sensitivity thresholds, FFT window sizes)

---

## Part 2: Colour Choreography on the LGP

Colour handling in the LGP context is fundamentally different from standard LED grids because:

1. **The LGP medium diffuses and mixes light** — sharp colour transitions become gradients through scattering
2. **Interference creates "phantom" hues** — overlapping wavelengths produce colours not present in the palette
3. **Low brightness is invisible** — the LGP requires a minimum brightness floor (~30/255) to be perceptible

### Colour Policy Dimensions

Each effect makes choices across these dimensions:

| Dimension | Options | Examples |
|-----------|---------|----------|
| **Palette usage** | Global palette, fixed hue, per-layer hue | BeatPulse uses palette[0..7], ChordGlow uses chordRoot hue |
| **Compositing mode** | Replace, additive, alpha blend | BeatPulse = replace, WaveReactive = additive |
| **Hue mapping** | Static, cyclic, audio-driven | SpectrumBars = bin → hue gradient, ChordGlow = chord → hue table |
| **Saturation policy** | Fixed, energy-driven, saliency-driven | SaliencyAware boosts saturation on harmonic peaks |
| **Brightness floor** | None, fixed (e.g., 30), adaptive | SpectrumAnalyzer = \`max(30, binValue)\` |
| **Anti-flicker** | None, EMA smoothing, hysteresis | SpectrumBars uses EMA (α=0.3), BeatPulse has attack/release envelope |

### Colour Choreography Patterns

#### Pattern 1: "Palette Walk" (Global Palette Indexing)

Used by: \`LGPBeatPulseEffect\`

\`\`\`cpp
CHSV baseColour = ctx.palette[m_colourIndex % 8];  // Cycle through palette
// Apply brightness modulation
baseColour.v = pulseAmplitude * 255;
\`\`\`

**LGP behaviour**: Successive beats cycle through palette colours, creating a "breathing" effect where each pulse has a distinct hue. The LGP medium softens transitions between pulses, so rapid beats blend into a chromatic wash.

---

#### Pattern 2: "Spectral Gradient" (Frequency → Hue Mapping)

Used by: \`SpectrumAnalyzerEffect\`, \`LGPSpectrumBarsEffect\`

\`\`\`cpp
for (int i = 0; i < numBins; i++) {
    uint8_t hue = map(i, 0, numBins, 0, 255);  // Low freq = red, high freq = blue
    CHSV colour(hue, 255, binAmplitude[i]);
    // Draw bar
}
\`\`\`

**LGP behaviour**: Low frequencies (bass) appear red/orange at the centre, high frequencies (treble) appear blue/violet at the edges. The LGP creates a "spectral column" effect where each bar bleeds into its neighbours, forming a continuous frequency gradient.

**Anti-flicker**: Temporal smoothing via EMA:
\`\`\`cpp
m_smoothedBins[i] = m_smoothedBins[i] * 0.7 + bins64[i] * 0.3;
\`\`\`

---

#### Pattern 3: "Harmonic Glow" (Chord/Saliency → Colour Stability)

Used by: \`LGPChordGlowEffect\`, \`SaliencyAwareEffect\`

\`\`\`cpp
// Chord detection → hue table lookup
uint8_t chordHue = chordToHue(ctx.audio.chordRoot);  // C=0°, G=210°, etc.
float glowIntensity = ctx.audio.chordStability;  // 0.0 (chaotic) to 1.0 (stable)

CHSV colour(chordHue, 255, glowIntensity * 255);
\`\`\`

**LGP behaviour**: Stable chords produce a steady, saturated glow that fills the entire LGP; dissonant passages fade to dim/desaturated. The centre-weighted Gaussian creates a "halo" effect where the glow is brightest at the centre and fades toward the edges.

**Attack/release filtering**:
\`\`\`cpp
// Slow attack (150ms) to avoid flicker on chord changes
m_displayedStability += (chordStability - m_displayedStability) * 0.1;
\`\`\`

---

#### Pattern 4: "Energy Saturation" (RMS → Saturation Boost)

Used by: \`SaliencyAwareEffect\`, \`StyleAdaptiveEffect\`

\`\`\`cpp
float energy = ctx.audio.rms;  // 0.0 to 1.0
uint8_t saturation = lerp(128, 255, energy);  // Low energy = desaturated, high energy = vivid
\`\`\`

**LGP behaviour**: Quiet passages appear pastel/washed-out; loud passages are vivid and saturated. The LGP amplifies this effect because high-saturation colours create stronger interference patterns.

---

#### Pattern 5: "Additive Layering" (Multi-Source Compositing)

Used by: \`WaveReactiveEffect\` (hypothetical multi-layer variant)

\`\`\`cpp
// Layer 1: Bass wave (red)
CRGB bassWave = CRGB(rmsLow * 255, 0, 0);
// Layer 2: Mid wave (green)
CRGB midWave = CRGB(0, rmsMid * 255, 0);
// Composite
ctx.leds[i] = bassWave + midWave;  // Additive blend
\`\`\`

**LGP behaviour**: Overlapping waves produce "phantom" colours (red + green = yellow) through additive mixing. The LGP medium enhances this because scattered light from multiple sources naturally sums.

---

### Visibility and Flicker Guards

The LGP requires special handling to remain perceptible and stable:

#### Minimum Brightness Floor

\`\`\`cpp
// Ensure value never drops below 30 (LGP visibility threshold)
if (colour.v < 30) colour.v = 0;  // Hard cutoff (better than dim flicker)
\`\`\`

**Rationale**: Brightness below ~30/255 is nearly invisible on the LGP due to material scattering losses. Better to go fully dark than flicker dimly.

---

#### Anti-Flicker Temporal Smoothing

\`\`\`cpp
// Exponential moving average (EMA)
m_smoothedValue = m_smoothedValue * (1 - alpha) + newValue * alpha;
// Typical alpha = 0.1 to 0.3 (lower = smoother, higher = more responsive)
\`\`\`

**Rationale**: FFT bins are noisy; raw bin values flicker at ~20-30 Hz, which is highly perceptible on the LGP. EMA smoothing reduces flicker while maintaining responsiveness to transients.

---

#### Attack/Release Envelopes

\`\`\`cpp
// Fast attack, slow release (percussive feel)
if (newValue > currentValue) {
    currentValue += (newValue - currentValue) * 0.5;  // Attack (50% per frame)
} else {
    currentValue += (newValue - currentValue) * 0.05;  // Release (5% per frame)
}
\`\`\`

**Rationale**: Audio transients (beats, notes) should appear immediately (fast attack) but fade gracefully (slow release) to avoid jarring on/off transitions.

---

## Part 3: Taxonomic Framework for Pattern Synthesis

This taxonomy is designed for **recombination and experimentation** in a TouchDesigner-style node-graph environment. Each "node" represents a composable stage in the pattern synthesis pipeline.

\`\`\`mermaid
graph TD
    AudioInput[Audio Input] --> FeatureExtraction[Feature Extraction]
    FeatureExtraction --> TemporalDriver[Temporal Driver]
    FeatureExtraction --> SpatialBasis[Spatial Basis Generator]
    
    TemporalDriver --> EventDetector[Event Detector]
    TemporalDriver --> ContinuousTracker[Continuous Tracker]
    
    SpatialBasis --> RadialPulse[Radial Pulse]
    SpatialBasis --> StandingBar[Standing Bar]
    SpatialBasis --> DiffuseGlow[Diffuse Glow]
    SpatialBasis --> MorphField[Morph Field]
    
    EventDetector --> RadialPulse
    ContinuousTracker --> StandingBar
    ContinuousTracker --> DiffuseGlow
    
    RadialPulse --> LayerCompositor[Layer Compositor]
    StandingBar --> LayerCompositor
    DiffuseGlow --> LayerCompositor
    MorphField --> LayerCompositor
    
    LayerCompositor --> ColourPolicy[Colour Policy]
    ColourPolicy --> AntiFlicker[Anti-Flicker Filter]
    AntiFlicker --> CentreOriginMapper[Centre-Origin Mapper]
    CentreOriginMapper --> LGPRender[LGP Render Output]
\`\`\`

### Node Taxonomy (Pipeline Stages)

#### Stage 1: Audio Input
- **Raw PCM** (16-bit, 44.1 kHz)
- **Pre-processed** (DC removal, noise gate)

#### Stage 2: Feature Extraction
- **Time-domain**: RMS, zero-crossing rate, spectral flux
- **Frequency-domain**: FFT bins (64/128), Mel bands (8/12), spectral centroid
- **High-level**: Beat detection (kick/snare), chord detection, musical style, saliency

#### Stage 3: Temporal Driver
Two sub-types:

**Event Detector** (discrete triggers):
- Beat detector (kick/snare/hat)
- Onset detector (note attacks)
- Chord change detector

**Continuous Tracker** (smooth envelopes):
- RMS tracker (attack/release envelope)
- Spectral flux tracker
- Energy tracker (band-limited RMS)

#### Stage 4: Spatial Basis Generator
Maps audio features to 1D radial space [0..79]:

- **Radial Pulse**: Travelling Gaussian ring
- **Standing Bar**: Static frequency → position mapping
- **Diffuse Glow**: Smooth spatial envelope (Gaussian, cosine, polynomial)
- **Morph Field**: Interpolation between basis functions

#### Stage 5: Layer Compositor
Combines multiple spatial basis outputs:

- **Replace**: Layer overwrites previous (last wins)
- **Additive**: Layers sum (RGB additive blend)
- **Alpha**: Layers blend with transparency (weighted average)
- **Max**: Take maximum brightness per pixel

#### Stage 6: Colour Policy
Maps brightness field to CRGB:

- **Palette indexing**: Global palette walk, per-layer palette
- **Hue mapping**: Static, cyclic, audio-driven (frequency → hue, chord → hue)
- **Saturation policy**: Fixed, energy-driven, saliency-driven
- **Brightness policy**: Direct (value = amplitude), scaled (value = amplitude^gamma), floored (value = max(floor, amplitude))

#### Stage 7: Anti-Flicker Filter
Temporal smoothing to stabilise output:

- **EMA smoothing**: Low-pass filter (alpha = 0.1–0.3)
- **Attack/release**: Asymmetric envelope (fast attack, slow release)
- **Hysteresis**: Threshold with dead zone (avoid chatter near cutoff)

#### Stage 8: Centre-Origin Mapper
Maps 1D radial field [0..79] to dual-strip [0..159, 0..159]:

\`\`\`cpp
SET_CENTER_PAIR(d, colour);  // Expands to ctx.leds[79-d] = ctx.leds[80+d] = colour
\`\`\`

#### Stage 9: LGP Render Output
Final output to WS2812 strips (320 LEDs, centre-origin topology).

---

### Taxonomy: Effect Classification

Each current effect can be classified by its dominant seeds and policies:

| Effect | Temporal Driver | Spatial Basis | Colour Policy | Compositing |
|--------|----------------|---------------|---------------|-------------|
| **LGPBeatPulseEffect** | Event (kick/snare) | Radial Pulse (dual) | Palette walk | Replace |
| **LGPSpectrumBarsEffect** | Continuous (bands) | Standing Bar | Spectral gradient | Replace |
| **SpectrumAnalyzerEffect** | Continuous (bins64) | Standing Bar | Spectral gradient | Replace |
| **WaveReactiveEffect** | Continuous (RMS/flux) | Radial Pulse (single) | Palette[0] | Replace |
| **SaliencyAwareEffect** | Continuous (saliency) | Diffuse Glow (dual-layer) | Energy saturation | Additive |
| **StyleAdaptiveEffect** | Event + Continuous (style) | Morph Field (pulse ↔ glow) | Style-driven hue | Alpha blend |
| **LGPChordGlowEffect** | Continuous (chord stability) | Diffuse Glow (centre-weighted) | Chord → hue table | Replace |
| **LGPAudioTestEffect** | Continuous (multi-feature) | Static regions | Cyclic hue | Replace |

---

## Part 4: Synthesis and Recombination Strategies

### Strategy 1: Cross-Pollinate Temporal Drivers and Spatial Bases

**Example**: Replace \`LGPBeatPulseEffect\`'s beat-driven pulses with chord-driven pulses:
- Trigger: \`chordChange\` instead of \`kickDetected\`
- Hue: \`chordRoot\` instead of \`palette[colourIndex]\`
- Result: Pulses that expand on each chord change, colour-coded by harmonic root

### Strategy 2: Multi-Layer Compositing

**Example**: Combine \`LGPSpectrumBarsEffect\` (background) + \`LGPBeatPulseEffect\` (foreground):
- Layer 1: Spectrum bars (replace mode, 50% brightness)
- Layer 2: Beat pulses (additive mode, full brightness)
- Result: Bars provide continuous spectral context, pulses add percussive accents

### Strategy 3: Adaptive Parameter Modulation

**Example**: Make \`WaveReactiveEffect\` velocity adaptive:
- Base velocity: \`2.5 pixels/frame\`
- Modulation: \`velocity *= (1 + spectralCentroid * 0.5)\`
- Result: High-frequency content (treble) makes waves move faster; low-frequency (bass) moves slower

### Strategy 4: Spatial Basis Library Expansion

New basis functions to implement:

- **Radial cosine**: \`brightness(d) = cos(2π * d / wavelength) * amplitude\`
- **Polynomial decay**: \`brightness(d) = (1 - d/maxD)^exponent * amplitude\`
- **Sawtooth wave**: \`brightness(d) = (d % wavelength) / wavelength * amplitude\`
- **Travelling sine**: \`brightness(d, t) = sin(2π * (d/wavelength - t/period)) * amplitude\`

### Strategy 5: Colour Policy Library Expansion

New colour policies to implement:

- **Saliency-driven palette**: Index palette by \`audioPeakSaliency\` (salient = vivid, non-salient = muted)
- **Flux-driven saturation**: \`saturation = lerp(64, 255, spectralFlux)\` (stable = desaturated, changing = saturated)
- **Chord-aware complementary**: On chord change, shift hue by 180° (complementary colour)
- **Energy-driven gamma**: \`value = amplitude^(2 - energy)\` (low energy = linear, high energy = sharp contrast)

---

## Part 5: Implementation Cookbook

### Recipe 1: Create a New Radial Pulse Effect

1. **Choose temporal driver**: Event (beat) or continuous (RMS)
2. **Define pulse state**:
   \`\`\`cpp
   float m_pulsePosition = 0.0f;
   float m_pulseAmplitude = 1.0f;
   \`\`\`
3. **Update loop**:
   \`\`\`cpp
   if (ctx.audio.kickDetected && m_pulsePosition == 0.0f) {
       m_pulsePosition = 0.0f;
       m_pulseAmplitude = 1.0f;
   }
   if (m_pulsePosition < 1.0f) {
       m_pulsePosition += dt / pulseDuration;
       m_pulseAmplitude *= decayFactor;
   }
   \`\`\`
4. **Render**:
   \`\`\`cpp
   for (int d = 0; d < 80; d++) {
       float distance = abs(d - m_pulsePosition * 79);
       float brightness = gaussian(distance, sigma) * m_pulseAmplitude;
       CHSV colour = ctx.palette[0];
       colour.v = brightness * 255;
       SET_CENTER_PAIR(d, colour);
   }
   \`\`\`

### Recipe 2: Create a New Standing Bar Effect

1. **Choose feature**: FFT bins, Mel bands, or custom band-pass filters
2. **Define spatial mapping**:
   \`\`\`cpp
   int numBars = 8;
   int pixelsPerBar = 80 / numBars;
   \`\`\`
3. **Update loop**:
   \`\`\`cpp
   for (int i = 0; i < numBars; i++) {
       m_smoothedBands[i] = m_smoothedBands[i] * 0.7 + ctx.audio.bands[i] * 0.3;
   }
   \`\`\`
4. **Render**:
   \`\`\`cpp
   for (int i = 0; i < numBars; i++) {
       uint8_t hue = map(i, 0, numBars, 0, 255);
       float brightness = m_smoothedBands[i];
       CHSV colour(hue, 255, brightness * 255);
       for (int p = i * pixelsPerBar; p < (i+1) * pixelsPerBar; p++) {
           SET_CENTER_PAIR(p, colour);
       }
   }
   \`\`\`

### Recipe 3: Create a New Diffuse Glow Effect

1. **Choose feature**: Chord stability, saliency, or RMS
2. **Define spatial basis**:
   \`\`\`cpp
   float gaussianWeight(int d, float sigma) {
       return exp(-d*d / (2*sigma*sigma));
   }
   \`\`\`
3. **Update loop**:
   \`\`\`cpp
   m_glowIntensity += (ctx.audio.chordStability - m_glowIntensity) * attackRate;
   \`\`\`
4. **Render**:
   \`\`\`cpp
   uint8_t hue = chordToHue(ctx.audio.chordRoot);
   for (int d = 0; d < 80; d++) {
       float weight = gaussianWeight(d, 20.0f);
       float brightness = m_glowIntensity * weight;
       CHSV colour(hue, 255, brightness * 255);
       SET_CENTER_PAIR(d, colour);
   }
   \`\`\`

---

## Part 6: Future Directions

### Semantic Mapping Beyond Current Implementation

Current system maps **low-level features** (RMS, FFT, beats) to **low-level visuals** (brightness, position, hue). Future work should explore **high-level semantic mappings**:

- **Musical tension → spatial compression** (tense passages squeeze patterns toward centre)
- **Harmonic consonance → colour harmony** (consonant chords use analogous hues, dissonant chords use complementary)
- **Rhythmic complexity → pattern density** (polyrhythms produce layered, offset pulses)
- **Melodic contour → spatial trajectory** (rising melodies shift patterns upward in virtual 2D space)

### Multi-Strip Asymmetry

Current implementation enforces strict mirror symmetry (\`SET_CENTER_PAIR\`). Future experiments:

- **Stereo field → spatial panning** (left channel biases strip 1, right channel biases strip 2)
- **Phase offset → travelling wave** (strip 1 leads strip 2 by φ, creating rotational illusion in LGP)

### Transition Engine Integration

Current effects are discrete; switching is abrupt. Future work:

- **Cross-fade between effects** (alpha blend over 500ms–1s)
- **Parameter morphing** (interpolate speed, colour, spatial scale during transitions)
- **Audio-driven transitions** (major section change → effect change)

---

## Conclusion

The audio-reactive pattern system in LightwaveOS is built on a foundation of **composable algorithmic primitives** (radial pulses, standing bars, diffuse glows, adaptive morphs) that map **audio features** (beats, spectrum, harmony, style) to **visual motion** in a **unique physical medium** (the Light Guide Plate).

The taxonomy presented here is designed for **systematic exploration and recombination** — every effect is a point in a multi-dimensional design space defined by:
- Temporal driver (event vs continuous)
- Spatial basis (pulse vs bar vs glow vs morph)
- Colour policy (palette vs gradient vs harmonic vs adaptive)
- Compositing mode (replace vs additive vs alpha vs max)

By treating these dimensions as **independent parameters**, you can generate a combinatorial explosion of new patterns through systematic variation and layering.

The LGP medium is the secret ingredient: it transforms discrete mathematical operations into **organic, fluid, interference-driven visuals** that feel alive and responsive to music in ways that standard LED grids cannot achieve.

---

## Related Documentation

- [Audio-Reactive Effects Analysis](audio-reactive-effects-analysis.md)
- [Audio-Visual Semantic Mapping](audio-visual-semantic-mapping.md)
- [Audio Bloom Implementation](audio-bloom-implementation.md)
- [Per-Effect Layer Audits](layer-audits/INDEX.md)

---

**End of Master Reference**
