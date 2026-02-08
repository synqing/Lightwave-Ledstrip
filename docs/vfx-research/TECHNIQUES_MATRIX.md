# VFX Techniques Selection Matrix

## Visual Effect Goal → Implementation Path

This document provides a decision matrix for selecting and implementing visual effects for the 320-LED strip.

---

## Decision Tree by Visual Category

### Category 1: Natural/Organic Effects

```
Goal: Natural flowing, organic motion
├─ Pattern Type: Smooth, continuous, non-repeating
├─ Best Technique: Perlin Noise
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 3.1
├─ Code: ALGORITHM_IMPLEMENTATIONS.md § 1
├─ Quick Ref: QUICK_REFERENCE.md "Perlin Noise Effect"
├─ Performance: 4.8ms (1 octave), 14.4ms (3 octaves)
├─ Memory: 1KB
└─ Examples:
   ├─ Flowing water: noise(pos + time)
   ├─ Organic growth: noise(pos, time) + exponential envelope
   ├─ Smoke/fire: multi-octave noise with colour mapping
   └─ Liquid metal: noise + bloom post-effect

Goal: Complex, detailed organic (heavier processing)
├─ Pattern Type: Multi-scale, fractal-like
├─ Best Technique: Multi-octave Perlin (3-4 octaves)
├─ Performance Cost: 10-15ms (may exceed budget)
├─ Optimization: Run at lower frame rate (60 FPS)
└─ Alternative: Use precomputed Simplex or cache results

Goal: Ripples / Wave motion
├─ Pattern Type: Radial waves from point(s)
├─ Best Technique: sin(distance - speed * time) per LED
├─ Performance: <1ms
├─ Memory: 0B
└─ Example: sin(fabs(i - centre) - shockTime * 100)
```

### Category 2: Particle & Impact Effects

```
Goal: Explosive impact / burst
├─ Pattern Type: Particles radiating from point
├─ Technique Stack:
│  ├─ Particle pool (emit burst)
│  ├─ Ring buffer (update positions)
│  ├─ Additive blend (render)
│  └─ Post-bloom (glow effect)
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 1 & 4
├─ Code: ALGORITHM_IMPLEMENTATIONS.md § 5
├─ Performance: 1-3ms
├─ Memory: 1.5KB (64 particles)
└─ Tuning: Burst size, velocity range, lifetime, colours

Goal: Shock wave / expanding ring
├─ Sub-components:
│  ├─ Initial flash (0-50ms): Full brightness at impact
│  ├─ Expanding ring (50-300ms): Band of brightness at distance
│  └─ Afterglow (300-800ms): Fade around impact centre
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 4.1
├─ Performance: 1-2ms per impact
├─ Example: Distance-based ring with exponential fade
└─ Stacking: Up to 8 simultaneous impacts at 1-2ms each

Goal: Motion trails / velocity blur
├─ Pattern Type: History buffer fading
├─ Best Technique: Position history + exponential fade
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 6
├─ Code: QUICK_REFERENCE.md "Motion Trail"
├─ Performance: 0.5-1ms
├─ Memory: 240B (30-frame history)
└─ Variation: Velocity-based stretch for fast motion

Goal: Particle spray / rain
├─ Pattern Type: Multiple particles with varied lifetimes
├─ Technique: ParticlePool with continuous emission
├─ Performance: Depends on particle count
│  ├─ 16 particles: 0.3ms
│  ├─ 32 particles: 0.6ms
│  ├─ 64 particles: 1.2ms
├─ Emission Patterns:
│  ├─ Point emitter: From single position
│  ├─ Ring emitter: From all LEDs
│  └─ Burst emitter: Multiple particles at once
└─ Visual Feel: Slow drift = calm, fast = chaotic
```

### Category 3: Light & Glow Effects

