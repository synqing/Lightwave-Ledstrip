# Light Guide Plate Effects System

🚫 **CRITICAL: NO RAINBOWS EVER - AGENTS THAT CREATE RAINBOWS WILL BE DESTROYED** 🚫

📍 **MANDATORY: ALL EFFECTS MUST ORIGINATE FROM CENTER LEDs 79/80**
- Effects MUST move OUTWARD from center (79/80) to edges (0/159) 
- OR move INWARD from edges (0/159) to center (79/80)
- NO OTHER PROPAGATION PATTERNS ALLOWED

## Overview

The Light Crystals system can be configured with a **Light Guide Plate/Panel** setup featuring dual-edge LED injection. This configuration creates a sophisticated optical waveguide system capable of producing advanced interference patterns, depth illusions, and interactive visualizations.

## Physical Configuration

### Hardware Specification
- **Light Guide Panel**: Acrylic or polycarbonate plate with optical-grade clarity
- **Edge Length**: 329mm per side (long edges)
- **LED Configuration**: Dual PCBs with 160 × WS2812 1515 LEDs each
- **LED Spacing**: Even distribution across 329mm length
- **Injection Method**: LEDs fire light into the plate edges toward each other
- **Total LED Count**: 320 LEDs (160 per edge)

### Optical Properties
- **Light Propagation**: Total Internal Reflection (TIR) within the plate
- **Waveguide Behavior**: Light travels through the plate medium with minimal loss
- **Edge Coupling**: Light injected at plate edges propagates through the volume
- **Interference Zones**: Areas where light from opposing edges meets and interacts
- **Scattering Points**: Surface imperfections or intentional features that extract light

## Technical Implementation

### Coordinate System
```cpp
// Light Guide Plate Coordinate Mapping
struct LightGuideCoords {
    float edge1_position;    // Position along Edge 1 (0.0 - 1.0)
    float edge2_position;    // Position along Edge 2 (0.0 - 1.0)
    float center_distance;   // Distance from plate center (0.0 - 1.0)
    float interference_zone; // Calculated interference intensity
};
```

### Edge-to-Center Mapping
- **Edge 1 (Strip 1)**: LEDs 0-159 mapped to physical positions 0-329mm
- **Edge 2 (Strip 2)**: LEDs 0-159 mapped to opposite edge positions 0-329mm
- **Center Zone**: Area between edges where light interference occurs
- **Propagation Distance**: 329mm between opposing edges

### Interference Calculation
```cpp
// Calculate interference pattern between opposing edges
float calculateInterference(uint16_t edge1_led, uint16_t edge2_led, float phase_offset) {
    float edge1_contribution = getEdgeIntensity(edge1_led);
    float edge2_contribution = getEdgeIntensity(edge2_led);
    float phase_difference = calculatePhase(edge1_led, edge2_led, phase_offset);
    
    // Constructive/destructive interference
    return edge1_contribution + edge2_contribution + 
           2 * sqrt(edge1_contribution * edge2_contribution) * cos(phase_difference);
}
```

## Effect Categories

### 1. Interference Pattern Effects

#### Standing Wave Patterns
- **Description**: Create stationary wave patterns where light from opposing edges meets
- **Physics**: Based on wave interference principles with constructive/destructive zones
- **Implementation**: Calculate phase relationships between opposing LEDs
- **Parameters**: Wavelength, phase offset, amplitude modulation
- **Visual Result**: Stable bands of light and dark regions across the plate

#### Moiré Interference
- **Description**: Complex patterns from overlapping frequency components
- **Physics**: Beat frequency effects from slightly different edge timing
- **Implementation**: Multiple sine waves with small frequency differences
- **Parameters**: Base frequency, frequency offset, pattern rotation
- **Visual Result**: Slowly moving interference patterns with complex geometry

#### Constructive/Destructive Zones
- **Description**: Areas of enhanced or cancelled light intensity
- **Physics**: Wave amplitude addition based on phase relationships
- **Implementation**: Real-time phase calculation between edge positions
- **Parameters**: Phase shift speed, zone boundary sharpness
- **Visual Result**: Bright and dark zones that shift dynamically

### 2. Depth Illusion Effects

#### Volumetric Display Simulation
- **Description**: Create apparent 3D objects floating within the plate
- **Physics**: Intensity gradients simulate depth perception
- **Implementation**: Control which edge dominates to create depth layers
- **Parameters**: Object position, depth layer count, gradient steepness
- **Visual Result**: Objects appearing to exist at different depths in the plate

