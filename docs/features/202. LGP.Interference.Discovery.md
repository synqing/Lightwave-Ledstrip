# Light Guide Plate Interference Pattern Discovery

## Executive Summary

During testing of the Strip BPM effect on a custom Light Guide Plate (LGP) display with opposing edge-lit LED strips, we discovered that the center-origin animation patterns create unexpected and visually striking optical interference phenomena. Instead of simple radial pulses, the LGP transforms these patterns into distinct moving boxes with secondary color layers - a completely different visual effect than traditional LED displays.

## Hardware Configuration

### Physical Setup
```
LED Strip 1 (160 LEDs) → → → → → |=========LGP=========| ← ← ← ← ← LED Strip 2 (160 LEDs)
                                  |                     |
                                  |   Interference      |
                                  |     Patterns        |
                                  |                     |
                                  |===================|
```

- **LED Strips**: 2x WS2812B strips, 160 LEDs each
- **Positioning**: Edge-lighting opposing long edges of LGP
- **Light Direction**: LEDs fire directly into LGP edges, toward each other
- **LGP Properties**: Acts as optical waveguide with internal reflections

## Observed Phenomena

### Primary Effect: Moving Boxes
- **Count**: 3-4 boxes per side, 6-8 total across display
- **Motion**: Lateral movement (left-right oscillation)
- **Appearance**: Distinct rectangular regions with sharp boundaries
- **Size**: Approximately 20-27 LEDs per box (160 LEDs ÷ 6-8 boxes)

### Secondary Effect: Color Waves
- **Direction**: From edges toward center
- **Behavior**: Colors converge at center and disappear
- **Layer**: Appears to move through/over the box pattern
- **Timing**: Synchronized with palette cycling

## Technical Analysis

### Why This Happens

1. **Optical Waveguide Behavior**
   - LGP acts as a 2D waveguide
   - Total Internal Reflection (TIR) creates light confinement
   - Edge coupling creates specific mode patterns

2. **Standing Wave Formation**
   ```
   Strip 1 wave: →→→→→→→→→
   Strip 2 wave: ←←←←←←←←←
   Interference: ▢ ▢ ▢ ▢ ▢ ▢
   ```

3. **Box Formation Mechanism**
   - Wavelength λ ≈ 160 LEDs ÷ 7 boxes ≈ 23 LED spacing
   - Suggests fundamental mode with 6-8 antinodes
   - Box boundaries occur at interference nulls

### Code Analysis

The Strip BPM effect's math accidentally creates perfect LGP patterns:

```cpp
// Original code
uint8_t colorIndex = gHue + (distFromCenter * 2);
uint8_t brightness = beat - gHue + (distFromCenter * 10);
```

The `distFromCenter * 10` term creates a spatial frequency that, when combined with opposing edge illumination, generates the observed standing wave pattern.

## LGP-Specific Effect Design Principles

### 1. Spatial Frequency Control
```cpp
// Box size formula
boxCount = spatialMultiplier * 2;  // Due to opposing edges
// For 6-8 boxes: spatialMultiplier should be 3-4

// Implementation
uint8_t colorIndex = gHue + (distFromCenter * 3.5);  // Creates ~7 boxes
```

### 2. Phase Relationships
- **In-phase**: Reinforces standing waves, sharper boxes
- **Anti-phase**: Creates traveling waves, flowing motion
- **Quadrature**: Creates rotating/spiral effects

### 3. Temporal Modulation
- Beat frequency controls box oscillation speed
- Phase velocity determines apparent motion direction

## Proposed LGP Interference Effects

### 1. Box Wave Controller
```cpp
void lgpBoxWave() {
    // Parameters
    float boxCount = 3.5;  // Number of boxes per side
    float motionSpeed = 1.0;
    float colorFlow = 2.0;
    
    for(int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - STRIP_CENTER_POINT);
        
        // Create box pattern
        float boxPhase = distFromCenter * boxCount * (PI / STRIP_HALF_LENGTH);
        uint8_t boxIntensity = 128 + 127 * sin(boxPhase + beat * motionSpeed);
        
        // Color wave overlay
        uint8_t colorIndex = gHue + (distFromCenter * colorFlow);
        
        // Anti-phase for opposing edges
        strip1[i] = CHSV(colorIndex, 255, boxIntensity);
        strip2[i] = CHSV(colorIndex + 128, 255, boxIntensity);
    }
}
```