```
Goal: Glowing highlights / bloom
├─ Technique Options:
│  ├─ Multi-pass bloom: 1.5ms, high quality
│  ├─ Local bloom: 0.5ms, fast, good enough
│  └─ Single-pixel glow: 0.1ms, minimal
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 2.1
├─ Code: ALGORITHM_IMPLEMENTATIONS.md § 2
├─ Threshold: 150-200 brightness
├─ Strength: 0.3-0.6 (blend factor)
└─ Usage:
   ├─ Post-processing: Apply to final image
   ├─ Per-effect: Apply during effect render
   └─ Selective: Only on specific colour ranges

Goal: Halo / radial glow
├─ Pattern Type: Brightness fades from centre outward
├─ Simple Implementation: exp(-distance² / radius²)
├─ Performance: <0.5ms
├─ Memory: 0B
└─ Tuning: Radius controls glow extent

Goal: Colour separation / chromatic aberration
├─ Pattern Type: RGB channels offset by position
├─ Best Technique: Channel shift by velocity
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 2.2
├─ Performance: 0.5-1ms
├─ Memory: 0B
└─ Usage: Disorientation, energy overload, teleport effects

Goal: Brightness/saturation shift
├─ Pattern Type: Colour space transformation
├─ Techniques:
│  ├─ Desaturate: RGB → Grayscale
│  ├─ Shift hue: RGB → HSV, rotate, → RGB
│  └─ Fade to colour: Blend towards target
├─ Performance: 0.5-1ms
├─ Memory: 0B
└─ Usage: State communication (danger = more red)
```

### Category 4: Procedural & Evolving Patterns

```
Goal: Standing wave interference pattern
├─ Pattern Type: Superposition of sine waves
├─ Best Technique: Multiple sin() calls per LED
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 3.3
├─ Code: ALGORITHM_IMPLEMENTATIONS.md § 3
├─ Performance: 2-5ms
├─ Memory: 500B
├─ Features:
│  ├─ Nodes: Zero-point cancellation
│  ├─ Anti-nodes: Maximum constructive interference
│  └─ Beat frequency: Frequency drift creates motion
└─ Music Sync: Update frequencies based on beat detection

Goal: Cellular automata / evolving rules
├─ Pattern Type: Rule-based state evolution
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 3.4
├─ Code: ALGORITHM_IMPLEMENTATIONS.md § 4
├─ Popular Rules:
│  ├─ Rule 30: Chaotic, fractal-like
│  ├─ Rule 110: Complex, self-organizing
│  ├─ Rule 184: Particle-like flow
│  └─ Rule 90: Sierpinski triangle patterns
├─ Performance: 1-2ms per generation
├─ Memory: 320B (single generation state)
├─ Update Rate: Every 5 frames for visibility
└─ Visual Feel: Organic evolution, hypnotic

Goal: Wave function collapse procedural generation
├─ Pattern Type: Constraint-based generation
├─ Complexity: High (not in embedded scope)
├─ Alternative: Use precomputed patterns
└─ Note: Possible future enhancement if PSRAM allocated

Goal: Fractal patterns
├─ Pattern Type: Self-similar at multiple scales
├─ Technique: Multi-octave noise or CA
├─ Performance: Depends on implementation
└─ Examples:
   ├─ Mandelbrot: Precompute, not real-time
   ├─ Simplex chains: Multi-octave (10-15ms)
   └─ Perlin fractals: Persistence-based (5-10ms)
```

### Category 5: State Communication & UI

