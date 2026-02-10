# Audio-Reactive LGP Effects: Expected Outcomes Specification

**Document Version**: 1.0
**Date**: 2025-12-28
**Branch**: `feature/dt-correction-upgrade`
**Author**: Claude Code (Analysis Agent)

---

## Executive Summary

This document specifies the precise expected visual outcomes, measurable verification criteria, failure modes, and edge-case behaviors for the audio-reactive Light Guide Plate (LGP) effects modifications in LightwaveOS v2. The changes address the "jog-dial" bidirectional motion bug and optimize visual smoothness for LGP viewing characteristics.

---

## Phase 1: Jog-Dial Motion Fix

### Files Modified
- `/v2/src/effects/ieffect/LGPInterferenceScannerEffect.cpp`
- `/v2/src/effects/ieffect/LGPInterferenceScannerEffect.h`
- `/v2/src/effects/ieffect/LGPWaveCollisionEffect.cpp`
- `/v2/src/effects/ieffect/LGPWaveCollisionEffect.h`

---

### 1.1 Speed Slew Limiter

**Change**: Added `m_speedScaleSmooth` with max rate-of-change limiting

| Parameter | Scanner Effect | Wave Collision |
|-----------|---------------|----------------|
| Max Slew Rate | 0.30 units/sec | 0.25 units/sec |
| Speed Range | 0.6 - 1.4 (2.3x) | 0.7 - 1.3 (1.86x) |
| Previous Range | 0.4 - 4.0 (10x) | 0.4 - 4.0 (10x) |

#### Expected Visual Behavior

**What the viewer should see:**
1. **Smooth acceleration/deceleration**: When audio energy changes, wave propagation speed should ramp smoothly over 3-4 seconds, not snap instantly
2. **Consistent directional flow**: Waves should move OUTWARD from center (LED 79/80) at all times, never reversing direction
3. **Subtle tempo following**: Speed variations should feel like gentle breathing (1.5-2.3x range), not erratic speeding/slowing

**Perceptual target**: The effect should feel like watching waves in a calm harbor - energy changes affect wave height (brightness), not wave direction or chaotic speed changes

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Speed transition time | 3.3-5.6 sec full range | Observe time from 0.6 to 1.4 at max slew |
| Direction reversals | 0 per minute | Visual observation during loud transients |
| Speed variance | <0.08 units/frame @120fps | Log `m_speedScaleSmooth` delta per frame |
| Jog-dial occurrences | 0 in 5-minute test | Count visible back-forth oscillations |

**Verification formula:**
```
Max frames to traverse full range = (1.4 - 0.6) / (0.3 * (1/120)) = 96 frames = 0.8 seconds minimum
Actual time with asymptotic approach: ~3.5 seconds for 90% of range
```

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Waves still jog-dial on beats | `energyDelta` still coupled to speed target | Check lines 111-112 (Scanner) or 110-111 (Collision) |
| Waves freeze/too slow | Slew rate too aggressive | Verify `maxSlewRate` is 0.25-0.30 |
| Speed snaps on loud transients | Slew limiter bypassed | Check dt clamping in smoothValue calculation |
| Asymmetric response (fast up, slow down) | Slew limiter applied unequally | Verify both positive and negative delta clamped |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Silence** (RMS < 0.01) | Speed decays to 0.6 (base), waves continue at minimum rate | `energyAvgSmooth` decays via 0.98 multiplier in else branch |
| **Clipping** (sustained max) | Speed caps at 1.4 (Scanner) / 1.3 (Collision), no overshoot | Hard clamp after slew addition |
| **Rapid transients** (200+ BPM) | Speed remains nearly constant (~1.0), brightness pulses instead | energyDelta removed from speed, only drives brightness |
| **dt spike** (frame drop) | Slew limit scaled by dt prevents large jump | `slewLimit = maxSlewRate * dt` |
| **Effect restart** | Starts at speedScale=1.0, ramps toward target | Init sets `m_speedScaleSmooth = 1.0f` |

