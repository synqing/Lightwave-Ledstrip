# Light Crystals Architecture

## Overview

Light Crystals is built with a modular architecture that separates concerns and allows for easy extension and maintenance.

## Directory Structure

```
src/
├── config/         # Configuration and compile-time settings
├── core/           # Core functionality (FxEngine, PaletteManager)
├── effects/        # Visual effects organized by complexity
├── hardware/       # Hardware interfaces and optimization
├── network/        # Web server and network functionality
└── utils/          # Utility classes (SerialMenu, StripMapper)
```

## Key Components

### FxEngine
The effect management system that handles:
- Effect registration and switching
- Smooth transitions between effects
- Performance tracking
- Render loop management

### PaletteManager
Manages color palettes:
- 33 built-in gradient palettes
- Smooth palette blending
- Palette switching and navigation

### EffectBase
Base class for all effects providing:
- Standard interface (render, init, cleanup)
- Helper functions for LED manipulation
- Frame timing and counters

### PerformanceMonitor
Comprehensive performance tracking:
- Frame timing breakdown
- CPU usage calculation
- Memory monitoring
- FPS tracking with history

### HardwareOptimizer
ESP32-specific optimizations:
- CPU frequency management
- Core affinity settings
- Memory alignment
- FastLED optimizations

## Effect Categories

### Basic Effects
Simple, efficient effects:
- Gradient: Color gradients with movement
- Wave: Sinusoidal patterns
- Pulse: Rhythmic pulsing
- Fibonacci: Golden ratio spirals
- Kaleidoscope: Symmetrical patterns

### Advanced Effects
Computationally intensive effects:
- HDR: 11-bit color depth with dithering
- Supersampled: Anti-aliased rendering
- TimeAlpha: Temporal blending
- FxWave: Physics-based animations

### Pipeline Effects
Modular, composable effects:
- Audio visualization (placeholder)
- Matrix rain
- Reaction-diffusion simulation
- Custom pipelines

## Data Flow

1. **Input** → Button press or serial command
2. **Control** → FxEngine processes input
3. **Effect** → Selected effect renders to LED array
4. **Output** → FastLED displays the result

## Memory Management

- Static allocation for LED arrays
- Dynamic allocation for effect-specific buffers
- Careful memory alignment for performance
- Monitoring via PerformanceMonitor

## Extension Points

### Adding New Effects
1. Create class inheriting from EffectBase
2. Implement render() method
3. Register in appropriate effect collection
4. Add to EffectRegistry

### Adding New Features
1. Define feature flag in features.h
2. Implement functionality with #if guards
3. Update platformio.ini if needed
4. Document in README

## Performance Considerations

- Target: 120 FPS on ESP32
- Effects optimized for linear LED strips
- Pre-calculated angle/radius mapping
- Hardware-specific optimizations enabled
- Efficient memory access patterns