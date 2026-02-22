# Audio-Visual Mapping: Quick Reference & Implementation Roadmap

**TL;DR version of the research. Use this for implementation decisions.**

---

## 1. Core Parameter Mappings (Copy-Paste Ready)

### The Seven Core Mappings

| # | Visual Parameter | Audio Feature | Formula | Range | Smoothing |
|---|------------------|---------------|---------|-------|-----------|
| 1 | **Brightness** | RMS Energy | `brightness = RMS * 255` | 10-100% | 0.5-1.0s EMA |
| 2 | **Hue (Colour)** | Spectral Centroid | `hue = (centroid_hz / 8000) * 360` | 0-360° | 1-2s EMA |
| 3 | **Saturation** | Harmonic Complexity | `saturation = 0.2 + (complexity * 0.8)` | 20-100% | 2-3s EMA |
| 4 | **Motion Speed** | Onset Density / BPM | `speed = BPM / 120` | 0.2x - 3.0x | 1-2s filter |
| 5 | **Spatial Spread** | Stereo Width | `spread = (correlation + 1) / 2` | 0.0-1.0 | 1-2s EMA |
| 6 | **Movement Direction** | Song Phase | `inward if verse; outward if chorus` | discrete | 4-8 bars |
| 7 | **Visual Weight** | Sub-Bass Energy | `weight = bass_power / total_power` | 0.0-1.0 | 0.5-1.0s EMA |

---

## 2. Colour Temperature Reference

**Hue Mapping (Spectral Centroid → HSV Hue)**

```
100 Hz   → RED (0°)          [sub-bass, warm, grounded]
300 Hz   → ORANGE (30°)      [bass presence]
500 Hz   → ORANGE (45°)      [low mids]
1000 Hz  → YELLOW (60°)      [warm mids]
2000 Hz  → GREEN-YELLOW (90°) [presence peak]
3000 Hz  → GREEN (120°)      [upper mids]
5000 Hz  → CYAN (210°)       [clarity, brightness]
8000 Hz  → BLUE (270°)       [treble, air, ethereal]
```

**Emotion → Colour Shorthand:**
- Sad/melancholic: Blue, desaturated
- Happy: Yellow, bright, saturated
- Excited/energetic: Red-Orange, very saturated
- Calm/peaceful: Green-Cyan, moderate saturation
- Angry: Red, highly saturated, high brightness
- Mysterious: Purple, moderate brightness

---

## 3. Audio Feature Extraction (TL;DR)

**What You Need to Compute Every 20ms:**

1. **RMS Energy** = square root of mean(audio^2)
   - Easy, real-time
   - Typical range: 0.01 - 0.3

2. **Spectral Centroid** = sum(freq_hz × magnitude[freq]) / sum(magnitude[freq])
   - Requires FFT (1024-point)
   - Typical range: 100 - 8000 Hz

3. **Onset Strength** = sum of positive changes in FFT magnitude per frame
   - Same FFT as centroid
   - Compare to previous frame's magnitudes

4. **Sub-Bass Power** = energy in 0-60 Hz band / total energy
   - Extract FFT bins 0-2 (at 48kHz)
   - Typical range: 0.05 - 0.4

5. **Stereo Correlation** = correlation(L_channel, R_channel)
   - Only if stereo input available
   - Typical range: -1 (opposite phase) to +1 (identical)

**Processing Pipeline:**
```
Audio Frame (960 samples) → FFT → Extract 5 features → Smooth → Map to Visual
```

**CPU Budget:** ~3-4ms per frame (available: 20ms)

---

## 4. Smoothing Strategy (Pick One)

### Simple Smoothing (Recommended for ESP32)

```cpp
// Exponential Moving Average with 1-second lag
float alpha = 0.02;  // at 50Hz update rate
smoothed = (1 - alpha) * smoothed_prev + alpha * new_sample;
```

**Time Constant Formula:**
```
alpha = 1.0 / (tau_seconds * sample_rate + 1)

// Examples:
// 0.5s lag @ 50Hz: alpha = 1.0 / (0.5 * 50 + 1) = 0.038
// 1.0s lag @ 50Hz: alpha = 1.0 / (1.0 * 50 + 1) = 0.0196
// 2.0s lag @ 50Hz: alpha = 1.0 / (2.0 * 50 + 1) = 0.0099
```

