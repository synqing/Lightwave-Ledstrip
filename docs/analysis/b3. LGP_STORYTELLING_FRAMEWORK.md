# LGP Storytelling Framework
## Narrative Construction and Emotional Impact Guide for Light Guide Plate Patterns

*Complete methodology for creating narrative light sequences, emotional impact frameworks, and temporal storytelling patterns*

---

## Table of Contents

1. [Narrative Engine Overview](#narrative-engine-overview)
2. [Emotional Impact Framework](#emotional-impact-framework)
3. [Pacing and Rhythm Guidelines](#pacing-and-rhythm-guidelines)
4. [Musical Structure Integration](#musical-structure-integration)
5. [Temporal Narrative Structures](#temporal-narrative-structures)
6. [Storytelling Patterns](#storytelling-patterns)
7. [Implementation Examples](#implementation-examples)

---

## Narrative Engine Overview

### BUILD/HOLD/RELEASE/REST System

The Narrative Engine provides a global temporal conductor for visual drama, managing dramatic timing arcs that all effects can query.

#### Phase Structure

```cpp
enum NarrativePhase {
    BUILD,      // Tension increases, energy builds
    HOLD,       // Peak intensity maintained
    RELEASE,    // Energy releases, tension decreases
    REST        // Calm period, preparation for next cycle
};
```

#### Phase Characteristics

| Phase | Duration | Intensity Curve | Emotional State |
|-------|----------|-----------------|-----------------|
| **BUILD** | 2-8 seconds | Exponential rise | Anticipation, tension |
| **HOLD** | 1-4 seconds | Plateau | Peak excitement, climax |
| **RELEASE** | 2-6 seconds | Exponential decay | Relief, resolution |
| **REST** | 3-10 seconds | Low baseline | Calm, preparation |

#### Usage in Effects

```cpp
// Query narrative intensity
float intensity = NarrativeEngine::getInstance().getIntensity();

// Use intensity to modulate effect parameters
float effectAmplitude = baseAmplitude * intensity;
uint8_t brightness = baseBrightness * intensity;
float speed = baseSpeed * (1.0f + intensity * 0.5f);
```

#### Zone-Based Spatial Choreography

```cpp
// Get intensity with zone offset for spatial effects
float intensity = NarrativeEngine::getInstance().getIntensity(zoneId);

// Different zones can be at different phases
// Creates wave-like propagation across display
```

### Configuration

#### Phase Duration Control

```cpp
// Set individual phase durations
narrativeEngine.setBuildDuration(4.0f);      // 4 seconds
narrativeEngine.setHoldDuration(2.0f);       // 2 seconds
narrativeEngine.setReleaseDuration(3.0f);    // 3 seconds
narrativeEngine.setRestDuration(5.0f);       // 5 seconds

// Or scale all phases proportionally
narrativeEngine.setTempo(14.0f);  // Total cycle duration
```

#### Curve Behaviour

```cpp
// Configure intensity curves
narrativeEngine.setBuildCurve(CURVE_EXPONENTIAL);   // Fast rise
narrativeEngine.setHoldCurve(CURVE_LINEAR);          // Constant
narrativeEngine.setReleaseCurve(CURVE_EASE_OUT);     // Smooth decay
narrativeEngine.setRestCurve(CURVE_LINEAR);          // Baseline
```

---

## Emotional Impact Framework

### Color Psychology Mapping

#### Hue → Emotion Mapping

| Hue Range | Color | Emotion | Energy Level |
|-----------|-------|---------|--------------|
| 0-30 | Red | Passion, intensity, urgency | High |
| 30-60 | Orange-Yellow | Warmth, energy, optimism | High |
| 60-120 | Green | Calm, growth, balance | Medium |
| 120-180 | Cyan-Blue | Serenity, depth, trust | Low-Medium |
| 180-240 | Blue-Magenta | Mystery, introspection | Low |
| 240-270 | Magenta | Creativity, transformation | Medium-High |
| 270-360 | Magenta-Red | Drama, intensity | High |

**Implementation:**
```cpp
CRGB emotionToColor(Emotion emotion) {
    switch(emotion) {
        case EMOTION_PASSION:
            return CHSV(0, 255, 255);      // Red
        case EMOTION_CALM:
            return CHSV(96, 200, 200);     // Green
        case EMOTION_MYSTERY:
            return CHSV(160, 255, 180);    // Blue
        case EMOTION_ENERGY:
            return CHSV(32, 255, 255);     // Orange
        default:
            return CHSV(128, 128, 128);    // Neutral
    }
}
```

#### Saturation → Intensity Mapping

| Saturation | Perceived Intensity | Emotional Impact |
|------------|-------------------|------------------|
| 0-85 | Pastel, soft | Gentle, calming |
| 85-170 | Moderate | Balanced, natural |
| 170-255 | Vivid, intense | Strong, dramatic |

#### Brightness → Energy Mapping

| Brightness | Energy Level | Emotional State |
|------------|--------------|-----------------|
| 0-85 | Low energy | Rest, contemplation |
| 85-170 | Medium energy | Active, engaged |
| 170-255 | High energy | Intense, peak excitement |

### Movement Pattern → Emotional Response

#### Motion Types and Emotions

| Motion Type | Pattern | Emotional Response |
|-------------|---------|-------------------|
| **Expansion** | Outward from centre | Growth, release, opening |
| **Contraction** | Inward to centre | Focus, intensity, gathering |
| **Oscillation** | Back and forth | Uncertainty, tension, rhythm |
| **Spiral** | Rotating expansion | Transformation, evolution |
| **Flow** | Smooth, continuous | Calm, peace, fluidity |
| **Staccato** | Sharp, sudden | Surprise, impact, energy |
| **Cascade** | Falling, flowing | Release, letting go |

**Implementation:**
```cpp
void applyEmotionalMotion(Emotion emotion, float& position, float& velocity) {
    switch(emotion) {
        case EMOTION_GROWTH:
            // Expansion motion
            velocity = 2.0f;
            break;
        case EMOTION_FOCUS:
            // Contraction motion
            velocity = -1.5f;
            break;
        case EMOTION_RHYTHM:
            // Oscillation
            velocity = sin(time * frequency) * amplitude;
            break;
    }
}
```

### Pacing and Intensity Curves

#### Dramatic Arc Curves

**Exponential Build:**
```cpp
float exponentialBuild(float t) {
    // t: 0.0 to 1.0
    return pow(t, 2.0f);  // Slow start, rapid rise
}
```

**Ease-Out Release:**
```cpp
float easeOutRelease(float t) {
    // t: 0.0 to 1.0
    return 1.0f - pow(1.0f - t, 2.0f);  // Fast start, slow end
}
```

**Smoothstep:**
```cpp
float smoothstep(float t) {
    // t: 0.0 to 1.0
    return t * t * (3.0f - 2.0f * t);  // Smooth S-curve
}
```

**Sinusoidal Pulse:**
```cpp
float sinusoidalPulse(float t) {
    // t: 0.0 to 1.0
    return 0.5f + 0.5f * sin(t * TWO_PI);  // Smooth oscillation
}
```

#### Intensity Curves for Dramatic Effect

| Curve Type | Use Case | Emotional Impact |
|------------|----------|------------------|
| **Linear** | Steady progression | Predictable, calm |
| **Exponential** | Building tension | Anticipation, excitement |
| **Logarithmic** | Gradual build | Subtle, gentle |
| **Smoothstep** | Natural transitions | Smooth, organic |
| **Pulse** | Rhythmic patterns | Energy, rhythm |

---

## Pacing and Rhythm Guidelines

### Temporal Pacing

#### Frame Rate Considerations

- **120 FPS Target:** Smooth, fluid motion
- **60 FPS Minimum:** Acceptable for slower patterns
- **30 FPS:** Noticeable stutter, avoid for motion

#### Timing Guidelines

| Pattern Type | Update Rate | Frame Duration |
|--------------|-------------|----------------|
| **Fast Motion** | Every frame (120 Hz) | 8.33ms |
| **Medium Motion** | Every 2 frames (60 Hz) | 16.67ms |
| **Slow Motion** | Every 4 frames (30 Hz) | 33.33ms |
| **Ambient** | Every 8 frames (15 Hz) | 66.67ms |

### Rhythm Synchronization

#### Beat Detection Integration

```cpp
void syncToBeat(const AudioData& audio) {
    if (audio.beat.detected) {
        // Reset phase on beat
        beatPhase = 0.0f;
        // Adjust rate based on BPM
        beatRate = audio.beat.bpm / 120.0f;  // Normalized to 120 BPM
    }
    
    // Advance phase
    beatPhase += 0.01f * beatRate;
    if (beatPhase > 1.0f) beatPhase -= 1.0f;
    
    // Create beat-synced wave
    float wave = sin(beatPhase * TWO_PI);
}
```

#### Musical Timing

| Musical Element | Visual Response | Timing |
|-----------------|-----------------|--------|
| **Beat** | Immediate pulse | Instant |
| **Bar** | Pattern change | 4 beats |
| **Phrase** | Major transition | 4 bars (16 beats) |
| **Section** | Effect change | 8-16 bars |

### Silence Utilization

#### "Silent Film Mode"

Patterns that create beauty from silence:

**Characteristics:**
- Slow, organic motion
- Subtle colour shifts
- Gentle breathing effects
- Ambient, meditative quality

**Implementation:**
```cpp
void silentFilmMode() {
    // Slow, organic motion without audio
    float organicPhase = millis() * 0.0001f;  // Very slow
    float breathing = 0.5f + 0.5f * sin(organicPhase);
    
    // Subtle colour shifts (mandate-compliant)
    // Do NOT rotate hue through the full spectrum; vary within a constrained palette family.
    uint8_t paletteIndex = 16 + (sin8((uint8_t)(organicPhase * 255)) >> 3);  // small local variation
    
    // Gentle brightness variation
    uint8_t brightness = 64 + breathing * 64;
    
    CRGB c = ColorFromPalette(currentPalette, paletteIndex, brightness);
    fill_solid(leds, NUM_LEDS, c);
}
```

---

## Musical Structure Integration

### Verse/Chorus/Bridge Mapping

#### Verse Characteristics
- **Intensity:** Medium (0.4-0.6)
- **Complexity:** Moderate
- **Motion:** Steady, flowing
- **Colour:** Muted, supporting

#### Chorus Characteristics
- **Intensity:** High (0.7-1.0)
- **Complexity:** High
- **Motion:** Dynamic, energetic
- **Colour:** Vivid, dominant

#### Bridge Characteristics
- **Intensity:** Variable (0.3-0.8)
- **Complexity:** Contrasting
- **Motion:** Transitional
- **Colour:** Shifted palette

**Implementation:**
```cpp
void mapMusicalStructure(const AudioData& audio) {
    MusicalSection section = detectSection(audio);
    
    switch(section) {
        case SECTION_VERSE:
            visualParams.intensity = 128;  // Medium
            visualParams.complexity = 100; // Moderate
            useMutedPalette();
            break;
            
        case SECTION_CHORUS:
            visualParams.intensity = 255;  // High
            visualParams.complexity = 200; // High
            useVividPalette();
            break;
            
        case SECTION_BRIDGE:
            visualParams.intensity = 180;  // Variable
            visualParams.complexity = 150; // Contrasting
            shiftPalette();
            break;
    }
}
```

### Transient Response Patterns

#### Attack Response
```cpp
void handleTransient(const AudioData& audio) {
    if (audio.transient.detected) {
        // Immediate response
        triggerPulse(audio.transient.position);
        increaseBrightness(1.5f);
        
        // Quick decay
        fadeBrightness(0.95f);
    }
}
```

#### Sustain Response
```cpp
void handleSustain(const AudioData& audio) {
    float sustainLevel = audio.amplitude;
    
    // Maintain level during sustain
    visualParams.intensity = sustainLevel * 255;
    
    // Gradual evolution
    evolvePattern(sustainLevel * 0.01f);
}
```

#### Release Response
```cpp
void handleRelease(const AudioData& audio) {
    if (audio.amplitude < 0.1f) {
        // Gradual fade
        fadeToBlack(0.98f);
        
        // Return to ambient state
        transitionToAmbient();
    }
}
```

---

## Temporal Narrative Structures

### Short-Form Patterns (30-60 seconds)

**Structure:**
1. **Opening** (5-10s): Establish pattern, introduce colours
2. **Development** (15-30s): Evolve pattern, build complexity
3. **Climax** (5-10s): Peak intensity, maximum complexity
4. **Resolution** (5-10s): Simplify, fade to conclusion

**Example:**
```cpp
void shortFormNarrative(float elapsedTime) {
    float progress = elapsedTime / 60.0f;  // 60 second duration
    
    if (progress < 0.15f) {
        // Opening: Simple, clear
        renderSimplePattern(progress * 6.67f);
    } else if (progress < 0.65f) {
        // Development: Building complexity
        renderComplexPattern((progress - 0.15f) * 2.0f);
    } else if (progress < 0.85f) {
        // Climax: Maximum intensity
        renderPeakPattern((progress - 0.65f) * 5.0f);
    } else {
        // Resolution: Simplifying
        renderResolutionPattern((progress - 0.85f) * 6.67f);
    }
}
```

### Long-Form Sequences (3-5 minutes)

**Structure:**
1. **Introduction** (30-60s): Establish themes
2. **Exposition** (60-120s): Develop patterns
3. **Rising Action** (60-90s): Build tension
4. **Climax** (30-60s): Peak intensity
5. **Falling Action** (30-60s): Release tension
6. **Resolution** (30-60s): Conclusion

**Implementation:**
```cpp
void longFormNarrative(float elapsedTime) {
    float totalDuration = 300.0f;  // 5 minutes
    float progress = elapsedTime / totalDuration;
    
    if (progress < 0.2f) {
        // Introduction
        narrativePhase = PHASE_REST;
    } else if (progress < 0.5f) {
        // Exposition + Rising Action
        narrativePhase = PHASE_BUILD;
    } else if (progress < 0.7f) {
        // Climax
        narrativePhase = PHASE_HOLD;
    } else if (progress < 0.9f) {
        // Falling Action
        narrativePhase = PHASE_RELEASE;
    } else {
        // Resolution
        narrativePhase = PHASE_REST;
    }
}
```

### Looping Strategies

#### Seamless Loop
```cpp
void seamlessLoop(float time) {
    float loopDuration = 30.0f;  // 30 second loop
    float loopPhase = fmod(time, loopDuration) / loopDuration;
    
    // Ensure smooth transition at loop boundary
    if (loopPhase < 0.01f || loopPhase > 0.99f) {
        // Smooth transition zone
        applyTransition(loopPhase);
    }
    
    renderPattern(loopPhase);
}
```

#### Varied Loop
```cpp
void variedLoop(float time) {
    float baseDuration = 30.0f;
    float variation = sin(time * 0.1f) * 5.0f;  // ±5 second variation
    float loopDuration = baseDuration + variation;
    
    float loopPhase = fmod(time, loopDuration) / loopDuration;
    renderPattern(loopPhase);
}
```

### Transition Narratives

#### Crossfade Transition
```cpp
void crossfadeTransition(Pattern& oldPattern, Pattern& newPattern, float progress) {
    // progress: 0.0 (old) to 1.0 (new)
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB oldColor = oldPattern.getColor(i);
        CRGB newColor = newPattern.getColor(i);
        leds[i] = blend(oldColor, newColor, progress * 255);
    }
}
```

#### Morph Transition
```cpp
void morphTransition(Pattern& oldPattern, Pattern& newPattern, float progress) {
    // Interpolate parameters instead of colours
    float intensity = lerp(oldPattern.intensity, newPattern.intensity, progress);
    float speed = lerp(oldPattern.speed, newPattern.speed, progress);
    float complexity = lerp(oldPattern.complexity, newPattern.complexity, progress);
    
    // Render with interpolated parameters
    renderPattern(intensity, speed, complexity);
}
```

---

## Storytelling Patterns

### The Hero's Journey

**Structure:**
1. **Ordinary World** (REST): Calm, familiar patterns
2. **Call to Adventure** (BUILD): New pattern emerges
3. **Crossing Threshold** (BUILD): Pattern intensifies
4. **Tests and Trials** (HOLD/RELEASE): Complex patterns
5. **Return** (RELEASE): Resolution, return to calm

### Emotional Arc

**Structure:**
1. **Establishment:** Set emotional baseline
2. **Rising Tension:** Build emotional intensity
3. **Climax:** Peak emotional moment
4. **Release:** Emotional resolution
5. **New Baseline:** Evolved emotional state

### Musical Journey

**Structure:**
1. **Introduction:** Establish musical theme
2. **Verse:** Develop theme, build complexity
3. **Chorus:** Peak intensity, main theme
4. **Bridge:** Contrast, new perspective
5. **Final Chorus:** Climactic return
6. **Outro:** Resolution, fade

---

## Implementation Examples

### Complete Narrative Pattern

```cpp
class NarrativePattern {
private:
    NarrativeEngine& narrative;
    float baseIntensity = 128.0f;
    CRGBPalette16 palette;
    
public:
    void render(CRGB* leds, const AudioData& audio) {
        // Get narrative intensity
        float narrativeIntensity = narrative.getIntensity();
        
        // Map to visual parameters
        float effectIntensity = baseIntensity * (1.0f + narrativeIntensity);
        float effectSpeed = 1.0f + narrativeIntensity * 0.5f;
        
        // Apply emotional colour mapping
        uint8_t hue = mapEmotionToHue(narrative.getPhase());
        uint8_t saturation = 200 + narrativeIntensity * 55;
        uint8_t brightness = 128 + narrativeIntensity * 127;
        
        // Render pattern with narrative modulation
        for (int i = 0; i < NUM_LEDS; i++) {
            float position = (float)i / NUM_LEDS;
            float patternValue = calculatePattern(position, effectSpeed);
            
            CRGB color = CHSV(hue + position * 30, saturation, brightness);
            color.nscale8(patternValue * 255);
            
            leds[i] = color;
        }
    }
    
private:
    uint8_t mapEmotionToHue(NarrativePhase phase) {
        switch(phase) {
            case PHASE_BUILD: return 0;      // Red (passion)
            case PHASE_HOLD: return 32;       // Orange (energy)
            case PHASE_RELEASE: return 96;    // Green (calm)
            case PHASE_REST: return 160;      // Blue (serenity)
            default: return 128;              // Neutral
        }
    }
    
    float calculatePattern(float position, float speed) {
        return 0.5f + 0.5f * sin(position * TWO_PI * 3.0f + millis() * 0.001f * speed);
    }
};
```

### Beat-Synchronized Narrative

```cpp
void beatSynchronizedNarrative(const AudioData& audio) {
    static float narrativePhase = 0.0f;
    
    if (audio.beat.detected) {
        // Advance narrative phase on beat
        narrativePhase += 0.25f;  // 4 beats per cycle
        if (narrativePhase > 1.0f) narrativePhase -= 1.0f;
    }
    
    // Map phase to narrative intensity
    float intensity;
    if (narrativePhase < 0.4f) {
        // BUILD
        intensity = narrativePhase / 0.4f;
    } else if (narrativePhase < 0.5f) {
        // HOLD
        intensity = 1.0f;
    } else if (narrativePhase < 0.8f) {
        // RELEASE
        intensity = 1.0f - ((narrativePhase - 0.5f) / 0.3f);
    } else {
        // REST
        intensity = 0.2f;
    }
    
    // Apply intensity to pattern
    applyNarrativeIntensity(intensity);
}
```

---

## Conclusion

This storytelling framework provides comprehensive methodologies for creating narrative light sequences that engage viewers emotionally and tell stories through light. By combining the Narrative Engine with emotional impact mapping, pacing guidelines, and musical structure integration, patterns can achieve both technical excellence and artistic expression.

The framework enables:
1. **Emotional Engagement:** Patterns that resonate with viewers
2. **Musical Synchronization:** Visuals that enhance musical experience
3. **Narrative Structure:** Patterns that tell stories over time
4. **Dynamic Adaptation:** Patterns that respond to audio and context

---

*Document Version 1.0*  
*LightwaveOS LGP Storytelling Framework*  
*For creating light sequences that tell stories*