---

### 1.2 Audio Reactivity Decoupling (Speed -> Brightness)

**Change**: Removed `energyDelta` from speed modulation, moved to brightness gain

**Scanner Effect** (lines 138-139):
```cpp
float audioGain = 0.4f + 0.5f * m_energyAvgSmooth + 0.5f * m_energyDeltaSmooth + 0.3f * fastFlux;
float pattern = wave1 * audioGain;
```

**Wave Collision** (line 136):
```cpp
float audioIntensity = 0.4f + 0.5f * m_energyAvgSmooth + 0.6f * m_collisionBoost + 0.4f * m_energyDeltaSmooth;
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Beat-synchronized brightness pulses**: On drum hits/transients, the entire wave pattern brightens uniformly
2. **Stable wave velocity**: Wave peaks travel at constant rate regardless of beat intensity
3. **Intensity variation range**: ~40% base brightness up to ~170% on loud transients (0.4 + 0.5 + 0.5 + 0.3 = 1.7 max)

**Perceptual target**: Imagine stadium lights dimming/brightening to music while laser patterns sweep steadily - brightness reacts, motion remains controlled

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Brightness range | 0.4 - 1.7 normalized | Calculate `audioGain` with min/max inputs |
| Speed independence | <5% variation on 10dB transient | Compare speed before/after beat |
| Phase continuity | Zero discontinuities | Log `m_scanPhase` or `m_phase` derivative |
| Visual energy correlation | r > 0.7 brightness vs RMS | Record LED brightness, correlate with audio |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Brightness doesn't pulse on beats | `energyDeltaSmooth` saturating too slowly | Check rise/fall time constants (250ms/400ms) |
| Still see speed changes on transients | Wrong smoothing applied to speedTarget | Verify `energyDelta` absent from speed formula |
| Uniform brightness (no dynamics) | `fastFlux` not being received | Check `ctx.audio.available` and `fastFlux()` return |
| Over-brightness (white saturation) | audioGain exceeding 1.7 + palette handling | Verify clamps in pattern calculation |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Silent** | audioGain = 0.4 (40% base brightness) | Minimum term provides ambient glow |
| **Sustained loud** | audioGain ~1.4 (energyAvg=1, delta=0, flux low) | Delta decays when no change, flux settles |
| **Sharp transient** | Flash to ~1.7, decay over 400ms | All terms spike, then release at different rates |
| **Audio disconnect** | Immediate decay to 40% base | `hasAudio=false` branch sets delta=0, avg*=0.98 |

---

### 1.3 Increased Time Constants

**Change**: Smoothing rise/fall times increased for slower response

| Parameter | Old Value | New Value | Effect |
|-----------|----------|-----------|--------|
| riseDelta (attack) | 80ms | 250ms | Slower transient response |
| fallDelta (release) | 250ms | 400ms | Much slower decay |
| riseAvg | ~100ms | 200ms | Gentler average tracking |
| fallAvg | ~300ms | 500ms | Slower average decay |

**Implementation** (lines 96-101 in both effects):
```cpp
float riseDelta = dt / (0.25f + dt);   // 250ms time constant
float fallDelta = dt / (0.40f + dt);   // 400ms time constant
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Gradual intensity builds**: Energy buildup takes ~250ms to reach 63% of target
2. **Long decay trails**: After transients, brightness fades over 400-800ms (2-3 time constants)
3. **Reduced flicker**: High-frequency audio content no longer causes visual jitter
4. **"Bloom" characteristic**: Light seems to swell and recede rather than flash

**Perceptual target**: Like viewing through frosted glass - rapid changes are softened while overall dynamics are preserved

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| 10-90% rise time | ~575ms | Inject step input, measure response |
| 90-10% fall time | ~920ms | Remove input, measure decay |
| -3dB frequency | ~0.4 Hz | Sinusoid sweep, find half-power point |
| Step response overshoot | 0% | Verify no ringing in smoothValue output |

