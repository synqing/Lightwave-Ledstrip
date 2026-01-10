# PDF Enhancement Plan Translation: Actionable Implementation Guide

**Date:** 2026-01-09  
**Source:** Full Audio-Reactivity Enhancement Plan for Lightwave-Ledstrip.pdf  
**Status:** Analysis Complete - Ready for Implementation

---

## Executive Summary

The PDF provides a comprehensive 3-phase roadmap for enhancing audio-reactivity in LightwaveOS. **Critical finding**: Many advanced features (musical saliency, style detection, 64-bin FFT) are **already implemented** but **underutilized** by effects. The plan focuses on:

1. **Phase 1**: Quick wins (bug fixes, beat sync, percussion triggers)
2. **Phase 2**: Advanced features (chord reactivity, unified color mapping, new effects)
3. **Phase 3**: Adaptive intelligence (style-based routing, saliency triggers, temporal context)

**Current State Assessment:**
- ✅ Musical saliency system: **IMPLEMENTED** (`MusicalSaliencyFrame`, `computeSaliency()`)
- ✅ Style detection: **IMPLEMENTED** (`MusicStyle` enum, `StyleDetector`)
- ✅ 64-bin FFT: **IMPLEMENTED** (`bins64[64]` in `ControlBusFrame`)
- ⚠️ **Underutilization**: Only ~40% of available features actively used by effects
- ❌ **Missing**: Adaptive routing, behavior selection, temporal context memory

---

## Phase 1: Quick Wins & Core Upgrades

### 1.1 Bug Fix: AudioWaveformEffect Color Flicker ✅ IDENTIFIED

**PDF Recommendation:**
> Fix AudioWaveformEffect to use `heavy_chroma` instead of `chroma` for stable color

**Current State:**
- **File**: `firmware/v2/src/effects/ieffect/AudioWaveformEffect.cpp`
- **Line 106, 118**: Uses `ctx.audio.controlBus.chroma[c]` (raw, causes flicker)
- **Should use**: `ctx.audio.controlBus.heavy_chroma[c]` (smoothed)

**Implementation:**
```cpp
// Line 106: Change from
float v = ctx.audio.controlBus.chroma[c];
// To:
float v = ctx.audio.controlBus.heavy_chroma[c];

// Line 118: Change from
float bin = ctx.audio.controlBus.chroma[c] * chromaNorm;
// To:
float bin = ctx.audio.controlBus.heavy_chroma[c] * chromaNorm;
```

**Impact:** Eliminates rapid color jitter, provides stable hue reflecting dominant tone

**Status:** ⚠️ **NOT FIXED** - Ready for immediate implementation

---

### 1.2 Global Beat Synchronization

**PDF Recommendation:**
> Ensure each audio-reactive effect has at least a subtle beat-aligned component

**Current State:**
- ✅ `LGPAudioTestEffect` - Uses `isOnBeat()`, `beatPhase()`
- ✅ `LGPBeatPulseEffect` - Beat-driven pulses
- ⚠️ `LGPChordGlowEffect` - No beat sync (harmonic-focused)
- ⚠️ `AudioBloomEffect` - No explicit beat sync

**Implementation Plan:**

**For ChordGlow:**
```cpp
// In LGPChordGlowEffect::render(), add subtle beat pulse:
if (ctx.audio.isOnBeat()) {
    // Gentle brightness boost on beat (5-10% increase)
    float beatBoost = 1.0f + (0.05f * ctx.audio.tempo.confidence);
    baseBrightness *= beatBoost;
}
```

**For AudioBloom:**
```cpp
// In AudioBloomEffect::render(), add beat-synced scroll speed:
float scrollSpeed = ctx.speed;
if (ctx.audio.tempo.confidence > 0.5f) {
    // Sync scroll to beat phase
    scrollSpeed *= (1.0f + 0.2f * sinf(ctx.audio.tempo.phase01 * TWO_PI));
}
```

