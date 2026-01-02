# Lessons Learned - Sensory Bridge Pattern Implementation

**Date**: 2025-01-01
**Context**: Comprehensive refactoring implementing Sensory Bridge audio-visual pipeline patterns

## Non-Obvious Insights

### 1. Audio-Motion Coupling is the Root of Jitter

**Discovery**: The primary source of jittery motion in audio-reactive effects was direct audio→motion coupling. When audio signals (BPM, RMS, flux) directly control motion speed, any uncertainty in audio analysis (±5-10 BPM, frame-to-frame RMS variation) creates visible jitter.

**Solution**: Sensory Bridge pattern separates concerns:
- **Audio → Color/Brightness** (audio-reactive)
- **Time → Motion Speed** (user-controlled, time-based)

**Impact**: Eliminated jitter completely while maintaining musical responsiveness.

**Key Insight**: Motion should be smooth and predictable. Audio should modulate visual properties (color, brightness, size), not motion speed.

### 2. Beat Phase vs Raw BPM

**Discovery**: Using raw BPM values (e.g., 138 BPM) for motion creates jitter because BPM detection has uncertainty (±5-10 BPM). Even with smoothing, the underlying value jumps around.

**Solution**: Use `beatPhase()` which returns a smooth 0.0-1.0 value that wraps at each beat. This is phase-locked to tempo but doesn't jitter with BPM uncertainty.

**Impact**: Smooth, phase-locked motion without jitter.

**Key Insight**: Phase is more stable than frequency for visual motion.

### 3. Frame-Rate Independent Smoothing is Critical

**Discovery**: Using approximation formulas like `alpha = dt / (tau + dt)` produces different results at different frame rates. At 60 FPS vs 120 FPS, the same time constant produces different smoothing behavior.

**Solution**: True exponential decay: `alpha = 1.0f - expf(-lambda * dt)`

**Impact**: Identical smoothing behavior at any frame rate.

**Key Insight**: Always use mathematically correct formulas, not approximations, for time-based calculations.

### 4. Confidence Semantics Matter

**Discovery**: K1 beat tracker always reported 1.00 confidence even when density agreement was low (DConf: 0.12). This was because confidence was derived from `clamp01(family_score)`, but `family_score` could exceed 1.0 due to additive bonuses.

**Solution**: Confidence now reflects actual grouped consensus: `density_conf * (1.0f - 0.2f * (1.0f - density_conf))`

**Impact**: Confidence values now accurately reflect beat tracking reliability.

**Key Insight**: Semantic correctness is more important than numerical convenience.

### 5. Relative Energy Detection is More Stable

**Discovery**: Using absolute energy levels (raw RMS) for visual response creates problems:
- Different tracks have different baseline energy
- AGC/gating shifts baseline over time
- Quiet passages still have some energy

**Solution**: Relative energy detection using rolling average:
- Track 4-hop rolling average
- Detect energy ABOVE baseline (positive delta only)
- Use smoothed averages for base intensity, delta for transients

**Impact**: More stable visual response across different tracks and volume levels.

**Key Insight**: Relative measurements are more robust than absolute measurements.

### 6. History Buffers Filter Single-Frame Spikes

**Discovery**: Single-frame audio spikes (from noise, clipping, etc.) cause visible glitches in effects. Smoothing alone doesn't help because the spike still affects the smoothed value.

**Solution**: 4-frame rolling average before smoothing:
- Filter out single-frame spikes
- Only affects smoothed value if spike persists
- Minimal latency (4 frames ≈ 33ms at 120 FPS)

**Impact**: Eliminated visual glitches from audio spikes.

**Key Insight**: Pre-filtering is more effective than post-smoothing for spike removal.

### 7. Slew Limiting Prevents Jitter from Rapid Changes

**Discovery**: Rapid audio changes (or jog-dial adjustments) cause visible jumps in effect parameters. Even with smoothing, rapid changes create visible artifacts.

**Solution**: Slew limiting (max change rate):
- Limit parameter changes to 0.25 units/sec
- Prevents jitter from rapid audio changes
- Maintains smoothness during transitions

