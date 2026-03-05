# Effects Behavioral Reference

> Auto-generated reference for all 174 registered effect IDs (173 active, 1 retired).
> Documents hidden behavioral gates that affect how effects respond to user controls.
>
> **Source files analysed:**
> - `src/config/effect_ids.h` -- EID constants, `needsToneMap()`, `needsSilenceGate()`
> - `src/effects/PatternRegistry.h` / `.cpp` -- `PATTERN_METADATA[]`, family/tag assignments, `shouldSkipColorCorrection()`, `isLGPSensitive()`, `isStatefulEffect()`, `isAudioReactive()`
> - `src/effects/ieffect/*.cpp` -- grep for `ctx.palette`, `ctx.gHue`, `ctx.speed`, hardcoded `CRGB`/`CHSV`
> - `src/effects/ieffect/esv11_reference/*.cpp`, `sensorybridge_reference/*.cpp` -- ES/SB reference shows
>
> **Generated:** 2026-03-02

---

## Behavioral Gate Systems

### 1. Color Correction (`processBuffer`)

The `ColorCorrectionEngine::processBuffer()` runs on every frame between effect rendering and LED output. It adjusts saturation, white balance, and brightness curves. However, **some effects skip it entirely** because color correction would destroy their intended output.

`PatternRegistry::shouldSkipColorCorrection(effectId)` returns `true` when ANY of these conditions match:

| Layer | Condition | Rationale |
|-------|-----------|-----------|
| **Hardcoded LGP-Sensitive** | `EID_INTERFERENCE`, `EID_LGP_BOX_WAVE`, `EID_LGP_INTERFERENCE_SCANNER`, `EID_LGP_MOIRE_CURTAINS`, `EID_LGP_FRESNEL_ZONES`, `EID_LGP_CHROMATIC_LENS`, `EID_LGP_CHROMATIC_PULSE`, `EID_CHROMATIC_INTERFERENCE` | These effects compute precise wavelength-based colours that CC would distort |
| **INTERFERENCE family** | `PatternFamily::INTERFERENCE` (any effect classified here) | Interference patterns need exact phase-to-colour mapping |
| **ADVANCED_OPTICAL + CENTER_ORIGIN** | Family = `ADVANCED_OPTICAL` AND tag includes `CENTER_ORIGIN` | Optical simulation effects with radial symmetry |
| **QUANTUM/ORGANIC + CENTER_ORIGIN + PHYSICS** | Family = `QUANTUM` or `ORGANIC`, AND tags include both `CENTER_ORIGIN` and `PHYSICS` | Physics simulations need precise amplitude values |
| **PHYSICS_BASED family** | `PatternFamily::PHYSICS_BASED` | Physics simulations (currently no effects classified here) |
| **MATHEMATICAL family** | `PatternFamily::MATHEMATICAL` | Mathematical mappings need exact RGB values |
| **Stateful effects** | `EID_CONFETTI`, `EID_RIPPLE`, `EID_LGP_STAR_BURST`, `EID_LGP_STAR_BURST_NARRATIVE` | These read the previous frame buffer; CC would compound frame-over-frame |

**Important:** Effects not in `PATTERN_METADATA[]` (the 3 radial variants) will return `false` from metadata-based checks because `getPatternMetadata()` returns `nullptr`. They only skip CC if they match the hardcoded ID list or stateful check.

### 2. Tone Mapping (Reinhard)

Effects that use **additive blending** (`ctx.leds[i] += color`) can produce values that clip to white. The Reinhard tone mapper compresses HDR values back into range. Only effects listed in `needsToneMap()` get this treatment; all others skip the tone mapper for sharper colour and ~3 ms/frame savings.

Tone-mapped effects (from `effect_ids.h`):

| EID | Name | Reason |
|-----|------|--------|
| `0x0A05` | Audio Bloom | `pulse += warmBoost` |
| `0x0A06` | LGP Star Burst (Narrative) | multi-layer `+= c` |
| `0x0E05` | LGP Spectrum Detail | additive spectrum bars |
| `0x0E06` | LGP Spectrum Detail Enhanced | additive spectrum bars |
| `0x0E0A` | Snapwave Linear | `pulse += fadedColor` |
| `0x0F00` | Trinity Test | additive white flash |
| `0x0601` | LGP Gravitational Lensing | `particle += palette` |
| `0x1B00` | Time-Reversal Mirror | tuned with tone map |
| `0x1B05` | Time-Reversal Mirror AR | tuned with tone map |
| `0x1B06` | Time-Reversal Mirror Mod1 | tuned with tone map |
| `0x1B07` | Time-Reversal Mirror Mod2 | tuned with tone map |
| `0x1B08` | Time-Reversal Mirror Mod3 | tuned with tone map |

### 3. Silence Gate

Effects in families `>= FAMILY_SHAPE_BANGERS (0x18)` were designed for audio reactivity and produce visual noise in silence. The silence gate fades them to black when no audio activity is detected.

Affected families:
- `0x18` -- Shape Bangers Pack (11 effects)
- `0x19` -- Holy Shit Bangers Pack (5 effects)
- `0x1A` -- Experimental Audio Pack (10 effects)
- `0x1B` -- Showpiece Pack 3 (9 effects)

**Total: 35 effects gated by silence.**

---

## Quick Reference: Effects That Skip Color Correction

These effects bypass `ColorCorrectionEngine::processBuffer()`. Changing CC settings (saturation, white balance) has **no visible effect** on them.

### By Hardcoded ID (always skip)