**Status:** ⚠️ **PARTIAL** - Some effects missing beat sync

---

### 1.3 Percussion Triggers in Existing Effects

**PDF Recommendation:**
> Add snare/hi-hat detection to AudioBloomEffect and AudioWaveformEffect

**Current State:**
- ✅ `LGPBeatPulseEffect` - Uses `isSnareHit()`, `isHihatHit()`
- ✅ `LGPStarBurstEffect` - Uses percussion triggers
- ❌ `AudioBloomEffect` - No percussion triggers
- ❌ `AudioWaveformEffect` - No percussion triggers

**Implementation Plan:**

**For AudioBloomEffect:**
```cpp
// After bloom scroll update, add percussion reactions:
if (ctx.audio.isSnareHit()) {
    // Brief flash/brighten on snare
    m_bloomIntensity = 1.0f;  // Reset to max
}

if (ctx.audio.isHihatHit()) {
    // Quick shimmer in outer LEDs
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        float dist = ctx.getDistanceFromCenter(i);
        if (dist > 60) {  // Outer 20 LEDs
            ctx.leds[i] += CRGB(32, 32, 32);  // Add sparkle
        }
    }
}
```

**For AudioWaveformEffect:**
```cpp
// On kick drum (bass spike), add center flash:
if (ctx.audio.bass() > 0.7f && ctx.audio.isOnBeat()) {
    // Momentary white flash at center
    ctx.leds[ctx.centerPoint] = CRGB(255, 255, 255);
    ctx.leds[ctx.centerPoint + 1] = CRGB(255, 255, 255);
}
```

**Status:** ❌ **NOT IMPLEMENTED** - Ready for Phase 1

---

### 1.4 Graceful Audio Absence Handling

**PDF Recommendation:**
> Standardize fallback behavior when audio unavailable

**Current State:**
- ✅ `LGPAudioTestEffect` - Has fake 120 BPM fallback
- ✅ `LGPBeatPulseEffect` - Has fallback animation
- ⚠️ Some effects: Return early or freeze when `ctx.audio.available == false`

**Implementation Plan:**

**Create standardized fallback pattern:**
```cpp
// In each effect's render():
if (!ctx.audio.available) {
    // Option 1: Slow time-based animation
    float time = ctx.totalTimeMs / 1000.0f;
    float phase = fmodf(time * 0.5f, 1.0f);  // Slow pulse
    
    // Option 2: Dimmed static pattern
    // Option 3: Very slow color rotation
    
    // Log warning (first time only)
    static bool warned = false;
    if (!warned) {
        Serial.println("Audio unavailable - using fallback");
        warned = true;
    }
}
```

**Status:** ⚠️ **INCONSISTENT** - Needs standardization

---

## Phase 2: Advanced Reactivity

### 2.1 Unified Note→Color Mapping

**PDF Recommendation:**
> Establish consistent chroma-to-color scheme across all effects

**Current State:**
- ✅ `LGPChordGlowEffect` - Has `rootNoteToHue()` mapping
- ⚠️ `AudioWaveformEffect` - Uses arbitrary palette offset
- ⚠️ `AudioBloomEffect` - Uses palette offset, not unified mapping

**Implementation Plan:**

**Create shared utility:**
```cpp
// In firmware/v2/src/effects/CoreEffects.h or new AudioColorMapping.h

// Chromatic circle mapping (C=0° red, D=42°, ... B=231° violet)
constexpr uint8_t NOTE_TO_HUE[12] = {
    0,    // C  = Red
    21,   // C# = Orange-red
    42,   // D  = Orange
    64,   // D# = Yellow-orange
    85,   // E  = Yellow
    106,  // F  = Yellow-green
    128,  // F# = Green
    149,  // G  = Cyan-green
    170,  // G# = Cyan
    191,  // A  = Blue
    213,  // A# = Blue-violet
    234   // B  = Violet
};

inline uint8_t rootNoteToHue(uint8_t rootNote) {
    return NOTE_TO_HUE[rootNote % 12];
}
```

