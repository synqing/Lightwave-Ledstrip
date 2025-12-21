# LGP Pattern Development Playbook
## Complete Implementation Guide for Light Guide Plate Pattern Development

*Step-by-step workflow, technical specifications, safety protocols, and testing procedures for creating LGP visual effects*

---

## Table of Contents

1. [Pattern Development Workflow](#pattern-development-workflow)
2. [Code Structure Templates](#code-structure-templates)
3. [Technical Specifications](#technical-specifications)
4. [Safety Protocols](#safety-protocols)
5. [Testing Procedures](#testing-procedures)
6. [Performance Optimization](#performance-optimization)
7. [Troubleshooting Guide](#troubleshooting-guide)

---

## Pattern Development Workflow

### Phase 1: Concept Development

#### Step 1: Define Visual Concept
- **What** should the pattern look like?
- **How** does it relate to music/audio?
- **Why** is this pattern unique or valuable?

#### Step 2: Identify Physics Basis
- Which optical/physical principles apply?
- What interference patterns are involved?
- How does dual-edge injection affect the pattern?

#### Step 3: Sketch Visual Signature
- Draw or describe the visual appearance
- Identify key visual characteristics
- Define motion dynamics

**Deliverable:** Concept document with visual description and physics basis

### Phase 2: Architecture Design

#### Step 1: Choose Base Class
```cpp
// Option 1: LightGuideEffect (for LGP-specific effects)
class MyPattern : public LightGuideEffect {
    // Inherits interference calculation, edge control
};

// Option 2: EffectBase (for general effects)
class MyPattern : public EffectBase {
    // More control, manual edge management
};
```

#### Step 2: Define State Structure
```cpp
struct PatternState {
    // Static allocation only - no dynamic memory
    float waveBuffer[160];
    float velocityBuffer[160];
    uint8_t activePulses;
    float phase;
    // ... other state variables
};
```

#### Step 3: Plan Memory Usage
- Calculate total memory requirements
- Ensure static allocation
- Verify within 320KB RAM budget

**Deliverable:** Architecture design document with memory budget

### Phase 3: Implementation

#### Step 1: Create Header File
```cpp
// MyPattern.h
#ifndef MY_PATTERN_H
#define MY_PATTERN_H

#include <FastLED.h>
#include "../lightguide/LightGuideEffect.h"

class MyPattern : public LightGuideEffect {
private:
    // State variables (static allocation)
    static float waveBuffer[160];
    static float phase;
    
public:
    MyPattern(CRGB* strip1, CRGB* strip2);
    void render() override;
    
private:
    void updatePhysics(float deltaTime);
    void renderToStrips();
};

#endif
```

#### Step 2: Implement Core Logic
```cpp
// MyPattern.cpp
#include "MyPattern.h"

float MyPattern::waveBuffer[160] = {0};
float MyPattern::phase = 0.0f;

MyPattern::MyPattern(CRGB* strip1, CRGB* strip2) 
    : LightGuideEffect("My Pattern", strip1, strip2) {
    // Initialization
}

void MyPattern::render() {
    float deltaTime = 0.00833f;  // 120 FPS = 8.33ms per frame
    
    updatePhysics(deltaTime);
    renderToStrips();
    
    // Copy to unified buffer for transitions
    for (int i = 0; i < 160; i++) {
        leds[i] = strip1[i];
        leds[i + 160] = strip2[i];
    }
}

void MyPattern::updatePhysics(float deltaTime) {
    // Physics simulation
    phase += deltaTime * paletteSpeed * 0.01f;
    
    // Wave propagation
    for (int i = 1; i < 159; i++) {
        float gradient = (waveBuffer[i+1] - waveBuffer[i-1]) * 0.5f;
        waveBuffer[i] += gradient * deltaTime;
        waveBuffer[i] *= 0.98f;  // Damping
    }
}

void MyPattern::renderToStrips() {
    for (int i = 0; i < 160; i++) {
        float value = waveBuffer[i];
        uint8_t brightness = 128 + value * 127;
        
        uint8_t hue = gHue + i * 2;
        setEdge1LED(i, CHSV(hue, visualParams.saturation, brightness));
        setEdge2LED(i, CHSV(hue + 128, visualParams.saturation, brightness));
    }
}
```

#### Step 3: Register in Main
```cpp
// src/main.cpp
#include "effects/lightguide/MyPattern.h"

// In effects array:
{"My Pattern", myPattern, EFFECT_TYPE_STANDARD},
```

**Deliverable:** Working pattern implementation

### Phase 4: Integration

#### Step 1: Add to Effect Registry
- Register in `src/main.cpp` effects array
- Add to appropriate category section
- Update effect count

#### Step 2: Test Basic Functionality
- Verify pattern renders
- Check centre-origin compliance
- Validate dual-strip coordination

#### Step 3: Parameter Integration
- Map encoder controls to pattern parameters
- Test parameter ranges
- Verify smooth transitions

**Deliverable:** Integrated pattern ready for testing

### Phase 5: Optimization

#### Step 1: Performance Profiling
```cpp
void profilePattern() {
    uint32_t start = micros();
    render();
    uint32_t duration = micros() - start;
    
    Serial.printf("Render time: %d us (target: <8333 us for 120 FPS)\n", duration);
}
```

#### Step 2: Memory Analysis
```cpp
void checkMemory() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
}
```

#### Step 3: Optimize Hot Paths
- Replace float math with FastLED integer functions
- Use lookup tables for expensive calculations
- Minimize branches in render loop

**Deliverable:** Optimized pattern meeting performance targets

---

## Code Structure Templates

### Template 1: Simple LGP Effect

```cpp
// SimpleLGP.h
#ifndef SIMPLE_LGP_H
#define SIMPLE_LGP_H

#include <FastLED.h>
#include "../lightguide/LightGuideEffect.h"

class SimpleLGP : public LightGuideEffect {
private:
    static float phase;
    
public:
    SimpleLGP(CRGB* strip1, CRGB* strip2);
    void render() override;
};

#endif

// SimpleLGP.cpp
#include "SimpleLGP.h"

float SimpleLGP::phase = 0.0f;

SimpleLGP::SimpleLGP(CRGB* strip1, CRGB* strip2)
    : LightGuideEffect("Simple LGP", strip1, strip2) {
}

void SimpleLGP::render() {
    phase += paletteSpeed * 0.01f;
    
    for (int i = 0; i < 160; i++) {
        float position = (float)i / 160.0f;
        float wave = sin(position * TWO_PI * 3.0f + phase);
        uint8_t brightness = 128 + wave * 127 * visualParams.getIntensityNorm();
        
        uint8_t hue = gHue + i * 2;
        setEdge1LED(i, CHSV(hue, visualParams.saturation, brightness));
        setEdge2LED(i, CHSV(hue + 128, visualParams.saturation, brightness));
    }
    
    // Copy to unified buffer
    for (int i = 0; i < 160; i++) {
        leds[i] = strip1[i];
        leds[i + 160] = strip2[i];
    }
}
```

### Template 2: Physics-Based Effect

```cpp
// PhysicsLGP.h
#ifndef PHYSICS_LGP_H
#define PHYSICS_LGP_H

#include <FastLED.h>
#include "../lightguide/LightGuideEffect.h"

class PhysicsLGP : public LightGuideEffect {
private:
    struct WaveState {
        float amplitude[160];
        float velocity[160];
    };
    
    static WaveState waveState;
    static uint32_t lastUpdate;
    
    void updatePhysics(float deltaTime);
    void renderPhysics();
    
public:
    PhysicsLGP(CRGB* strip1, CRGB* strip2);
    void render() override;
};

#endif

// PhysicsLGP.cpp
#include "PhysicsLGP.h"

PhysicsLGP::WaveState PhysicsLGP::waveState = {{0}, {0}};
uint32_t PhysicsLGP::lastUpdate = 0;

PhysicsLGP::PhysicsLGP(CRGB* strip1, CRGB* strip2)
    : LightGuideEffect("Physics LGP", strip1, strip2) {
}

void PhysicsLGP::render() {
    uint32_t now = millis();
    float deltaTime = (now - lastUpdate) * 0.001f;
    lastUpdate = now;
    
    // Clamp deltaTime to prevent large jumps
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    updatePhysics(deltaTime);
    renderPhysics();
    
    // Copy to unified buffer
    for (int i = 0; i < 160; i++) {
        leds[i] = strip1[i];
        leds[i + 160] = strip2[i];
    }
}

void PhysicsLGP::updatePhysics(float deltaTime) {
    const float tension = 0.025f;
    const float damping = 0.999f;
    
    // Calculate forces
    for (int i = 1; i < 159; i++) {
        float force = (waveState.amplitude[i-1] + waveState.amplitude[i+1]) / 2.0f 
                     - waveState.amplitude[i];
        waveState.velocity[i] += force * tension;
    }
    
    // Update positions
    for (int i = 0; i < 160; i++) {
        waveState.velocity[i] *= damping;
        waveState.amplitude[i] += waveState.velocity[i];
    }
}

void PhysicsLGP::renderPhysics() {
    for (int i = 0; i < 160; i++) {
        float value = waveState.amplitude[i];
        uint8_t brightness = constrain(128 + value * 127, 0, 255);
        
        uint8_t hue = gHue + i * 2;
        setEdge1LED(i, CHSV(hue, visualParams.saturation, brightness));
        setEdge2LED(i, CHSV(hue + 128, visualParams.saturation, brightness));
    }
}
```

### Template 3: Audio-Reactive Effect

```cpp
// AudioReactiveLGP.h
#ifndef AUDIO_REACTIVE_LGP_H
#define AUDIO_REACTIVE_LGP_H

#include <FastLED.h>
#include "../lightguide/LightGuideEffect.h"
#include "../../core/AudioSystem.h"

class AudioReactiveLGP : public LightGuideEffect {
private:
    static float smoothedAmplitude;
    static bool lastBeatState;
    
    void updateAudioResponse(const AudioData& audio);
    void renderAudioReactive();
    
public:
    AudioReactiveLGP(CRGB* strip1, CRGB* strip2);
    void render() override;
};

#endif

// AudioReactiveLGP.cpp
#include "AudioReactiveLGP.h"

float AudioReactiveLGP::smoothedAmplitude = 0.0f;
bool AudioReactiveLGP::lastBeatState = false;

AudioReactiveLGP::AudioReactiveLGP(CRGB* strip1, CRGB* strip2)
    : LightGuideEffect("Audio Reactive LGP", strip1, strip2) {
}

void AudioReactiveLGP::render() {
    AudioData audio = AudioSystem::getInstance().getCurrentFrame();
    
    updateAudioResponse(audio);
    renderAudioReactive();
    
    // Copy to unified buffer
    for (int i = 0; i < 160; i++) {
        leds[i] = strip1[i];
        leds[i + 160] = strip2[i];
    }
}

void AudioReactiveLGP::updateAudioResponse(const AudioData& audio) {
    // Smooth amplitude
    float attack = 0.1f;
    float release = 0.05f;
    
    float target = audio.amplitude;
    if (target > smoothedAmplitude) {
        smoothedAmplitude += (target - smoothedAmplitude) * attack;
    } else {
        smoothedAmplitude += (target - smoothedAmplitude) * release;
    }
    
    // Detect beats
    if (audio.beat.detected && !lastBeatState) {
        // Trigger beat response
        triggerBeatResponse();
    }
    lastBeatState = audio.beat.detected;
}

void AudioReactiveLGP::renderAudioReactive() {
    float amplitude = smoothedAmplitude * visualParams.getIntensityNorm();
    
    for (int i = 0; i < 160; i++) {
        float position = (float)i / 160.0f;
        float pattern = sin(position * TWO_PI * 3.0f + millis() * 0.001f);
        uint8_t brightness = 128 + pattern * 127 * amplitude;
        
        uint8_t hue = gHue + audio.dominantFrequency * 255;
        setEdge1LED(i, CHSV(hue, visualParams.saturation, brightness));
        setEdge2LED(i, CHSV(hue + 128, visualParams.saturation, brightness));
    }
}
```

---

## Technical Specifications

### FastLED Optimization Patterns

#### Integer Trigonometry
```cpp
// BAD: Float trigonometry (slow)
float wave = sin(position * TWO_PI);

// GOOD: FastLED integer trigonometry (10x faster)
uint16_t position16 = position * 65535;
int16_t wave16 = sin16(position16);
uint8_t wave = wave16 >> 8;
```

#### Scaling Operations
```cpp
// BAD: Division (slow)
uint8_t scaled = (value * scale) / 255;

// GOOD: FastLED scale8 (20x faster)
uint8_t scaled = scale8(value, scale);
```

#### Beat Functions
```cpp
// BAD: Manual beat calculation
float beat = sin(millis() * 0.001f * bpm / 60.0f * TWO_PI);

// GOOD: FastLED beatsin8 (automatic phase management)
uint8_t beat = beatsin8(bpm, 0, 255);
```

### Memory Allocation Strategies

#### Static Allocation (Preferred)
```cpp
// GOOD: Static allocation
static float waveBuffer[160];
static uint8_t lookupTable[256];

void render() {
    // Use static buffers
    for (int i = 0; i < 160; i++) {
        waveBuffer[i] = calculateValue(i);
    }
}
```

#### Stack Allocation
```cpp
// GOOD: Stack allocation for temporary data
void render() {
    float tempBuffer[32];  // Small, temporary
    // Use tempBuffer
}  // Automatically freed
```

#### Avoid Dynamic Allocation
```cpp
// BAD: Dynamic allocation (causes fragmentation)
float* buffer = new float[160];
delete[] buffer;

// BAD: String operations (heap allocation)
String debug = "Value: " + String(value);
```

### Centre-Origin Compliance

#### Mandatory Constraint
```cpp
// ALL patterns MUST originate from centre LEDs 79/80
constexpr uint16_t CENTER_LED = 79;  // or 80

// Outward propagation
void renderOutward(float radius) {
    for (int i = 0; i < 160; i++) {
        float dist = abs(i - CENTER_LED);
        if (dist < radius) {
            float intensity = 1.0f - (dist / radius);
            renderLED(i, intensity);
        }
    }
}

// Inward contraction
void renderInward(float progress) {
    uint16_t edge1 = CENTER_LED - (progress * CENTER_LED);
    uint16_t edge2 = CENTER_LED + (progress * (160 - CENTER_LED));
    
    for (int i = edge1; i <= edge2; i++) {
        renderLED(i, 1.0f);
    }
}
```

### Dual-Strip Coordination

#### Complementary Patterns
```cpp
void renderComplementary() {
    for (int i = 0; i < 160; i++) {
        uint8_t hue = gHue + i * 2;
        
        // Strip 1: Standard pattern
        setEdge1LED(i, CHSV(hue, 255, brightness));
        
        // Strip 2: Complementary (inverted hue)
        setEdge2LED(i, CHSV(hue + 128, 255, brightness));
    }
}
```

#### Interference Mode
```cpp
void renderInterference() {
    for (int i = 0; i < 160; i++) {
        float interference = calculateInterference(i, 159 - i, phase);
        
        uint8_t brightness1 = brightness * interference;
        uint8_t brightness2 = brightness * (1.0f - interference);
        
        setEdge1LED(i, CHSV(hue, 255, brightness1));
        setEdge2LED(i, CHSV(hue + 128, 255, brightness2));
    }
}
```

---

## Safety Protocols

### Performance Constraints

#### Frame Rate Requirement
- **Target:** 120 FPS (8.33ms per frame)
- **Minimum:** 60 FPS (16.67ms per frame)
- **Measurement:**
```cpp
void checkFrameRate() {
    static uint32_t lastFrame = 0;
    uint32_t now = millis();
    uint32_t frameTime = now - lastFrame;
    lastFrame = now;
    
    if (frameTime > 16) {  // > 60 FPS
        Serial.printf("WARNING: Frame time %d ms (>16 ms target)\n", frameTime);
    }
}
```

#### CPU Usage Limits
- **Target:** <25% CPU usage
- **Maximum:** <50% CPU usage
- **Measurement:**
```cpp
void checkCPUUsage() {
    uint32_t renderStart = micros();
    render();
    uint32_t renderTime = micros() - renderStart;
    
    float cpuPercent = (renderTime / 8333.0f) * 100.0f;  // 120 FPS = 8333us budget
    
    if (cpuPercent > 50.0f) {
        Serial.printf("WARNING: CPU usage %.1f%% (>50%% limit)\n", cpuPercent);
    }
}
```

### Memory Limits

#### RAM Budget
- **Total Available:** 320KB
- **Pattern Budget:** <10KB per pattern
- **Measurement:**
```cpp
void checkMemory() {
    size_t freeHeap = ESP.getFreeHeap();
    size_t largestBlock = ESP.getMaxAllocHeap();
    
    Serial.printf("Free heap: %d bytes\n", freeHeap);
    Serial.printf("Largest block: %d bytes\n", largestBlock);
    
    if (freeHeap < 50000) {  // <50KB free
        Serial.println("WARNING: Low memory!");
    }
}
```

#### Heap Fragmentation Prevention
- Use static allocation whenever possible
- Avoid dynamic memory allocation
- Pre-allocate all buffers at compile time

### Power Consumption Guidelines

#### Brightness Limits
- **Maximum:** 255 (full brightness)
- **Recommended:** 128-200 for extended use
- **Low Power:** 64-128 for ambient effects

#### Thermal Management
- Monitor LED temperature
- Reduce brightness if overheating detected
- Implement thermal throttling

### Code Safety

#### Bounds Checking
```cpp
// ALWAYS check bounds before array access
void setLED(uint16_t index, CRGB color) {
    if (index < 160) {  // Bounds check
        leds[index] = color;
    }
}
```

#### Null Pointer Checks
```cpp
// Check pointers before use
if (strip1 != nullptr && strip2 != nullptr) {
    render();
}
```

#### Division by Zero Prevention
```cpp
// Prevent division by zero
float result = (divisor != 0.0f) ? (numerator / divisor) : 0.0f;
```

---

## Testing Procedures

### Visual Quality Validation

#### Test Checklist
- [ ] Pattern renders correctly on both strips
- [ ] Centre-origin compliance verified
- [ ] Dual-strip coordination correct
- [ ] Colour accuracy acceptable
- [ ] Motion smoothness adequate
- [ ] No visual artifacts or glitches

#### Visual Test Procedure
1. Display pattern for 60 seconds
2. Observe both strips simultaneously
3. Check for synchronization issues
4. Verify centre-origin behaviour
5. Test parameter adjustments
6. Document visual characteristics

### Performance Benchmarking

#### Performance Test Suite
```cpp
void performanceTest() {
    uint32_t totalTime = 0;
    uint32_t minTime = UINT32_MAX;
    uint32_t maxTime = 0;
    int frameCount = 0;
    
    for (int i = 0; i < 1000; i++) {
        uint32_t start = micros();
        render();
        uint32_t duration = micros() - start;
        
        totalTime += duration;
        minTime = min(minTime, duration);
        maxTime = max(maxTime, duration);
        frameCount++;
        
        delay(8);  // ~120 FPS
    }
    
    float avgTime = totalTime / (float)frameCount;
    Serial.printf("Average: %.2f us\n", avgTime);
    Serial.printf("Min: %d us\n", minTime);
    Serial.printf("Max: %d us\n", maxTime);
    Serial.printf("FPS: %.1f\n", 1000000.0f / avgTime);
}
```

#### Performance Targets
- **Average Frame Time:** <8333μs (120 FPS)
- **Maximum Frame Time:** <16667μs (60 FPS minimum)
- **CPU Usage:** <25% average

### Audio Reactivity Testing

#### Test Scenarios
1. **Silence:** Pattern should maintain ambient state
2. **Pure Tone:** Pattern should respond appropriately
3. **Music:** Pattern should follow musical structure
4. **Beats:** Pattern should synchronize with beats
5. **Transients:** Pattern should respond to sudden changes

#### Audio Test Procedure
```cpp
void audioReactivityTest() {
    // Test with various audio scenarios
    TestScenarios scenarios[] = {
        {"Silence", generateSilence()},
        {"Pure Sine", generateSineWave(440)},
        {"White Noise", generateWhiteNoise()},
        {"Bass Heavy", generateBassHeavy()},
        {"Complex Music", loadTestTrack()}
    };
    
    for (auto& scenario : scenarios) {
        audioSource.setSource(scenario.audio);
        runFrames(300);  // 5 seconds at 60fps
        
        auto metrics = analyzeResponse();
        assert(metrics.responseTime < 100, "Response too slow");
        assert(metrics.smoothness > 0.8, "Motion not smooth");
    }
}
```

### Edge Case Handling

#### Test Cases
- [ ] Zero amplitude audio
- [ ] Maximum amplitude audio
- [ ] Rapid parameter changes
- [ ] Effect switching during rendering
- [ ] Memory pressure conditions
- [ ] Extended runtime (1+ hours)

#### Edge Case Test Procedure
```cpp
void edgeCaseTest() {
    // Test zero amplitude
    setAmplitude(0.0f);
    render();
    verifyNoCrash();
    
    // Test maximum amplitude
    setAmplitude(1.0f);
    render();
    verifyNoOverflow();
    
    // Test rapid changes
    for (int i = 0; i < 100; i++) {
        setParameter(random(0, 255));
        render();
    }
    verifyStability();
}
```

---

## Performance Optimization

### Optimization Checklist

#### Level 1: Basic Optimizations
- [ ] Replace `sin()` with `sin16()`
- [ ] Replace divisions with `scale8()`
- [ ] Use `beatsin8()` for beat synchronization
- [ ] Eliminate unnecessary calculations
- [ ] Remove debug Serial.print() statements

#### Level 2: Advanced Optimizations
- [ ] Create lookup tables for expensive calculations
- [ ] Pre-calculate constant values
- [ ] Use fixed-point math where appropriate
- [ ] Minimize branches in hot paths
- [ ] Optimize memory access patterns

#### Level 3: Aggressive Optimizations
- [ ] Unroll small loops
- [ ] Use SIMD-style operations
- [ ] Cache frequently accessed values
- [ ] Reduce function call overhead
- [ ] Profile and optimize hot paths

### Optimization Examples

#### Lookup Table Creation
```cpp
// Create sine lookup table
static uint8_t sineLUT[256];

void initializeLUT() {
    for (int i = 0; i < 256; i++) {
        float angle = (i / 255.0f) * TWO_PI;
        sineLUT[i] = (sin(angle) + 1.0f) * 127.5f;
    }
}

// Use lookup table
uint8_t fastSin(uint8_t angle) {
    return sineLUT[angle];
}
```

#### Pre-calculation
```cpp
// Pre-calculate position step
static const float POSITION_STEP = 1.0f / 160.0f;

void render() {
    for (int i = 0; i < 160; i++) {
        float position = i * POSITION_STEP;  // No division
        // ...
    }
}
```

---

## Troubleshooting Guide

### Common Issues

#### Issue: Pattern Not Rendering
**Symptoms:** No visual output
**Causes:**
- Pattern not registered in effects array
- Buffer not copied to `leds[]`
- Brightness set to zero
**Solution:**
- Verify registration in `src/main.cpp`
- Check buffer copy to unified `leds[]` array
- Verify brightness values > 0

#### Issue: Low Frame Rate
**Symptoms:** Stuttering, <60 FPS
**Causes:**
- Expensive calculations in render loop
- Too many operations per frame
- Memory allocation during rendering
**Solution:**
- Profile and optimize hot paths
- Use FastLED integer math
- Pre-calculate expensive values
- Eliminate dynamic allocation

#### Issue: Memory Issues
**Symptoms:** Crashes, heap fragmentation
**Causes:**
- Dynamic memory allocation
- Large stack allocations
- Memory leaks
**Solution:**
- Use static allocation
- Reduce stack usage
- Check for memory leaks

#### Issue: Centre-Origin Violation
**Symptoms:** Pattern doesn't originate from centre
**Causes:**
- Missing centre-origin constraint
- Incorrect position calculation
**Solution:**
- Apply `applyCenterOriginConstraint()`
- Verify position calculations
- Test with centre-origin test pattern

---

## Conclusion

This playbook provides comprehensive guidance for developing Light Guide Plate patterns from concept to implementation. By following the workflow, using the templates, adhering to safety protocols, and thorough testing, developers can create high-quality patterns that meet performance targets and visual standards.

**Key Principles:**
1. **Static Allocation:** Avoid dynamic memory
2. **FastLED Optimization:** Use integer math functions
3. **Centre-Origin Compliance:** Mandatory for all patterns
4. **Performance First:** Maintain 120 FPS target
5. **Thorough Testing:** Validate visual quality and performance

---

*Document Version 1.0*  
*LightwaveOS LGP Pattern Development Playbook*  
*For developers creating LGP visual effects*