**Verification formula:**
```
Single-pole filter: output = input * alpha + prev * (1-alpha)
where alpha = dt / (tau + dt)

For tau=250ms, dt=8.33ms (120fps): alpha = 0.0322
Rise to 63%: -tau * ln(0.37) = 250ms
Rise to 90%: -tau * ln(0.10) = 575ms
```

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Response too fast/flashy | Time constants not applied | Verify smoothValue() using correct alpha |
| Response too sluggish | Time constants too long | Reduce tau values if needed |
| Different behavior per strip | Different dt values | Verify dt clamping consistent |
| Oscillation/ringing | Invalid alpha (>1) | Check dt clamping prevents negative tau |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Frame spike (50ms dt)** | Alpha=0.167 (vs 0.032 normal), larger step but limited | dt/(tau+dt) naturally handles variable dt |
| **Very fast dt (2ms)** | Alpha=0.008, smoother but still tracking | System becomes more filtered |
| **dt=0** | Alpha=0, no change (safe) | Division by tau returns 0 |

---

## Phase 2: Per-Band Normalization & Asymmetric Smoothing

### Files Modified
- `/v2/src/audio/AudioActor.cpp` (lines 534-548)
- `/v2/src/audio/contracts/ControlBus.cpp`
- `/v2/src/audio/contracts/ControlBus.h`
- `/v2/src/audio/AudioTuning.h`

---

### 2.1 Per-Band Gain Normalization

**Change**: Frequency-dependent gain adjustment for visual balance

```cpp
float perBandGains[8] = {0.8f, 0.85f, 1.0f, 1.2f, 1.5f, 1.8f, 2.0f, 2.2f};
```

| Band | Center Freq | Gain | Rationale |
|------|------------|------|-----------|
| 0 | 60 Hz | 0.8x | Attenuate sub-bass (overwhelming, not visible) |
| 1 | 120 Hz | 0.85x | Attenuate bass (HVAC mask, visual dominance) |
| 2 | 250 Hz | 1.0x | Neutral (lower mids) |
| 3 | 500 Hz | 1.2x | Slight boost (vocal fundamentals) |
| 4 | 1 kHz | 1.5x | Boost (presence band) |
| 5 | 2 kHz | 1.8x | Strong boost (brilliance) |
| 6 | 4 kHz | 2.0x | Strong boost (detail) |
| 7 | 7.8 kHz | 2.2x | Maximum boost (air, cymbals) |

#### Expected Visual Behavior

**What the viewer should see:**
1. **Balanced spectrum visualization**: High frequencies visible during vocals/cymbals, not drowned by bass
2. **Reduced bass dominance**: Kick drums contribute but don't overpower the visualization
3. **Enhanced detail**: Hi-hats, shakers, and vocal sibilance create visible activity
4. **Genre adaptability**: Both bass-heavy EDM and treble-rich acoustic music render well

**Perceptual target**: A spectrum analyzer that shows musical content, not just bass energy - similar to how pro audio meters are weighted

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Band 0:7 visual ratio | 1:2.75 (0.8/2.2) | Compare LED brightness with sine sweeps |
| Pink noise uniformity | <3dB variation across bands | Play pink noise, measure band outputs |
| Kick drum visibility | <50% of total energy | Play kick-heavy track, measure band 0+1 vs total |
| Hi-hat visibility | >15% of total energy | Play hi-hat pattern, measure band 6+7 vs total |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Still bass-dominated | Gains not applied | Check AudioActor.cpp line 538 |
| Treble too dominant | Gains too aggressive | Reduce bands 5-7 gains |
| Silent on treble-only content | Noise floor gating treble | Check usePerBandNoiseFloor |
| Clipping on loud content | Gain pushes above 1.0 | Verify clamp at line 539 |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Pure 60Hz sine** | 80% of what it would be without gains | Band 0 attenuated |
| **Pure 7.8kHz sine** | 220% brightness, clamped to 100% | Gain boosts then clamps |
| **White noise** | Visible tilt toward treble | Gains sum: bass attenuated, treble boosted |
| **Silence** | All bands at noise floor | Gains don't amplify zero |

