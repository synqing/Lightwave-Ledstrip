# Contributor Guide - Sensory Bridge Pattern Implementation

**Last Updated**: 2025-01-01
**Context**: Guide for maintaining and extending Sensory Bridge audio-visual pipeline patterns

## Maintenance Instructions

### Modifying Audio-Reactive Effects

**When to Modify**:
- Adding new audio-reactive features
- Fixing audio reactivity bugs
- Improving visual response to audio

**How to Modify Safely**:
1. **Preserve Separation of Concerns**
   - Audio → Color/Brightness (audio-reactive)
   - Time → Motion Speed (time-based, user-controlled)
   - Never couple audio signals directly to motion speed

2. **Use SmoothingEngine Primitives**
   - Use `ExpDecay` for exponential smoothing
   - Use `AsymmetricFollower` for audio signals (fast attack, slow release)
   - Use `Spring` for natural motion with momentum
   - Never use approximation formulas: `alpha = dt / (tau + dt)`

3. **Use EffectContext Audio Signals**
   - `ctx.audio.controlBus.heavy_chroma[12]` for color (pre-smoothed)
   - `ctx.audio.rms()` for energy (with smoothing)
   - `ctx.audio.beatPhase()` for phase-locked motion (not raw BPM)
   - `ctx.audio.controlBus.silentScale` for silence gating

4. **Provide Fallback Behavior**
   ```cpp
   #if FEATURE_AUDIO_SYNC
   if (ctx.audio.available) {
       // Audio-reactive code
   } else
   #endif
   {
       // Fallback: time-based or palette-based
   }
   ```

5. **Test at Different Frame Rates**
   - Verify behavior at 60 FPS and 120 FPS
   - Ensure frame-rate independence
   - Check delta time clamping doesn't affect behavior

### Modifying K1 Beat Tracker

**When to Modify**:
- Fixing beat tracking bugs
- Improving tempo detection accuracy
- Adding new beat tracking features

**How to Modify Safely**:
1. **Preserve Confidence Semantics**
   - Confidence must reflect actual grouped consensus
   - Use `density_conf` for confidence calculation
   - Never use `clamp01(family_score)` for confidence

2. **Maintain Lock Verification**
   - Keep UNLOCKED → PENDING → VERIFIED state machine
   - Preserve 2.5s verification period
   - Track strongest competitor during verification

3. **Use Novelty Normaliser**
   - Never use linear flux→z mapping
   - Use running-stat normaliser (EWMA with 2s time constants)
   - Clamp to ±4 sigma max

4. **Test with Reference Tracks**
   - 138 BPM track (previously mis-locked to 123-129)
   - 120 BPM track (near prior center)
   - Ambiguous polyrhythmic tracks

### Modifying SmoothingEngine

**When to Modify**:
- Adding new smoothing primitives
- Fixing smoothing bugs
- Improving performance

**How to Modify Safely**:
1. **Preserve Frame-Rate Independence**
   - Always use true exponential decay: `alpha = 1.0f - expf(-lambda * dt)`
   - Never use approximation formulas
   - Test at different frame rates (60 FPS, 120 FPS)

2. **Maintain API Consistency**
   - Keep `update(target, dt)` signature
   - Preserve factory methods (`withTimeConstant()`)
   - Maintain reset functionality

3. **Document Time Constants**
   - Specify time constants in seconds
   - Document expected behavior
   - Provide usage examples

### Modifying AudioBehaviorSelector

**When to Modify**:
- Adding new narrative phases
- Improving behavior selection logic
- Adding new behaviors

**How to Modify Safely**:
1. **Preserve Narrative Phase Semantics**
   - REST: Low energy, contemplative
   - BUILD: Rising energy, tension building
   - HOLD: Peak energy, maximum presence
   - RELEASE: Falling energy, resolution

2. **Maintain Behavior Registration**
   - Effects register supported behaviors
   - Priority-based selection
   - Fallback when audio unavailable

3. **Preserve Transition Smoothness**
   - Configurable transition time (default 500ms)
   - Progress tracking for blending
   - State machine management

## Improvement Areas

### High Priority

1. **Adaptive Time Constants**
   - Current: Fixed time constants for smoothing
   - Improvement: Adaptive based on audio characteristics
   - Impact: Better visual response to different music styles

