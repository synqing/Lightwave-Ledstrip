# Encoder Parameter Tuning Guide

## Overview
LightwaveOS uses 8 encoders to control various aspects of the LED effects. This guide explains how each encoder affects the tuned versions of Fire, Strip BPM, Sinelon, and Gravity Well effects.

## Global Encoder Mappings

| Encoder | Parameter | Range | Description |
|---------|-----------|-------|-------------|
| 0 | Effect Selection | 0-NUM_EFFECTS | Selects which effect to display |
| 1 | Brightness | 0-255 | Global LED brightness |
| 2 | Palette | 0-NUM_PALETTES | Color palette selection |
| 3 | Speed | 0-255 | Animation speed (effect-specific) |
| 4 | Intensity | 0-255 | Effect intensity/amplitude |
| 5 | Saturation | 0-255 | Color saturation (0=white, 255=full color) |
| 6 | Complexity | 0-255 | Effect complexity/detail level |
| 7 | Variation | 0-255 | Effect variation/mode selector |

## Effect-Specific Parameter Mappings

### 🔥 Fire Effect (fireTuned)

| Encoder | Parameter | Effect on Visual |
|---------|-----------|------------------|
| **Speed (3)** | Animation Rate | How fast the flames flicker and move |
| **Intensity (4)** | Fire Height | Controls spark frequency and maximum flame height |
| **Saturation (5)** | Color Richness | 0 = White/blue fire, 255 = Rich orange/red fire |
| **Complexity (6)** | Turbulence | Low = Smooth flames, High = Turbulent, realistic fire |
| **Variation (7)** | Flame Color | 0-85 = Normal fire, 86-170 = Blue fire, 171-255 = Green/chemical fire |

**Tuning Tips:**
- For realistic fire: Intensity=180, Saturation=200, Complexity=180
- For stylized fire: Intensity=255, Saturation=255, Complexity=100
- For ghostly flames: Intensity=100, Saturation=50, Variation=128 (blue)

### 🎵 Strip BPM Effect (stripBPMTuned)

| Encoder | Parameter | Effect on Visual |
|---------|-----------|------------------|
| **Speed (3)** | BPM | 30-180 beats per minute |
| **Intensity (4)** | Pulse Strength | Brightness and reach of each pulse |
| **Saturation (5)** | Color Vibrancy | Mix between white and colored pulses |
| **Complexity (6)** | Pulse Count | 1-4 simultaneous pulses with phase offsets |
| **Variation (7)** | Rhythm Pattern | 0-85 = Single pulse, 86-170 = Double time, 171-255 = Triple with swing |

**Tuning Tips:**
- For club feel: Speed=120 (maps to ~140 BPM), Complexity=200, Variation=170
- For ambient: Speed=60, Intensity=128, Complexity=64
- For intense: Speed=180, Intensity=255, Complexity=255, Variation=255

### 〰️ Sinelon Effect (sinelonTuned)

| Encoder | Parameter | Effect on Visual |
|---------|-----------|------------------|
| **Speed (3)** | Oscillation Rate | How fast dots move back and forth |
| **Intensity (4)** | Trail Length | Length and brightness of motion trails |
| **Saturation (5)** | Color Purity | Blend between white and colored dots |
| **Complexity (6)** | Dot Count | 1-5 oscillating dots |
| **Variation (7)** | Movement | 0-63 = Sine wave, 64-127 = Double frequency, 128-191 = Bounce, 192-255 = Chaotic |

**Tuning Tips:**
- For hypnotic: Speed=30, Intensity=200, Complexity=128, Variation=0
- For energetic: Speed=180, Intensity=255, Complexity=255, Variation=192
- For meditation: Speed=20, Intensity=150, Complexity=64, Variation=64

### 🌌 Gravity Well Effect (gravityWellTuned)