**Update effects to use unified mapping:**
- `AudioWaveformEffect`: Use `rootNoteToHue(chord.rootNote)` for hue
- `AudioBloomEffect`: Align palette offset with unified mapping

**Status:** ⚠️ **PARTIAL** - ChordGlow has it, others need update

---

### 2.2 Full Chord Reactive Behaviors

**PDF Recommendation:**
> Bring chord-reactivity into more effects (SpectrumBars, BassBreath)

**Current State:**
- ✅ `LGPChordGlowEffect` - Full chord visualization
- ❌ `LGPSpectrumBarsEffect` - No chord reactivity
- ❌ `LGPBassBreathEffect` - No chord reactivity

**Implementation Plan:**

**For SpectrumBars:**
```cpp
// In LGPSpectrumBarsEffect::render():
// On chord change, shift palette toward chord root color
if (ctx.audio.chordState.confidence > 0.5f) {
    uint8_t chordHue = rootNoteToHue(ctx.audio.chordState.rootNote);
    // Blend current palette with chord hue
    float blend = 0.3f;  // 30% chord influence
    // Adjust each band's hue toward chord color
}
```

**For BassBreath:**
```cpp
// In LGPBassBreathEffect::render():
// Modulate hue when chord changes
if (ctx.audio.chordState.confidence > 0.5f) {
    uint8_t chordHue = rootNoteToHue(ctx.audio.chordState.rootNote);
    m_hueShift = lerp8by8(m_hueShift, chordHue, 0.1f);  // Slow blend
}
```

**Status:** ❌ **NOT IMPLEMENTED** - Ready for Phase 2

---

### 2.3 Dedicated Percussion Visualization

**PDF Recommendation:**
> Create new `LGPDrumVisualizerEffect` focused on drum kit elements

**Implementation Plan:**

**New Effect Structure:**
```cpp
class LGPDrumVisualizerEffect : public IEffect {
    // Kick: Outer LEDs flash inward
    // Snare: Center-to-mid radius burst
    // Hi-hat: Edge shimmer
    
    void render(EffectContext& ctx) override {
        // Kick visualization (outer → center)
        if (ctx.audio.isOnBeat() && ctx.audio.bass() > 0.5f) {
            // Flash outer LEDs, fade inward
            for (uint16_t i = 0; i < ctx.ledCount; i++) {
                float dist = ctx.getDistanceFromCenter(i);
                if (dist > 70) {  // Outer 10 LEDs
                    ctx.leds[i] = CRGB(255, 100, 0);  // Warm kick color
                }
            }
        }
        
        // Snare visualization (center → mid)
        if (ctx.audio.isSnareHit()) {
            // Expanding ring from center
            float snareRadius = m_snarePulsePos;
            // ... ring rendering logic
        }
        
        // Hi-hat visualization (edges only)
        if (ctx.audio.isHihatHit()) {
            // Random sparkle in outer LEDs
            // ... shimmer logic
        }
    }
};
```

**Status:** ❌ **NOT IMPLEMENTED** - New effect for Phase 2

---

## Phase 3: Adaptive Intelligence

### 3.1 Musical Saliency Triggers

**PDF Recommendation:**
> Use saliency metrics to trigger special events or mode changes

**Current State:**
- ✅ Saliency system: **IMPLEMENTED** (`ctx.audio.harmonicSaliency()`, `rhythmicSaliency()`, etc.)
- ❌ **Usage**: **0%** - No effects currently use saliency

**Implementation Plan:**

**Add saliency triggers to effects:**
```cpp
// In any effect's render():
float harmonicSal = ctx.audio.harmonicSaliency();
float rhythmicSal = ctx.audio.rhythmicSaliency();

// Harmonic saliency → color transition
if (harmonicSal > 0.5f) {
    triggerColorTransition();  // Big color shift
}

// Rhythmic saliency → pulse/strobe
if (rhythmicSal > 0.5f) {
    increaseStrobeRate();  // Double brightness or flash
}
```