2. **Per-Effect MOOD Interpretation**
   - Current: MOOD normalized to 0.0-1.0 consistently
   - Improvement: Each effect interprets MOOD differently
   - Impact: More intuitive user control

3. **Real-Time Parameter Tuning**
   - Current: Parameters are compile-time or fixed
   - Improvement: Real-time tuning via web API
   - Impact: Better user experience, easier debugging

### Medium Priority

4. **Machine Learning for Behavior Selection**
   - Current: Heuristic-based behavior selection
   - Improvement: Train ML model on music features
   - Impact: More accurate behavior selection

5. **Multi-Strip Synchronization**
   - Current: Each strip renders independently
   - Improvement: Synchronize multiple strips
   - Impact: Coordinated multi-strip visuals

6. **Enhanced Chromagram Processing**
   - Current: Simple 12-bin chromagram
   - Improvement: Harmonic analysis, chord detection
   - Impact: More musically accurate color mapping

### Low Priority

7. **Effect Presets**
   - Current: Manual parameter adjustment
   - Improvement: Save/load effect presets
   - Impact: Easier effect configuration

8. **Audio Visualization Debugging Tools**
   - Current: Serial commands for debugging
   - Improvement: Web-based visualization tools
   - Impact: Better development experience

## Gotchas and Pitfalls

### 1. Audio-Motion Coupling

**Pitfall**: Directly coupling audio signals (BPM, RMS, flux) to motion speed.

**Why It's Bad**: Creates jittery motion from audio uncertainty.

**How to Avoid**: Always separate audio→color/brightness from time→motion.

**Example (Wrong)**:
```cpp
float speed = ctx.audio.rms() * 10.0f;  // BAD: Audio → Motion
m_phase += speed * dt;
```

**Example (Correct)**:
```cpp
float brightness = ctx.audio.rms() * ctx.audio.rms();  // GOOD: Audio → Brightness
float speed = ctx.speed / 100.0f;  // GOOD: Time → Motion
m_phase += speed * dt;
```

### 2. Approximation Formulas for Smoothing

**Pitfall**: Using `alpha = dt / (tau + dt)` for smoothing.

**Why It's Bad**: Frame-rate dependent, produces different results at 60 FPS vs 120 FPS.

**How to Avoid**: Always use true exponential decay: `alpha = 1.0f - expf(-lambda * dt)`.

**Example (Wrong)**:
```cpp
float alpha = dt / (tau + dt);  // BAD: Approximation
value += (target - value) * alpha;
```

**Example (Correct)**:
```cpp
float alpha = 1.0f - expf(-dt / tau);  // GOOD: True exponential
value += (target - value) * alpha;
```

### 3. Raw BPM for Motion

**Pitfall**: Using raw BPM values for motion phase.

**Why It's Bad**: BPM has uncertainty (±5-10 BPM), creates jitter.

**How to Avoid**: Use `beatPhase()` which returns smooth 0.0-1.0 phase.

**Example (Wrong)**:
```cpp
float bpm = ctx.audio.bpm();  // BAD: Jittery
float phase = (bpm / 60.0f) * time;
```

**Example (Correct)**:
```cpp
float phase = ctx.audio.beatPhase();  // GOOD: Smooth
```

### 4. Chromagram Color Accumulation Without Pre-Scaling

**Pitfall**: Summing chromagram colors directly without pre-scaling.

**Why It's Bad**: Causes white accumulation (12 bins * 255 = 3060 overflow).

**How to Avoid**: Pre-scale each bin by ~70% (180/255) before accumulation.

**Example (Wrong)**:
```cpp
for (int i = 0; i < 12; i++) {
    CRGB color = hsv2rgb(...);
    sum += color;  // BAD: White accumulation
}
```

**Example (Correct)**:
```cpp
constexpr uint8_t PRE_SCALE = 180;
for (int i = 0; i < 12; i++) {
    CRGB color = hsv2rgb(...);
    color.r = scale8(color.r, PRE_SCALE);  // GOOD: Pre-scale
    color.g = scale8(color.g, PRE_SCALE);
    color.b = scale8(color.b, PRE_SCALE);
    sum += color;
}
```

### 5. Symmetric Smoothing for Audio Signals

**Pitfall**: Using same time constant for rise and fall.

**Why It's Bad**: Doesn't match audio behavior (attacks are fast, decays are smooth).