---

### 2.2 Per-Band Noise Floor Gating

**Change**: Frequency-specific noise floor thresholds

```cpp
float perBandNoiseFloors[8] = {0.0008f, 0.0012f, 0.0006f, 0.0005f,
                               0.0008f, 0.0010f, 0.0012f, 0.0006f};
bool usePerBandNoiseFloor = true;  // In LGP_SMOOTH preset
```

| Band | Noise Floor | Ambient Source |
|------|-------------|---------------|
| 0 (60Hz) | 0.0008 | Power supply hum |
| 1 (120Hz) | 0.0012 | HVAC fundamental |
| 2 (250Hz) | 0.0006 | Generally quiet |
| 3 (500Hz) | 0.0005 | Quietest band |
| 4 (1kHz) | 0.0008 | Fan harmonics |
| 5 (2kHz) | 0.0010 | Fan noise |
| 6 (4kHz) | 0.0012 | Fan noise peak |
| 7 (7.8kHz) | 0.0006 | Generally quiet |

#### Expected Visual Behavior

**What the viewer should see:**
1. **Clean black during silence**: No flickering or false triggers from HVAC/fans
2. **Crisp onset detection**: Music starts from true black, making first notes more impactful
3. **Band-specific gating**: 120Hz hum doesn't trigger while 60Hz bass does (higher threshold)
4. **Preserved dynamics**: Signals above noise floor pass unchanged

**Perceptual target**: Effects appear "asleep" during silence, "wake up" cleanly when music starts

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| False trigger rate (HVAC on) | <1 per minute | Count LED activations in silence |
| Gate attack time | <1 hop (5.8ms) | Hard gate, no attack smoothing |
| Above-threshold distortion | 0% | Signal at 2x threshold should be 2x output |
| Cross-band isolation | Complete | Gating band 1 shouldn't affect band 2 |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Still flickers in silence | Noise floor too low | Increase perBandNoiseFloors values |
| Quiet music doesn't register | Noise floor too high | Decrease values or disable feature |
| Audible pumping in quiet sections | Gate causing abrupt cutoffs | Consider adding hysteresis |
| Some bands gate, others don't | Inconsistent calibration | Re-measure ambient noise spectrum |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Signal exactly at threshold** | Gates to 0 | Strict `<` comparison |
| **Signal at 1.01x threshold** | Passes unchanged | No compression above gate |
| **usePerBandNoiseFloor=false** | Global noise floor only | Feature disabled |
| **All bands below threshold** | Complete silence, no LED activity | Intended behavior |

---

### 2.3 Asymmetric Attack/Release Smoothing

**Change**: Different rise and fall rates for band/chroma data

| Parameter | Value | Purpose |
|-----------|-------|---------|
| bandAttack | 0.15 | 15% step toward target per hop (fast rise) |
| bandRelease | 0.03 | 3% step toward target per hop (slow fall) |
| heavyBandAttack | 0.08 | 8% step (extra slow rise) |
| heavyBandRelease | 0.015 | 1.5% step (ultra slow fall) |

