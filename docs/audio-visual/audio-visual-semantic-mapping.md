# Audio-Visual Intelligence Architecture

**Version:** 2.0.0
**Last Updated:** 2025-12-28
**Status:** Reference Document

For the concrete PRISM/Trinity → firmware transport protocol, see `docs/audio-visual/TRINITY_WS_PROTOCOL.md`.

This document establishes the architectural framework for audio-reactive visualizations in LightwaveOS. It rejects rigid frequency-to-visual bindings in favor of adaptive, musical-intelligence-driven approaches.

**See also:** `docs/audio-visual/prism-trinity-bridge.md` for the concrete PRISM/Trinity WebSocket protocol and the firmware-side semantic bridge (segments → narrative + parameter sweeps).

---

## Table of Contents

1. [The Pattern Cage Problem](#the-pattern-cage-problem)
2. [The Binding Trap (What NOT To Do)](#the-binding-trap-what-not-to-do)
3. [Musical Intelligence Principles](#musical-intelligence-principles)
4. [ControlBus Signal Inventory](#controlbus-signal-inventory)
5. [Adaptive Response Architecture](#adaptive-response-architecture)
6. [Sensory Bridge Lessons](#sensory-bridge-lessons)
7. [Mandatory Layer Audit Protocol](#mandatory-layer-audit-protocol)

---

## The Pattern Cage Problem

### The Original Cage

From audit of 76 effects, all patterns use **bounded state spaces** that create predictable loops:

| Cage Type | Boundary | Problem |
|-----------|----------|---------|
| Phase cage | 0 → 2π | Always wraps at same point |
| Radial cage | 0 → 80 LEDs | Always resets at edge |
| Time cage | 0.6-2.0s states | Predictable state machine |
| Energy cage | 0.0-1.0 | Same normalization |

**Result:** Users recognize the pattern within 2-3 loops. Audio only modifies SPEED or INTENSITY within the cage, never the STRUCTURE.

### Three Composition Patterns

1. **Sequential Fade-Render** - Simple effects
2. **Radial Buffer Pipeline** - Bloom/ripple effects
3. **State Machine** - Narrative effects

All are cages. Audio breaks free only when it becomes the PRIMARY DRIVER.

---

## The Binding Trap (What NOT To Do)

### The Seductive Mistake

It's tempting to create rigid semantic bindings:

```
WRONG APPROACH:
bass → expansion
treble → shimmer
chord → hue
snare → burst
```

**This is just another cage.** Same input = same output. Forever. Predictable.

### Why Rigid Bindings Fail

| Problem | Why It Happens |
|---------|---------------|
| Vocal songs feel dead | No bass = no expansion = boring |
| Ambient music is erratic | No beat = confused response |
| EDM drops feel same as verse | No context awareness |
| Classical is over-sensitive | Tiny dynamics = huge reactions |
| Users "figure out" the pattern | Deterministic = predictable |

### The Frequency Spectrum Trap

Frequency spectrum analysis is the **most basic, amateur** level of audio DSP:

- 60-250Hz = bass? What about bass guitar harmonics at 500Hz?
- Treble = shimmer? What about harsh treble that should feel aggressive?
- Same frequencies mean different things in different musical contexts

**Musical intelligence has evolved past frequency isolation.**

---

## Musical Intelligence Principles

### Core Insight: Audio as DRIVER, Not Modifier

**Wrong model:**
```cpp
// Audio modifies existing pattern
phase += baseRate * speedNorm * audioModifier;
//       ^^^^^^^^^ the cage   ^^^^^^^^^^^^^^^^ modification
```

**Correct model:**
```cpp
// Audio determines what happens
if (musicalSaliency.harmonic > musicalSaliency.rhythmic) {
    // Chord changes drive visuals
    visualBehavior = HARMONIC_DRIFT;
} else {
    // Beat drives visuals
    visualBehavior = RHYTHMIC_PULSE;
}
```

### The Five Principles

#### 1. Musical Saliency Analysis

Ask: **"What's perceptually important RIGHT NOW?"**

| Saliency Type | Detected Via | Visual Implication |
|---------------|--------------|-------------------|
| Harmonic novelty | Chord changes, key shifts | Color/mood changes |
| Rhythmic novelty | Beat pattern changes | Motion changes |
| Timbral novelty | Spectral flux, instrument changes | Texture changes |
| Dynamic novelty | RMS envelope, crescendo/decrescendo | Intensity changes |

The MOST SALIENT feature should drive visuals. Not all features simultaneously.

#### 2. Style-Adaptive Response

Music has different **intrinsic drivers**:

| Music Style | Primary Driver | Secondary | Visual Strategy |
|-------------|---------------|-----------|-----------------|
| EDM/Hip-hop | Rhythm (beat) | Bass energy | Pulse on beat, expand on bass |
| Jazz/Classical | Harmony (chords) | Dynamics | Drift with harmony, breathe with dynamics |
| Vocal Pop | Melody (treble) | Rhythm | Shimmer with melody, pulse with beat |
| Ambient/Drone | Texture (flux) | Harmonic | Organic drift, color evolution |
| Orchestral | Dynamics (RMS) | Harmony | Build/release, harmonic color |

**The system should DETECT the style and ADAPT its response strategy.**

#### 3. Behavior Selection (Not Value Mapping)

Instead of: `bass_energy → expansion_amount` (deterministic)

Use: `musical_context → behavior_type` (adaptive)

| Behavior | What It Does | When To Use |
|----------|--------------|-------------|
| `PULSE_ON_BEAT` | Sharp expansion on beat tick | Rhythmic music |
| `DRIFT_WITH_HARMONY` | Slow color evolution with chords | Harmonic music |
| `SHIMMER_WITH_MELODY` | Sparkle following treble | Melodic music |
| `BREATHE_WITH_DYNAMICS` | Organic swell with RMS | Dynamic music |
| `TEXTURE_FLOW` | Gradual morphing with flux | Textural music |

#### 4. Temporal Context Awareness

**Music has memory.** Visuals should too.

| Context | What It Tracks | Visual Use |
|---------|---------------|------------|
| Tension level | Harmonic distance from tonic | Building anticipation |
| Energy trend | RMS slope over 4-8 bars | Crescendo/decrescendo response |
| Repetition | Pattern self-similarity | Reduce complexity when music repeats |
| Section | Verse/chorus/drop detection | Mode switches at section boundaries |

#### 5. Non-Deterministic Variation

**Same input should NOT always produce same output.**

Mechanisms:
- Weighted random behavior selection
- Generative procedural layers
- Novelty-triggered rerouting (only change when music changes)
- Hysteresis (prevent rapid switching)

---

## ControlBus Signal Inventory

### Available Signals (19+)

We have far more than basic spectrum analysis:

#### Energy Signals
| Signal | Description | Temporal Class |
|--------|-------------|----------------|
| `rms` | Overall amplitude | Sustained |
| `fast_rms` | Fast-attack RMS | Reactive |
| `flux` | Spectral change rate | Reactive |
| `novelty` | Onset detection proxy | Reactive |

#### Rhythm Signals
| Signal | Description | Temporal Class |
|--------|-------------|----------------|
| `beat_tick` | Beat boundary pulse | Impulsive |
| `beat_phase` | Position within beat (0-1) | Sustained |
| `downbeat_tick` | Bar boundary pulse | Impulsive |
| `bpm` | Estimated tempo | Slow |
| `tempo_confidence` | Beat detection confidence | Slow |

#### Percussion Signals
| Signal | Description | Temporal Class |
|--------|-------------|----------------|
| `snareEnergy` | 150-300Hz band energy | Reactive |
| `hihatEnergy` | 6-12kHz band energy | Reactive |
| `snareTrigger` | Snare onset detected | Impulsive |
| `hihatTrigger` | Hi-hat onset detected | Impulsive |

#### Harmonic Signals
| Signal | Description | Temporal Class |
|--------|-------------|----------------|
| `chroma[12]` | Pitch class energy (C-B) | Sustained |
| `heavy_chroma[12]` | Smoothed pitch classes | Slow |
| `chordState.root` | Detected chord root (0-11) | Slow |
| `chordState.type` | Major/Minor/Dim/Aug | Slow |
| `chordState.confidence` | Detection confidence | Slow |

#### Spectral Signals
| Signal | Description | Temporal Class |
|--------|-------------|----------------|
| `bands[8]` | Octave band energies | Sustained |
| `heavy_bands[8]` | Smoothed bands | Slow |
| `bins64[64]` | Full 64-semitone spectrum | Reactive |
| `waveform[128]` | Time-domain samples | Impulsive |

### Currently Underutilized

These powerful signals are barely used:
- `bins64[64]` - Full chromatic resolution
- `heavy_bands[8]` / `heavy_chroma[12]` - Ultra-smooth variants
- `chordState.confidence` - Could gate chord-based responses
- `beat_phase` fractional - Could create smooth beat-synced motion
- `tempo_confidence` - Could modulate response certainty

### Derivable Higher-Order Features

From existing signals, we could compute:
- **Harmonic complexity** - Entropy of chroma distribution
- **Tonal center** - Weighted pitch class dominance
- **Harmonic tension** - Distance from tonic
- **Spectral centroid** - "Brightness" (works on ALL music including vocals)
- **Zero-crossing rate** - Complexity/noisiness
- **Dynamic range** - Peak vs RMS over time window

---

## Adaptive Response Architecture

### Proposed 5-Layer System

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1: Musical Saliency Analysis                      │
│  - What's perceptually important RIGHT NOW?              │
│  - Harmonic / Rhythmic / Timbral / Dynamic novelty       │
└───────────────────────┬─────────────────────────────────┘
                        ▼
┌─────────────────────────────────────────────────────────┐
│  Layer 2: Style Classification (Runtime Adaptive)        │
│  - RHYTHMIC_DRIVEN (EDM, hip-hop)                        │
│  - HARMONIC_DRIVEN (jazz, classical)                     │
│  - MELODIC_DRIVEN (vocal pop)                            │
│  - TEXTURE_DRIVEN (ambient, drone)                       │
│  - DYNAMIC_DRIVEN (orchestral)                           │
└───────────────────────┬─────────────────────────────────┘
                        ▼
┌─────────────────────────────────────────────────────────┐
│  Layer 3: Dynamic Parameter Router                       │
│  - Based on detected style, route different signals      │
│  - Rhythmic: beat→intensity, flux→speed                  │
│  - Harmonic: chord→hue, type→saturation                  │
│  - Melodic: treble→brightness, mid→saturation            │
└───────────────────────┬─────────────────────────────────┘
                        ▼
┌─────────────────────────────────────────────────────────┐
│  Layer 4: Behavior Selection                             │
│  - Select visual BEHAVIORS, not just values              │
│  - PULSE_ON_BEAT vs DRIFT_WITH_HARMONY                   │
│  - Weighted selection based on saliency                  │
└───────────────────────┬─────────────────────────────────┘
                        ▼
┌─────────────────────────────────────────────────────────┐
│  Layer 5: Contextual Awareness                           │
│  - Track tension/release arcs                            │
│  - Detect section boundaries                             │
│  - Reduce complexity on repetition                       │
│  - Build anticipation before drops                       │
└─────────────────────────────────────────────────────────┘
```

### What This Handles

| Scenario | Rigid Bindings | Adaptive System |
|----------|----------------|-----------------|
| Vocal song (no bass) | Dead (bass→expansion=nothing) | Detects melodic saliency, responds to treble |
| Quiet classical | Over-sensitive | Recognizes dynamic-driven, adjusts sensitivity |
| EDM drop after breakdown | Same as rest | Context tracks tension, triggers burst |
| Ambient drone | Erratic (no beat) | Classifies as texture-driven, uses flux |

---

## Sensory Bridge Lessons

### What Makes It Feel "Alive"

Reference: Sensory Bridge 3.1.0 firmware

#### 1. Asymmetric Envelope Following
```cpp
// Fast attack to respond to transients
if (raw > smoothed) smoothed += delta * 0.25;  // FAST
// Slow decay for sustained notes
else smoothed -= delta * 0.005;  // SLOW
```
This mimics human perception. We notice attacks instantly but releases fade slowly.

#### 2. MOOD Knob = Responsiveness, Not Mapping

MOOD doesn't change WHAT responds. It changes HOW FAST.

| MOOD Position | Behavior |
|---------------|----------|
| Left (0.0-0.5) | Viscous, delayed, organic (follower algorithm) |
| Right (0.5-1.0) | Snappy, crisp, percussive (exponential averaging) |

**Key insight:** Responsiveness is a separate dimension from mapping.

#### 3. Chromagram (Musical Domain)

Not 64 frequency bins. **12 musical notes** (C through B).

Visualizations respond to "what note is playing" not "which frequency is loud."

#### 4. Look-Ahead Smoothing

```cpp
// If frame N is a spike, average with neighbors
if (look_ahead_1_rising != look_ahead_2_rising) {
    frame[N] = average(frame[N-1], frame[N+1]);
}
```

**Predictive deglitching.** The system looks ahead to prevent flicker.

#### 5. Audio Presence as First-Class State

- Silent → Normal → Loud state machine
- Silence triggers dimming (not just "low volume")
- Audio PRESENCE controls visual EXISTENCE

---

## Mandatory Layer Audit Protocol

### REQUIREMENT

**Before ANY audio-reactive work on an effect, agents MUST complete this protocol.**

This ensures understanding of the effect's structure before adding reactivity.

### Step 1: Document All Layers

Read the effect and identify distinct layers:

```markdown
## Effect: [EffectName]

### Layers Identified
1. **[Layer Name]**: [Purpose]
   - State variables: [list]
   - Update frequency: [per-frame/per-hop/on-trigger]

2. **[Layer Name]**: [Purpose]
   ...
```

### Step 2: Map Current Audio Usage

| Layer | Signal Used | How Used | Temporal Class |
|-------|-------------|----------|----------------|
| ... | ... | ... | ... |

### Step 3: Identify Musical Saliency Fit

Ask: **What kind of music would make this effect shine?**

- If rhythmic → ensure beat/percussion drives key layer
- If harmonic → ensure chord/chroma drives color layer
- If textural → ensure flux/RMS drives motion layer

### Step 4: Check for Rigid Binding Traps

Verify NONE of these:
- [ ] Same frequency band ALWAYS drives same visual
- [ ] No consideration of musical context
- [ ] No temporal smoothing or history
- [ ] No variation mechanism

### Step 5: Record in Effect Header

```cpp
/**
 * @file MyEffect.h
 *
 * ## Layer Audit (YYYY-MM-DD)
 *
 * ### Layers
 * 1. [Layer]: [description]
 * 2. [Layer]: [description]
 *
 * ### Musical Fit
 * Best for: [rhythmic/harmonic/melodic/textural] music
 *
 * ### Adaptive Response
 * - [What changes based on musical context]
 *
 * ### Temporal Awareness
 * - [What history/smoothing is used]
 */
```

---

## Quick Reference

### DO NOT

- Create rigid frequency → visual bindings
- Assume bass/beat always present
- Use same response for all music types
- Map instantaneously without temporal context
- Make deterministic input→output functions

### DO

- Analyze musical saliency (what's important NOW)
- Adapt response based on detected music style
- Select behaviors, not just values
- Track temporal context (tension, sections, repetition)
- Add variation mechanisms
- Use asymmetric envelopes (fast attack, slow decay)
- Consider audio presence as a state

### Music Type → Visual Strategy

| Type | Primary Driver | Strategy |
|------|---------------|----------|
| EDM/Hip-hop | Rhythm | Pulse on beat, expand on bass |
| Jazz/Classical | Harmony | Drift with chords, breathe with dynamics |
| Vocal Pop | Melody | Shimmer with treble, pulse with beat |
| Ambient | Texture | Organic flux-driven motion |
| Orchestral | Dynamics | Build/release with RMS envelope |

### Signal Temporal Classes

| Class | Response Time | Examples | Visual Use |
|-------|---------------|----------|-----------|
| Impulsive | 1 frame | beat_tick, triggers | Spawn events |
| Reactive | 100-300ms | flux, fast_rms | Quick response |
| Sustained | 300ms-2s | rms, bands, chroma | Smooth modulation |
| Slow | 500ms-5s | chord, bpm, heavy_* | Mood/palette |

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-12-28 | 2.0.0 | Complete rewrite - removed rigid bindings, added adaptive architecture |
| 2025-12-28 | 1.0.0 | Initial (REJECTED - contained rigid binding trap) |
