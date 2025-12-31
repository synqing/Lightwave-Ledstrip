# BPM Effect Audio-Visual Cohesion Audit Report

**Date:** 2025-12-31  
**Effect ID:** 6  
**Effect Name:** BPM  
**Auditor:** Captain (Codex Agent)  
**Reference:** SB_APVP_Analysis.md

---

## Executive Summary

The BPM effect (Effect #6) currently fails to deliver a cohesive audio-visual experience because it **does not integrate musical content** into its visual output. While it correctly uses beat phase for smooth motion, it lacks:
1. **Chromatic color mapping** - No connection to musical pitch content
2. **Energy envelope modulation** - No response to overall audio energy
3. **Visual pattern coherence** - Static radial falloff doesn't "breathe" with music

The effect follows a **partial** Sensory Bridge pattern (beat phase for motion) but misses the critical **audio→color** and **energy→brightness** coupling that creates cohesive experiences.

---

## Task 1: Current Implementation Analysis

### Visual Pattern Analysis

**File:** `v2/src/effects/ieffect/BPMEffect.cpp` (lines 29-78)

#### Current Rendering Logic:

```cpp
// 1. Beat phase → sine wave intensity
float beatPhase = ctx.audio.beatPhase();  // 0.0-1.0
uint8_t phaseAngle = (uint8_t)(beatPhase * 255.0f);
uint8_t rawBeat = sin8(phaseAngle);  // 0-255 sine
beat = map8(rawBeat, 64, 255);  // Scale to 64-255 range

// 2. Distance-based intensity falloff
uint8_t intensity = beat - (uint8_t)(distFromCenter * 3);
intensity = max(intensity, (uint8_t)32);

// 3. Beat pulse boost (stronger at center)
float centerFade = 1.0f - (distFromCenter / HALF_LENGTH);
intensity = qadd8(intensity, (uint8_t)(pulseBoost * centerFade));

// 4. Color from palette (NOT audio-derived)
uint8_t colorIndex = ctx.gHue + (uint8_t)(distFromCenter * 2);
CRGB color = ctx.palette.getColor(colorIndex, intensity);
```

#### Visual Pattern Behavior:

1. **Sine Wave Pulsing**: `beat` oscillates between 64-255 based on beat phase
   - Creates a pulsing effect that syncs to tempo
   - BUT: This is just intensity modulation, not expanding/contracting pattern

2. **Radial Falloff**: `intensity = beat - (distFromCenter * 3)`
   - Center LEDs get full `beat` value
   - Each LED further from center loses 3 units of intensity
   - Creates a radial gradient that pulses in brightness
   - **Problem**: Pattern doesn't expand/contract, just pulses in place

3. **Beat Pulse Boost**: Adds extra brightness on beat detection
   - Decay rate: 0.85 (fixed, not BPM-adaptive)
   - Boost: `pulseBoost * centerFade` (stronger at center)
   - **Problem**: Fixed decay doesn't match musical tempo

4. **Color Selection**: Uses palette with `gHue` offset
   - `colorIndex = ctx.gHue + (distFromCenter * 2)`
   - Creates a radial color gradient
   - **Problem**: Color has NO connection to musical content (no chromagram)

#### What the Pattern Actually Does:

- **Static radial pattern** that pulses in brightness
- **No expansion/contraction** - pattern stays the same size
- **No color-music connection** - colors don't reflect pitch content
- **No energy response** - quiet and loud passages look similar (except beat pulse)

#### What's Missing for Cohesion:

1. **Expanding/contracting visual rhythm** - Pattern should grow/shrink with beat phase
2. **Musical color** - Should use chromagram to reflect pitch content
3. **Energy envelope** - Should scale overall brightness with RMS energy
4. **BPM-adaptive decay** - Beat pulse should decay at tempo-appropriate rate

---

## Task 2: Comparison with Sensory Bridge Bloom Pattern

### Sensory Bridge Bloom Architecture (from SB_APVP_Analysis.md)

**Key Separation of Concerns:**
1. **Motion**: Time-based, user-controlled (`CONFIG.MOOD`)
2. **Color**: Audio-derived from chromagram (12 pitch classes)
3. **Brightness**: Audio-derived from chromagram energy (squared)
4. **Global Gate**: `silent_scale` fades output during silence

**Bloom Algorithm Flow:**
```
1. Sprite Motion (AUDIO-INDEPENDENT)
   - position = 0.250 + 1.750 * CONFIG.MOOD
   - draw_sprite() shifts previous frame outward
   - Creates expanding pattern

2. Color Calculation (AUDIO-REACTIVE)
   - Sum chromagram[12] → HSV colors
   - Each pitch class contributes hue
   - Energy² for brightness (non-linear)

3. Center Injection
   - New color injected at LEDs 63-64
   - Previous colors shifted outward
   - Creates "bloom" expansion

4. Spatial Falloff
   - Quadratic edge fade
   - Creates soft boundaries
```

### BPM Effect vs. Sensory Bridge Bloom

| Aspect | Sensory Bridge Bloom | BPM Effect (Current) | Gap |
|--------|---------------------|---------------------|-----|
| **Motion** | Time-based expansion (`CONFIG.MOOD`) | Beat phase → sine intensity | ❌ No expansion, just pulsing |
| **Color** | Chromagram → 12 pitch classes → HSV | Palette + `gHue` offset | ❌ No musical color |
| **Brightness** | Chromagram energy² | Beat pulse boost only | ❌ No energy envelope |
| **Pattern** | Expanding sprite (center→outward) | Static radial falloff | ❌ No motion, just intensity |
| **Decay** | Sprite decay (alpha=0.99) | Fixed 0.85 decay | ⚠️ Not BPM-adaptive |

### What Makes SB Bloom "Cohesive":

1. **Visual motion matches musical phrasing** - Expanding pattern creates sense of rhythm
2. **Colors reflect musical content** - Chromagram drives hue, creating musical color
3. **Energy modulates brightness** - Quiet passages dim, loud passages brighten
4. **Smooth, predictable motion** - Time-based expansion avoids jitter

### What BPM Effect Lacks:

1. **No visual expansion** - Pattern doesn't "breathe" with music
2. **No musical color** - Colors are arbitrary (palette rotation)
3. **No energy response** - Same brightness regardless of audio level
4. **Fixed decay** - Beat pulse doesn't adapt to tempo

---

## Task 3: Audio Data Availability

### Available Audio Features in EffectContext

**File:** `v2/src/plugins/api/EffectContext.h`

#### Currently Used by BPM Effect:
- ✅ `ctx.audio.beatPhase()` - Used for sine wave intensity
- ✅ `ctx.audio.isOnBeat()` - Used for beat pulse trigger
- ❌ `ctx.audio.bpm()` - Available but NOT used (could be used for adaptive decay)

#### Available but NOT Used:
- ❌ `ctx.audio.rms()` - RMS energy (0.0-1.0) - **SHOULD USE for energy envelope**
- ❌ `ctx.audio.fastRms()` - Fast RMS (0.0-1.0) - Alternative for energy
- ❌ `ctx.audio.beatStrength()` - Beat confidence (0.0-1.0) - **SHOULD USE for pulse scaling**
- ❌ `ctx.audio.tempoConfidence()` - Tempo lock confidence - **SHOULD USE for adaptive behavior**
- ❌ `ctx.audio.controlBus.chroma[12]` - **12-bin chromagram - SHOULD USE for musical color**

#### Chromagram Access Pattern:

**Direct Access Required:**
```cpp
// Chromagram is in controlBus, not exposed via accessor
const float* chroma = ctx.audio.controlBus.chroma;  // 12 values [0.0-1.0]
// OR
float chromaValue = ctx.audio.controlBus.chroma[i];  // i = 0-11 (C, C#, D, ..., B)
```

**Note:** There's no `ctx.audio.chroma(i)` accessor method. Effects must access `controlBus.chroma` directly.

#### Other Available Features:
- `ctx.audio.bass()`, `ctx.audio.mid()`, `ctx.audio.treble()` - Frequency bands
- `ctx.audio.flux()` - Spectral flux (onset detection)
- `ctx.audio.chordState()` - Chord detection (MAJOR/MINOR/etc.)
- `ctx.audio.saliencyFrame()` - Musical saliency metrics

---

## Task 4: Analysis of Working Effects

### BreathingEffect - BPM-Adaptive Decay Pattern

**File:** `v2/src/effects/ieffect/BreathingEffect.cpp` (lines 330-385)

**Key Pattern:**
```cpp
// BPM-adaptive decay calculation
float bpm = ctx.audio.bpm();
if (bpm < 60.0f) bpm = 60.0f;   // Floor
if (bpm > 180.0f) bpm = 180.0f; // Cap

// Decay should reach ~20% by next beat
float beatPeriodFrames = (60.0f / bpm) * 120.0f;
decayRate = powf(0.2f, 1.0f / beatPeriodFrames);

// Apply decay
m_pulseIntensity *= decayRate;
```

**What Works:**
- Decay rate adapts to BPM (faster BPM = faster decay)
- Clamps BPM to prevent wild decay rates
- Uses tempo confidence to gate behavior

**BPM Effect Should Adopt:**
- Replace fixed `0.85f` decay with BPM-adaptive calculation
- Use `ctx.audio.tempoConfidence()` to gate adaptive behavior

### LGPStarBurstEffect - Beat Phase and Chromagram Usage

**File:** `v2/src/effects/ieffect/LGPStarBurstEffect.cpp` (lines 180-206)

**Key Pattern:**
```cpp
// Beat phase for timing
float beatPhase = ctx.audio.beatPhase();

// Detect downbeat (phase wraps)
if (beatPhase < 0.15f && m_lastBeatPhase > 0.85f) {
    m_beatCount++;
    // Bar-level logic (every 4 beats)
    if ((m_beatCount & 3) == 0) {
        m_burst = fminf(m_burst + 0.3f, 1.0f);
    }
}
m_lastBeatPhase = beatPhase;
```

**What Works:**
- Tracks beat phase transitions for downbeat detection
- Uses phase wrapping to detect beat boundaries
- Combines beat phase with other audio features

**BPM Effect Could Adopt:**
- Track phase transitions for better beat pulse timing
- Use downbeat detection for stronger pulses

### AudioBloomEffect - Chromagram Color Mapping

**File:** `v2/src/effects/ieffect/AudioBloomEffect.cpp` (lines 148-199)

**Key Pattern:**
```cpp
// Compute sum_color from chromagram (matching Sensory Bridge light_mode_bloom)
CRGB sum_color = CRGB(0, 0, 0);
const float led_share = 255.0f / 12.0f;

for (uint8_t i = 0; i < 12; ++i) {
    float bin = ctx.audio.controlBus.heavy_chroma[i];  // Use heavy_chroma for stability
    
    // Non-linear brightness: bin² (like Sensory Bridge)
    float bright = bin;
    for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
        bright *= bright;
    }
    bright *= 1.5f;
    if (bright > 1.0f) bright = 1.0f;
    bright *= led_share;
    
    // Convert pitch class to hue
    float prog = i / float(12);
    CHSV hsvColor(255 * prog, 255, bright);
    CRGB rgbColor;
    hsv2rgb_spectrum(hsvColor, rgbColor);
    sum_color += rgbColor;
}
```

**What Works:**
- Uses `heavy_chroma` (extra-smoothed) for stability
- Applies quadratic contrast (bin²) like Sensory Bridge
- Combines all 12 pitch classes into single color
- Chord-driven hue adjustment for emotional response

**BPM Effect Should Adopt:**
- Use `computeChromaticColor()` helper (already exists in BreathingEffect)
- OR implement inline chromagram→color conversion
- Use `heavy_chroma` for stability (less flicker than raw `chroma`)

---

## Task 5: Specific Cohesion Failures

### Failure #1: No Musical Color Connection

**Symptom:** Colors don't reflect musical content

**Current Behavior:**
- Uses `ctx.palette.getColor(colorIndex, intensity)`
- `colorIndex = ctx.gHue + (distFromCenter * 2)`
- Creates radial color gradient based on palette rotation
- **No connection to actual audio pitch content**

**Expected Behavior (Sensory Bridge Pattern):**
- Sum chromagram[12] → derive hue from dominant pitch classes
- Colors should shift with musical key/chord changes
- Visual should "feel" the music, not just pulse to it

**Impact:** Visual feels disconnected from music - colors are arbitrary

---

### Failure #2: No Energy Envelope Modulation

**Symptom:** Quiet and loud passages look similar

**Current Behavior:**
- Beat pulse boost provides some brightness variation
- But overall pattern brightness doesn't scale with audio energy
- Quiet passages: same brightness as loud passages (except beat pulse)

**Expected Behavior (Sensory Bridge Pattern):**
- Overall brightness should scale with RMS energy
- `brightness *= rms * rms` (quadratic for contrast)
- Quiet passages should dim, loud passages should brighten

**Impact:** Visual doesn't respond to dynamics - lacks "breathing" with music

---

### Failure #3: Static Pattern (No Expansion/Contraction)

**Symptom:** Pattern doesn't "breathe" with musical rhythm

**Current Behavior:**
- Radial falloff pattern stays the same size
- Only intensity pulses (brightness changes)
- No expanding/contracting motion

**Expected Behavior (Sensory Bridge Pattern):**
- Pattern should expand outward on beats
- Use beat phase to drive expansion radius
- Create visual "breathing" that matches musical phrasing

**Impact:** Visual feels static - lacks sense of rhythm and motion

---

### Failure #4: Fixed Decay Rate

**Symptom:** Beat pulse doesn't match musical tempo

**Current Behavior:**
- Fixed decay: `m_beatPulse *= 0.85f`
- Same decay rate for 60 BPM and 180 BPM
- Pulse doesn't "fit" the musical timing

**Expected Behavior (BreathingEffect Pattern):**
- BPM-adaptive decay: faster BPM = faster decay
- Decay should reach ~20% by next beat
- Pulse should feel "in sync" with tempo

**Impact:** Beat pulse feels disconnected from musical timing

---

## Task 6: Implementation Recommendations

### Recommendation 1: Add Chromatic Color Mapping

**Implementation:**

**Option A: Reuse Existing Helper (Recommended)**
```cpp
// BreathingEffect already has this function - extract to shared header or inline
// File: v2/src/effects/ieffect/BreathingEffect.cpp (lines 49-82)

static CRGB computeChromaticColor(const float chroma[12]) {
    CRGB sum = CRGB::Black;
    float share = 1.0f / 6.0f;  // Divide brightness among notes (Sensory Bridge uses 1/6.0)
    
    for (int i = 0; i < 12; i++) {
        float hue = i / 12.0f;  // 0.0 to 0.917 (0° to 330°)
        float brightness = chroma[i] * chroma[i] * share;  // Quadratic contrast
        
        if (brightness > 1.0f) brightness = 1.0f;
        
        CRGB noteColor;
        hsv2rgb_spectrum(CHSV((uint8_t)(hue * 255), 255, (uint8_t)(brightness * 255)), noteColor);

        // PRE-SCALE: Prevent white accumulation (scale8 by 180/255 = ~70%)
        constexpr uint8_t PRE_SCALE = 180;
        noteColor.r = scale8(noteColor.r, PRE_SCALE);
        noteColor.g = scale8(noteColor.g, PRE_SCALE);
        noteColor.b = scale8(noteColor.b, PRE_SCALE);

        sum.r = qadd8(sum.r, noteColor.r);
        sum.g = qadd8(sum.g, noteColor.g);
        sum.b = qadd8(sum.b, noteColor.b);
    }
    
    // Clamp to valid range
    if (sum.r > 255) sum.r = 255;
    if (sum.g > 255) sum.g = 255;
    if (sum.b > 255) sum.b = 255;

    return sum;
}
```

**Usage in BPM Effect:**
```cpp
// Use heavy_chroma for stability (less flicker)
CRGB chromaticColor = computeChromaticColor(ctx.audio.controlBus.heavy_chroma);

// Apply intensity scaling (intensity is already 0-255)
color = CRGB(
    scale8(chromaticColor.r, intensity),
    scale8(chromaticColor.g, intensity),
    scale8(chromaticColor.b, intensity)
);
```

**Note:** Consider extracting `computeChromaticColor()` to a shared utility header (`CoreEffects.h` or new `ChromaUtils.h`) to avoid code duplication.

---

### Recommendation 2: Add Energy Envelope Modulation

**Implementation:**
```cpp
// Get energy envelope (quadratic for contrast)
float energyEnvelope = ctx.audio.rms() * ctx.audio.rms();

// Scale overall brightness
intensity = (uint8_t)(intensity * energyEnvelope);

// Optional: Add minimum brightness floor
uint8_t minBrightness = 32;
intensity = max(intensity, minBrightness);
```

**Alternative (using beatStrength for beat-synced energy):**
```cpp
// Combine RMS with beat strength for more dynamic response
float energyEnvelope = ctx.audio.rms() * ctx.audio.rms();
float beatEnergy = 0.5f + 0.5f * ctx.audio.beatStrength();  // 0.5-1.0 range
intensity = (uint8_t)(intensity * energyEnvelope * beatEnergy);
```

---

### Recommendation 3: Add Expanding/Contracting Pattern

**Implementation:**
```cpp
// Use beat phase to drive expansion radius
float beatPhase = ctx.audio.beatPhase();
float expansionRadius = sinf(beatPhase * 2.0f * M_PI) * 0.5f + 0.5f;  // 0.0-1.0

// Calculate effective distance with expansion
float baseDist = (float)centerPairDistance((uint16_t)i) / (float)HALF_LENGTH;
float expandedDist = baseDist / (expansionRadius * 0.7f + 0.3f);  // Expand pattern

// Intensity based on expanded distance
uint8_t intensity = beat - (uint8_t)(expandedDist * HALF_LENGTH * 3);
```

**Alternative (sprite expansion pattern like Sensory Bridge):**
```cpp
// Track expansion state
static float expansionPhase = 0.0f;
expansionPhase += beatPhase * 0.1f;  // Slow expansion
if (expansionPhase > 1.0f) expansionPhase -= 1.0f;

float expansionRadius = expansionPhase * HALF_LENGTH;
float dist = (float)centerPairDistance((uint16_t)i);

// Intensity based on distance from expansion front
if (dist < expansionRadius) {
    float fade = 1.0f - (dist / expansionRadius);
    intensity = (uint8_t)(beat * fade);
}
```

---

### Recommendation 4: BPM-Adaptive Decay

**Implementation:**
```cpp
float decayRate = 0.85f;  // Default fallback

#if FEATURE_AUDIO_SYNC
if (ctx.audio.available) {
    float tempoConf = ctx.audio.tempoConfidence();
    
    if (tempoConf > 0.4f) {
        // RELIABLE BEAT TRACKING: BPM-adaptive decay
        float bpm = ctx.audio.bpm();
        if (bpm < 60.0f) bpm = 60.0f;   // Floor
        if (bpm > 180.0f) bpm = 180.0f; // Cap
        
        // Decay should reach ~20% by next beat
        float beatPeriodFrames = (60.0f / bpm) * 120.0f;  // Frames per beat at 120 FPS
        decayRate = powf(0.2f, 1.0f / beatPeriodFrames);
        
        // Scale pulse by beat strength
        if (ctx.audio.isOnBeat()) {
            float strength = ctx.audio.beatStrength();
            m_beatPulse = fmaxf(m_beatPulse, strength);
        }
    }
}
#endif

// Apply decay
m_beatPulse *= decayRate;
if (m_beatPulse < 0.01f) m_beatPulse = 0.0f;
```

---

## Summary of Required Changes

### High Priority (Core Cohesion):
1. ✅ **Add chromatic color mapping** - Use `ctx.audio.controlBus.chroma[12]`
2. ✅ **Add energy envelope** - Use `ctx.audio.rms()` for brightness scaling
3. ✅ **BPM-adaptive decay** - Replace fixed 0.85 with tempo-adaptive calculation

### Medium Priority (Enhanced Cohesion):
4. ⚠️ **Expanding/contracting pattern** - Use beat phase to drive expansion radius
5. ⚠️ **Beat strength scaling** - Use `ctx.audio.beatStrength()` for pulse intensity

### Low Priority (Polish):
6. ⚠️ **Downbeat detection** - Stronger pulses on downbeats
7. ⚠️ **Tempo confidence gating** - Only use adaptive features when tempo is locked

---

## Files to Modify

1. **`v2/src/effects/ieffect/BPMEffect.cpp`**
   - Add chromatic color computation
   - Add energy envelope modulation
   - Replace fixed decay with BPM-adaptive decay
   - Optionally add expansion/contraction pattern

2. **`v2/src/effects/ieffect/BPMEffect.h`**
   - May need to add helper method for chromatic color (or inline)

---

## Testing Checklist

After implementation, verify:
- [ ] Colors shift with musical key/chord changes
- [ ] Brightness scales with audio energy (quiet = dim, loud = bright)
- [ ] Beat pulse decay matches musical tempo
- [ ] Visual pattern feels "connected" to music
- [ ] No performance regressions (still runs at 120 FPS)
- [ ] Fallback behavior works when audio unavailable

---

## Conclusion

The BPM effect currently implements **partial** Sensory Bridge patterns (beat phase for motion) but misses the critical **audio→color** and **energy→brightness** coupling. By adding chromatic color mapping, energy envelope modulation, and BPM-adaptive decay, the effect will achieve the cohesive audio-visual experience demonstrated by professional systems like Sensory Bridge.

**Priority:** High - These changes address fundamental cohesion failures that prevent the effect from delivering a musically-responsive visual experience.

---

## Implementation Priority Matrix

| Change | Impact | Effort | Priority |
|--------|--------|--------|----------|
| **Chromatic Color** | High - Core cohesion | Low - Helper exists | **P0 - Critical** |
| **Energy Envelope** | High - Dynamic response | Low - Simple multiply | **P0 - Critical** |
| **BPM-Adaptive Decay** | Medium - Tempo sync | Low - Copy from BreathingEffect | **P1 - High** |
| **Expanding Pattern** | Medium - Visual rhythm | Medium - New logic | **P2 - Medium** |
| **Beat Strength Scaling** | Low - Polish | Low - Simple addition | **P3 - Low** |

**Recommended Implementation Order:**
1. Add energy envelope (5 min) - **Immediate impact**  
2. Add chromatic color (10 min) - **Core cohesion**  
3. Add BPM-adaptive decay (10 min) - **Tempo sync**  
4. Consider expanding pattern (30 min) - **Enhanced rhythm**  

**Estimated Total Time:** 55 minutes for P0+P1 changes (core cohesion fixes)