### Recommended Values by Parameter

| Parameter | Smoothing Time | Alpha (50Hz) |
|-----------|-----------------|-------------|
| Brightness (RMS) | 0.5-1.0s | 0.04-0.02 |
| Hue (Centroid) | 1-2s | 0.02-0.01 |
| Saturation | 2-3s | 0.01-0.006 |
| Motion Speed | 1-2s | 0.02-0.01 |
| Sub-Bass | 1-2s | 0.02-0.01 |

---

## 5. Brightness Coupling Rules

**To avoid harsh, unnatural visuals:**

```cpp
// Rule 1: Reduce saturation when brightness is very high
if (brightness > 0.8) {
    saturation *= (1.0 - (brightness - 0.8) * 0.5);
    // e.g., at brightness=1.0, saturation reduced by 50%
}

// Rule 2: Darken strip proportional to sub-bass energy
brightness *= (1.0 - sub_bass_weight * 0.2);
// e.g., at sub_bass_weight=1.0, brightness reduced by 20%

// Rule 3: Clamp brightness to perceptible range
brightness = clamp(brightness, 0.1, 1.0);
// Never go dark (<10%) or max out (<100%) without reason

// Rule 4: Apply gamma correction for perceptually linear brightness
brightness_corrected = pow(brightness, 1.0 / 2.2);
// Makes LED intensity feel natural to human eye
```

---

## 6. Movement Direction Interpretation

**Psychological/Neurological Basis:**

- **INWARD (centripetal):** Convergence, gathering, tension, buildup
  - Used during: Verses, pre-choruses, buildups
  - Feel: Focus, anticipation, "something is coming"
  - LED pattern: Suppress outer zones, emphasize center

- **OUTWARD (centrifugal):** Divergence, expansion, release, energy
  - Used during: Choruses, drops, climaxes
  - Feel: Release, euphoria, "payoff arrived"
  - LED pattern: Amplify outer zones, full strip activation

- **ALTERNATING:** Uncertainty, contrast, bridge segments
  - Used during: Bridges, breakdowns, unexpected moments
  - Feel: Ambiguity, development, perspective shift

---

## 7. Real-Time vs. Pre-Computed Strategy

### Real-Time Features (Always Available)

✓ RMS Energy
✓ Spectral Centroid
✓ Onset Strength
✓ Sub-Bass Power
✓ Stereo Width

**Responsiveness:** 50 Hz (every 20ms)
**Ideal For:** DJing, live performances, reactive visualizations

### Pre-Computed Features (Enhance Choreography)

✓ Song structure (beats, segments, verse/chorus labels)
✓ Key & harmonic function per segment
✓ Emotion trajectory (valence/arousal curve)
✓ Peak moments (drops, climaxes)

**How to Get:**
- Spotify Web API (segments, key, time_signature)
- librosa + trained ML model (beat tracking, segmentation)
- Manual annotation (for curated playlists)

**Ideal For:** Pre-choreographed playlists, story-driven installations

### Recommended Approach

**Hybrid Strategy:**
1. **Always use real-time features** for immediate responsiveness
2. **Optionally load pre-computed features** for narrative enhancement
3. **If pre-computed available:** Layer inward/outward direction, adjust colour palette per phase
4. **If not available:** Use real-time features alone (still effective)

---

## 8. Cross-Cultural Universals vs. Configurable

### HARD-CODE (Universals)

These work across cultures without modification:
- Fast music → Motion increase ✓
- Slow music → Motion decrease ✓
- Loud → Brightness increase ✓
- High pitch → Brighter colour hue ✓
- Low pitch → Warmer colour hue ✓

### MAKE CONFIGURABLE (Culture-Specific)

These vary by listener background:
- Major mode → Happy (Western; not universal)
- Minor mode → Sad (Western; not universal)
- Red → Danger (Western; luck in Chinese)
- White → Purity (Western; death in East Asian)