**Impact**: Smooth parameter transitions even during rapid audio changes.

**Key Insight**: Rate limiting is as important as smoothing for stability.

### 8. Chromagram Color Mapping Requires Pre-Scaling

**Discovery**: Summing 12-bin chromagram colors directly causes white accumulation. With 12 bins at full intensity, worst case is 12 * 255 = 3060 (overflow).

**Solution**: Pre-scale each bin by ~70% (180/255) before accumulation:
- Prevents white accumulation
- Maintains color saturation
- Leaves headroom for accumulation

**Impact**: Musical colors without white washout.

**Key Insight**: Always consider worst-case accumulation when summing colors.

### 9. Asymmetric Smoothing is Essential for Audio

**Discovery**: Symmetric smoothing (same time constant for rise and fall) doesn't match audio behavior. Attacks should be fast, decays should be smooth.

**Solution**: Asymmetric followers:
- Fast attack (0.05-0.15s rise time)
- Slow release (0.20-0.50s fall time)
- Matches human perception of audio

**Impact**: More natural visual response to audio.

**Key Insight**: Audio visualization should match audio perception, not just audio signals.

### 10. Lock Verification Prevents Wrong Initial Locks

**Discovery**: K1 beat tracker would lock to wrong tempo immediately on first detection, then resist correction due to stability bonus.

**Solution**: Lock verification state machine:
- UNLOCKED → PENDING (2.5s verification)
- PENDING → VERIFIED (after verification period)
- Track strongest competitor during verification
- Switch if competitor is >10% better and >5 BPM away

**Impact**: Prevents wrong initial locks, allows correction during verification.

**Key Insight**: Initial decisions need verification before commitment.

## Troubleshooting Steps

### Issue: Effect Still Has Jittery Motion

**Symptoms**: Motion speed varies erratically, not smooth.

**Diagnosis**:
1. Check if effect uses audio signals for motion speed (BPM, RMS, flux)
2. Verify effect uses `beatPhase()` instead of raw BPM
3. Check if motion is time-based (user-controlled speed parameter)

**Solution**:
1. Separate audio→color/brightness from time→motion
2. Use `beatPhase()` for phase-locked motion
3. Use time-based phase accumulation for motion speed

### Issue: Colors Are Too White/Washed Out

**Symptoms**: Chromagram colors appear white or desaturated.

**Diagnosis**:
1. Check if chromagram colors are pre-scaled before accumulation
2. Verify accumulation uses `qadd8()` for overflow protection
3. Check if final color is clamped to valid range

**Solution**:
1. Pre-scale each chromagram bin by ~70% (180/255)
2. Use `qadd8()` for safe accumulation
3. Clamp final RGB values to [0, 255]

### Issue: Effect Doesn't Respond to Audio

**Symptoms**: Effect appears static, no audio reactivity.

**Diagnosis**:
1. Check if `ctx.audio.available` is true
2. Verify effect uses `heavy_chroma` or `rms()` from audio context
3. Check if silence gating is too aggressive (`silentScale` near 0)

**Solution**:
1. Ensure audio is available: `if (ctx.audio.available) { ... }`
2. Use `ctx.audio.controlBus.heavy_chroma[12]` for color
3. Check `ctx.audio.controlBus.silentScale` (should be > 0.1)

### Issue: Smoothing Behaves Differently at Different Frame Rates

**Symptoms**: Effect looks different at 60 FPS vs 120 FPS.

**Diagnosis**:
1. Check if smoothing uses approximation formula: `alpha = dt / (tau + dt)`
2. Verify time constants are in seconds, not frames

**Solution**:
1. Use true exponential decay: `alpha = 1.0f - expf(-lambda * dt)`
2. Use SmoothingEngine primitives (ExpDecay, AsymmetricFollower)
3. Ensure time constants are in seconds

### Issue: Beat Tracking Locks to Wrong Tempo

**Symptoms**: K1 reports wrong BPM (e.g., 123 instead of 138).

**Diagnosis**:
1. Check K1 debug output: `k1` command
2. Verify confidence value (should reflect density agreement)
3. Check if lock is in PENDING state (verification period)