| EID | Name |
|-----|------|
| `0x010A` | Interference |
| `0x0200` | LGP Box Wave |
| `0x0203` | LGP Interference Scanner |
| `0x0400` | LGP Moire Curtains |
| `0x0406` | LGP Fresnel Zones |
| `0x0900` | LGP Chromatic Lens |
| `0x0901` | LGP Chromatic Pulse |
| `0x0902` | Chromatic Interference |

### By Stateful (always skip)

| EID | Name |
|-----|------|
| `0x0103` | Confetti |
| `0x0108` | Ripple |
| `0x0306` | LGP Star Burst |
| `0x0A06` | LGP Star Burst (Narrative) |

### By INTERFERENCE Family

| EID | Name |
|-----|------|
| `0x0200` | LGP Box Wave |
| `0x0201` | LGP Holographic |
| `0x0202` | LGP Modal Resonance |
| `0x0203` | LGP Interference Scanner |
| `0x0204` | LGP Wave Collision |
| `0x0E03` | LGP Interference Scanner Enhanced |
| `0x0E08` | LGP Wave Collision Enhanced |
| `0x1000` | LGP Holographic Auto-Cycle |
| `0x1202` | LGP Holographic (ES tuned) |
| `0x1800` | LGP Talbot Carpet |
| `0x1801` | LGP Airy Comet |
| `0x1802` | LGP Moire Cathedral |
| `0x1809` | LGP Cymatic Ladder |
| `0x180A` | LGP Mach Diamonds |
| `0x1A04` | LGP Treble Net |
| `0x1A08` | LGP Transient Lattice |
| `0x1B02` | Gold Code Speckle |

### By ADVANCED_OPTICAL + CENTER_ORIGIN

| EID | Name |
|-----|------|
| `0x0400` | LGP Moire Curtains |
| `0x0401` | LGP Radial Ripple |
| `0x0402` | LGP Holographic Vortex |
| `0x0403` | LGP Evanescent Drift |
| `0x0404` | LGP Chromatic Shear |
| `0x0405` | LGP Modal Cavity |
| `0x0406` | LGP Fresnel Zones |
| `0x0407` | LGP Photonic Crystal |
| `0x0900` | LGP Chromatic Lens |
| `0x0901` | LGP Chromatic Pulse |
| `0x0902` | Chromatic Interference |
| `0x0B02` | LGP Perlin Caustics |
| `0x0B03` | LGP Perlin Interference Weave |
| `0x0C02` | LGP Perlin Caustics Ambient |
| `0x0C03` | LGP Perlin Interference Weave Ambient |
| `0x0E04` | LGP Photonic Crystal Enhanced |
| `0x1600` | LGP Opal Film |
| `0x1601` | LGP Grating Scan |
| `0x1602` | LGP Stress Glass |
| `0x1603` | LGP Moire Silk |
| `0x1604` | LGP Caustic Shards |
| `0x1605` | LGP Parallax Depth |
| `0x1606` | LGP Stress Glass (Melt) |
| `0x1607` | LGP Grating Scan (Breakup) |
| `0x1608` | LGP Water Caustics |
| `0x1609` | LGP Schlieren Flow |

### By QUANTUM + CENTER_ORIGIN + PHYSICS

| EID | Name |
|-----|------|
| `0x0600` | LGP Quantum Tunneling |
| `0x0601` | LGP Gravitational Lensing |
| `0x0602` | LGP Time Crystal |
| `0x0603` | LGP Soliton Waves |
| `0x0604` | LGP Metamaterial Cloak |
| `0x0605` | LGP GRIN Cloak |
| `0x0606` | LGP Caustic Fan |
| `0x0607` | LGP Birefringent Shear |
| `0x0608` | LGP Anisotropic Cloak |
| `0x0609` | LGP Evanescent Skin |
| `0x1901` | Catastrophe Caustics |
| `0x1B00` | Time-Reversal Mirror |
| `0x1B04` | Fresnel Caustic Sweep |
| `0x1B05` | Time-Reversal Mirror AR |
| `0x1B06` | Time-Reversal Mirror Mod1 |
| `0x1B07` | Time-Reversal Mirror Mod2 |
| `0x1B08` | Time-Reversal Mirror Mod3 |

### By ORGANIC + CENTER_ORIGIN + PHYSICS

| EID | Name |
|-----|------|
| `0x0505` | LGP Fluid Dynamics |

### By MATHEMATICAL Family

| EID | Name |
|-----|------|
| `0x1900` | Chimera Crown |
| `0x1903` | Lorenz Ribbon |
| `0x1B01` | KdV Soliton Pair |

---

## Quick Reference: Effects That Ignore Palette

These effects hardcode RGB values directly. Changing the palette selection has **no visible effect** on them.