#### Parallax-Like Effects
- **Description**: Objects that appear to move differently based on viewing angle
- **Physics**: Varying edge intensity creates apparent motion parallax
- **Implementation**: Time-varying intensity ratios between edges
- **Parameters**: Parallax speed, object size, movement amplitude
- **Visual Result**: Objects that seem to move through 3D space

#### Z-Depth Mapping
- **Description**: Map effect intensity to perceived depth layers
- **Physics**: Closer objects appear brighter, distant objects dimmer
- **Implementation**: Distance-based intensity scaling
- **Parameters**: Near/far clipping planes, depth curve, fog density
- **Visual Result**: Realistic depth perception with atmospheric perspective

### 3. Physics Simulation Effects

#### Plasma Field Visualization
- **Description**: Simulate charged particle interactions between edges
- **Physics**: Electric field visualization with charges at edges
- **Implementation**: Calculate field strength at each point in the plate
- **Parameters**: Charge magnitude, field decay rate, particle density
- **Visual Result**: Flowing plasma-like patterns with realistic physics

#### Magnetic Field Lines
- **Description**: Visualize magnetic field patterns between opposing magnets
- **Physics**: Dipole magnetic field equations
- **Implementation**: Calculate field lines from edge "magnets" to center
- **Parameters**: Magnetic strength, field line density, visualization style
- **Visual Result**: Curved field lines showing magnetic flux patterns

#### Particle Collision Chamber
- **Description**: Simulate particle physics experiments with edge accelerators
- **Physics**: Momentum conservation, collision detection, particle tracks
- **Implementation**: Particle system with physics-based movement
- **Parameters**: Particle speed, collision energy, track persistence
- **Visual Result**: Particle beams from edges colliding in center with debris

#### Wave Tank Simulation
- **Description**: Realistic water wave propagation from edges
- **Physics**: Shallow water wave equations
- **Implementation**: 2D wave simulation with edge wave generators
- **Parameters**: Wave amplitude, frequency, damping, reflection
- **Visual Result**: Realistic water waves with interference and reflection

### 4. Interactive Applications

#### Proximity Sensing
- **Description**: Detect hand/object position above the plate
- **Physics**: Light occlusion changes internal light patterns
- **Implementation**: Monitor light intensity changes for occlusion detection
- **Parameters**: Sensitivity threshold, response area, tracking smoothing
- **Visual Result**: Effects that respond to hand position and movement

#### Gesture Recognition
- **Description**: Recognize basic hand gestures from light patterns
- **Physics**: Shadow pattern analysis and movement tracking
- **Implementation**: Computer vision algorithms on light intensity data
- **Parameters**: Gesture library, recognition threshold, response mapping
- **Visual Result**: Effects controlled by hand gestures above the plate

#### Touch-Reactive Surfaces
- **Description**: Respond to direct contact with the plate surface
- **Physics**: Surface vibration detection through light modulation
- **Implementation**: Accelerometer integration or light pattern analysis
- **Parameters**: Touch sensitivity, response radius, decay time
- **Visual Result**: Ripples or effects emanating from touch points

#### Data Visualization Modes
- **Description**: Display real-time data using the plate as a 2D display
- **Physics**: Spatial mapping of data values to light intensity
- **Implementation**: Data value mapping to LED positions and intensities
- **Parameters**: Data range, color mapping, update rate, visualization style
- **Visual Result**: Real-time graphs, heat maps, or flow visualizations

### 5. Advanced Optical Effects

#### Edge Coupling Resonance
- **Description**: Create resonance patterns between edges
- **Physics**: Optical resonance with feedback between edges
- **Implementation**: Feedback loops with phase-locked edge control
- **Parameters**: Resonance frequency, Q-factor, coupling strength
- **Visual Result**: Self-sustaining oscillations with complex patterns

#### Light Tennis/Pong Games
- **Description**: Interactive games using light as the "ball"
- **Physics**: Reflection and bouncing physics simulation
- **Implementation**: Ball physics with edge "paddles" controlled by encoders
- **Parameters**: Ball speed, paddle size, physics realism, game rules
- **Visual Result**: Playable games with light-based objects

#### Energy Transfer Visualization
- **Description**: Show energy flow between the edges
- **Physics**: Power flow simulation with conservation laws
- **Implementation**: Particle systems showing energy packet movement
- **Parameters**: Transfer rate, packet size, efficiency losses, visualization style
- **Visual Result**: Animated energy packets flowing between edges