**Implementation:**
```
Provide 3-4 preset colour palettes:
1. Western (major=happy, minor=sad)
2. Alternative (reversed or custom mappings)
3. Thermal (frequency-only, no mode interpretation)
4. User Custom (allows full remapping)
```

---

## 9. Implementation Roadmap (Phased)

### Phase 1: MVP (Week 1-2)
- [x] RMS Energy → Brightness
- [x] Spectral Centroid → Hue
- [x] Basic smoothing (EMA)
- [x] Test with 3-4 songs
- **Features active:** 2/7 core mappings

### Phase 2: Completeness (Week 3-4)
- [ ] Onset Strength → Motion Speed
- [ ] Sub-Bass Energy → Visual Weight
- [ ] Harmonic Complexity → Saturation (simplified)
- [ ] Stereo Width → Spatial Spread
- [ ] Coupling rules (brightness/saturation interaction)
- **Features active:** 6/7 core mappings

### Phase 3: Polish (Week 5-6)
- [ ] Song phase detection (real-time or pre-computed)
- [ ] Movement direction (inward/outward)
- [ ] User preset system
- [ ] Calibration UI
- [ ] Test across 20+ songs, multiple genres
- **Features active:** 7/7 core mappings + narrative

### Phase 4: Advanced (Ongoing)
- [ ] ML-based emotion recognition (valence/arousal)
- [ ] Beat-synchronous colour shifts
- [ ] Custom user mapping editor
- [ ] Live concert mode
- [ ] Immersive art installation mode

---

## 10. Parameter Ranges & Defaults

### Brightness
```
Min floor: 10% (never go completely dark)
Max ceiling: 100% (full LED power)
Typical peak: 70-90% (avoid constant maxing out)
Gamma: 2.2 (perceptual linearity)
```

### Hue
```
Range: 0-360° (HSV colour wheel)
Mapping: 100Hz → 0° (red), 8000Hz → 360° (red again)
Transition speed: 1-2s smoothing (gradual feel)
```

### Saturation
```
Min: 20% (never completely desaturated, looks lifeless)
Max: 100% (fully saturated)
Typical range: 30-90%
Brightness coupling: Reduce by (brightness-0.8)*0.5 if brightness > 0.8
```

### Motion Speed
```
Min: 0.2x (very slow, once every 5 seconds)
Max: 3.0x (very fast, 3 Hz)
Typical: 0.5x - 2.0x
Mapping: speed = BPM / 120 (normalized)
```

### Spatial Spread
```
Range: 0.0 - 1.0 (mono to fully stereo)
0.0 = concentrate in center zone (20 LEDs)
0.5 = spread to 160 LEDs total
1.0 = use all 320 LEDs
```

### Sub-Bass Weight
```
Range: 0.0 - 1.0 (ratio of sub-bass energy)
Low (<0.1): Acoustic, vocals, high frequencies
Mid (0.1-0.3): Balanced mix
High (>0.3): Bass-heavy, EDM, hip-hop, dubstep
Application: Darkens overall brightness by weight * 0.2
```

---

## 11. Failure Mode Recovery

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| LEDs flicker rapidly | Insufficient smoothing | Increase EMA alpha or median window |
| Colours change too slowly | Over-smoothing | Decrease EMA alpha or window size |
| Brightness never reaches 100% | RMS scaling wrong | Calibrate peak RMS value |
| Hue stuck on red/blue | Centroid calculation error | Verify FFT is working |
| Motion speed unresponsive | Onset detection failing | Check spectral flux calculation |
| Colours appear washed out | Saturation too low globally | Increase base saturation floor |
| LEDs feel "behind" music | Processing latency too high | Profile DSP timing, optimize FFT |
| Stereo width not working | Only mono input received | Verify L/R channels are separate |

---

## 12. Testing Checklist

- [ ] FFT output visually correct (use spectrum analyzer to verify)
- [ ] RMS responds to loud/quiet sections correctly
- [ ] Centroid shifts with EQ changes (bright filter = blue shift)
- [ ] Motion speed increases during fast drum fills
- [ ] Sub-bass recognized in 20-60 Hz range
- [ ] Stereo width narrows for mono content
- [ ] Brightness stays between 10-100%
- [ ] No LED flickering or jitter
- [ ] Colour transitions smooth (no abrupt jumps)
- [ ] Works with 5+ different songs/genres
- [ ] CPU usage <30% of available budget
- [ ] Audio-to-LED latency <50ms