| EID | Name | Color Method |
|-----|------|-------------|
| `0x0100` | Fire | `HeatColor()` heat palette |
| `0x0501` | LGP Bioluminescent Waves | Hardcoded CRGB(0, green, blue) |
| `0x0503` | LGP Neural Network | Hardcoded CRGB (purple/magenta tones) |
| `0x0506` | LGP Neural Network Radial | Hardcoded CRGB (purple/magenta tones) |
| `0x0708` | LGP Chromatic Aberration | Physics-computed chromatic RGB |
| `0x0709` | ~~LGP Perceptual Blend~~ | **Retrofitted** 2026-03-02 — now uses `ctx.palette.getColor()` |
| `0x0F00` | Trinity Test | Hardcoded CRGB (white/magenta/cyan phase) |
| `0x1100` | ES Analog (Ref) | HSV via `hsvProgress()` (no palette, uses gHue) |
| `0x1101` | ES Spectrum (Ref) | HSV via `hsvProgress()` (no palette, uses gHue) |
| `0x1102` | ES Octave (Ref) | HSV via `hsvProgress()` (no palette, uses gHue) |
| `0x1103` | ES Bloom (Ref) | HSV via `hsvProgress()` (no palette, uses gHue) |
| `0x1104` | ES Waveform (Ref) | HSV via `hsvProgress()` (no palette, uses gHue) |
| `0x1600` | LGP Opal Film | Thin-film physics-computed RGB |
| `0x1401` | Beat Pulse (Shockwave) | Uses palette internally? (no `ctx.palette` found) |
| `0x1400` | Beat Pulse (Stack) | Uses palette internally? (no `ctx.palette` found) |

**Note:** Beat Pulse Shockwave and Stack may use `ctx.palette` indirectly via shared utility code not detected by file-level grep. Mark as uncertain.

---

## Quick Reference: Effects That Ignore gHue

These effects do NOT reference `ctx.gHue`. The global hue rotation has **no visible effect** on them.

Notable effects without gHue:

| EID | Name |
|-----|------|
| `0x0100` | Fire |
| `0x0501` | LGP Bioluminescent Waves |
| `0x0503` | LGP Neural Network |
| `0x0506` | LGP Neural Network Radial |
| `0x0708` | LGP Chromatic Aberration |
| `0x0709` | ~~LGP Perceptual Blend~~ | **Retrofitted** 2026-03-02 — now responds to gHue |
| `0x0F00` | Trinity Test |
| `0x0A04` | Audio Waveform |
| `0x0107` | Wave (ambient) |
| All Beat Pulse variants (Stack/Shockwave/Void/Resonant/Ripple/Cascade/Spectral/Spectral Pulse/Breathe/LGP Interference) | -- |
| All Perlin Ambient variants | -- |
| `0x0500` | LGP Aurora Borealis |
| `0x0502` | LGP Plasma Membrane |
| `0x0705` | LGP Color Accelerator |
| `0x0404` | LGP Chromatic Shear |
| `0x1201` | Heartbeat (ES tuned) |
| `0x1202` | LGP Holographic (ES tuned) |
| `0x0A03` | Bass Breath |
| `0x0A01` | Beat Pulse |

---

## Centre-Origin Exception Effects

These effects have an explicit `CENTRE-ORIGIN EXCEPTION` comment in their source code, meaning they use **linear** (edge-to-edge) mapping instead of the standard centre-origin radial mapping despite being tagged `CENTER_ORIGIN`:

| File | Effect |
|------|--------|
| `LGPBirefringentShearEffect.cpp` | LGP Birefringent Shear (`0x0607`) |
| `LGPNeuralNetworkEffect.cpp` | LGP Neural Network (`0x0503`) |
| `LGPSolitonWavesEffect.cpp` | LGP Soliton Waves (`0x0603`) |

---

## Full Effects Table

### Legend

| Column | Meaning |
|--------|---------|
| **EID** | Hex effect ID (`0xFFSS` where FF=family, SS=sequence) |
| **Name** | Display name from `PATTERN_METADATA[]` |
| **Family** | `PatternFamily` enum classification |
| **Palette** | Y = uses `ctx.palette`, N = hardcodes color, HSV = uses CHSV directly |
| **gHue** | Y = references `ctx.gHue`, N = does not |
| **CC Skip** | Y = skips color correction, N = processed by CC |
| **Skip Reason** | Why CC is skipped (hardcoded/family/stateful/tags) |
| **Tone Map** | Y = uses Reinhard tone mapping, N = skips |
| **Silence Gate** | Y = fades to black in silence (family >= 0x18), N = plays in silence |
| **Audio** | Y = in `REACTIVE_EFFECT_IDS[]`, N = ambient/time-based |
| **Centre Origin** | Y = tagged `CENTER_ORIGIN`, N = not tagged |
| **Speed** | Y = uses `ctx.speed`, N = ignores speed control |
| **Notes** | Special behavioral quirks |

### Core / Classic (0x01xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0100` | Fire | Fluid & Plasma | N | N | N | -- | N | N | N | Y | Y | Uses `HeatColor()` builtin palette; ignores user palette and gHue entirely |
| `0x0101` | Ocean | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0102` | Plasma | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0103` | Confetti | Fluid & Plasma | Y | Y | **Y** | Stateful | N | N | N | Y | N | Reads previous frame buffer; CC would compound |
| `0x0104` | Sinelon | Geometric | Y | Y | N | -- | N | N | N | Y | N | |
| `0x0105` | Juggle | Geometric | Y | Y | N | -- | N | N | N | Y | N | |
| `0x0106` | BPM | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0107` | Wave | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | Y | |
| `0x0108` | Ripple | Fluid & Plasma | Y | Y | **Y** | Stateful | N | N | Y | Y | Y | Reads previous frame buffer |
| `0x0109` | Heartbeat | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x010A` | Interference | Interference | Y | Y | **Y** | Hardcoded + Interference family | N | N | Y | Y | Y | Precise wavelength colours |
| `0x010B` | Breathing | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x010C` | Pulse | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | |

### LGP Interference (0x02xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0200` | LGP Box Wave | Interference | Y | Y | **Y** | Hardcoded + Interference family | N | N | N | Y | Y | |
| `0x0201` | LGP Holographic | Interference | Y | Y | **Y** | Interference family | N | N | N | Y | Y | Multi-layer depth illusion; flagship effect |
| `0x0202` | LGP Modal Resonance | Interference | Y | Y | **Y** | Interference family | N | N | N | Y | Y | |
| `0x0203` | LGP Interference Scanner | Interference | Y | Y | **Y** | Hardcoded + Interference family | N | N | Y | Y | Y | |
| `0x0204` | LGP Wave Collision | Interference | Y | Y | **Y** | Interference family | N | N | Y | Y | Y | |