**Status:** ❌ **NOT USED** - Feature exists but unused

---

### 3.2 Style-Based Adaptive Routing

**PDF Recommendation:**
> Create adaptive effect that changes behavior based on music style

**Current State:**
- ✅ Style detection: **IMPLEMENTED** (`ctx.audio.musicStyle()`)
- ❌ **Usage**: **0%** - No adaptive routing

**Implementation Plan:**

**Option A: Adaptive Meta-Effect**
```cpp
class AdaptiveAudioEffect : public IEffect {
    void render(EffectContext& ctx) override {
        MusicStyle style = ctx.audio.musicStyle();
        
        switch (style) {
            case MusicStyle::RHYTHMIC_DRIVEN:
                // Use BeatPulse-like behavior
                renderRhythmicMode(ctx);
                break;
            case MusicStyle::HARMONIC_DRIVEN:
                // Use ChordGlow-like behavior
                renderHarmonicMode(ctx);
                break;
            case MusicStyle::MELODIC_DRIVEN:
                // Use Bloom-like behavior
                renderMelodicMode(ctx);
                break;
            // ... other styles
        }
    }
};
```

**Option B: Dynamic Parameter Routing**
```cpp
// In EffectContext, add routing layer:
float getMappedValue(const char* signalName) {
    MusicStyle style = audio.musicStyle();
    
    if (strcmp(signalName, "pulse") == 0) {
        switch (style) {
            case RHYTHMIC_DRIVEN: return audio.bass();
            case HARMONIC_DRIVEN: return audio.flux();
            case MELODIC_DRIVEN: return audio.mid();
            default: return audio.rms();
        }
    }
    // ... other signal mappings
}
```

**Status:** ❌ **NOT IMPLEMENTED** - Ready for Phase 3

---

### 3.3 Temporal Context & Memory

**PDF Recommendation:**
> Track audio level trends, section changes, repetition patterns

**Implementation Plan:**

**Audio Presence State Machine:**
```cpp
enum class AudioPresenceState {
    SILENT,   // RMS < 0.01 for >5 seconds
    QUIET,    // RMS 0.01-0.3
    LOUD      // RMS > 0.3
};

class TemporalContext {
    AudioPresenceState m_state = AudioPresenceState::LOUD;
    float m_rmsHistory[32];  // 32-frame window
    uint32_t m_silentDuration = 0;
    
    void update(float rms) {
        // Update history
        // Detect state transitions
        // Track trends
    }
};
```

**Energy Trend Tracking:**
```cpp
// Track RMS over 8 bars (~16 seconds @ 120 BPM)
float computeEnergyTrend() {
    float recentAvg = average(m_rmsHistory, 0, 8);
    float olderAvg = average(m_rmsHistory, 8, 16);
    
    if (recentAvg > olderAvg * 1.2f) {
        return 1.0f;  // Building up
    } else if (recentAvg < olderAvg * 0.8f) {
        return -1.0f;  // Winding down
    }
    return 0.0f;  // Stable
}
```

**Status:** ❌ **NOT IMPLEMENTED** - New feature for Phase 3

---

## Implementation Priority Matrix

| Feature | Phase | Effort | Impact | Current Status | Priority |
|---------|-------|--------|--------|----------------|----------|
| Waveform chroma fix | 1 | Low | High | ❌ Not fixed | **P0** |
| Beat sync in ChordGlow | 1 | Low | Medium | ⚠️ Missing | P1 |
| Percussion in Bloom | 1 | Low | Medium | ❌ Not done | P1 |
| Unified color mapping | 2 | Medium | High | ⚠️ Partial | P1 |
| Chord reactivity expansion | 2 | Medium | Medium | ❌ Not done | P2 |
| DrumVisualizer effect | 2 | High | Medium | ❌ Not done | P2 |
| Saliency triggers | 3 | Medium | High | ❌ Unused | P1 |
| Style-based routing | 3 | High | High | ❌ Not done | P1 |
| Temporal context | 3 | High | Medium | ❌ Not done | P2 |