```
Goal: Health/energy status indicator
├─ Pattern Type: Colour + animation based on value
├─ Techniques:
│  ├─ Colour coding: Green=healthy, yellow=caution, red=danger
│  ├─ Fill level: Width/brightness proportional to value
│  └─ Pulse pattern: Pulse speed/intensity = urgency
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 7
├─ Performance: <0.5ms
├─ Memory: 0B
├─ Implementation:
│  ├─ Healthy (>70%): Full brightness, steady colour
│  ├─ Caution (40-70%): Pulsing, centre region bright
│  ├─ Warning (15-40%): Flashing, reduced region
│  └─ Critical (<15%): Strobing, only centre
└─ Example Code: QUICK_REFERENCE.md "StateIndicatorEffect"

Goal: Action feedback / confirmation
├─ Pattern Type: Instant visual response to trigger
├─ Techniques:
│  ├─ Success: Green flash, brief
│  ├─ Failure: Red burst, extended
│  └─ Neutral: Blue pulse
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 7.2
├─ Performance: <0.5ms
├─ Duration: 0.2-0.5 seconds
└─ Example Code: QUICK_REFERENCE.md "ConfirmationFeedback"

Goal: Warning / alert signal
├─ Pattern Type: Attention-grabbing animation
├─ Techniques:
│  ├─ Strobe: Fast on/off
│  ├─ Pulse: Slow brightness oscillation
│  ├─ Sweep: Moving brightness across strip
│  └─ Colour flash: Attention-grabbing hue
├─ Performance: <0.5ms
├─ Urgency mapping:
│  ├─ Info: Slow pulse, blue
│  ├─ Warning: Medium pulse, yellow
│  ├─ Alert: Fast strobe, red
│  └─ Critical: Ultra-fast strobe, red+white
└─ Duration: Until cleared by user action

Goal: Mode/function indicator
├─ Pattern Type: Static or slow animation
├─ Techniques:
│  ├─ Colour per mode: Red=attack, blue=defence, green=passive
│  ├─ Pattern per mode: Solid=active, pulse=standby, off=disabled
│  └─ Position: Indicator on specific LEDs
├─ Performance: 0B (no animation) or <0.2ms
├─ Examples:
│  ├─ Mode A: Solid red at position 0
│  ├─ Mode B: Blinking green at position 100
│  └─ Mode C: Off (no indicator)
└─ Combinations: Multiple modes = multiple indicators
```

### Category 6: Charge / Power Effects

```
Goal: Charging accumulation
├─ Pattern Type: Fill expanding from centre/edges
├─ Sub-components:
│  ├─ Core: Solid colour (progress indicator)
│  ├─ Edge: Pulsing bright colour (energy building)
│  └─ Field: Perlin noise for organic waviness
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 5.1
├─ Performance: 2-3ms (with noise)
├─ Perception:
│  ├─ Edge pulse = energy building
│  ├─ Fill direction = progress
│  └─ Noise = instability
└─ Example Code: QUICK_REFERENCE.md "Charging effect"

Goal: Peak / fully charged
├─ Pattern Type: Maximum intensity, unstable
├─ Techniques:
│  ├─ Bright glow: Full brightness + bloom
│  ├─ Fast pulse: High frequency oscillation
│  ├─ Colour shift: Cycle hue showing instability
│  └─ Noise field: Perlin noise around peak
├─ Performance: 1-2ms
├─ Duration: Brief (0.5-1s)
└─ Visual: Feels "overcharged", ready to release

Goal: Release / discharge
├─ Pattern Type: Sudden release of energy
├─ Phases:
│  ├─ Phase 1 (0-100ms): Bright white flash
│  ├─ Phase 2 (100-600ms): Energy wave expanding outward
│  └─ Phase 3 (600ms+): Fade to baseline
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 5.2
├─ Performance: 2-3ms per phase
├─ Appearance:
│  ├─ Wave colour: Often opposite of charging colour
│  ├─ Wave speed: Fast (100-200 LEDs/sec)
│  └─ Falloff: Exponential fade
└─ Example Code: QUICK_REFERENCE.md "EnergyReleaseEffect"

Goal: Cooldown / recovery
├─ Pattern Type: Slow fade to baseline
├─ Techniques:
│  ├─ Gradual fade: Linear brightness decrease
│  ├─ Colour transition: Fade to base colour
│  └─ Settling: Reduced noise/animation
├─ Performance: <0.5ms
├─ Duration: 1-3 seconds
└─ Perception: Effect is "recovering"
```

### Category 7: Music-Reactive Effects