### LGP Geometric (0x03xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0300` | LGP Diamond Lattice | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0301` | LGP Hexagonal Grid | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0302` | LGP Spiral Vortex | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0303` | LGP Sierpinski | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0304` | LGP Chevron Waves | Geometric | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0305` | LGP Concentric Rings | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0306` | LGP Star Burst | Geometric | Y | Y | **Y** | Stateful | N | N | Y | Y | Y | Reads previous frame buffer |
| `0x0307` | LGP Mesh Network | Geometric | Y | Y | N | -- | N | N | N | Y | Y | |

### LGP Advanced Optical (0x04xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0400` | LGP Moire Curtains | Advanced Optical | Y | Y | **Y** | Hardcoded + Adv.Optical+CO | N | N | N | Y | Y | |
| `0x0401` | LGP Radial Ripple | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0402` | LGP Holographic Vortex | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0403` | LGP Evanescent Drift | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0404` | LGP Chromatic Shear | Advanced Optical | Y | N | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0405` | LGP Modal Cavity | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0406` | LGP Fresnel Zones | Advanced Optical | Y | Y | **Y** | Hardcoded + Adv.Optical+CO | N | N | N | Y | N | |
| `0x0407` | LGP Photonic Crystal | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | Y | Y | Y | |

### LGP Organic (0x05xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0500` | LGP Aurora Borealis | Organic | Y | N | N | -- | N | N | N | Y | Y | Has CO tag but no PHYSICS tag, so no CC skip |
| `0x0501` | LGP Bioluminescent Waves | Organic | **N** | N | N | -- | N | N | N | Y | Y | Hardcodes CRGB(0, green, blue); ignores palette |
| `0x0502` | LGP Plasma Membrane | Organic | Y | N | N | -- | N | N | N | Y | Y | |
| `0x0503` | LGP Neural Network | Organic | **N** | N | N | -- | N | N | N | Y | Y | Hardcodes purple/magenta CRGB; CENTRE-ORIGIN EXCEPTION (linear mapping) |
| `0x0504` | LGP Crystalline Growth | Organic | Y | Y | N | -- | N | N | N | Y | Y | Has CO tag but no PHYSICS tag |
| `0x0505` | LGP Fluid Dynamics | Organic | Y | Y | **Y** | Organic + CO + Physics | N | N | N | Y | Y | Only Organic effect with PHYSICS tag |
| `0x0506` | LGP Neural Network Radial | *Not in metadata* | **N** | N | N | No metadata entry | N | N | N | ? | Y | Radial variant; hardcodes CRGB; no PATTERN_METADATA entry |

### LGP Quantum (0x06xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0600` | LGP Quantum Tunneling | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0601` | LGP Gravitational Lensing | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | N | N | Y | Y | Additive particle blending |
| `0x0602` | LGP Time Crystal | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0603` | LGP Soliton Waves | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | CENTRE-ORIGIN EXCEPTION (linear mapping) |
| `0x0604` | LGP Metamaterial Cloak | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0605` | LGP GRIN Cloak | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0606` | LGP Caustic Fan | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0607` | LGP Birefringent Shear | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | CENTRE-ORIGIN EXCEPTION (linear mapping) |
| `0x0608` | LGP Anisotropic Cloak | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x0609` | LGP Evanescent Skin | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | N | N | Y | Y | |
| `0x060A` | LGP Birefringent Shear Radial | *Not in metadata* | Y | Y | N | No metadata entry | N | N | N | ? | Y | Radial variant; no PATTERN_METADATA entry; CC not skipped (no metadata = no family check) |
| `0x060B` | LGP Soliton Waves Radial | *Not in metadata* | Y | Y | N | No metadata entry | N | N | N | ? | Y | Radial variant; no PATTERN_METADATA entry; CC not skipped (no metadata = no family check) |

### LGP Colour Mixing (0x07xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0700` | LGP Color Temperature | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0701` | LGP RGB Prism | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0702` | LGP Complementary Mixing | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0703` | LGP Quantum Colors | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0704` | LGP Doppler Shift | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0705` | LGP Color Accelerator | Color Mixing | Y | N | N | -- | N | N | N | Y | Y | |
| `0x0706` | LGP DNA Helix | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0707` | LGP Phase Transition | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | |
| `0x0708` | LGP Chromatic Aberration | Color Mixing | **N** | N | N | -- | N | N | N | Y | Y | Physics-computed chromatic RGB; ignores palette |
| `0x0709` | LGP Perceptual Blend | Color Mixing | Y | Y | N | -- | N | N | N | Y | Y | Retrofitted 2026-03-02: palette-driven 3-point interpolation with perceptual smoothness |

### LGP Novel Physics (0x08xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0800` | LGP Chladni Harmonics | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | Novel Physics family does not trigger CC skip |
| `0x0801` | LGP Gravitational Wave Chirp | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | Has PHYSICS tag but Novel Physics family != QUANTUM/ORGANIC |
| `0x0802` | LGP Quantum Entanglement | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | Has PHYSICS tag but Novel Physics family != QUANTUM/ORGANIC |
| `0x0803` | LGP Mycelial Network | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | No PHYSICS tag |
| `0x0804` | LGP Riley Dissonance | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | No PHYSICS tag |