**How to Avoid**: Use asymmetric followers (fast attack, slow release).

**Example (Wrong)**:
```cpp
float alpha = dt / (tau + dt);  // BAD: Symmetric
value += (target - value) * alpha;
```

**Example (Correct)**:
```cpp
float tau = (target > value) ? riseTau : fallTau;  // GOOD: Asymmetric
float alpha = 1.0f - expf(-dt / tau);
value += (target - value) * alpha;
```

### 6. Missing Fallback Behavior

**Pitfall**: Effect only works when audio is available.

**Why It's Bad**: Effect appears broken when no audio input.

**How to Avoid**: Always provide fallback behavior.

**Example (Wrong)**:
```cpp
if (ctx.audio.available) {
    // Audio-reactive code
}
// BAD: Nothing happens when audio unavailable
```

**Example (Correct)**:
```cpp
if (ctx.audio.available) {
    // Audio-reactive code
} else {
    // GOOD: Fallback behavior
    color = ctx.palette.getColor(ctx.gHue, 128);
    brightness = 0.3f;
}
```

### 7. Delta Time Not Clamped

**Pitfall**: Using raw delta time without clamping.

**Why It's Bad**: Very large delta times (from system hiccups) cause physics explosion.

**How to Avoid**: Always use `getSafeDeltaSeconds()` which clamps to [0.001, 0.05].

**Example (Wrong)**:
```cpp
float dt = ctx.deltaTimeMs * 0.001f;  // BAD: Not clamped
value += velocity * dt;
```

**Example (Correct)**:
```cpp
float dt = ctx.getSafeDeltaSeconds();  // GOOD: Clamped
value += velocity * dt;
```

## Code References

### Key Files to Understand

1. **SmoothingEngine.h**
   - Location: `v2/src/effects/enhancement/SmoothingEngine.h`
   - Purpose: Frame-rate independent smoothing primitives
   - Key Classes: `ExpDecay`, `Spring`, `AsymmetricFollower`, `SubpixelRenderer`

2. **AudioBehaviorSelector.h/cpp**
   - Location: `v2/src/audio/AudioBehaviorSelector.*`
   - Purpose: Narrative-driven behavior selection
   - Key Classes: `AudioBehaviorSelector`, `NarrativePhase`

3. **EffectContext.h**
   - Location: `v2/src/plugins/api/EffectContext.h`
   - Purpose: Audio signal API for effects
   - Key Methods: `beatPhase()`, `getSafeDeltaSeconds()`, `getMoodNormalized()`

4. **ControlBus.h/cpp**
   - Location: `v2/src/audio/contracts/ControlBus.*`
   - Purpose: Audio signal aggregation
   - Key Fields: `heavy_chroma[12]`, `silentScale`

5. **K1TactusResolver.cpp**
   - Location: `v2/src/audio/k1/K1TactusResolver.cpp`
   - Purpose: Beat tracking and tempo detection
   - Key Methods: `updateFromResonators()`, `computeGroupedDensityConf_()`

### Example Implementations

1. **BPMEffect.cpp**
   - Location: `v2/src/effects/ieffect/BPMEffect.cpp`
   - Pattern: Beat-Sync (phase-locked motion, audio→color, energy→brightness)
   - Key Features: Beat phase, chromagram color, BPM-adaptive decay

2. **BreathingEffect.cpp**
   - Location: `v2/src/effects/ieffect/BreathingEffect.cpp`
   - Pattern: Sensory Bridge Bloom (audio→color/brightness, time→motion)
   - Key Features: Multi-stage smoothing, frame persistence, AudioBehaviorSelector

3. **LGPPhotonicCrystalEffect.cpp**
   - Location: `v2/src/effects/ieffect/LGPPhotonicCrystalEffect.cpp`
   - Pattern: Wave Collision (relative energy, slew-limited speed)
   - Key Features: SmoothingEngine, AudioBehaviorSelector, narrative phases

### Related Documentation

1. **Sensory Bridge Analysis**
   - Location: `docs/analysis/SB_APVP_Analysis.md`
   - Purpose: Comprehensive Sensory Bridge architecture analysis
   - Key Sections: Audio-motion coupling, smoothing pipeline, frame persistence

