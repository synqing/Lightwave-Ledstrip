# Light Crystals Effects Guide

## Effect Categories

### Basic Effects

#### Gradient
- Smooth color transitions across the strip
- Parameters: fade amount, speed
- Uses angle mapping for circular gradients

#### Fibonacci Wave
- Based on golden ratio mathematics
- Creates organic, flowing patterns
- Multiple overlapping sine waves
- Temporal blending for smoothness

#### Wave
- Simple sinusoidal patterns
- Adjustable wavelength and speed
- Color varies by position

#### Pulse
- Rhythmic brightness pulsing
- Can create concentric pulses using radius mapping
- Synchronized or phased pulsing

#### Kaleidoscope
- Symmetrical patterns with multiple reflection points
- Rotating symmetry axes
- Creates mandala-like visuals

### Advanced Effects

#### HDR 11-bit
- Extended color depth (2048 levels vs 256)
- Temporal dithering for smooth gradients
- Reduces color banding
- Higher computational cost

#### Supersampled
- Renders at 4x resolution
- Antialiasing filter for smoothness
- Reduces aliasing artifacts
- Memory intensive

#### Time Alpha
- Blends multiple time frames
- Creates motion blur effects
- 4-frame history buffer
- Weighted temporal blending

#### FxWave Effects
- **Ripple**: Expanding circular waves
- **Interference**: Two interfering wave patterns
- **Orbital**: Orbiting light sources with trails

### Pipeline Effects (Experimental)

#### Audio Visualizer
- Placeholder for future audio input
- 8-band frequency visualization
- Reactive brightness and color

#### Matrix Rain
- Digital rain effect
- Falling light drops with trails
- Variable speed and density

#### Reaction-Diffusion
- Gray-Scott chemical simulation
- Creates organic patterns
- Self-organizing behavior

## Effect Parameters

### Global Parameters
- **Brightness**: Overall LED brightness (0-255)
- **Fade Amount**: Trail/persistence (0-255)
- **Palette Speed**: Animation speed (1-50)
- **Palette**: Color scheme selection

### Effect-Specific Parameters
Each effect may have additional parameters accessible through:
- Serial menu system
- Web interface (when enabled)
- Direct code modification

## Creating Custom Effects

### Basic Template
```cpp
class MyEffect : public EffectBase {
public:
    MyEffect() : EffectBase("My Effect", 128, 10, 20) {}
    
    void render() override {
        // Your effect code here
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Use angles[i] and radii[i] for mapping
            leds[i] = CRGB::Red;
        }
    }
};
```

### Best Practices
1. Use fadeToBlackBy() for trails
2. Utilize angle/radius arrays for interesting patterns
3. Keep frame time under 8.3ms for 120 FPS
4. Use ColorFromPalette() for consistent theming
5. Implement init() for setup, cleanup() for teardown

## Performance Tips

### Optimization Strategies
- Pre-calculate values in init()
- Use integer math when possible
- Minimize per-pixel calculations
- Use FastLED's optimized functions
- Consider memory access patterns

### Profiling
Use PerformanceMonitor to track:
- Effect processing time
- FastLED.show() time
- Overall frame time
- CPU usage percentage