### LGP Chromatic (0x09xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0900` | LGP Chromatic Lens | Advanced Optical | Y | Y | **Y** | Hardcoded + Adv.Optical+CO | N | N | N | Y | Y | Classified as Adv.Optical in metadata despite 0x09 family byte |
| `0x0901` | LGP Chromatic Pulse | Advanced Optical | Y | Y | **Y** | Hardcoded + Adv.Optical+CO | N | N | N | Y | Y | Classified as Adv.Optical in metadata |
| `0x0902` | Chromatic Interference | Advanced Optical | Y | Y | **Y** | Hardcoded + Adv.Optical+CO | N | N | N | Y | Y | Classified as Adv.Optical in metadata |

### Audio Reactive (0x0Axx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0A00` | Audio Test | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | N | Diagnostic audio visualizer |
| `0x0A01` | Beat Pulse | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x0A02` | Spectrum Bars | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | N | |
| `0x0A03` | Bass Breath | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | Y | |
| `0x0A04` | Audio Waveform | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x0A05` | Audio Bloom | Fluid & Plasma | Y | Y | N | -- | **Y** | N | Y | Y | Y | Additive blending; `pulse += warmBoost` |
| `0x0A06` | LGP Star Burst (Narrative) | Geometric | Y | Y | **Y** | Stateful | **Y** | N | Y | Y | Y | Reads prev frame + additive blending |
| `0x0A07` | Chord Glow | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0A08` | Wave Reactive | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | Y | |

### Perlin Reactive (0x0Bxx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0B00` | LGP Perlin Veil | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0B01` | LGP Perlin Shocklines | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0B02` | LGP Perlin Caustics | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | Y | Y | Y | Classified as Adv.Optical despite 0x0B family byte |
| `0x0B03` | LGP Perlin Interference Weave | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | Y | Y | Y | Classified as Adv.Optical despite 0x0B family byte |

### Perlin Ambient (0x0Cxx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0C00` | LGP Perlin Veil Ambient | Fluid & Plasma | Y | N | N | -- | N | N | N | Y | Y | |
| `0x0C01` | LGP Perlin Shocklines Ambient | Fluid & Plasma | Y | N | N | -- | N | N | N | Y | Y | |
| `0x0C02` | LGP Perlin Caustics Ambient | Advanced Optical | Y | N | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x0C03` | LGP Perlin Interference Weave Ambient | Advanced Optical | Y | N | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |

### Perlin Backend Test (0x0Dxx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0D00` | Perlin Test: FastLED | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | Test harness |
| `0x0D01` | Perlin Test: Emotiscope2 Full | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | Test harness |
| `0x0D02` | Perlin Test: Emotiscope2 Quarter | Fluid & Plasma | Y | Y | N | -- | N | N | N | Y | Y | Test harness |

### Enhanced Audio-Reactive (0x0Exx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0E00` | BPM Enhanced | Geometric | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0E01` | Breathing Enhanced | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0E02` | LGP Chevron Waves Enhanced | Geometric | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0E03` | LGP Interference Scanner Enhanced | Interference | Y | Y | **Y** | Interference family | N | N | Y | Y | Y | |
| `0x0E04` | LGP Photonic Crystal Enhanced | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | Y | Y | Y | |
| `0x0E05` | LGP Spectrum Detail | Fluid & Plasma | Y | Y | N | -- | **Y** | N | Y | Y | N | Additive spectrum bars |
| `0x0E06` | LGP Spectrum Detail Enhanced | Fluid & Plasma | Y | Y | N | -- | **Y** | N | Y | Y | N | Additive spectrum bars |
| `0x0E07` | LGP Star Burst Enhanced | Geometric | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0E08` | LGP Wave Collision Enhanced | Interference | Y | Y | **Y** | Interference family | N | N | Y | Y | Y | |
| `0x0E09` | Ripple Enhanced | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x0E0A` | Snapwave Linear | Fluid & Plasma | Y | Y | N | -- | **Y** | N | Y | Y | N | SB 4.1.1 bloom parity; `pulse += fadedColor` |

### Diagnostic (0x0Fxx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x0F00` | Trinity Test | Fluid & Plasma | **N** | N | N | -- | **Y** | N | Y | Y | N | Hardcoded CRGB white/magenta/cyan phases; additive white flash; diagnostic parity effect |

### Palette Auto-Cycle (0x10xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1000` | LGP Holographic Auto-Cycle | Interference | Y | Y | **Y** | Interference family | N | N | Y | Y | Y | Internal palette cycling overrides user palette selection |

### ES v1.1 Reference (0x11xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1100` | ES Analog (Ref) | Geometric | **N** | Y (via hsvProgress) | N | -- | N | N | Y | Y | ? | Uses `hsvProgress()` HSV with gHue; no palette; ES v1.1 reference |
| `0x1101` | ES Spectrum (Ref) | Fluid & Plasma | **N** | Y (via hsvProgress) | N | -- | N | N | Y | Y | ? | Uses `hsvProgress()` HSV; ES v1.1 reference |
| `0x1102` | ES Octave (Ref) | Fluid & Plasma | **N** | Y (via hsvProgress) | N | -- | N | N | Y | Y | ? | Uses `hsvProgress()` HSV; ES v1.1 reference |
| `0x1103` | ES Bloom (Ref) | Fluid & Plasma | **N** | Y (via hsvProgress) | N | -- | N | N | Y | Y | ? | Uses `hsvProgress()` HSV; ES v1.1 reference |
| `0x1104` | ES Waveform (Ref) | Fluid & Plasma | **N** | Y (via hsvProgress) | N | -- | N | N | Y | Y | ? | Uses `hsvProgress()` HSV; ES v1.1 reference |