**Solution**:
1. Wait for verification period (2.5s) - lock may correct itself
2. Check if stronger competitor appears during verification
3. Verify novelty normaliser is working (no saturation)

## Workarounds

### Workaround 1: Fallback When Audio Unavailable

**Problem**: Effects need to work without audio input.

**Solution**: Always provide fallback behavior:
```cpp
#if FEATURE_AUDIO_SYNC
if (ctx.audio.available) {
    // Audio-reactive code
} else
#endif
{
    // Fallback: time-based or palette-based
    chromaticColor = ctx.palette.getColor(ctx.gHue, 128);
    brightness = 0.3f;  // Dim fallback
}
```

**Limitation**: Fallback may be less visually interesting.

### Workaround 2: MOOD Parameter Normalization

**Problem**: MOOD parameter (0-255) needs consistent behavior across effects.

**Solution**: Normalize to 0.0-1.0 range:
```cpp
float moodNorm = ctx.getMoodNormalized();  // 0.0-1.0
```

**Limitation**: Some effects may need different MOOD interpretations.

### Workaround 3: Delta Time Clamping

**Problem**: Very large delta times (from system hiccups) cause physics explosion.

**Solution**: Clamp delta time to safe range:
```cpp
float dt = ctx.getSafeDeltaSeconds();  // Clamped to [0.001, 0.05]
```

**Limitation**: Very slow effects may be affected by clamping.

## Environment Considerations

### Platform-Specific Notes

**ESP32-S3**:
- Floating-point performance is good (use `float`, not `double`)
- FastLED library is optimized for ESP32
- Audio processing runs on Core 0, LED rendering on Core 1

**Build Configuration**:
- `FEATURE_AUDIO_SYNC` must be enabled for audio-reactive features
- SmoothingEngine requires `<cmath>` (available on ESP32)
- AudioBehaviorSelector requires C++17 (available on ESP32)

### Runtime Dependencies

**Required**:
- FastLED library (for LED control)
- Audio analysis pipeline (K1 beat tracker, ControlBus)
- EffectContext with audio signals

**Optional**:
- WiFi (for web dashboard)
- Serial interface (for debugging)

### Performance Considerations

**CPU Impact**:
- SmoothingEngine: Minimal (~1-2% CPU per effect)
- AudioBehaviorSelector: Low (~0.5% CPU per effect)
- Chromagram color mapping: Low (~0.1% CPU per effect)

**Memory Impact**:
- SmoothingEngine: ~50 bytes per instance
- AudioBehaviorSelector: ~200 bytes per instance
- History buffers: ~16 bytes per buffer (4 floats)

**Frame Rate**:
- Target: 120 FPS
- Minimum: 60 FPS (with delta time clamping)
- SmoothingEngine is frame-rate independent

## Future Improvements

### 1. Adaptive Time Constants

**Current**: Fixed time constants for smoothing.

**Improvement**: Adaptive time constants based on audio characteristics:
- Fast music → faster smoothing
- Slow music → slower smoothing
- Dynamic music → adaptive smoothing

### 2. Machine Learning for Behavior Selection

**Current**: Heuristic-based behavior selection.

**Improvement**: Train ML model on music features to predict best behavior:
- More accurate behavior selection
- Learns from user preferences
- Adapts to music style

### 3. Per-Effect MOOD Interpretation

**Current**: MOOD normalized to 0.0-1.0 consistently.

**Improvement**: Each effect interprets MOOD differently:
- Some effects: MOOD = speed
- Some effects: MOOD = smoothness
- Some effects: MOOD = intensity

### 4. Real-Time Parameter Tuning

**Current**: Parameters are compile-time or fixed.

**Improvement**: Real-time parameter tuning via web API:
- Adjust smoothing time constants
- Tune behavior selection thresholds
- Fine-tune effect parameters

### 5. Multi-Strip Synchronization

**Current**: Each strip renders independently.

**Improvement**: Synchronize multiple strips:
- Phase-locked rendering
- Shared audio analysis
- Coordinated behavior selection

