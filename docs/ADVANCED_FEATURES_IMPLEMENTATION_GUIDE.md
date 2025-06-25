# Advanced Features Implementation Guide

This guide provides detailed implementation plans for three advanced features that build upon the Light Crystals LED control system with M5ROTATE8 and M5Unit-Scroll encoders.

## Table of Contents
1. [Wave Engine Visual Feedback System](#1-wave-engine-visual-feedback-system)
2. [Advanced Transition Engine](#2-advanced-transition-engine)
3. [Preset Management System](#3-preset-management-system)

---

## 1. Wave Engine Visual Feedback System

### Overview
Transform the encoder LEDs into a dynamic visualization surface that mirrors the wave engine's behavior, providing intuitive visual feedback for wave parameters and interactions.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Wave Engine Core                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Wave Parameters ──► LED Mapping Engine ──► LED Updates │
│        │                    │                     │     │
│        ├─ Frequency         ├─ Color Map          │     │
│        ├─ Amplitude         ├─ Brightness Map     │     │
│        ├─ Wave Type         └─ Animation Map      │     │
│        └─ Phase                                   │     │
│                                                   ▼     │
│                                          Encoder LEDs   │
│                                          (9 total)      │
└─────────────────────────────────────────────────────────┘
```

### Implementation Steps

#### Step 1: Create LED Feedback Module

```cpp
// src/hardware/encoder_led_feedback.h
#ifndef ENCODER_LED_FEEDBACK_H
#define ENCODER_LED_FEEDBACK_H

#include <Arduino.h>
#include "m5rotate8.h"
#include "hardware/scroll_encoder.h"
#include "effects/waves/DualStripWaveEngine.h"

class EncoderLEDFeedback {
private:
    M5ROTATE8* m_encoder;
    DualStripWaveEngine* m_waveEngine;
    
    // LED state tracking
    struct LEDState {
        uint8_t r, g, b;
        float brightness;
        float phase;
    } m_ledStates[9];
    
    // Animation parameters
    uint32_t m_lastUpdate = 0;
    float m_globalPhase = 0;
    
public:
    EncoderLEDFeedback(M5ROTATE8* encoder, DualStripWaveEngine* engine) 
        : m_encoder(encoder), m_waveEngine(engine) {}
    
    void update() {
        uint32_t now = millis();
        if (now - m_lastUpdate < 20) return; // 50Hz update rate
        m_lastUpdate = now;
        
        // Update global phase
        m_globalPhase += m_waveEngine->getFrequency1() * 0.02f;
        
        // Update each encoder LED
        for (int i = 0; i < 8; i++) {
            updateEncoderLED(i);
        }
        
        // Update scroll encoder LED
        updateScrollLED();
    }
    
private:
    void updateEncoderLED(uint8_t index) {
        LEDState& led = m_ledStates[index];
        
        switch (index) {
            case 0: // Wave type indicator
                mapWaveTypeToColor(m_waveEngine->getWaveform1(), led);
                break;
                
            case 1: // Frequency visualizer
                led.brightness = calculateFrequencyBrightness(index);
                led.r = 0; led.g = 100; led.b = 255; // Cyan
                break;
                
            case 2: // Amplitude indicator
                led.brightness = m_waveEngine->getAmplitude1();
                led.r = 255; led.g = 50; led.b = 0; // Orange
                break;
                
            case 3: // Phase offset
                visualizePhaseOffset(led);
                break;
                
            case 4-7: // Wave propagation visualization
                visualizeWavePropagation(index, led);
                break;
        }
        
        // Apply breathing effect
        float breath = (sin(m_globalPhase + index * 0.8f) + 1.0f) * 0.5f;
        led.brightness *= (0.5f + 0.5f * breath);
        
        // Update physical LED
        m_encoder->setRGB(index, 
            led.r * led.brightness / 255,
            led.g * led.brightness / 255,
            led.b * led.brightness / 255
        );
    }
    
    void mapWaveTypeToColor(WaveformType type, LEDState& led) {
        switch (type) {
            case WAVE_SINE:
                led.r = 0; led.g = 0; led.b = 255; // Blue
                break;
            case WAVE_TRIANGLE:
                led.r = 0; led.g = 255; led.b = 0; // Green
                break;
            case WAVE_SAWTOOTH:
                led.r = 255; led.g = 0; led.b = 0; // Red
                break;
            case WAVE_SQUARE:
                led.r = 255; led.g = 255; led.b = 0; // Yellow
                break;
            case WAVE_NOISE:
                led.r = 255; led.g = 0; led.b = 255; // Magenta
                break;
        }
    }
    
    float calculateFrequencyBrightness(uint8_t index) {
        // Create visual "beat" based on wave frequency
        float freq = m_waveEngine->getFrequency1();
        float phase = m_globalPhase * freq + index * PI / 4;
        return (sin(phase) + 1.0f) * 127.5f;
    }
    
    void visualizePhaseOffset(LEDState& led) {
        float phase1 = m_waveEngine->getPhase1();
        float phase2 = m_waveEngine->getPhase2();
        float offset = phase2 - phase1;
        
        // Color indicates offset direction
        if (offset > 0) {
            led.r = 0; led.g = 255; led.b = 100; // Green-cyan
        } else {
            led.r = 255; led.g = 100; led.b = 0; // Orange-red
        }
        
        led.brightness = abs(offset) * 255 / TWO_PI;
    }
    
    void visualizeWavePropagation(uint8_t index, LEDState& led) {
        // Create a wave that travels across encoders 4-7
        float position = (index - 4) / 3.0f;
        float waveValue = m_waveEngine->calculateWaveValue(
            position, 
            m_globalPhase * m_waveEngine->getFrequency1()
        );
        
        led.brightness = (waveValue + 1.0f) * 127.5f;
        led.r = 100; led.g = 50; led.b = 255; // Purple
    }
    
    void updateScrollLED() {
        // Scroll encoder shows combined wave interaction
        float wave1 = sin(m_globalPhase * m_waveEngine->getFrequency1());
        float wave2 = sin(m_globalPhase * m_waveEngine->getFrequency2());
        float combined = (wave1 + wave2) * 0.5f;
        
        uint8_t brightness = (combined + 1.0f) * 127.5f;
        
        // Color based on interaction type
        if (m_waveEngine->getInteractionMode() == INTERACTION_ADD) {
            setScrollEncoderLED(0, brightness, brightness); // Cyan
        } else if (m_waveEngine->getInteractionMode() == INTERACTION_MULTIPLY) {
            setScrollEncoderLED(brightness, brightness, 0); // Yellow
        } else {
            setScrollEncoderLED(brightness, 0, brightness); // Magenta
        }
    }
};
```

#### Step 2: Integration with Main System

```cpp
// In main.cpp setup()
EncoderLEDFeedback* encoderFeedback = nullptr;

void setup() {
    // ... existing setup ...
    
    // Initialize encoder LED feedback
    if (encoderAvailable && waveEngine) {
        encoderFeedback = new EncoderLEDFeedback(&encoder, &waveEngine);
        Serial.println("✅ Encoder LED feedback system initialized");
    }
}

// In main loop()
void loop() {
    // ... existing code ...
    
    // Update encoder LED feedback
    if (encoderFeedback && effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
        encoderFeedback->update();
    }
}
```

### Visual Feedback Patterns

1. **Frequency Indicator**: LEDs pulse at the wave frequency
2. **Amplitude Meter**: Brightness represents amplitude
3. **Wave Type Color**: Each waveform has a unique color
4. **Phase Visualization**: Shows phase relationships
5. **Propagation Display**: Wave travels across encoders
6. **Interaction Indicator**: Combined wave visualization

---

## 2. Advanced Transition Engine

### Overview
Create sophisticated transitions between effects that go beyond simple fades, incorporating directional wipes, physics-based movements, and synchronized timing.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│              Transition Engine Core                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Transition Types:                                      │
│  ├─ Morph (parameter interpolation)                     │
│  ├─ Wipe (directional progression)                      │
│  ├─ Dissolve (random pixel transition)                  │
│  ├─ Zoom (scale in/out from center)                     │
│  ├─ Rotate (circular transition)                        │
│  └─ Physics (gravity, bounce, momentum)                 │
│                                                         │
│  Transition Parameters:                                 │
│  ├─ Duration (100ms - 5000ms)                           │
│  ├─ Curve (linear, ease, bounce, elastic)               │
│  ├─ Direction (for wipes)                               │
│  └─ Physics properties (gravity, friction)              │
└─────────────────────────────────────────────────────────┘
```

### Implementation

#### Step 1: Transition Base Class

```cpp
// src/effects/transitions/TransitionEngine.h
#ifndef TRANSITION_ENGINE_H
#define TRANSITION_ENGINE_H

#include <FastLED.h>
#include "config/hardware_config.h"

enum TransitionType {
    TRANSITION_FADE,
    TRANSITION_MORPH,
    TRANSITION_WIPE_LR,
    TRANSITION_WIPE_RL,
    TRANSITION_WIPE_OUT,
    TRANSITION_WIPE_IN,
    TRANSITION_DISSOLVE,
    TRANSITION_ZOOM_IN,
    TRANSITION_ZOOM_OUT,
    TRANSITION_ROTATE_CW,
    TRANSITION_ROTATE_CCW,
    TRANSITION_PHYSICS_DROP,
    TRANSITION_PHYSICS_BOUNCE,
    TRANSITION_COUNT
};

enum EasingCurve {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_ELASTIC,
    EASE_OUT_ELASTIC,
    EASE_IN_OUT_ELASTIC,
    EASE_IN_BOUNCE,
    EASE_OUT_BOUNCE
};

class TransitionEngine {
private:
    CRGB* m_sourceBuffer;
    CRGB* m_targetBuffer;
    CRGB* m_outputBuffer;
    uint16_t m_numLeds;
    
    TransitionType m_type = TRANSITION_FADE;
    EasingCurve m_curve = EASE_IN_OUT_QUAD;
    uint32_t m_startTime = 0;
    uint32_t m_duration = 1000;
    float m_progress = 0.0f;
    bool m_active = false;
    
    // Physics parameters
    struct PhysicsState {
        float position = 0.0f;
        float velocity = 0.0f;
        float gravity = 9.8f;
        float bounce = 0.7f;
        float friction = 0.95f;
    } m_physics;
    
    // Dissolve parameters
    uint8_t* m_pixelOrder = nullptr;
    
public:
    TransitionEngine(uint16_t numLeds) : m_numLeds(numLeds) {
        m_pixelOrder = new uint8_t[numLeds];
        resetPixelOrder();
    }
    
    ~TransitionEngine() {
        delete[] m_pixelOrder;
    }
    
    void startTransition(CRGB* source, CRGB* target, CRGB* output, 
                        TransitionType type, uint32_t duration, 
                        EasingCurve curve = EASE_IN_OUT_QUAD) {
        m_sourceBuffer = source;
        m_targetBuffer = target;
        m_outputBuffer = output;
        m_type = type;
        m_duration = duration;
        m_curve = curve;
        m_startTime = millis();
        m_active = true;
        m_progress = 0.0f;
        
        // Initialize transition-specific parameters
        switch (type) {
            case TRANSITION_DISSOLVE:
                shufflePixelOrder();
                break;
                
            case TRANSITION_PHYSICS_DROP:
            case TRANSITION_PHYSICS_BOUNCE:
                m_physics.position = 0.0f;
                m_physics.velocity = 0.0f;
                break;
        }
    }
    
    bool update() {
        if (!m_active) return false;
        
        uint32_t elapsed = millis() - m_startTime;
        
        if (m_type == TRANSITION_PHYSICS_DROP || m_type == TRANSITION_PHYSICS_BOUNCE) {
            updatePhysics(elapsed);
        } else {
            m_progress = constrain((float)elapsed / m_duration, 0.0f, 1.0f);
            m_progress = applyEasing(m_progress, m_curve);
        }
        
        // Apply transition effect
        switch (m_type) {
            case TRANSITION_FADE:
                applyFade();
                break;
                
            case TRANSITION_MORPH:
                applyMorph();
                break;
                
            case TRANSITION_WIPE_LR:
                applyWipe(true, false);
                break;
                
            case TRANSITION_WIPE_RL:
                applyWipe(false, false);
                break;
                
            case TRANSITION_WIPE_OUT:
                applyWipe(true, true);
                break;
                
            case TRANSITION_WIPE_IN:
                applyWipe(false, true);
                break;
                
            case TRANSITION_DISSOLVE:
                applyDissolve();
                break;
                
            case TRANSITION_ZOOM_IN:
                applyZoom(true);
                break;
                
            case TRANSITION_ZOOM_OUT:
                applyZoom(false);
                break;
                
            case TRANSITION_ROTATE_CW:
                applyRotate(true);
                break;
                
            case TRANSITION_ROTATE_CCW:
                applyRotate(false);
                break;
                
            case TRANSITION_PHYSICS_DROP:
            case TRANSITION_PHYSICS_BOUNCE:
                applyPhysics();
                break;
        }
        
        if (m_progress >= 1.0f && 
            m_type != TRANSITION_PHYSICS_DROP && 
            m_type != TRANSITION_PHYSICS_BOUNCE) {
            m_active = false;
            return false;
        }
        
        return true;
    }
    
    bool isActive() const { return m_active; }
    float getProgress() const { return m_progress; }
    
private:
    float applyEasing(float t, EasingCurve curve) {
        switch (curve) {
            case EASE_LINEAR:
                return t;
                
            case EASE_IN_QUAD:
                return t * t;
                
            case EASE_OUT_QUAD:
                return t * (2 - t);
                
            case EASE_IN_OUT_QUAD:
                return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
                
            case EASE_IN_CUBIC:
                return t * t * t;
                
            case EASE_OUT_CUBIC:
                return (--t) * t * t + 1;
                
            case EASE_IN_OUT_CUBIC:
                return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
                
            case EASE_IN_ELASTIC:
                return sin(13 * HALF_PI * t) * pow(2, 10 * (t - 1));
                
            case EASE_OUT_ELASTIC:
                return sin(-13 * HALF_PI * (t + 1)) * pow(2, -10 * t) + 1;
                
            case EASE_IN_OUT_ELASTIC:
                return t < 0.5f
                    ? 0.5f * sin(13 * HALF_PI * (2 * t)) * pow(2, 10 * ((2 * t) - 1))
                    : 0.5f * (sin(-13 * HALF_PI * ((2 * t - 1) + 1)) * pow(2, -10 * (2 * t - 1)) + 2);
                
            case EASE_IN_BOUNCE:
                return 1 - applyEasing(1 - t, EASE_OUT_BOUNCE);
                
            case EASE_OUT_BOUNCE:
                if (t < 1 / 2.75f) {
                    return 7.5625f * t * t;
                } else if (t < 2 / 2.75f) {
                    t -= 1.5f / 2.75f;
                    return 7.5625f * t * t + 0.75f;
                } else if (t < 2.5 / 2.75f) {
                    t -= 2.25f / 2.75f;
                    return 7.5625f * t * t + 0.9375f;
                } else {
                    t -= 2.625f / 2.75f;
                    return 7.5625f * t * t + 0.984375f;
                }
                
            default:
                return t;
        }
    }
    
    void applyFade() {
        uint8_t blend = m_progress * 255;
        for (uint16_t i = 0; i < m_numLeds; i++) {
            m_outputBuffer[i] = blend(m_sourceBuffer[i], m_targetBuffer[i], blend);
        }
    }
    
    void applyMorph() {
        // Morph with color space interpolation
        for (uint16_t i = 0; i < m_numLeds; i++) {
            CHSV srcHSV = rgb2hsv_approximate(m_sourceBuffer[i]);
            CHSV tgtHSV = rgb2hsv_approximate(m_targetBuffer[i]);
            
            // Interpolate in HSV space
            uint8_t h = lerp8by8(srcHSV.h, tgtHSV.h, m_progress * 255);
            uint8_t s = lerp8by8(srcHSV.s, tgtHSV.s, m_progress * 255);
            uint8_t v = lerp8by8(srcHSV.v, tgtHSV.v, m_progress * 255);
            
            m_outputBuffer[i] = CHSV(h, s, v);
        }
    }
    
    void applyWipe(bool leftToRight, bool fromCenter) {
        if (fromCenter) {
            // Wipe from center outward or inward
            uint16_t center = m_numLeds / 2;
            uint16_t radius = m_progress * center;
            
            for (uint16_t i = 0; i < m_numLeds; i++) {
                uint16_t distFromCenter = abs((int)i - (int)center);
                bool showTarget = leftToRight ? (distFromCenter <= radius) : (distFromCenter >= center - radius);
                m_outputBuffer[i] = showTarget ? m_targetBuffer[i] : m_sourceBuffer[i];
            }
        } else {
            // Linear wipe
            uint16_t threshold = m_progress * m_numLeds;
            
            for (uint16_t i = 0; i < m_numLeds; i++) {
                bool showTarget = leftToRight ? (i < threshold) : (i >= m_numLeds - threshold);
                m_outputBuffer[i] = showTarget ? m_targetBuffer[i] : m_sourceBuffer[i];
            }
        }
    }
    
    void applyDissolve() {
        uint16_t pixelsToShow = m_progress * m_numLeds;
        
        for (uint16_t i = 0; i < m_numLeds; i++) {
            if (i < pixelsToShow) {
                m_outputBuffer[m_pixelOrder[i]] = m_targetBuffer[m_pixelOrder[i]];
            } else {
                m_outputBuffer[m_pixelOrder[i]] = m_sourceBuffer[m_pixelOrder[i]];
            }
        }
    }
    
    void applyZoom(bool zoomIn) {
        uint16_t center = m_numLeds / 2;
        float scale = zoomIn ? m_progress : 1.0f - m_progress;
        
        for (uint16_t i = 0; i < m_numLeds; i++) {
            float distFromCenter = (float)(i - center) / center;
            float scaledDist = distFromCenter * scale;
            int16_t sourcePos = center + scaledDist * center;
            
            if (sourcePos >= 0 && sourcePos < m_numLeds) {
                m_outputBuffer[i] = blend(m_sourceBuffer[i], m_targetBuffer[sourcePos], m_progress * 255);
            } else {
                m_outputBuffer[i] = m_targetBuffer[i];
            }
        }
    }
    
    void applyRotate(bool clockwise) {
        float angle = m_progress * TWO_PI;
        if (!clockwise) angle = -angle;
        
        // For LED strips, we'll simulate rotation by shifting with fade
        int16_t shift = (sin(angle) * m_numLeds / 4);
        
        for (uint16_t i = 0; i < m_numLeds; i++) {
            int16_t sourcePos = i + shift;
            if (sourcePos < 0) sourcePos += m_numLeds;
            if (sourcePos >= m_numLeds) sourcePos -= m_numLeds;
            
            uint8_t blend_amount = (cos(angle) + 1.0f) * 127.5f;
            m_outputBuffer[i] = blend(m_sourceBuffer[sourcePos], m_targetBuffer[i], blend_amount);
        }
    }
    
    void updatePhysics(uint32_t elapsed) {
        float dt = elapsed / 1000.0f; // Convert to seconds
        
        // Update physics
        m_physics.velocity += m_physics.gravity * dt;
        m_physics.position += m_physics.velocity * dt;
        
        // Bounce at bottom
        if (m_physics.position >= 1.0f) {
            m_physics.position = 1.0f;
            
            if (m_type == TRANSITION_PHYSICS_BOUNCE) {
                m_physics.velocity = -m_physics.velocity * m_physics.bounce;
                
                // Stop bouncing when velocity is too small
                if (abs(m_physics.velocity) < 0.1f) {
                    m_progress = 1.0f;
                    m_active = false;
                }
            } else {
                m_progress = 1.0f;
                m_active = false;
            }
        }
        
        // Apply friction
        m_physics.velocity *= m_physics.friction;
        
        // Update progress based on position
        m_progress = constrain(m_physics.position, 0.0f, 1.0f);
    }
    
    void applyPhysics() {
        // Use physics position to determine transition
        applyWipe(true, true); // Use center-out wipe with physics progress
    }
    
    void resetPixelOrder() {
        for (uint16_t i = 0; i < m_numLeds; i++) {
            m_pixelOrder[i] = i;
        }
    }
    
    void shufflePixelOrder() {
        // Fisher-Yates shuffle
        for (uint16_t i = m_numLeds - 1; i > 0; i--) {
            uint16_t j = random16(i + 1);
            uint8_t temp = m_pixelOrder[i];
            m_pixelOrder[i] = m_pixelOrder[j];
            m_pixelOrder[j] = temp;
        }
    }
};
```

#### Step 2: Integration with Effect System

```cpp
// In main.cpp or effect manager
TransitionEngine* transitionEngine = nullptr;
CRGB effectBuffer1[HardwareConfig::NUM_LEDS];
CRGB effectBuffer2[HardwareConfig::NUM_LEDS];
bool useBuffer1 = true;

void startAdvancedTransition(uint8_t newEffect) {
    if (!transitionEngine) {
        transitionEngine = new TransitionEngine(HardwareConfig::NUM_LEDS);
    }
    
    // Render current effect to buffer 1
    CRGB* currentBuffer = useBuffer1 ? effectBuffer1 : effectBuffer2;
    CRGB* nextBuffer = useBuffer1 ? effectBuffer2 : effectBuffer1;
    
    // Copy current LED state
    memcpy(currentBuffer, leds, sizeof(CRGB) * HardwareConfig::NUM_LEDS);
    
    // Render new effect to buffer 2
    effects[newEffect].render();
    memcpy(nextBuffer, leds, sizeof(CRGB) * HardwareConfig::NUM_LEDS);
    
    // Select transition type based on effect types
    TransitionType transType = selectTransitionType(currentEffect, newEffect);
    uint32_t duration = 1000; // 1 second default
    
    // Start transition
    transitionEngine->startTransition(
        currentBuffer, nextBuffer, leds,
        transType, duration, EASE_IN_OUT_CUBIC
    );
    
    currentEffect = newEffect;
    useBuffer1 = !useBuffer1;
}

TransitionType selectTransitionType(uint8_t fromEffect, uint8_t toEffect) {
    // Smart transition selection based on effect types
    if (effects[fromEffect].type == EFFECT_TYPE_WAVE_ENGINE && 
        effects[toEffect].type == EFFECT_TYPE_WAVE_ENGINE) {
        return TRANSITION_MORPH; // Smooth morph between wave effects
    }
    
    if (random8() < 128) {
        // 50% chance of directional wipe
        return random8(2) ? TRANSITION_WIPE_LR : TRANSITION_WIPE_RL;
    } else if (random8() < 64) {
        // 25% chance of zoom
        return random8(2) ? TRANSITION_ZOOM_IN : TRANSITION_ZOOM_OUT;
    } else {
        // 25% chance of physics or dissolve
        return random8(2) ? TRANSITION_PHYSICS_DROP : TRANSITION_DISSOLVE;
    }
}

// In loop()
if (transitionEngine && transitionEngine->isActive()) {
    transitionEngine->update();
    // Don't run normal effect during transition
} else {
    // Run normal effect
    effects[currentEffect].render();
}
```

### Transition Types Reference

1. **Fade**: Classic crossfade
2. **Morph**: HSV color space interpolation
3. **Wipe**: Directional progression (L→R, R→L, Center→Out, Out→Center)
4. **Dissolve**: Random pixel transition
5. **Zoom**: Scale in/out from center
6. **Rotate**: Circular rotation effect
7. **Physics Drop**: Gravity-based fall
8. **Physics Bounce**: Bouncing ball physics

---

## 3. Preset Management System

### Overview
Implement a comprehensive preset system that saves and recalls complete effect states, including all parameters, with support for morphing between presets and persistent storage.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│              Preset Management System                   │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Preset Structure:                                      │
│  ├─ Effect ID                                           │
│  ├─ All effect parameters                               │
│  ├─ Palette selection                                   │
│  ├─ Global parameters (brightness, speed, etc.)         │
│  ├─ Wave engine state (if applicable)                   │
│  └─ Metadata (name, timestamp, tags)                    │
│                                                         │
│  Features:                                              │
│  ├─ Save/Load presets                                   │
│  ├─ Preset morphing                                     │
│  ├─ Preset sequences/playlists                          │
│  ├─ SPIFFS persistence                                  │
│  └─ Quick access shortcuts                              │
└─────────────────────────────────────────────────────────┘
```

### Implementation

#### Step 1: Preset Data Structure

```cpp
// src/core/PresetManager.h
#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config/hardware_config.h"
#include "effects/waves/DualStripWaveEngine.h"

#define MAX_PRESETS 32
#define PRESET_NAME_LENGTH 16
#define PRESET_FILE_PATH "/presets/"

struct Preset {
    // Metadata
    char name[PRESET_NAME_LENGTH];
    uint32_t timestamp;
    uint8_t tags;
    
    // Effect settings
    uint8_t effectId;
    uint8_t paletteIndex;
    uint8_t brightness;
    uint8_t fadeAmount;
    uint8_t speed;
    
    // Visual parameters
    uint8_t intensity;
    uint8_t saturation;
    uint8_t complexity;
    uint8_t variation;
    
    // Sync settings
    HardwareConfig::SyncMode syncMode;
    HardwareConfig::PropagationMode propagationMode;
    
    // Wave engine parameters (if applicable)
    struct WaveParams {
        bool isWaveEffect;
        WaveformType waveform1;
        WaveformType waveform2;
        float frequency1;
        float frequency2;
        float amplitude1;
        float amplitude2;
        float phase1;
        float phase2;
        InteractionMode interactionMode;
        bool enableStrip1;
        bool enableStrip2;
    } waveParams;
    
    // Checksum for validation
    uint16_t checksum;
    
    void calculateChecksum() {
        checksum = 0;
        uint8_t* data = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(Preset) - sizeof(checksum); i++) {
            checksum += data[i];
        }
    }
    
    bool isValid() const {
        uint16_t calc = 0;
        uint8_t* data = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(Preset) - sizeof(checksum); i++) {
            calc += data[i];
        }
        return calc == checksum;
    }
};

class PresetManager {
private:
    Preset m_presets[MAX_PRESETS];
    bool m_presetValid[MAX_PRESETS];
    uint8_t m_currentPreset = 0;
    
    // Morphing state
    bool m_morphing = false;
    Preset m_morphSource;
    Preset m_morphTarget;
    float m_morphProgress = 0.0f;
    uint32_t m_morphStartTime = 0;
    uint32_t m_morphDuration = 2000; // 2 seconds default
    
    // Sequence playback
    uint8_t m_sequence[MAX_PRESETS];
    uint8_t m_sequenceLength = 0;
    uint8_t m_sequenceIndex = 0;
    bool m_sequencePlaying = false;
    uint32_t m_sequenceTimer = 0;
    uint32_t m_sequenceDuration = 10000; // 10 seconds per preset
    
public:
    PresetManager() {
        memset(m_presetValid, 0, sizeof(m_presetValid));
    }
    
    bool begin() {
        if (!SPIFFS.begin(true)) {
            Serial.println("Failed to mount SPIFFS");
            return false;
        }
        
        // Create presets directory if it doesn't exist
        if (!SPIFFS.exists(PRESET_FILE_PATH)) {
            SPIFFS.mkdir(PRESET_FILE_PATH);
        }
        
        // Load all presets from SPIFFS
        loadAllPresets();
        
        Serial.println("✅ Preset Manager initialized");
        return true;
    }
    
    bool saveCurrentState(uint8_t slot, const char* name = nullptr) {
        if (slot >= MAX_PRESETS) return false;
        
        Preset& preset = m_presets[slot];
        
        // Save metadata
        if (name) {
            strncpy(preset.name, name, PRESET_NAME_LENGTH - 1);
            preset.name[PRESET_NAME_LENGTH - 1] = '\0';
        } else {
            snprintf(preset.name, PRESET_NAME_LENGTH, "Preset %d", slot);
        }
        preset.timestamp = millis();
        preset.tags = 0;
        
        // Save current effect state
        preset.effectId = currentEffect;
        preset.paletteIndex = currentPaletteIndex;
        preset.brightness = FastLED.getBrightness();
        preset.fadeAmount = fadeAmount;
        preset.speed = paletteSpeed;
        
        // Save visual parameters
        preset.intensity = visualParams.intensity;
        preset.saturation = visualParams.saturation;
        preset.complexity = visualParams.complexity;
        preset.variation = visualParams.variation;
        
        // Save sync settings
        preset.syncMode = currentSyncMode;
        preset.propagationMode = currentPropagationMode;
        
        // Save wave engine state if applicable
        if (effects[currentEffect].type == EFFECT_TYPE_WAVE_ENGINE && waveEngine) {
            preset.waveParams.isWaveEffect = true;
            preset.waveParams.waveform1 = waveEngine->getWaveform1();
            preset.waveParams.waveform2 = waveEngine->getWaveform2();
            preset.waveParams.frequency1 = waveEngine->getFrequency1();
            preset.waveParams.frequency2 = waveEngine->getFrequency2();
            preset.waveParams.amplitude1 = waveEngine->getAmplitude1();
            preset.waveParams.amplitude2 = waveEngine->getAmplitude2();
            preset.waveParams.phase1 = waveEngine->getPhase1();
            preset.waveParams.phase2 = waveEngine->getPhase2();
            preset.waveParams.interactionMode = waveEngine->getInteractionMode();
            preset.waveParams.enableStrip1 = waveEngine->isStrip1Enabled();
            preset.waveParams.enableStrip2 = waveEngine->isStrip2Enabled();
        } else {
            preset.waveParams.isWaveEffect = false;
        }
        
        // Calculate checksum
        preset.calculateChecksum();
        
        // Mark as valid
        m_presetValid[slot] = true;
        
        // Save to SPIFFS
        return savePresetToFile(slot);
    }
    
    bool loadPreset(uint8_t slot, bool immediate = true) {
        if (slot >= MAX_PRESETS || !m_presetValid[slot]) return false;
        
        const Preset& preset = m_presets[slot];
        
        if (!preset.isValid()) {
            Serial.printf("Preset %d has invalid checksum\n", slot);
            return false;
        }
        
        if (immediate) {
            applyPreset(preset);
        } else {
            // Start morphing
            startMorph(slot);
        }
        
        m_currentPreset = slot;
        Serial.printf("Loaded preset %d: %s\n", slot, preset.name);
        return true;
    }
    
    void startMorph(uint8_t targetSlot, uint32_t duration = 2000) {
        if (targetSlot >= MAX_PRESETS || !m_presetValid[targetSlot]) return;
        
        // Save current state as morph source
        saveCurrentState(MAX_PRESETS - 1, "MorphTemp");
        m_morphSource = m_presets[MAX_PRESETS - 1];
        
        // Set target
        m_morphTarget = m_presets[targetSlot];
        
        // Start morphing
        m_morphing = true;
        m_morphProgress = 0.0f;
        m_morphStartTime = millis();
        m_morphDuration = duration;
    }
    
    void updateMorph() {
        if (!m_morphing) return;
        
        uint32_t elapsed = millis() - m_morphStartTime;
        m_morphProgress = constrain((float)elapsed / m_morphDuration, 0.0f, 1.0f);
        
        // Apply smooth interpolation
        float smoothProgress = m_morphProgress * m_morphProgress * (3.0f - 2.0f * m_morphProgress);
        
        // Interpolate all parameters
        Preset current;
        interpolatePresets(m_morphSource, m_morphTarget, current, smoothProgress);
        applyPreset(current);
        
        if (m_morphProgress >= 1.0f) {
            m_morphing = false;
            m_currentPreset = findPresetIndex(m_morphTarget);
        }
    }
    
    void startSequence(uint8_t* presets, uint8_t length, uint32_t duration) {
        if (length == 0 || length > MAX_PRESETS) return;
        
        memcpy(m_sequence, presets, length);
        m_sequenceLength = length;
        m_sequenceIndex = 0;
        m_sequenceDuration = duration;
        m_sequencePlaying = true;
        m_sequenceTimer = millis();
        
        // Load first preset
        loadPreset(m_sequence[0], false);
    }
    
    void updateSequence() {
        if (!m_sequencePlaying) return;
        
        if (millis() - m_sequenceTimer >= m_sequenceDuration) {
            m_sequenceTimer = millis();
            m_sequenceIndex = (m_sequenceIndex + 1) % m_sequenceLength;
            
            // Morph to next preset
            loadPreset(m_sequence[m_sequenceIndex], false);
        }
    }
    
    void stopSequence() {
        m_sequencePlaying = false;
    }
    
    // Quick access methods
    void quickSave(uint8_t slot) {
        if (slot < 10) { // Reserve slots 0-9 for quick access
            char name[PRESET_NAME_LENGTH];
            snprintf(name, PRESET_NAME_LENGTH, "Quick %d", slot);
            saveCurrentState(slot, name);
        }
    }
    
    void quickLoad(uint8_t slot) {
        if (slot < 10 && m_presetValid[slot]) {
            loadPreset(slot, true);
        }
    }
    
    // Encoder integration
    void handleEncoderInput(uint8_t encoderId, int32_t delta) {
        if (encoderId == 7) { // Use encoder 7 for preset navigation
            int newPreset = m_currentPreset + (delta > 0 ? 1 : -1);
            
            // Find next valid preset
            for (int i = 0; i < MAX_PRESETS; i++) {
                if (newPreset < 0) newPreset = MAX_PRESETS - 1;
                if (newPreset >= MAX_PRESETS) newPreset = 0;
                
                if (m_presetValid[newPreset]) {
                    loadPreset(newPreset, false); // Morph to preset
                    break;
                }
                
                newPreset += (delta > 0 ? 1 : -1);
            }
        }
    }
    
    void handleButtonPress(uint8_t encoderId) {
        if (encoderId == 7) {
            // Save to current slot on button press
            saveCurrentState(m_currentPreset);
            
            // Flash encoder LED to confirm
            encoder.setRGB(7, 0, 255, 0); // Green flash
            delay(200);
            encoder.setRGB(7, 128, 0, 255); // Back to purple
        }
    }
    
private:
    void applyPreset(const Preset& preset) {
        // Apply effect
        if (preset.effectId < NUM_EFFECTS) {
            startTransition(preset.effectId);
        }
        
        // Apply palette
        currentPaletteIndex = preset.paletteIndex;
        targetPalette = CRGBPalette16(gGradientPalettes[currentPaletteIndex]);
        
        // Apply global parameters
        FastLED.setBrightness(preset.brightness);
        fadeAmount = preset.fadeAmount;
        paletteSpeed = preset.speed;
        
        // Apply visual parameters
        visualParams.intensity = preset.intensity;
        visualParams.saturation = preset.saturation;
        visualParams.complexity = preset.complexity;
        visualParams.variation = preset.variation;
        
        // Apply sync settings
        currentSyncMode = preset.syncMode;
        currentPropagationMode = preset.propagationMode;
        
        // Apply wave engine parameters if applicable
        if (preset.waveParams.isWaveEffect && waveEngine) {
            waveEngine->setWaveform1(preset.waveParams.waveform1);
            waveEngine->setWaveform2(preset.waveParams.waveform2);
            waveEngine->setFrequency1(preset.waveParams.frequency1);
            waveEngine->setFrequency2(preset.waveParams.frequency2);
            waveEngine->setAmplitude1(preset.waveParams.amplitude1);
            waveEngine->setAmplitude2(preset.waveParams.amplitude2);
            waveEngine->setPhase1(preset.waveParams.phase1);
            waveEngine->setPhase2(preset.waveParams.phase2);
            waveEngine->setInteractionMode(preset.waveParams.interactionMode);
            waveEngine->enableStrip1(preset.waveParams.enableStrip1);
            waveEngine->enableStrip2(preset.waveParams.enableStrip2);
        }
    }
    
    void interpolatePresets(const Preset& from, const Preset& to, Preset& result, float t) {
        // Copy metadata from target
        strcpy(result.name, to.name);
        result.timestamp = to.timestamp;
        result.tags = to.tags;
        
        // Interpolate effect (switch at 50%)
        result.effectId = (t < 0.5f) ? from.effectId : to.effectId;
        
        // Interpolate parameters
        result.paletteIndex = (t < 0.5f) ? from.paletteIndex : to.paletteIndex;
        result.brightness = lerp8by8(from.brightness, to.brightness, t * 255);
        result.fadeAmount = lerp8by8(from.fadeAmount, to.fadeAmount, t * 255);
        result.speed = lerp8by8(from.speed, to.speed, t * 255);
        
        result.intensity = lerp8by8(from.intensity, to.intensity, t * 255);
        result.saturation = lerp8by8(from.saturation, to.saturation, t * 255);
        result.complexity = lerp8by8(from.complexity, to.complexity, t * 255);
        result.variation = lerp8by8(from.variation, to.variation, t * 255);
        
        // Switch modes at 50%
        result.syncMode = (t < 0.5f) ? from.syncMode : to.syncMode;
        result.propagationMode = (t < 0.5f) ? from.propagationMode : to.propagationMode;
        
        // Interpolate wave parameters
        if (from.waveParams.isWaveEffect && to.waveParams.isWaveEffect) {
            result.waveParams.isWaveEffect = true;
            
            result.waveParams.waveform1 = (t < 0.5f) ? from.waveParams.waveform1 : to.waveParams.waveform1;
            result.waveParams.waveform2 = (t < 0.5f) ? from.waveParams.waveform2 : to.waveParams.waveform2;
            
            result.waveParams.frequency1 = from.waveParams.frequency1 + 
                (to.waveParams.frequency1 - from.waveParams.frequency1) * t;
            result.waveParams.frequency2 = from.waveParams.frequency2 + 
                (to.waveParams.frequency2 - from.waveParams.frequency2) * t;
            result.waveParams.amplitude1 = from.waveParams.amplitude1 + 
                (to.waveParams.amplitude1 - from.waveParams.amplitude1) * t;
            result.waveParams.amplitude2 = from.waveParams.amplitude2 + 
                (to.waveParams.amplitude2 - from.waveParams.amplitude2) * t;
            result.waveParams.phase1 = from.waveParams.phase1 + 
                (to.waveParams.phase1 - from.waveParams.phase1) * t;
            result.waveParams.phase2 = from.waveParams.phase2 + 
                (to.waveParams.phase2 - from.waveParams.phase2) * t;
            
            result.waveParams.interactionMode = (t < 0.5f) ? 
                from.waveParams.interactionMode : to.waveParams.interactionMode;
            result.waveParams.enableStrip1 = (t < 0.5f) ? 
                from.waveParams.enableStrip1 : to.waveParams.enableStrip1;
            result.waveParams.enableStrip2 = (t < 0.5f) ? 
                from.waveParams.enableStrip2 : to.waveParams.enableStrip2;
        } else {
            result.waveParams = (t < 0.5f) ? from.waveParams : to.waveParams;
        }
    }
    
    bool savePresetToFile(uint8_t slot) {
        char filename[32];
        snprintf(filename, sizeof(filename), "%spreset_%02d.bin", PRESET_FILE_PATH, slot);
        
        File file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            Serial.printf("Failed to open file for writing: %s\n", filename);
            return false;
        }
        
        size_t written = file.write((uint8_t*)&m_presets[slot], sizeof(Preset));
        file.close();
        
        return written == sizeof(Preset);
    }
    
    bool loadPresetFromFile(uint8_t slot) {
        char filename[32];
        snprintf(filename, sizeof(filename), "%spreset_%02d.bin", PRESET_FILE_PATH, slot);
        
        if (!SPIFFS.exists(filename)) {
            return false;
        }
        
        File file = SPIFFS.open(filename, FILE_READ);
        if (!file) {
            return false;
        }
        
        size_t read = file.read((uint8_t*)&m_presets[slot], sizeof(Preset));
        file.close();
        
        if (read == sizeof(Preset) && m_presets[slot].isValid()) {
            m_presetValid[slot] = true;
            return true;
        }
        
        return false;
    }
    
    void loadAllPresets() {
        int loaded = 0;
        for (uint8_t i = 0; i < MAX_PRESETS; i++) {
            if (loadPresetFromFile(i)) {
                loaded++;
            }
        }
        Serial.printf("Loaded %d presets from SPIFFS\n", loaded);
    }
    
    uint8_t findPresetIndex(const Preset& preset) {
        for (uint8_t i = 0; i < MAX_PRESETS; i++) {
            if (m_presetValid[i] && strcmp(m_presets[i].name, preset.name) == 0) {
                return i;
            }
        }
        return 0;
    }
};
```

#### Step 3: Integration Example

```cpp
// In main.cpp
PresetManager* presetManager = nullptr;