### ES Tuned (0x12xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1200` | Ripple (ES tuned) | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |
| `0x1201` | Heartbeat (ES tuned) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | Y | |
| `0x1202` | LGP Holographic (ES tuned) | Interference | Y | N | **Y** | Interference family | N | N | Y | Y | Y | |

### SensoryBridge Reference (0x13xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1300` | SB Waveform (Ref) | Fluid & Plasma | **N** | Y | N | -- | N | N | Y | Y | ? | Uses CHSV directly with gHue; no palette; SensoryBridge 3.1.0 reference |

### Beat Pulse Family (0x14xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1400` | Beat Pulse (Stack) | Fluid & Plasma | ? | N | N | -- | N | N | Y | Y | N | No `ctx.palette` found in file; may use shared utility |
| `0x1401` | Beat Pulse (Shockwave) | Fluid & Plasma | ? | N | N | -- | N | N | Y | Y | N | No `ctx.palette` found in file; may use shared utility |
| `0x1402` | Beat Pulse (Shockwave In) | Fluid & Plasma | ? | ? | N | -- | N | N | Y | Y | ? | **RETIRED** -- maps to `INVALID_EFFECT_ID` in migration |
| `0x1403` | Beat Pulse (Void) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1404` | Beat Pulse (Resonant) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1405` | Beat Pulse (Ripple) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1406` | Beat Pulse (Shockwave Cascade) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1407` | Beat Pulse (Spectral) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1408` | Beat Pulse (Spectral Pulse) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x1409` | Beat Pulse (Breathe) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | |
| `0x140A` | Beat Pulse (LGP Interference) | Fluid & Plasma | Y | N | N | -- | N | N | Y | Y | N | Dual-strip standing wave mode |
| `0x140B` | Beat Pulse (Bloom) | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | Y | |

### Transport / Parity (0x15xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1500` | Bloom (Parity) | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | N | Stateful HDR history buffer; subpixel transport |
| `0x1501` | Kuramoto Transport | Fluid & Plasma | Y | Y | N | -- | N | N | Y | Y | N | 80-oscillator Kuramoto + Bloom-style transport |

### Holographic Variants (0x16xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1600` | LGP Opal Film | Advanced Optical | **N** | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | Thin-film physics RGB; palette not used; gHue used for OpalFilm via metadata CO |
| `0x1601` | LGP Grating Scan | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1602` | LGP Stress Glass | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1603` | LGP Moire Silk | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1604` | LGP Caustic Shards | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1605` | LGP Parallax Depth | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1606` | LGP Stress Glass (Melt) | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1607` | LGP Grating Scan (Breakup) | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1608` | LGP Water Caustics | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |
| `0x1609` | LGP Schlieren Flow | Advanced Optical | Y | Y | **Y** | Adv.Optical + CO | N | N | N | Y | Y | |

### Reaction Diffusion (0x17xx)

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1700` | LGP Reaction Diffusion | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | Gray-Scott slime; classified as Novel Physics |
| `0x1701` | LGP RD Triangle | Novel Physics | Y | Y | N | -- | N | N | N | Y | Y | |

### Shape Bangers Pack (0x18xx)

All effects in this family have **Silence Gate = Y** (family >= 0x18).

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1800` | LGP Talbot Carpet | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | |
| `0x1801` | LGP Airy Comet | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | |
| `0x1802` | LGP Moire Cathedral | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | |
| `0x1803` | LGP Living Glyph | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | Superformula morphing |
| `0x1804` | LGP Spirograph Crown | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | |
| `0x1805` | LGP Rose Bloom | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | |
| `0x1806` | LGP Harmonograph Halo | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | |
| `0x1807` | LGP Rule 30 Cathedral | Novel Physics | Y | Y | N | -- | N | **Y** | N | Y | Y | Elementary CA textile |
| `0x1808` | LGP Langton Highway | Novel Physics | Y | Y | N | -- | N | **Y** | N | Y | Y | Langton's ant 64x64 grid |
| `0x1809` | LGP Cymatic Ladder | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | |
| `0x180A` | LGP Mach Diamonds | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | |

### Holy Shit Bangers Pack (0x19xx)

All effects in this family have **Silence Gate = Y** (family >= 0x18).

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1900` | Chimera Crown | Mathematical | Y | Y | **Y** | Mathematical family | N | **Y** | N | Y | Y | Kuramoto-Sakaguchi chimera states |
| `0x1901` | Catastrophe Caustics | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | **Y** | N | Y | Y | |
| `0x1902` | Hyperbolic Portal | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | |
| `0x1903` | Lorenz Ribbon | Mathematical | Y | Y | **Y** | Mathematical family | N | **Y** | N | Y | Y | Lorenz attractor; never repeats |
| `0x1904` | IFS Botanical Relic | Organic | Y | Y | N | -- | N | **Y** | N | Y | Y | Barnsley fern IFS; no PHYSICS tag |

### Experimental Audio Pack (0x1Axx)