```
Goal: Beat-synchronized effects
├─ Pattern Type: Animation triggered by beat detection
├─ Technique Stack:
│  ├─ Beat detection: Compare magnitude to moving average
│  ├─ Frequency mapping: Map frequency to visual element
│  └─ Sync mechanism: Phase lock to beat grid
├─ Reference: GAME_VFX_TO_LED_PATTERNS.md § 1 & 3.3
├─ Code: QUICK_REFERENCE.md "Beat Sync Integration"
├─ Performance: 0.2-0.5ms (detection) + effect time
├─ Components:
│  ├─ Bass (~60Hz): Controls brightness/intensity
│  ├─ Mid (~1kHz): Controls animation frequency
│  └─ Treble (~10kHz): Controls detail/noise
└─ Visual Result: Lights pulse with music

Goal: Frequency-responsive effects
├─ Pattern Type: Different visuals per frequency band
├─ Techniques:
│  ├─ 3-band: Bass (red), Mid (green), Treble (blue)
│  ├─ Multi-band: 8-16 frequency bands mapped to regions
│  └─ Spectrum: FFT output directly to LED positions
├─ Performance: Depends on FFT implementation
│  ├─ Simple beat (1 value): <0.5ms
│  ├─ 8-band analysis: 2-3ms
│  ├─ Full FFT (1024 bins): 10-20ms (separate thread)
├─ Integration: Use precomputed FFT if available
└─ Visuals: Per-band brightness/colour creates spectrum effect

Goal: Tempo-locked animations
├─ Pattern Type: Animations synchronized to BPM
├─ Technique: Derive phase from beat time
├─ Reference: QUICK_REFERENCE.md "Time Sync (BPM)"
├─ Performance: <0.5ms
├─ Implementation:
│  ├─ Measure BPM (from beat detection)
│  ├─ Derive beatPhase: fmod(time, 60/BPM) / (60/BPM)
│  └─ Map phase to animation (0.0 to 1.0)
├─ Synced Elements:
│  ├─ Pulse timing: On/off with beat
│  ├─ Animation phase: Resets with beat
│  └─ Colour shift: Cycles per beat
└─ Perception: Effects feel "locked" to music
```

### Category 8: Complex Multi-Layer Effects

```
Goal: Complete effect composition
├─ Example: "Electric Storm" effect
│  ├─ Layer 1: Perlin noise background (dark blue)
│  ├─ Layer 2: Random spark particles (orange bursts)
│  ├─ Layer 3: Post-bloom glow
│  └─ Total: ~4-5ms per frame
│
├─ Example: "Resonance" effect
│  ├─ Layer 1: Wave interference (standing waves)
│  ├─ Layer 2: Beat-driven frequency shift
│  ├─ Layer 3: Hue modulation by wave phase
│  └─ Total: ~3-5ms per frame
│
└─ Example: "Impact Cascade"
   ├─ Layer 1: Multiple simultaneous impacts
   ├─ Layer 2: Particle effects around impacts
   ├─ Layer 3: Bloom post-processing
   └─ Total: ~5-7ms per frame

Reference: GAME_VFX_TO_LED_PATTERNS.md § 8 (Integration Patterns)
```

---

## Technique Performance Comparison

### By Rendering Time (ms per 120 FPS frame)

```
<0.5ms (Very Fast)
├─ State indicators (colour + pulse)
├─ Simple sine wave
├─ Linear fill
├─ Single LED glow
└─ Chromatic aberration

0.5-2ms (Fast)
├─ Local bloom
├─ Particle update (32 particles)
├─ Cellular automata (1 generation)
├─ Motion trail (history render)
├─ Colour transformations
└─ Shockwave / expanding ring

2-5ms (Medium)
├─ Perlin noise (1 octave, 320 LEDs)
├─ Wave interference (multiple sources)
├─ Particle effects (64 particles)
├─ Beat detection + response
├─ Combined particle + bloom
└─ Charging effect + noise

5-10ms (Slow - Careful Use)
├─ Perlin noise (2 octaves)
├─ Multi-band audio analysis
├─ Complex particle combinations
└─ Heavy post-processing stacks

>10ms (Exceeds Budget)
├─ Perlin noise (3+ octaves)
├─ Full FFT (1024 bins, real-time)
├─ Unoptimized algorithms
└─ Multiple expensive effects combined
```