### 2. Holographic Shimmer
```cpp
void lgpHolographic() {
    // Creates depth illusion through controlled interference
    for(int i = 0; i < STRIP_LENGTH; i++) {
        float dist = abs(i - STRIP_CENTER_POINT);
        
        // Multiple spatial frequencies for depth layers
        float layer1 = sin(dist * 0.1 + beat * 0.01);
        float layer2 = sin(dist * 0.3 + beat * 0.02);
        float layer3 = sin(dist * 0.5 + beat * 0.03);
        
        uint8_t brightness = 85 * (layer1 + layer2 + layer3) + 128;
        
        strip1[i] = CHSV(gHue + dist, 200, brightness);
        strip2[i] = CHSV(gHue - dist, 200, brightness);
    }
}
```

### 3. Modal Resonance Explorer
```cpp
void lgpModalResonance() {
    // Sweeps through different modal patterns
    float modalNumber = 1 + (beatsin8(10) / 255.0 * 10);  // 1-11 modes
    
    for(int i = 0; i < STRIP_LENGTH; i++) {
        float position = (float)i / STRIP_LENGTH;
        float modalPattern = sin(position * modalNumber * TWO_PI);
        
        uint8_t brightness = 128 + 127 * modalPattern;
        strip1[i] = ColorFromPalette(currentPalette, i * 2, brightness);
        strip2[i] = ColorFromPalette(currentPalette, i * 2 + 128, brightness);
    }
}
```

### 4. Interference Scanner
```cpp
void lgpInterferenceScanner() {
    // Creates moving interference patterns
    static float phaseOffset = 0;
    phaseOffset += 0.01;
    
    for(int i = 0; i < STRIP_LENGTH; i++) {
        float dist = abs(i - STRIP_CENTER_POINT);
        
        // Create interference between multiple frequencies
        float wave1 = sin(dist * 0.2 + phaseOffset);
        float wave2 = sin(dist * 0.2 - phaseOffset);
        float interference = (wave1 + wave2) / 2;
        
        uint8_t brightness = 128 + 64 * interference;
        
        // Opposing phase creates motion
        strip1[i] = CHSV(gHue, 255, brightness);
        strip2[i] = CHSV(gHue + 128, 255, 255 - brightness);
    }
}
```

## Testing Protocol

### Phase 1: Box Count Calibration
1. Test spatial multipliers from 1-10
2. Document box count for each value
3. Find optimal values for 4, 6, 8, 10, 12 boxes

### Phase 2: Motion Dynamics
1. Test phase relationships (0°, 90°, 180°, 270°)
2. Document motion patterns
3. Test beat frequencies (slow to fast)

### Phase 3: Color Interference
1. Test matching vs opposing colors
2. Test gradient steepness
3. Test palette effects

### Phase 4: Advanced Patterns
1. Multi-frequency interference
2. Traveling vs standing waves
3. Mode jumping effects

## Implementation Plan

### Step 1: Create LGP Test Suite
- Dedicated file: `LGPInterferenceEffects.cpp`
- Parameter controls via encoders
- Real-time tuning capability

### Step 2: Measurement Tools
- Box counter visualization
- Phase relationship display
- Frequency spectrum analyzer

### Step 3: Effect Optimization
- Fine-tune for specific box counts
- Optimize for visual impact
- Create smooth transitions

## Future Research Directions

1. **Acoustic Analogy**: Model as Kundt's tube visualization
2. **Fourier Analysis**: Decompose patterns into frequency components
3. **3D Effects**: Create depth illusions using phase delays
4. **Reactive Modes**: Audio-driven interference patterns
5. **AI Pattern Generation**: Train models on interference patterns

## Conclusion

The discovery that LGP edge-lighting transforms center-origin effects into complex interference patterns opens up entirely new possibilities for LED visualization. By understanding and controlling these optical phenomena, we can create effects impossible with traditional LED displays:

- Physical motion illusions without moving parts
- Depth perception in 2D displays
- Complex wave interactions visible to the naked eye
- Holographic-like visual effects

This positions the LightwaveOS LGP display as not just an LED controller, but as an **optical computer** capable of real-time wave processing and visualization.

## Next Steps

1. Implement the four proposed test effects
2. Create encoder mappings for real-time parameter control
3. Document optimal values for different visual goals
4. Share findings with the creative LED community
5. Explore commercial applications (art installations, etc.)

---

*Discovery Date: July 4, 2025*  
*Hardware: LightwaveOS with dual WS2812B strips on Light Guide Plate*  
*Effect: Strip BPM creating unexpected interference patterns*