All effects in this family have **Silence Gate = Y** (family >= 0x18).
All effects in this pack are **Audio Reactive = Y**.

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1A00` | LGP Flux Rift | Fluid & Plasma | Y | Y | N | -- | N | **Y** | Y | Y | N | Implemented in LGPExperimentalAudioPack.cpp |
| `0x1A01` | LGP Beat Prism | Geometric | Y | Y | N | -- | N | **Y** | Y | Y | N | Implemented in LGPExperimentalAudioPack.cpp |
| `0x1A02` | LGP Harmonic Tide | Fluid & Plasma | Y | Y | N | -- | N | **Y** | Y | Y | N | Chord-anchored tidal bands |
| `0x1A03` | LGP Bass Quake | Fluid & Plasma | Y | Y | N | -- | N | **Y** | Y | Y | N | Sub-bass compression waves |
| `0x1A04` | LGP Treble Net | Interference | Y | Y | **Y** | Interference family | N | **Y** | Y | Y | N | |
| `0x1A05` | LGP Rhythmic Gate | Geometric | Y | Y | N | -- | N | **Y** | Y | Y | N | |
| `0x1A06` | LGP Spectral Knot | Geometric | Y | Y | N | -- | N | **Y** | Y | Y | N | |
| `0x1A07` | LGP Saliency Bloom | Fluid & Plasma | Y | Y | N | -- | N | **Y** | Y | Y | N | |
| `0x1A08` | LGP Transient Lattice | Interference | Y | Y | **Y** | Interference family | N | **Y** | Y | Y | N | |
| `0x1A09` | LGP Wavelet Mirror | Geometric | Y | Y | N | -- | N | **Y** | Y | Y | N | |

### Showpiece Pack 3 (0x1Bxx)

All effects in this family have **Silence Gate = Y** (family >= 0x18).

| EID | Name | Family | Palette | gHue | CC Skip | Skip Reason | Tone Map | Silence Gate | Audio | Centre Origin | Speed | Notes |
|-----|------|--------|---------|------|---------|-------------|----------|-------------|-------|---------------|-------|-------|
| `0x1B00` | Time-Reversal Mirror | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | **Y** | N | Y | Y | Record/playback wave; tuned with tone map |
| `0x1B01` | KdV Soliton Pair | Mathematical | Y | Y | **Y** | Mathematical family | N | **Y** | N | Y | Y | Elastic soliton collision |
| `0x1B02` | Gold Code Speckle | Interference | Y | Y | **Y** | Interference family | N | **Y** | N | Y | Y | CDMA pseudo-random speckle |
| `0x1B03` | Quasicrystal Lattice | Geometric | Y | Y | N | -- | N | **Y** | N | Y | Y | Penrose-tiling aperiodic |
| `0x1B04` | Fresnel Caustic Sweep | Quantum | Y | Y | **Y** | Quantum + CO + Physics | N | **Y** | N | Y | Y | |
| `0x1B05` | Time-Reversal Mirror AR | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | **Y** | N | Y | Y | Audio-reactive variant |
| `0x1B06` | Time-Reversal Mirror Mod1 | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | **Y** | N | Y | Y | Dispersive medium variant |
| `0x1B07` | Time-Reversal Mirror Mod2 | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | **Y** | N | Y | Y | Nonlinear medium variant |
| `0x1B08` | Time-Reversal Mirror Mod3 | Quantum | Y | Y | **Y** | Quantum + CO + Physics | **Y** | **Y** | N | Y | Y | Lossy medium variant |

---

### 5-Layer AR Variants (0x1Cxx)

The 20 AR variants are integrated in `CoreEffects.cpp`, `display_order.h`, and `PatternRegistry.cpp` with compile-time gating:

- `FEATURE_AR_1C_EXPERIMENTAL=0` (default): registers **17** stable AR effects.
- `FEATURE_AR_1C_EXPERIMENTAL=1`: registers full **20** AR effects.

Always-on AR effects (`FEATURE_AR_1C_EXPERIMENTAL=0`):

| EID | Name |
|-----|------|
| `0x1C00` | LGP Water Caustics (5L-AR) |
| `0x1C01` | LGP Schlieren Flow (5L-AR) |
| `0x1C03` | LGP Airy Comet (5L-AR) |
| `0x1C04` | LGP Cymatic Ladder (5L-AR) |
| `0x1C05` | LGP Mach Diamonds (5L-AR) |
| `0x1C06` | LGP Reaction Diffusion (5L-AR) |
| `0x1C07` | LGP RD Triangle (5L-AR) |
| `0x1C08` | LGP Moire Cathedral (5L-AR) |
| `0x1C09` | LGP Superformula Glyph (5L-AR) |
| `0x1C0A` | LGP Spirograph Crown (5L-AR) |
| `0x1C0B` | LGP Rose Bloom (5L-AR) |
| `0x1C0C` | LGP Harmonograph Halo (5L-AR) |
| `0x1C0D` | LGP Rule 30 Cathedral (5L-AR) |
| `0x1C0E` | LGP Langton Highway (5L-AR) |
| `0x1C10` | LGP Catastrophe Caustics (5L-AR) |
| `0x1C11` | LGP Hyperbolic Portal (5L-AR) |
| `0x1C13` | LGP IFS Bio Relic (5L-AR) |

Experimental-gated AR effects (`FEATURE_AR_1C_EXPERIMENTAL=1` required):

| EID | Name | Gate reason |
|-----|------|-------------|
| `0x1C02` | LGP Talbot Carpet (5L-AR) | heavy harmonic inner-loop cost |
| `0x1C0F` | LGP Chimera Crown (5L-AR) | heavy Kuramoto coupling loop |
| `0x1C12` | LGP Lorenz Ribbon (5L-AR) | heavy trail projection loop |

---

## Summary Statistics

| Metric | Count |
|--------|-------|
| Registered effects (default build) | 183 |
| Registered effects (`FEATURE_AR_1C_EXPERIMENTAL=1`) | 186 |
| Reactive effects (default build) | 93 |
| Reactive effects (`FEATURE_AR_1C_EXPERIMENTAL=1`) | 96 |
| Experimental-gated AR effects | 3 (`0x1C02`, `0x1C0F`, `0x1C12`) |
| No PATTERN_METADATA entry | 3 (radial variants: `0x0506`, `0x060A`, `0x060B`) |

---

## Known Quirks and Gotchas

### 1. Metadata vs. EID Family Byte Mismatch

The `PatternFamily` enum in `PatternRegistry.h` is a **taxonomy classification**, not a 1:1 mapping of the `effect_ids.h` family byte. Several effects are classified differently:

| EID Family Byte | `effect_ids.h` Family | `PatternMetadata` Family | Affected Effects |
|:---:|---|---|---|
| `0x09` | CHROMATIC | ADVANCED_OPTICAL | Chromatic Lens, Chromatic Pulse, Chromatic Interference |
| `0x0B` | PERLIN_REACTIVE | Fluid & Plasma / ADVANCED_OPTICAL | Perlin Caustics, Perlin Interference Weave classified as Adv.Optical |
| `0x0C` | PERLIN_AMBIENT | Fluid & Plasma / ADVANCED_OPTICAL | Same ambient variants classified as Adv.Optical |
| `0x0E` | ENHANCED_AUDIO | Geometric / Interference / Fluid & Plasma | Various depending on original effect |
| `0x14` | BEAT_PULSE | Fluid & Plasma | All Beat Pulse variants |
| `0x18` | SHAPE_BANGERS | Various (Interference, Geometric, Novel Physics) | Depends on mathematical basis |
| `0x19` | HOLY_SHIT_BANGERS | Mathematical / Quantum / Geometric / Organic | Depends on physics model |
| `0x1A` | EXPERIMENTAL_AUDIO | Various | Depends on visual approach |
| `0x1B` | SHOWPIECE_PACK3 | Quantum / Mathematical / Geometric / Interference | Depends on physics model |

This means `shouldSkipColorCorrection()` uses the **taxonomy family**, not the EID family byte. An effect with EID `0x18xx` (Shape Bangers) can be classified as `PatternFamily::INTERFERENCE` and skip CC via the Interference rule.

### 2. Radial Variants Missing Metadata

The 3 radial variants (`0x0506`, `0x060A`, `0x060B`) have no entry in `PATTERN_METADATA[]`. This means:

- `shouldSkipColorCorrection()` returns `false` for them (no metadata = no family check passes)
- `isLGPSensitive()` returns `false` (no metadata = early return)
- `isAudioReactive()` returns `false` (not in `REACTIVE_EFFECT_IDS[]`)
- `validateEffectId()` returns `INVALID_EFFECT_ID` for them

The original linear effects (Birefringent Shear, Soliton Waves, Neural Network) DO skip CC via Quantum+CO+Physics or Organic classification. The radial variants **do not** skip CC despite being in the same EID family range. This is likely a bug or intentional simplification.

### 3. Silence Gate Catches Non-Audio Effects

The silence gate triggers for ALL effects with family byte >= `0x18`, regardless of whether they're audio-reactive. Several Shape Bangers and Showpiece Pack 3 effects are **not** in `REACTIVE_EFFECT_IDS[]` but still get silence-gated. In practice this means they fade to black when there's no audio, even though they could run as ambient effects.

### 4. LGP Holographic Auto-Cycle Overrides Palette

Effect `0x1000` cycles palettes internally. The user's palette selection is overridden. The effect still reads `ctx.palette` but swaps it internally.

### 5. ES Reference Effects Use HSV, Not Palette

All 5 ES v1.1 reference effects (`0x1100`-`0x1104`) use `hsvProgress()` which maps directly to CHSV with `ctx.gHue`. They respond to gHue rotation but **ignore** the user's palette selection entirely.

### 6. SB Waveform Reference Uses gHue But Not Palette

Effect `0x1300` uses `CHSV(ctx.gHue, ...)` for colour summation. It responds to gHue but ignores palette.

### 7. Stateful Effects Cannot Be CC-Processed Safely

Confetti, Ripple, Star Burst, and Star Burst Narrative read from `ctx.leds[]` from the previous frame. If CC processed them, the corrections would compound frame-over-frame, causing runaway saturation or desaturation.

### 8. Beat Pulse Stack and Shockwave Palette Status Uncertain

Files `BeatPulseStackEffect.cpp` and `BeatPulseShockwaveEffect.cpp` do not contain `ctx.palette` or `palette.` strings. They may use colour via shared beat pulse utility functions or via `CRGB` directly. Their actual palette responsiveness needs runtime verification.

### 9. Speed Parameter Not Universal

About 28 effects ignore `ctx.speed` entirely. Most of these are audio-reactive effects where timing is driven by beat detection / audio features rather than a user speed control. Notable ambient effects that also ignore speed: Confetti, Juggle, Sinelon.

### 10. LGP Film Post Is Infrastructure, Not an Effect

`LGPFilmPost.cpp` appears in the effects directory but is a post-processing utility, not a registered effect. It has no EID.