---

## 13. Reference Audio Features by Genre

**Use this to validate your implementation:**

| Genre | RMS Peak | Centroid | Onset Density | Sub-Bass | Stereo |
|-------|----------|----------|---------------|----------|--------|
| Orchestral | 0.15-0.25 | 2000-4000 Hz | Low | Low | Wide |
| Pop | 0.20-0.30 | 3000-5000 Hz | Moderate | Moderate | Medium |
| EDM | 0.25-0.35 | 2000-3000 Hz | High | High | Medium |
| Hip-Hop | 0.20-0.30 | 2000-4000 Hz | High | Very High | Narrow |
| Acoustic | 0.10-0.20 | 3000-6000 Hz | Low | Low | Wide |
| Ambient | 0.05-0.10 | 2000-4000 Hz | Very Low | Low | Variable |
| Heavy Bass | 0.30-0.40 | 1500-3000 Hz | Very High | Very High | Narrow |

---

## 14. Recommended Audio Analysis Tools

**For real-time extraction on ESP32:**
- Espressif ESP-DSP (built-in, optimized)
- ARM CMSIS-DSP (portable, proven)
- Kiss FFT (lightweight, open-source)

**For offline analysis (pre-compute):**
- Librosa (Python, comprehensive)
- Essentia (powerful, many features)
- Spotify Web API (pre-computed, ready-to-use)

**For validation/debugging:**
- Audacity (spectrogram visualization)
- ffmpeg (batch processing)
- SoX (command-line audio tool)

---

## 15. Quick Decision Tree

```
Are you building a:

  ├─ DJ/Live Visualizer?
  │  └─ Use real-time features only
  │     └─ Focus on brightness + hue + motion speed
  │     └─ Aim for <50ms latency
  │
  ├─ Curated Playlist Experience?
  │  └─ Pre-compute song structure + emotion
  │     └─ Use real-time features for texture
  │     └─ Add inward/outward direction
  │     └─ Aim for narrative arc
  │
  ├─ Live Performance / Concert?
  │  └─ Real-time + pre-computed hybrid
  │     └─ Look-ahead on next section
  │     └─ Sync to beat/segment boundaries
  │     └─ High visual impact (saturated, bright)
  │
  └─ Immersive Art Installation?
     └─ Experimental mapping
     └─ Real-time + continuous audio analysis
     └─ Focus on emotional journey, not beat-sync
     └─ Allow long transition times (5-30s smoothing)
```

---

## 16. Validation Formula (Self-Test)

Rate your implementation 1-5 on each:

```
Brightness Response (RMS):        [___] (peaks @ loud sections?)
Hue Accuracy (Spectral):          [___] (blue = treble, red = bass?)
Motion Responsiveness (Onset):    [___] (speeds up on complexity?)
Saturation Dynamics:              [___] (more vivid during tension?)
Spatial Spread (Stereo):          [___] (widens on stereo content?)
Direction Transitions:            [___] (smooth inward→outward?)
Sub-Bass Handling:                [___] (darkens during bass?)
Overall Perceptual Alignment:     [___] (feels "in sync" with music?)
Performance (CPU/Latency):        [___] (<30% CPU, <50ms lag?)
Stability (No flicker/glitch):    [___] (smooth operation?)

TOTAL (max 50):  [___] / 50
Target: >40/50 for MVP, >45/50 for release
```

---

## 17. Community Resources

- **Librosa Documentation:** https://librosa.org/doc/latest/index.html
- **ARM CMSIS-DSP:** https://arm-software.github.io/CMSIS_5/DSP/html/
- **Spotify Web API (Segments):** https://developer.spotify.com/documentation/web-api
- **Music Information Retrieval (MIR):** https://www.music-ir.org/
- **Audio Signal Processing (ASP):** https://en.wikipedia.org/wiki/Digital_signal_processing