### By Memory Allocation (SRAM)

```
0B (No Allocation)
├─ Sine-based animations
├─ Chromatic aberration
├─ Colour transformations
├─ Shockwave rings
└─ State indicators

<500B (Minimal)
├─ Wave interference (sources array)
├─ Simple particle pool setup
├─ Cellular automata (1 generation)
└─ Beat detection buffers

500B-1.5KB (Small)
├─ Perlin noise (1KB permutation)
├─ Motion trail (240B history)
├─ Particle pool (32 particles)
└─ Audio analysis (1KB buffers)

1.5-3KB (Medium)
├─ Particle pool (64 particles = 1.5KB)
├─ Bloom buffers (2KB)
├─ Combined noise + particle effects
└─ Multi-octave noise setup

>3KB (Large - Use PSRAM if possible)
├─ Precomputed lookup tables
├─ Audio buffers (FFT)
├─ HD effect data
└─ Cache for expensive computations
```

---

## Selection Flowchart

```
START: What visual do you want?

├─ ORGANIC / NATURAL
│  ├─ Smooth flow? → Perlin noise (§ 3.1) → 4-5ms
│  ├─ Waves? → Wave interference (§ 3.3) → 2-5ms
│  └─ Evolving? → Cellular automata (§ 3.4) → 1-2ms
│
├─ IMPACT / PARTICLE
│  ├─ Explosion? → Layered burst (§ 4.1) → 2-3ms
│  ├─ Trail? → History buffer (§ 6.1) → 0.5-1ms
│  └─ Spray? → Continuous particles → 1-2ms
│
├─ LIGHT / GLOW
│  ├─ Highlights? → Bloom effect (§ 2.1) → 0.5-1.5ms
│  ├─ Halo? → Gaussian falloff → <0.5ms
│  └─ Colour shift? → Chromatic aberration (§ 2.2) → 0.5ms
│
├─ STATE / UI
│  ├─ Health bar? → Fill + colour (§ 7.1) → <0.5ms
│  ├─ Feedback? → Flash pattern (§ 7.2) → <0.5ms
│  └─ Mode indicator? → Static colour → 0B
│
├─ POWER / ENERGY
│  ├─ Charging? → Fill + pulse + noise (§ 5.1) → 2-3ms
│  ├─ Discharge? → Flash + wave (§ 5.2) → 2-3ms
│  └─ Peak? → Glow + oscillate (§ 5) → 1-2ms
│
└─ MUSIC-REACTIVE
   ├─ Beat sync? → Detection + effect (§ 3.3) → varies
   ├─ Frequency? → Multi-band analysis → 2-5ms
   └─ Tempo lock? → Phase calculation → <0.5ms

COMBINE: Stack multiple effects using composition (§ 8)
- Total budget: ~8ms per frame
- Typical combinations: 5-7ms
- Safety margin: ~1-2ms
```

---

## Quick Selection by Constraint

### If you have <1ms to spare:
- Simple sine pulses
- State indicators
- Single LED glow
- Linear fill indicators

### If you have 2-3ms:
- Perlin noise (1 octave)
- Local bloom
- Particle effects (32)
- Cellular automata
- Motion trails

### If you have 5+ ms:
- Perlin noise + bloom
- Wave interference + particles
- Charging effects
- Multi-layer compositions
- Music sync effects

### If SRAM is tight (<500B free):
- Sine-based effects (0B)
- Colour transforms (0B)
- Chromatic aberration (0B)
- Simple state indicators (0B)

### If PSRAM is available:
- Multi-octave noise (3-4 octaves)
- FFT audio analysis
- Precomputed lookup tables
- Large effect buffers

---

## Summary Table: 30 Common Effects

