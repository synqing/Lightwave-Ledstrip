# Visual Feedback System & Transition Engine Test Guide

## Quick Start

After integrating the VFS and Transition Engine using the `integration_patch.cpp`, here's how to test the new features:

### 1. Visual Feedback System Testing

The encoder LEDs should now show dynamic feedback:

- **Encoder 0 (Red)**: Pulses with breathing effect, flashes white on effect change
- **Encoder 1 (White)**: Brightness matches actual LED brightness
- **Encoder 2 (Purple)**: Cycles through current palette colors
- **Encoder 3 (Yellow)**: Pulses faster with higher speed setting
- **Encoder 4 (Orange)**: Brightness indicates intensity parameter
- **Encoder 5 (Cyan)**: Shows saturation level with breathing
- **Encoder 6 (Green)**: Complex pattern based on complexity setting
- **Encoder 7 (Magenta)**: Different patterns for variation modes

**Test Steps:**
1. Turn any encoder - it should flash green briefly
2. Change effects with encoder 0 - should flash white
3. Adjust parameters and watch the corresponding encoder LED respond
4. The LEDs provide immediate visual confirmation of changes

### 2. Transition Engine Testing

The new transition engine provides these transition types:

- **FADE**: Classic smooth crossfade
- **WIPE (L→R, R→L)**: Directional wipes
- **WIPE (Out/In)**: CENTER ORIGIN compliant wipes
- **DISSOLVE**: Random pixel transitions
- **ZOOM (In/Out)**: Scale effects from/to center
- **MELT**: Thermal melting effect
- **SHATTER**: Particle explosion
- **GLITCH**: Digital corruption effect
- **PHASE SHIFT**: Frequency-based morphing

**Test Steps:**
1. Change effects rapidly to see different transitions
2. The system randomly selects transitions for variety
3. All transitions use smooth easing curves
4. Transitions maintain CENTER ORIGIN philosophy

### 3. Performance Optimization

Both systems are optimized for 120 FPS:

- **VFS**: Updates at 50Hz (every 20ms) to reduce overhead
- **TE**: Uses FastLED's optimized `blend()` function
- **Memory**: Static allocation, no heap fragmentation
- **CPU**: Efficient integer math where possible

### 4. Customization

#### Adjust Transition Duration
```cpp
// In startTransition(), change duration (milliseconds):
transitionEngine->startTransition(
    currentBuffer, nextBuffer, leds,
    transType, 2000, EASE_IN_OUT_CUBIC  // 2 seconds instead of 1
);
```

#### Force Specific Transitions
```cpp
// Instead of random selection:
TransitionType transType = TRANSITION_MELT;  // Always use melt
```

#### Customize VFS Colors
```cpp
// In updateEffectIndicator():
led.r = 0;    // Change from red
led.g = 255;  // to green
led.b = 0;
```

### 5. Troubleshooting

**Encoder LEDs not updating:**
- Check if `encoderManager.isAvailable()` returns true
- Verify I2C connection to M5ROTATE8
- Ensure `encoderFeedback` is initialized

**Transitions look jerky:**
- Check frame rate isn't dropping below 60 FPS
- Reduce transition complexity for slower boards
- Use simpler transitions (FADE, WIPE) for testing

**Memory issues:**
- The system uses ~1KB for VFS, ~2KB for TE
- Ensure at least 10KB free RAM available
- Disable unused features to free memory

### 6. Advanced Features

**Parameter-Aware Transitions:**
The transition engine can be extended to consider effect parameters:
- Fire effects → Use MELT transition
- Wave effects → Use PHASE_SHIFT transition
- Particle effects → Use SHATTER transition

**Synchronized Feedback:**
VFS can be synchronized with the actual LED effects:
- Mirror effect colors on encoders
- Show effect "energy" levels
- Indicate beat detection (if implemented)

## Summary

The Visual Feedback System and Transition Engine work together to create a professional, responsive interface:

- **Immediate visual feedback** for all encoder adjustments
- **Smooth, varied transitions** between effects
- **Performance optimized** for 120 FPS operation
- **CENTER ORIGIN compliant** for consistent aesthetics
- **Extensible design** for future enhancements

These systems significantly enhance the user experience while maintaining the high performance standards of the K1-LIGHTWAVE system.