**Implementation** (ControlBus.cpp lines 62-85):
```cpp
float alpha = (target > m_bands_s[i]) ? m_band_attack : m_band_release;
m_bands_s[i] = lerp(m_bands_s[i], target, alpha);
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Punchy transient response**: Beats trigger immediate brightness increase (~15% per 5.8ms hop = ~26ms to 90%)
2. **Graceful decay**: After beats, brightness fades slowly over ~200ms (3% per hop = ~65 hops to 10%)
3. **Heavy chroma ultra-smooth**: Color changes flow like slow-moving aurora (~400ms rise, ~1.3s fall)
4. **No flickering**: Rapid audio changes smoothed out, no strobing effect

**Perceptual target**: Like a VU meter with ballistic response - needles jump up quickly but fall slowly

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Band 10-90% rise time | ~26ms (4.5 hops) | Step input, measure response |
| Band 90-10% fall time | ~200ms (34 hops) | Remove input, measure decay |
| Heavy band rise time | ~50ms (8.6 hops) | Same method |
| Heavy band fall time | ~400ms (69 hops) | Same method |
| Attack/release ratio (bands) | 5:1 | 0.15 / 0.03 |
| Attack/release ratio (heavy) | 5.33:1 | 0.08 / 0.015 |

**Verification formula:**
```
Alpha-based smoothing: steps to X% = ln(1-X) / ln(1-alpha)
For alpha=0.15, steps to 90% = ln(0.1) / ln(0.85) = 14.2 hops = 82ms
For alpha=0.03, steps to 10% = ln(0.1) / ln(0.97) = 75.6 hops = 438ms
```

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Slow attack (mushy transients) | Attack alpha too low | Increase bandAttack toward 0.25 |
| Fast decay (no sustain) | Release alpha too high | Decrease bandRelease toward 0.02 |
| No difference from normal | Heavy values not used | Verify effects using heavy_chroma[] |
| Unnatural pumping | Attack/release ratio too extreme | Balance values closer to 3:1 |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Rapid beats (200 BPM)** | Attacks sum, never fully release between beats | Attack overwhelms release |
| **Single transient** | Sharp rise, long ~400ms decay | Asymmetric response by design |
| **Slow fade in** | Tracks linearly (attack alpha applied continuously) | Attack wins when target > current |
| **Hop skipped** | Larger step next hop (accumulated delta) | Single lerp step per hop |

---

## Phase 3: Effect-Specific Tuning

### Files Modified
- `/v2/src/effects/ieffect/AudioWaveformEffect.cpp`
- `/v2/src/effects/ieffect/AudioWaveformEffect.h`
- `/v2/src/effects/ieffect/AudioBloomEffect.cpp`
- `/v2/src/effects/ieffect/AudioBloomEffect.h`

---

### 3.1 AudioWaveformEffect: Slower Peak Following

**Change**: Reduced transient tracking aggressiveness

| Parameter | Old Value | New Value | Effect |
|-----------|----------|-----------|--------|
| PEAK_FOLLOW_ATTACK | 0.25 | 0.12 | 52% slower peak rise |
| PEAK_SCALE_ATTACK | 0.25 | 0.15 | 40% slower scaling |
| Peak scaled alpha | 0.05 | 0.03 | 40% slower smoothing |

**Implementation** (AudioWaveformEffect.h lines 40-42):
```cpp
static constexpr float PEAK_FOLLOW_ATTACK = 0.12f;   // Reduced from 0.25
static constexpr float PEAK_FOLLOW_RELEASE = 0.005f; // Unchanged
static constexpr float PEAK_SCALE_ATTACK = 0.15f;    // Reduced from 0.25
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Stable waveform amplitude**: Peak envelope tracks music loudness, not individual transients
2. **Reduced "AGC pumping"**: Quiet passages don't cause waveform to suddenly expand
3. **Consistent vertical scale**: Waveform height stays relatively stable within a song section
4. **Smooth dynamic changes**: Volume changes reflected over 1-2 seconds, not per-beat

**Perceptual target**: Like an oscilloscope with auto-scale that's been slowed down - waveform fills the display consistently without jumping around

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Peak follower rise time | ~48ms (8.7 hops) | Compare to old 23ms (4 hops) |
| Peak scale rise time | ~39ms (6.7 hops) | Compare to old 23ms |
| Smoothed peak response | ~200ms 10-90% | Due to 0.03/0.97 smoothing |
| Inter-beat variation | <15% height change | Measure peak-to-peak on steady beat |