| Effect | Category | Best Technique | Time | Memory | Complexity | Visual Impact |
|--------|----------|-----------------|------|--------|-----------|---------------|
| Flowing water | Organic | Perlin 1 octave | 4.8ms | 1KB | Med | High |
| Fire effect | Organic | Perlin + colour | 5ms | 1KB | Med | High |
| Ripples | Organic | sin(distance) | <0.5ms | 0B | Low | Medium |
| Standing waves | Music-reactive | Wave interference | 3ms | 500B | Med | High |
| Cellular pattern | Evolving | Rule 110 CA | 1.5ms | 320B | Low | Medium |
| Explosion | Impact | Burst particles + shockwave | 3ms | 1.5KB | Med | High |
| Shock wave | Impact | Expanding ring | 1ms | 0B | Low | High |
| Spark burst | Particle | Particle pool | 1.5ms | 1.5KB | Med | Medium |
| Motion trail | Particle | History buffer | 0.8ms | 240B | Low | Medium |
| Glow highlight | Light | Local bloom | 0.5ms | 0B | Low | Medium |
| Bloom effect | Light | Multi-pass bloom | 1.5ms | 2KB | Low-Med | High |
| Colour shift | Light | RGB transform | 0.5ms | 0B | Very Low | Low-Med |
| Chromatic aberration | Light | Channel offset | 0.5ms | 0B | Low | Medium |
| Health bar | State | Fill + colour | 0.3ms | 0B | Very Low | Low |
| Status pulse | State | sin pulse + colour | 0.2ms | 0B | Very Low | Low |
| Charging bar | Power | Fill + pulse + noise | 2.5ms | 1KB | Med | High |
| Charging peak | Power | Glow + oscillate | 1.5ms | 0B | Low | Med |
| Release burst | Power | Flash + wave | 2ms | 0B | Low-Med | High |
| Beat pulse | Music-reactive | Beat detection + sin | 0.5ms | 0B | Low | Medium |
| Beat flash | Music-reactive | Magnitude → brightness | 0.2ms | 0B | Very Low | Medium |
| Frequency pulse | Music-reactive | Band analysis | 1ms | 500B | Med | Medium |
| Tempo sync | Music-reactive | Beat phase → anim | 0.3ms | 0B | Low | Medium |
| Idle pulse | Simple | slow sin pulse | 0.1ms | 0B | Very Low | Low |
| Alert strobe | Simple | Fast on/off | 0.1ms | 0B | Very Low | High |
| Warning scan | Simple | Moving highlight | 0.5ms | 0B | Low | Medium |
| Fade to colour | Simple | Lerp to target | 0.2ms | 0B | Very Low | Low |
| Rainbow sweep | BANNED | Don't use | — | — | — | — |
| Solid colour | Simple | memset | 0.1ms | 0B | Very Low | — |
| Composite effect | Complex | Multiple layers | 5-7ms | 2-3KB | High | Very High |
| Custom blend | Complex | User-defined | varies | varies | High | varies |

---

## Integration Path: From Idea to Effect

### Phase 1: Conceptualization (5 min)
1. Describe visual goal in one sentence
2. Find category above
3. Note suggested technique & performance
4. Check if performance fits budget

### Phase 2: Research (5-10 min)
1. Read relevant section in GAME_VFX_TO_LED_PATTERNS.md
2. Review code in ALGORITHM_IMPLEMENTATIONS.md
3. Check QUICK_REFERENCE.md for similar effects

### Phase 3: Prototyping (10-30 min)
1. Copy base code from examples
2. Integrate into effect class
3. Set initial parameters
4. Test on physical LEDs

### Phase 4: Optimization (5-20 min)
1. Profile using EffectProfiler
2. Identify bottlenecks
3. Apply optimization tips from § 9
4. Retest to verify improvement

### Phase 5: Polish (10-30 min)
1. Tune parameters for visual quality
2. Test with other effects
3. Verify frame rate stays at 120 FPS
4. Test edge cases (empty, full brightness, etc.)

---

## References

- **Full techniques guide:** GAME_VFX_TO_LED_PATTERNS.md
- **Code implementations:** ALGORITHM_IMPLEMENTATIONS.md
- **Quick snippets:** QUICK_REFERENCE.md
- **Overview & navigation:** README.md

---

Generated 2026-02-08 from research of 40+ sources in game development, VFX, and embedded systems.