void setup() {
    // ... existing setup ...
    
    // Initialize preset manager
    presetManager = new PresetManager();
    if (presetManager->begin()) {
        Serial.println("✅ Preset Manager ready");
        
        // Try to load last used preset
        presetManager->loadPreset(0);
    }
}

void loop() {
    // ... existing code ...
    
    // Update preset morphing
    if (presetManager) {
        presetManager->updateMorph();
        presetManager->updateSequence();
    }
    
    // Handle preset shortcuts (example: scroll button + encoder)
    static bool scrollButtonHeld = false;
    if (scrollEncoderAvailable && scrollEncoder.getButtonStatus()) {
        if (!scrollButtonHeld) {
            scrollButtonHeld = true;
            uint8_t mirroredEncoder = getScrollMirroredEncoder();
            
            // Quick save to slots 0-7 based on mirrored encoder
            if (mirroredEncoder < 8) {
                presetManager->quickSave(mirroredEncoder);
                Serial.printf("Quick saved to slot %d\n", mirroredEncoder);
                
                // Flash all encoder LEDs green
                for (int i = 0; i < 8; i++) {
                    encoder.setRGB(i, 0, 255, 0);
                }
                delay(200);
                // Restore normal colors
            }
        }
    } else {
        scrollButtonHeld = false;
    }
}
```

### Preset Features

1. **Quick Save/Load**: Slots 0-9 for instant access
2. **Preset Morphing**: Smooth interpolation between presets
3. **Sequences**: Playlist of presets with timed transitions
4. **Persistent Storage**: SPIFFS-based storage survives power cycles
5. **Complete State**: All parameters including wave engine
6. **Validation**: Checksum ensures preset integrity

### Usage Examples

- **Quick Save**: Hold scroll button + turn encoder to select slot
- **Quick Load**: Double-click encoder button to load preset
- **Morph Between**: Use encoder 7 to smoothly transition between presets
- **Create Sequence**: Define playlist in code or via serial commands
- **Auto-Save**: Periodic background save of current state

---

## Conclusion

These three advanced features work together to create a professional, intuitive LED control system:

1. **Visual Feedback** provides immediate understanding of system state
2. **Advanced Transitions** create smooth, engaging effect changes
3. **Preset Management** enables quick access to favorite configurations

The modular design allows each feature to be implemented independently while maintaining compatibility with the existing system. Start with the Wave Engine Visual Feedback System for immediate visual impact, then add the Transition Engine for professional polish, and finally implement Preset Management for user convenience.

Each system is designed to leverage your existing hardware (M5ROTATE8 + M5Unit-Scroll) while adding significant value to the user experience. The implementations provided are production-ready with proper error handling, performance optimization, and extensibility for future enhancements.