**Verification formula:**
```
Old: steps to 90% @ alpha=0.25 = ln(0.1)/ln(0.75) = 8 hops = 46ms
New: steps to 90% @ alpha=0.12 = ln(0.1)/ln(0.88) = 18 hops = 104ms

Final smoothing (0.03/0.97): steps to 90% = ln(0.1)/ln(0.97) = 75 hops = 435ms
```

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Waveform clips/saturates | Peak follower too slow | Increase PEAK_FOLLOW_ATTACK |
| Waveform still jumpy | Old values still in use | Verify .h file recompiled |
| Waveform too small | Peak follower overcompensating | Check SWEET_SPOT_MIN_LEVEL |
| No visible audio reaction | Peak scaling broken | Log m_waveformPeakScaled values |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Sudden loud sound** | Waveform grows over ~100ms, not instantly | Slower attack by design |
| **Sudden silence** | Waveform shrinks very slowly (5% per hop release) | Release unchanged at 0.005 |
| **Quiet whisper content** | SWEET_SPOT_MIN_LEVEL (750) provides floor | Prevents division by near-zero |
| **Clipped audio input** | Peak saturates at int16 max (~32767) | maxWaveformValRaw capped by type |

---

### 3.2 AudioBloomEffect: Heavy Chroma Source

**Change**: Switched from `chroma[12]` to `heavy_chroma[12]`

**Implementation** (AudioBloomEffect.cpp line 62):
```cpp
float bin = ctx.audio.controlBus.heavy_chroma[i];  // Was: chroma[i]
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Slower color evolution**: Chroma-driven hue shifts happen over ~400ms, not per-beat
2. **Reduced color flickering**: Rapid pitch changes smoothed out
3. **Flowing aurora effect**: Colors blend and transition like slow-moving northern lights
4. **Preserved tonal character**: Still responds to harmonic content, just more slowly

**Perceptual target**: Like watching a lava lamp driven by music - colors respond to the music but with graceful, flowing transitions

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Color change rise time | ~50ms (vs ~26ms normal) | Inject chroma step, measure heavy |
| Color change fall time | ~400ms (vs ~200ms normal) | Remove chroma, measure decay |
| Hue stability | <30 degrees variance per beat | Measure dominant hue jitter |
| Cross-note blending | >50% overlap | Two rapid notes should blend colors |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Colors still flickery | Not using heavy_chroma | Verify line 62 change |
| Colors too sluggish | Heavy smoothing too aggressive | Reduce heavyBandRelease |
| No color at all | heavy_chroma array not populated | Check ControlBus UpdateFromHop |
| Wrong colors | heavy_chroma incorrectly smoothed | Verify chroma -> heavy_chroma path |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Rapid arpeggio** | Colors blend into average of notes | Heavy smoothing integrates |
| **Sustained note** | Color stabilizes to note's chroma | Eventually tracks target |
| **Pitch bend** | Smooth color glide | Continuous tracking with smoothing |
| **No tonal content (drums only)** | Weak/neutral coloring | Chroma energy low for percussive |

---

### 3.3 AudioBloomEffect: Fractional Scroll Accumulator

**Change**: Variable-rate scrolling with fractional accumulation

**Implementation** (AudioBloomEffect.cpp lines 91-95):
```cpp
float scrollRate = 0.3f + (ctx.speed / 50.0f) * 2.2f;  // 0.3-2.5 LEDs/hop
m_scrollPhase += scrollRate;
uint8_t step = (uint8_t)m_scrollPhase;
m_scrollPhase -= step;  // Keep fractional remainder
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **Variable scroll speed**: UI speed control maps to 0.3-2.5 LEDs pushed outward per hop
2. **Sub-pixel smoothness**: At slow speeds, scroll appears continuous, not stepped
3. **Speed-responsive bloom**: Faster settings create energetic outward burst, slower creates gentle expansion
4. **No temporal aliasing**: Integer rounding artifacts eliminated by fractional accumulation