2. **K1 Tempo Mis-Lock Analysis**
   - Location: `docs/analysis/K1_TEMPO_MISLOCK_ANALYSIS.md`
   - Purpose: Root cause analysis and fixes
   - Key Sections: Novelty saturation, confidence semantics, lock verification

3. **Audio-Visual Semantic Mapping**
   - Location: `docs/audio-visual/AUDIO_VISUAL_SEMANTIC_MAPPING.md`
   - Purpose: Architectural framework for audio-reactive visualizations
   - Key Sections: Pattern cage problem, binding trap, musical intelligence

4. **Lessons Learned**
   - Location: `LESSONS_LEARNED.md`
   - Purpose: Non-obvious insights and troubleshooting
   - Key Sections: Audio-motion coupling, frame-rate independence, confidence semantics

## Testing Requirements

### Before Committing Changes

1. **Compile Test**
   - Verify code compiles without errors
   - Check for warnings (address if critical)
   - Test on target platform (ESP32-S3)

2. **Frame Rate Test**
   - Verify behavior at 60 FPS and 120 FPS
   - Ensure frame-rate independence
   - Check delta time clamping

3. **Audio Test**
   - Test with audio input (various music styles)
   - Test without audio input (fallback behavior)
   - Verify silence gating works correctly

4. **Effect Test**
   - Verify effect renders correctly
   - Check for visual glitches
   - Verify smooth transitions

5. **Integration Test**
   - Test with other effects
   - Verify zone mode works
   - Check web dashboard integration

### Regression Testing

1. **Reference Tracks**
   - 138 BPM track (K1 should lock correctly)
   - 120 BPM track (near prior center)
   - Ambiguous polyrhythmic tracks

2. **Effect Validation**
   - Run `validate <id>` command for each modified effect
   - Verify centre-origin compliance
   - Check hue span (<60°)
   - Verify FPS and heap delta

3. **Performance Test**
   - Check FPS (target: 120 FPS)
   - Monitor CPU usage
   - Check heap usage (no leaks)

## Code Style

### Naming Conventions

- **Classes**: PascalCase (`AudioBehaviorSelector`)
- **Methods**: camelCase (`update()`, `getSafeDeltaSeconds()`)
- **Variables**: camelCase (`m_phase`, `targetBrightness`)
- **Constants**: UPPER_SNAKE_CASE (`HISTORY_SIZE`, `FORESHORTEN_EXP`)

### Comments

- **File Headers**: Describe purpose, pattern, key features
- **Method Comments**: Describe purpose, parameters, return value
- **Complex Logic**: Explain why, not just what
- **Pattern References**: Cite Sensory Bridge or Emotiscope patterns

### Code Organization

- **Includes**: System headers, then project headers
- **Namespaces**: Use `lightwaveos::effects::ieffect`
- **Initialization**: Reset all state in `init()`
- **Cleanup**: Free resources in `cleanup()`

## Common Questions

### Q: Should I use SmoothingEngine or implement my own smoothing?

**A**: Always use SmoothingEngine primitives. They're frame-rate independent and well-tested. Only implement custom smoothing if SmoothingEngine doesn't meet your needs.

### Q: How do I choose time constants for smoothing?

**A**: 
- Fast response: 0.05-0.10s (for beat reactivity)
- Medium response: 0.15-0.30s (for energy/brightness)
- Slow response: 0.40-0.60s (for structural parameters)

### Q: When should I use AudioBehaviorSelector?

**A**: Use AudioBehaviorSelector when your effect has multiple visual behaviors that should adapt to musical content. Simple effects may not need it.

### Q: How do I test frame-rate independence?

**A**: Run effect at 60 FPS and 120 FPS, compare visual output. Should be identical (or very close). Use SmoothingEngine primitives to ensure frame-rate independence.

### Q: What's the difference between `heavy_chroma` and raw chromagram?

**A**: `heavy_chroma` is pre-smoothed (0.75 alpha), less flicker. Use `heavy_chroma` for visual effects. Raw chromagram is for analysis.

### Q: How do I handle silence?

**A**: Use `ctx.audio.controlBus.silentScale` which fades to black after 10s silence. Multiply final brightness by `silentScale`.

### Q: Should I use `beatPhase()` or raw BPM?

**A**: Always use `beatPhase()` for motion. It's smooth and phase-locked. Raw BPM is jittery and should only be used for display or analysis.