---

## Translation Strategy

### Immediate Actions (This Week)

1. **Fix AudioWaveformEffect chroma flicker** (30 minutes)
   - Change `chroma[c]` → `heavy_chroma[c]` (2 lines)
   - Test with music, verify color stability

2. **Add beat sync to ChordGlow** (1 hour)
   - Add subtle brightness pulse on `isOnBeat()`
   - Test with rhythmic music

3. **Add percussion to AudioBloom** (2 hours)
   - Snare flash, hi-hat shimmer
   - Test with drum-heavy tracks

### Short-Term (Next 2 Weeks)

4. **Unified color mapping utility** (4 hours)
   - Create `AudioColorMapping.h` with `rootNoteToHue()`
   - Update AudioWaveformEffect and AudioBloomEffect

5. **Chord reactivity in SpectrumBars** (3 hours)
   - Palette shift on chord changes
   - Test with chord progressions

### Medium-Term (Next Month)

6. **Saliency triggers** (1 week)
   - Add saliency checks to 3-4 key effects
   - Create trigger behaviors (color shift, strobe)

7. **Style-based adaptive effect** (2 weeks)
   - Create `AdaptiveAudioEffect` class
   - Implement style routing logic

### Long-Term (Next Quarter)

8. **Temporal context system** (2-3 weeks)
   - Audio presence state machine
   - Energy trend tracking
   - Section change detection

9. **Full documentation** (1 week)
   - Effect documentation template
   - Audio signal usage tables
   - Musical fit guidelines

---

## Key Insights from PDF Analysis

### What We Already Have ✅

1. **Musical Intelligence System (MIS)** - Fully implemented:
   - Musical saliency metrics (harmonic, rhythmic, timbral, dynamic)
   - Style detection (5 categories)
   - Chord detection with confidence
   - 64-bin FFT spectrum

2. **Rich Audio Context API** - 30+ accessors available:
   - `ctx.audio.bin(index)` - 64-bin FFT
   - `ctx.audio.harmonicSaliency()` - Saliency metrics
   - `ctx.audio.musicStyle()` - Style classification
   - `ctx.audio.chordState` - Chord detection

### What's Missing ❌

1. **Usage of Advanced Features** - Only ~40% utilized:
   - Saliency: 0% usage
   - Style detection: 0% usage
   - 64-bin FFT: Limited usage

2. **Adaptive Routing** - No dynamic signal routing based on style

3. **Behavior Selection** - No event-driven behavior triggers

4. **Temporal Memory** - No context tracking over time

### Critical Gaps

1. **AudioWaveformEffect bug** - Still uses raw chroma (causes flicker)
2. **No unified color mapping** - Each effect uses different hue schemes
3. **No adaptive intelligence** - Effects don't adapt to music style
4. **Underutilized features** - Advanced features exist but unused

---

## Next Steps

1. **Review this document** with team
2. **Prioritize Phase 1 items** (quick wins)
3. **Create GitHub issues** for each feature
4. **Start with AudioWaveformEffect fix** (immediate)
5. **Plan Phase 2/3 implementation** (sprint planning)

---

## References

- **Source PDF**: `docs/guides/Full Audio-Reactivity Enhancement Plan for Lightwave-Ledstrip.pdf`
- **Current Analysis**: `firmware/v2/docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md`
- **Saliency System**: `firmware/v2/src/audio/contracts/MusicalSaliency.h`
- **Style Detection**: `firmware/v2/src/audio/StyleDetector.h`
- **Effect Context API**: `firmware/v2/src/plugins/api/EffectContext.h`

---

*Last updated: 2026-01-09*