**Perceptual target**: Like adjusting the speed of a fountain - from gentle bubbling to vigorous spray

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Speed=1 scroll rate | 0.34 LEDs/hop | (1/50)*2.2 + 0.3 = 0.344 |
| Speed=25 scroll rate | 1.4 LEDs/hop | (25/50)*2.2 + 0.3 = 1.4 |
| Speed=50 scroll rate | 2.5 LEDs/hop | (50/50)*2.2 + 0.3 = 2.5 |
| Time to scroll 80 LEDs @speed=1 | ~1350ms (233 hops) | 80 / 0.344 / 172 Hz |
| Time to scroll 80 LEDs @speed=50 | ~185ms (32 hops) | 80 / 2.5 / 172 Hz |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Jerky/stepped motion | Integer truncation | Verify fractional accumulation |
| Wrong speed mapping | ctx.speed not normalized | Check speed range (1-50) |
| Scroll too fast/slow | Rate formula incorrect | Log scrollRate calculation |
| Wraparound artifacts | Step exceeds HALF_LENGTH | Check clamp at line 97 |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Speed=0** (shouldn't happen) | scrollRate=0.3, minimal motion | Base rate provides floor |
| **Fractional remainder >0.9** | May get 2-step next hop | Accumulator catches up |
| **Effect restart** | m_scrollPhase=0, starts fresh | Init resets accumulator |
| **step > HALF_LENGTH-1** | Clamped to max safe value | Prevents buffer overflow |

---

### 3.4 AudioBloomEffect: Reduced Saturation Boost

**Change**: Saturation increase reduced from 32 to 24

**Implementation** (AudioBloomEffect.cpp line 123):
```cpp
increaseSaturation(m_radial, HALF_LENGTH, 24);  // Was: 32
```

#### Expected Visual Behavior

**What the viewer should see:**
1. **More natural colors**: Less neon/oversaturated appearance
2. **Better white balance**: Neutral tones remain visible, not all pushed to vivid
3. **Preserved pastel range**: Subtle colors not artificially boosted
4. **Reduced banding**: Less visible quantization in color transitions

**Perceptual target**: Colors that look "rich" rather than "electric" - more natural museum lighting than disco

#### Measurable Criteria

| Metric | Target Value | Measurement Method |
|--------|-------------|-------------------|
| Max saturation boost | +24/255 = +9.4% | Fixed increment per pixel |
| White input result | S increases 0->24 (9.4%) | Input CRGB(255,255,255) |
| Already-saturated input | Clamps via qadd8 | Input with S=240 -> S=255 (not 264) |
| Visual saturation increase | ~25% reduction from before | Compare screenshots |

#### Failure Modes

| Symptom | Likely Cause | Diagnostic |
|---------|-------------|-----------|
| Colors still oversaturated | Value not changed | Verify line 123 |
| Colors too washed out | May need increase | Try 28-30 if too muted |
| Banding/posterization | rgb2hsv_approximate precision | Known FastLED limitation |
| White turning colored | Saturation boost on neutral | Expected behavior (adds slight tint) |

#### Edge Cases

| Condition | Expected Behavior | Rationale |
|-----------|------------------|-----------|
| **Pure white input** | Slight tint introduced | S goes 0->24 |
| **S=255 input** | Unchanged (qadd8 saturates) | Can't exceed 255 |
| **S=240 input** | S becomes 255 (clamped) | qadd8 prevents overflow |
| **Black input** | Stays black | V=0 means S boost invisible |

---

## Composite Verification Checklist

### Pre-Test Setup
- [ ] Build with `pio run -e esp32dev_audio_esv11 -t upload`
- [ ] Verify audio input connected and calibrated
- [ ] Set brightness to 50% (128) for consistent measurement
- [ ] Use LGP_SMOOTH preset for full feature set

### Visual Tests (5 minutes each)

| Test | Pass Criteria | Effect |
|------|--------------|--------|
| **Jog-dial elimination** | Zero direction reversals in 5 min | Scanner, Collision |
| **Smooth acceleration** | Speed changes over >2 seconds | Scanner, Collision |
| **Brightness pulsing** | Visible brightness sync to beats | Scanner, Collision |
| **Balanced spectrum** | Treble visible on acoustic music | All audio effects |
| **Clean silence** | No flickering when paused | All audio effects |
| **Slow color flow** | Hue shifts over ~400ms | Bloom |
| **Variable scroll** | Speed 1 vs 50 clearly different | Bloom |
| **Stable waveform** | Peak envelope stable within sections | Waveform |
| **Natural saturation** | Not "neon" appearance | Bloom |

### Quantitative Tests (log data)

| Metric | Target | Log Command |
|--------|--------|-------------|
| m_speedScaleSmooth delta/frame | <0.003 | Serial debug output |
| Band rise time (10-90%) | ~26ms | Inject step, measure |
| Heavy chroma fall time | ~400ms | Remove signal, measure |
| Scroll rate @ speed=25 | 1.4 LED/hop | Log scrollRate |

---

## Appendix A: Code Reference Quick Links

| Change | File | Lines |
|--------|------|-------|
| Speed slew (Scanner) | LGPInterferenceScannerEffect.cpp | 114-120 |
| Speed slew (Collision) | LGPWaveCollisionEffect.cpp | 113-119 |
| Audio->brightness (Scanner) | LGPInterferenceScannerEffect.cpp | 138-139 |
| Audio->brightness (Collision) | LGPWaveCollisionEffect.cpp | 136-137 |
| Time constants | Both LGP*.cpp | 96-101 |
| Per-band gains | AudioActor.cpp | 537-539 |
| Per-band noise floor | AudioActor.cpp | 542-544 |
| Asymmetric smoothing | ControlBus.cpp | 62-85 |
| Heavy chroma arrays | ControlBus.h | 38-39 |
| Waveform peak attack | AudioWaveformEffect.h | 40-42 |
| Bloom heavy chroma | AudioBloomEffect.cpp | 62 |
| Fractional scroll | AudioBloomEffect.cpp | 91-95 |
| Saturation boost | AudioBloomEffect.cpp | 123 |

---

## Appendix B: Tuning Parameter Summary

### LGP_SMOOTH Preset (AudioTuning.h lines 259-291)

```cpp
// AGC
agcAttack = 0.06f;
agcRelease = 0.015f;

// ControlBus smoothing
controlBusAlphaFast = 0.20f;
controlBusAlphaSlow = 0.06f;

// Asymmetric band smoothing
bandAttack = 0.12f;
bandRelease = 0.025f;
heavyBandAttack = 0.06f;
heavyBandRelease = 0.012f;

// Per-band gains
perBandGains = {0.8, 0.85, 1.0, 1.2, 1.5, 1.8, 2.0, 2.2};

// Per-band noise floors
perBandNoiseFloors = {0.0008, 0.0012, 0.0006, 0.0005, 0.0008, 0.0010, 0.0012, 0.0006};
usePerBandNoiseFloor = true;
```

### Effect-Specific Parameters

| Effect | Parameter | Value |
|--------|-----------|-------|
| Scanner | maxSlewRate | 0.30 |
| Scanner | speedRange | 0.6-1.4 |
| Collision | maxSlewRate | 0.25 |
| Collision | speedRange | 0.7-1.3 |
| Both LGP | riseDelta tau | 250ms |
| Both LGP | fallDelta tau | 400ms |
| Waveform | PEAK_FOLLOW_ATTACK | 0.12 |
| Waveform | PEAK_SCALE_ATTACK | 0.15 |
| Bloom | scroll rate | 0.3-2.5 LED/hop |
| Bloom | saturation boost | 24 |

---

*End of Expected Outcomes Specification*