| Encoder | Parameter | Effect on Visual |
|---------|-----------|------------------|
| **Speed (3)** | Gravity Force | Strength of gravitational pull |
| **Intensity (4)** | Particle Glow | Brightness and trail intensity |
| **Saturation (5)** | Color Depth | Vibrancy of particle colors |
| **Complexity (6)** | Particle Count | 5-30 active particles |
| **Variation (7)** | Physics Mode | 0-63 = Normal, 64-127 = Low friction, 128-191 = Oscillating center, 192-255 = Chaotic |

**Tuning Tips:**
- For space sim: Speed=50, Intensity=180, Complexity=150, Variation=64
- For particle storm: Speed=200, Intensity=255, Complexity=255, Variation=192
- For gentle drift: Speed=30, Intensity=128, Complexity=100, Variation=0

## Advanced Tuning Techniques

### Cross-Parameter Interactions
Some parameters work best when adjusted together:

1. **Fire Effect**
   - High Complexity + Low Intensity = Smoky effect
   - Low Complexity + High Intensity = Torch-like flames
   - High Saturation + Blue Variation = Gas flame effect

2. **BPM Effect**
   - High Complexity + Low Speed = Polyrhythmic patterns
   - Low Complexity + High Speed = Strobe-like pulses
   - Variation changes + Speed = Different groove feelings

3. **Sinelon Effect**
   - High Complexity + Chaotic Variation = Firefly swarm
   - Low Intensity + High Speed = Sharp, precise movements
   - High Intensity + Low Speed = Smooth, flowing trails

4. **Gravity Well**
   - Low Speed + High Complexity = Asteroid field
   - High Speed + Low Complexity = Black hole effect
   - Oscillating Variation + Medium Speed = Tidal forces

### Performance Considerations

- **High Complexity** settings use more CPU
- **Many particles** (Gravity Well) or dots (Sinelon) impact frame rate
- **Long trails** (high Intensity) require more memory bandwidth

### Creating Presets

Example preset values for different moods:

**Relaxation Suite:**
```
Fire:     Speed=40,  Intensity=120, Saturation=180, Complexity=100, Variation=0
BPM:      Speed=50,  Intensity=100, Saturation=200, Complexity=64,  Variation=0
Sinelon:  Speed=30,  Intensity=180, Saturation=220, Complexity=128, Variation=64
Gravity:  Speed=40,  Intensity=150, Saturation=200, Complexity=120, Variation=64
```

**Party Mode:**
```
Fire:     Speed=180, Intensity=255, Saturation=255, Complexity=200, Variation=0
BPM:      Speed=140, Intensity=255, Saturation=255, Complexity=255, Variation=170
Sinelon:  Speed=200, Intensity=255, Saturation=255, Complexity=200, Variation=192
Gravity:  Speed=180, Intensity=255, Saturation=255, Complexity=255, Variation=192
```

**Ambient Environment:**
```
Fire:     Speed=20,  Intensity=80,  Saturation=120, Complexity=60,  Variation=128
BPM:      Speed=40,  Intensity=80,  Saturation=150, Complexity=100, Variation=85
Sinelon:  Speed=25,  Intensity=120, Saturation=180, Complexity=80,  Variation=0
Gravity:  Speed=35,  Intensity=100, Saturation=160, Complexity=140, Variation=128
```

## Integration Notes

To use these tuned effects in your main.cpp, add them to your effects array:

```cpp
#include "effects/strip/TunedEffects.h"

// In your effects array:
{"Fire Tuned", fireTuned, EFFECT_TYPE_STANDARD},
{"BPM Tuned", stripBPMTuned, EFFECT_TYPE_STANDARD},
{"Sinelon Tuned", sinelonTuned, EFFECT_TYPE_STANDARD},
{"Gravity Tuned", gravityWellTuned, EFFECT_TYPE_STANDARD},
```

The tuned versions provide much more nuanced control over the visual output while maintaining the center-origin design principle of LightwaveOS.