#### Holographic Interference Patterns
- **Description**: Create patterns similar to holographic recordings
- **Physics**: Complex interference patterns with reference beams
- **Implementation**: Mathematical interference pattern generation
- **Parameters**: Reference beam angle, object complexity, reconstruction quality
- **Visual Result**: Hologram-like patterns with apparent depth and detail

## Technical Architecture

### Hardware Interface
```cpp
namespace LightGuide {
    // Physical constants
    constexpr float PLATE_LENGTH_MM = 329.0f;
    constexpr uint16_t LEDS_PER_EDGE = 160;
    constexpr float LED_SPACING_MM = PLATE_LENGTH_MM / LEDS_PER_EDGE;
    
    // Optical properties
    constexpr float REFRACTIVE_INDEX = 1.49f;  // Acrylic
    constexpr float CRITICAL_ANGLE = 42.2f;    // For total internal reflection
    constexpr float PROPAGATION_LOSS_DB_M = 0.1f;  // Material loss
}
```

### Effect Base Classes
```cpp
class LightGuideEffect : public EffectBase {
protected:
    // Edge control
    virtual void setEdge1LED(uint16_t index, CRGB color) = 0;
    virtual void setEdge2LED(uint16_t index, CRGB color) = 0;
    
    // Interference calculation
    virtual float calculateInterference(uint16_t edge1_pos, uint16_t edge2_pos) = 0;
    
    // Coordinate mapping
    virtual LightGuideCoords mapToPlate(uint16_t edge1_pos, uint16_t edge2_pos) = 0;
    
public:
    // Common parameters
    float interference_strength = 1.0f;
    float phase_offset = 0.0f;
    float propagation_speed = 1.0f;
};
```

### Synchronization Modes for Light Guide
```cpp
enum class LightGuideSyncMode : uint8_t {
    INTERFERENCE,      // Full interference calculation
    INDEPENDENT,       // Edges operate independently
    MIRRORED,         // Edge 2 mirrors Edge 1
    PHASE_LOCKED,     // Edges locked with phase offset
    ALTERNATING,      // Edges alternate dominance
    COOPERATIVE       // Edges work together for combined effects
};
```

## Performance Considerations

### Computational Requirements
- **Interference Calculations**: O(n²) complexity for full edge-to-edge calculation
- **Real-time Requirements**: Must maintain 120 FPS for smooth animation
- **Memory Usage**: Additional buffers for interference pattern storage
- **Processing Distribution**: Use dual-core architecture for parallel calculation

### Optimization Strategies
1. **Lookup Tables**: Pre-calculate common interference patterns
2. **Spatial Sampling**: Calculate interference at key points, interpolate between
3. **Temporal Coherence**: Use previous frame data to accelerate calculations
4. **Hardware Acceleration**: Utilize ESP32 floating-point unit efficiently

### Memory Management
```cpp
// Efficient interference pattern storage
struct InterferenceMap {
    uint8_t pattern[INTERFERENCE_MAP_SIZE];    // Compressed pattern data
    uint16_t timestamp;                        // For cache invalidation
    float phase_offset;                        // Pattern phase information
};

// Ring buffer for temporal effects
template<size_t BUFFER_SIZE>
class TemporalBuffer {
    CRGB frames[BUFFER_SIZE][HardwareConfig::NUM_LEDS];
    uint8_t current_frame = 0;
    // ... buffer management methods
};
```

## Implementation Roadmap

### Phase 1: Core Infrastructure (Week 1)
1. **Create base classes** for light guide effects
2. **Implement coordinate mapping** between edges and plate space
3. **Add configuration options** for light guide mode detection
4. **Create basic interference calculation** framework

### Phase 2: Fundamental Effects (Week 2)
1. **Standing wave patterns** with adjustable frequency
2. **Simple interference zones** with real-time calculation
3. **Edge-to-center gradients** for depth illusion basics
4. **Cross-fade algorithms** for smooth transitions

### Phase 3: Physics Simulations (Week 3)
1. **Plasma field visualization** with realistic physics
2. **Wave propagation** with proper damping and reflection
3. **Particle collision system** with momentum conservation
4. **Magnetic field line rendering** with dipole calculations

### Phase 4: Interactive Features (Week 4)
1. **Proximity detection** using light occlusion analysis
2. **Basic gesture recognition** (up/down/left/right movements)
3. **Touch-reactive zones** with ripple effects
4. **Data visualization framework** for real-time display

