# Layer Audit: LGPAudioTestEffect

**Effect ID**: (varies by firmware configuration)  
**Source File**: `firmware/v2/src/effects/ieffect/LGPAudioTestEffect.cpp`  
**Category**: Diagnostic, Continuous  
**Family**: Test Pattern

[← Back to Master Reference](../AUDIO_REACTIVE_PATTERNS_MASTER.md) | [← Layer Audit Index](INDEX.md)

---

## Quick Summary

**Logic Seed**: Diagnostic "test" patterns (Seed E)  
**Driver**: Continuous (multi-feature)  
**Spatial Pattern**: 8 static regions, each visualising a different audio feature  
**Colour Policy**: Cyclic hue (global) + feature-driven brightness  
**Compositing**: Replace mode

---

## Purpose

**NOT an artistic effect.** This is a diagnostic tool for:
- Verifying audio pipeline is working (FFT, beat detection, chord detection, etc.)
- Tuning sensitivity thresholds (is kick detector too sensitive?)
- Debugging feature extraction (is saliency calculation correct?)
- Calibrating audio input levels

---

## Layer Breakdown

### 8 Diagnostic Regions

**Spatial Layout**:
```cpp
int pixelsPerRegion = 80 / 8;  // 10 pixels per region

Region 0 (pixels 0-9):   RMS
Region 1 (pixels 10-19): Spectral Flux
Region 2 (pixels 20-29): Beat Confidence
Region 3 (pixels 30-39): Kick Detected
Region 4 (pixels 40-49): Snare Detected
Region 5 (pixels 50-59): Audio Peak Saliency
Region 6 (pixels 60-69): Chord Stability
Region 7 (pixels 70-79): Energy High
```

**Rendering**:
```cpp
for (int regionIdx = 0; regionIdx < 8; regionIdx++) {
    float featureValue = getFeature(regionIdx);  // 0.0 to 1.0
    
    uint8_t hue = gHue;  // Global cycling hue (changes over time)
    uint8_t brightness = featureValue * 255;
    
    CHSV colour(hue, 255, brightness);
    
    for (int p = regionIdx * 10; p < (regionIdx + 1) * 10; p++) {
        SET_CENTER_PAIR(p, colour);
    }
}
```

---

## Audio Signals Used

**All of them.** This effect is a comprehensive audio feature visualiser.

| Region | Signal | Type | Expected Behaviour |
|--------|--------|------|-------------------|
| 0 | `ctx.audio.rms` | Continuous | Tracks overall loudness |
| 1 | `ctx.audio.spectralFlux` | Continuous | Spikes on timbre changes |
| 2 | `ctx.audio.beatConfidence` | Continuous | High during rhythmic passages |
| 3 | `ctx.audio.kickDetected` | Boolean | Flash on kick drum |
| 4 | `ctx.audio.snareDetected` | Boolean | Flash on snare drum |
| 5 | `ctx.audio.audioPeakSaliency` | Continuous | High on melodic content |
| 6 | `ctx.audio.chordStability` | Continuous | High on clear chords |
| 7 | `ctx.audio.energyHigh` | Continuous | Tracks high-frequency energy |

---

## Colour Policy

### Cyclic Hue

```cpp
gHue += 1;  // Increments every frame (~0.5°/frame at 120 FPS)
```

**Why cyclic hue?**  
- Makes all regions "breathe" together in colour
- Easy to see if strip is rendering correctly (smooth hue progression)
- Distinguishes different test runs (hue changes over time)

**Saturation**: Fixed at 255.

**Brightness**: Direct 1:1 mapping of feature value.

---

## Diagnostic Use Cases

### Use Case 1: Verify Audio Input

**Test**: Play silence → all regions should be dark.  
**Play loud white noise** → Region 0 (RMS) and Region 7 (energyHigh) should be bright.

**Failure modes**:
- All regions bright during silence → audio input not connected, reading noise
- All regions dark during loud audio → audio input level too low, or input not connected

---

### Use Case 2: Tune Beat Detection Thresholds

**Test**: Play metronome at 120 BPM.  
**Expected**: Region 3 (kick) flashes exactly 120 times per minute.

**Failure modes**:
- Too many flashes → threshold too sensitive (false positives)
- Too few flashes → threshold too insensitive (missed beats)

**Fix**: Adjust `KICK_THRESHOLD` in audio feature extraction pipeline.

---

### Use Case 3: Verify Chord Detection

**Test**: Play sustained piano chord (e.g., C major).  
**Expected**: Region 6 (chordStability) should be bright and steady.

**Failure modes**:
- Flickering → chord detector unstable, needs temporal smoothing
- Always dark → chord detector not working, check pitch detection pipeline

---

### Use Case 4: Debug Saliency Calculation

**Test**: Play pure sine tone vs white noise.  
**Expected**: 
- Sine tone → Region 5 (saliency) bright (harmonic content)
- White noise → Region 5 dark (non-harmonic)

**Failure modes**:
- Both bright or both dark → saliency calculation broken

---

## LGP Considerations

### Hard Boundaries

**Unlike artistic effects**, this has sharp region boundaries (no blending). This is intentional for diagnostic clarity.

**Effect on LGP**: LGP will soften boundaries slightly through scattering, but regions remain visually distinct.

### No Anti-Flicker

**No temporal smoothing** (raw feature values). This is intentional:
- Diagnostic tool should show raw data
- Smoothing would hide underlying issues (e.g., noisy FFT bins)

---

## Performance Characteristics

**Heap Allocations**: None.

**Per-Frame Cost**:
- 8× feature reads
- 80× pixel writes

**Typical FPS Impact**: ~0.05 ms per frame (simplest effect).

---

## Not for Artistic Use

**This effect is intentionally NOT visually appealing.** It's a tool, not art.

**For artistic multi-feature visualisation**, consider:
- [SaliencyAwareEffect](SaliencyAwareEffect.md) — Focuses on harmonic content
- [StyleAdaptiveEffect](StyleAdaptiveEffect.md) — Adapts to musical style

---

## Taxonomy Classification

| Dimension | Value |
|-----------|-------|
| **Motion seed** | Test pattern (Seed E) |
| **Temporal driver** | Continuous tracker (multi-feature) |
| **Spatial basis** | Static regions |
| **Colour policy** | Cyclic hue + direct brightness mapping |
| **Compositing mode** | Replace |
| **Anti-flicker** | None (intentional) |
| **LGP sensitivity** | Low (diagnostic, not aesthetic) |

---

## Related Effects

None (diagnostic tool, not part of artistic effect family).

---

**End of Layer Audit: LGPAudioTestEffect**
