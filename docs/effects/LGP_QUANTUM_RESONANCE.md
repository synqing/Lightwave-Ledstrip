# LGP Quantum Resonance Effect

## Overview
The LGP Quantum Resonance effect is an advanced audio-reactive visualization that combines quantum wave mechanics with the dual-strip light guide plate configuration. It creates a mesmerizing display of wave probability fields that collapse and reform in response to music, with particles that surf along wave gradients.

## Key Features

### 1. **Quantum Wave Probability Fields**
- Dual wave fields (one per edge) with organic initialization
- Wave evolution with fluid-like velocity dynamics
- Natural decay and regeneration cycles
- Audio-influenced wave amplitude modulation

### 2. **Audio-Reactive Wave Collapse**
- Beat detection triggers wave function collapse events
- Collapse centers determined by peak frequency analysis
- Gaussian probability distribution for natural collapse patterns
- Complementary collapses on opposing edges create interference

### 3. **Particle Physics System**
- 8 particles per edge with mass-based acceleration
- Particles follow wave probability gradients
- Energy-based velocity limiting and boundary bouncing
- Audio boosts particle energy and creates trails

### 4. **Triadic Color Harmony**
- Dynamic color zones based on position
- Smooth blending between color regions
- Colors rotate with global hue shifts
- Saturation varies with wave intensity

### 5. **Multi-Layer Audio Analysis**
- Beat detection for major events
- Energy tracking for smooth transitions
- Frequency mapping to wave positions
- Transient detection for particle activation

## Control Parameters

### Encoder Mappings
- **Speed (Encoder 3)**: Animation and particle movement speed
- **Intensity (Encoder 4)**: Overall brightness and wave amplitude
- **Saturation (Encoder 5)**: Color saturation control
- **Complexity (Encoder 6)**: Wave collapse sensitivity and width
- **Variation (Encoder 7)**: Audio reactivity strength (not yet implemented)

### Visual Params Integration
The effect uses the global `visualParams` object:
- `getIntensityNorm()`: Scales wave brightness and particle glow
- `getSaturationNorm()`: Controls color vibrancy
- `getComplexityNorm()`: Affects collapse behavior and wave detail
- `paletteSpeed`: Global animation speed multiplier

## Technical Implementation

### Wave Field Physics
```cpp
// Wave evolution with gradient-based velocity
float gradient = wave[i+1].probability - wave[i-1].probability;
velocity += gradient * dampingFactor;
probability += velocity * deltaTime;
```

### Particle Dynamics
```cpp
// Mass-based acceleration from wave gradient
float mass = 1.5f - particle.energy * 0.5f;
float acceleration = gradient * 0.25f / mass;
particle.velocity += acceleration * deltaTime;
```

### Audio Processing
- Normalizes current energy against recent average
- Detects sudden energy increases for beat detection
- Natural decay curves for smooth visual response
- Multiple timescales for varied reactivity

### Interference Calculations
The effect leverages the `LightGuideEffect` base class:
- `calculateInterference()`: Phase-based wave interference
- `applyCenterOriginConstraint()`: Ensures outward propagation
- Complementary patterns on opposing edges

## Performance Optimizations

1. **Fixed Update Rate**: Limits wave field updates to maintain consistent performance
2. **Particle Pooling**: Pre-allocated particle arrays avoid dynamic memory
3. **Lookup Tables**: Uses sine/cosine approximations for speed
4. **Selective Rendering**: Only active particles are processed
5. **Constrained Operations**: All values clamped to valid ranges

## Visual Characteristics

### Without Audio
- Gentle wave oscillations with organic flow
- Particles move smoothly along wave gradients
- Slow color rotation through triadic harmony
- Subtle interference patterns between edges

### With Audio
- Dramatic wave collapses on beats
- Particles energized by frequency content
- Dynamic bloom effects scale with volume
- Color zones shift based on spectral content
- Synchronized patterns create visual rhythm

## Integration with LGP System

The effect fully utilizes the Light Guide Plate architecture:
- **Center-Origin Constraint**: All waves propagate outward from center
- **Edge Interference**: Opposing waves create complex patterns
- **Optical Simulation**: Brightness falls off naturally from sources
- **Dual-Strip Coordination**: Complementary behaviors enhance depth

## Future Enhancements

1. **Variation Parameter**: Implement audio sensitivity control
2. **Frequency Bands**: Map specific frequency ranges to color zones
3. **Pattern Memory**: Short-term pattern recognition for repetitive beats
4. **Gesture Control**: Respond to motion sensors for interactive mode
5. **Network Sync**: Coordinate multiple units for large installations

## Usage Tips

- **Low Complexity**: Creates wider, more stable wave patterns
- **High Complexity**: Narrow collapses for sharp, precise reactions
- **Speed Control**: Balance between smooth flow and snappy response
- **Intensity**: Adjust for ambient brightness without losing detail

The LGP Quantum Resonance effect showcases the potential of combining sophisticated audio analysis with physics-based visualization on the unique dual-strip light guide plate hardware.