### Phase 5: Advanced Optical Effects (Week 5)
1. **Resonance patterns** with feedback loops
2. **Energy transfer visualizations** with conservation laws
3. **Holographic interference patterns** with complex mathematics
4. **Interactive games** (light tennis, optical pong)

### Phase 6: Optimization and Polish (Week 6)
1. **Performance optimization** for real-time operation
2. **Memory usage optimization** with efficient data structures
3. **User interface integration** with encoder controls
4. **Documentation and examples** for effect creation

## Configuration Integration

### Hardware Detection
```cpp
// Automatic light guide mode detection
bool detectLightGuideMode() {
    // Check for specific hardware configuration
    // Could use I2C device detection, pin configuration, or user setting
    return (LIGHT_GUIDE_MODE_PIN == HIGH) || 
           (checkHardwareSignature() == LIGHT_GUIDE_SIGNATURE);
}
```

### Feature Flags
```cpp
// Add to features.h
#define FEATURE_LIGHT_GUIDE_MODE 1      // Enable light guide effects
#define FEATURE_INTERFERENCE_CALC 1     // Enable interference calculations
#define FEATURE_INTERACTIVE_SENSING 1   // Enable proximity/touch sensing
#define FEATURE_PHYSICS_SIMULATION 1    // Enable physics-based effects
#define FEATURE_HOLOGRAPHIC_PATTERNS 1  // Enable holographic effects
```

### M5Stack Encoder Integration
```cpp
// Encoder assignments for light guide mode
enum LightGuideEncoderFunction {
    EFFECT_SELECTION = 0,     // Choose light guide effect
    INTERFERENCE_STRENGTH,    // Control interference intensity
    PHASE_OFFSET,            // Adjust phase relationships
    PROPAGATION_SPEED,       // Control wave/pattern speed
    EDGE_BALANCE,            // Balance between Edge 1 and Edge 2
    DEPTH_LAYER,             // Select depth layer for 3D effects
    INTERACTION_SENSITIVITY, // Adjust touch/proximity sensitivity
    PHYSICS_PARAMETER        // Control effect-specific physics
};
```

## Testing and Validation

### Visual Tests
1. **Interference Pattern Verification**: Confirm mathematical patterns match visual output
2. **Depth Perception Testing**: Validate 3D illusion effectiveness
3. **Color Accuracy**: Ensure color reproduction through the light guide
4. **Brightness Uniformity**: Check for even illumination across the plate

### Performance Benchmarks
1. **Frame Rate Stability**: Maintain 120 FPS under all conditions
2. **Interference Calculation Speed**: Measure calculation time per frame
3. **Memory Usage Monitoring**: Track heap usage and fragmentation
4. **Interactive Response Time**: Measure lag from input to visual response

### Physics Validation
1. **Wave Equation Accuracy**: Compare simulated waves to theoretical predictions
2. **Interference Pattern Correctness**: Validate against optical interference theory
3. **Particle Physics Realism**: Check momentum conservation and collision accuracy
4. **Field Visualization Accuracy**: Compare to known electromagnetic field patterns

## Future Enhancements

### Hardware Additions
- **Multiple Light Sources**: Add controllable point sources within the plate
- **Sensors Integration**: Accelerometers, gyroscopes for motion-reactive effects
- **Camera Integration**: Computer vision for advanced gesture recognition
- **Audio Input**: Sound-reactive light guide effects

### Software Enhancements
- **Machine Learning**: AI-powered gesture recognition and pattern generation
- **Network Integration**: Synchronized light guide displays across multiple units
- **Advanced Physics**: Quantum mechanics visualizations, relativistic effects
- **User-Generated Content**: Framework for users to create custom effects

### Applications
- **Art Installations**: Museum and gallery display systems
- **Educational Tools**: Physics demonstration and visualization
- **Entertainment**: Interactive gaming and immersive experiences
- **Scientific Visualization**: Data display for research and analysis

## Conclusion

The Light Guide Plate configuration transforms the Light Crystals system into a sophisticated optical display medium capable of advanced visualizations that are impossible with traditional LED arrangements. The dual-edge injection creates unique opportunities for interference patterns, depth illusions, and interactive experiences that showcase the intersection of physics, mathematics, and visual art.

This comprehensive system leverages the existing modular architecture while adding specialized capabilities for optical waveguide effects, ensuring both powerful functionality and maintainable code structure.