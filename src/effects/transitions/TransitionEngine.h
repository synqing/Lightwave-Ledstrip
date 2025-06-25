#ifndef TRANSITION_ENGINE_H
#define TRANSITION_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"

/**
 * Advanced Transition Engine
 * 
 * Provides sophisticated transitions between effects with multiple
 * transition types, easing curves, and optimized performance.
 * 
 * Features:
 * - Multiple transition types (fade, wipe, dissolve, zoom, etc.)
 * - Easing curves for smooth animations
 * - Dual-strip aware transitions
 * - CENTER ORIGIN compliant animations
 * - Memory-efficient implementation
 */

enum TransitionType {
    TRANSITION_FADE,          // Classic crossfade
    TRANSITION_WIPE_OUT,      // Wipe from center outward  
    TRANSITION_WIPE_IN,       // Wipe from edges inward
    TRANSITION_PHASE_SHIFT,   // Frequency-based morph
    TRANSITION_SPIRAL,        // Helical spiral from center
    TRANSITION_RIPPLE,        // Concentric wave ripples
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
    EASE_OUT_BOUNCE,
    EASE_IN_BACK,
    EASE_OUT_BACK,
    EASE_IN_OUT_BACK
};

class TransitionEngine {
private:
    // Buffer pointers
    CRGB* m_sourceBuffer;
    CRGB* m_targetBuffer;
    CRGB* m_outputBuffer;
    uint16_t m_numLeds;
    
    // Transition state
    TransitionType m_type = TRANSITION_FADE;
    EasingCurve m_curve = EASE_IN_OUT_QUAD;
    uint32_t m_startTime = 0;
    uint32_t m_duration = 1000;
    float m_progress = 0.0f;
    bool m_active = false;
    
    // CENTER ORIGIN support
    uint16_t m_centerPoint;
    bool m_dualStripMode = false;
    
    // Performance optimization: pre-calculated values
    uint8_t m_easedProgress = 0;  // Pre-calculated eased progress as 0-255
    uint8_t m_fadeRadius = 0;     // Current fade radius for center-origin effects
    
    // Effect-specific state - OPTIMIZED FOR MEMORY
    struct TransitionState {
        // Phase shift
        float phaseOffset;
        
        // Spiral effect - CENTER ORIGIN ONLY
        float spiralAngle;
        float spiralSpeed;
        
        // Ripple effect - CENTER ORIGIN WAVES
        struct RippleWave {
            float radius;      // Distance from center
            float amplitude;   // Wave amplitude
            uint32_t birthTime;
            bool active;
        } ripples[3];  // Reduced count
        uint8_t rippleCount;
    } m_state;
    
public:
    TransitionEngine(uint16_t numLeds = HardwareConfig::NUM_LEDS) 
        : m_numLeds(numLeds), m_centerPoint(numLeds / 2) {
        // Initialize state
        resetState();
    }
    
    // Start a new transition
    void startTransition(
        CRGB* source, 
        CRGB* target, 
        CRGB* output,
        TransitionType type, 
        uint32_t duration, 
        EasingCurve curve = EASE_IN_OUT_QUAD
    );
    
    // Update transition (returns true while active)
    bool update();
    
    // Query state
    bool isActive() const { return m_active; }
    float getProgress() const { return m_progress; }
    TransitionType getCurrentType() const { return m_type; }
    
    // Configure for dual-strip mode
    void setDualStripMode(bool enabled, uint16_t stripLength = HardwareConfig::STRIP_LENGTH) {
        m_dualStripMode = enabled;
        if (enabled) {
            m_centerPoint = HardwareConfig::STRIP_CENTER_POINT;  // LED 79 - TRUE CENTER ORIGIN
        }
    }
    
    // Get a random transition type (excludes some for variety)
    static TransitionType getRandomTransition();
    
private:
    // Reset transition state
    void resetState();
    
    // Easing functions
    float applyEasing(float t, EasingCurve curve);
    
    // Transition implementations
    void applyFade();
    void applyWipe(bool leftToRight, bool fromCenter);
    void applyPhaseShift();
    void applySpiral();
    void applyRipple();
    
    // Helper functions
    void initializeSpiral();
    void initializeRipple();
    
    // Utility functions
    CRGB lerpColor(CRGB from, CRGB to, uint8_t progress);
    uint16_t getCenterPoint(uint16_t index);
    float getDistanceFromCenter(uint16_t index);
};

// Implementation
inline void TransitionEngine::startTransition(
    CRGB* source, 
    CRGB* target, 
    CRGB* output,
    TransitionType type, 
    uint32_t duration, 
    EasingCurve curve
) {
    m_sourceBuffer = source;
    m_targetBuffer = target;
    m_outputBuffer = output;
    m_type = type;
    m_duration = duration;
    m_curve = curve;
    m_startTime = millis();
    m_active = true;
    m_progress = 0.0f;
    
    // Initialize transition-specific state
    resetState();
    
    switch (type) {
        case TRANSITION_SPIRAL:
            initializeSpiral();
            break;
        case TRANSITION_RIPPLE:
            initializeRipple();
            break;
        default:
            break;
    }
}

inline bool TransitionEngine::update() {
    if (!m_active) return false;
    
    // Calculate progress
    uint32_t elapsed = millis() - m_startTime;
    if (elapsed >= m_duration) {
        m_progress = 1.0f;
        m_easedProgress = 255;
        m_active = false;
        
        // Copy final state - use optimized block copy
        uint32_t* src = (uint32_t*)m_targetBuffer;
        uint32_t* dst = (uint32_t*)m_outputBuffer;
        uint16_t count = (m_numLeds * sizeof(CRGB)) / sizeof(uint32_t);
        while (count--) *dst++ = *src++;
        
        return false;
    }
    
    // Apply easing with pre-calculated 8-bit result
    float rawProgress = (float)elapsed / m_duration;
    m_progress = applyEasing(rawProgress, m_curve);
    m_easedProgress = m_progress * 255;
    
    // Apply transition effect
    switch (m_type) {
        case TRANSITION_FADE:
            applyFade();
            break;
        case TRANSITION_WIPE_OUT:
            applyWipe(true, true);
            break;
        case TRANSITION_WIPE_IN:
            applyWipe(false, true);
            break;
        case TRANSITION_PHASE_SHIFT:
            applyPhaseShift();
            break;
        case TRANSITION_SPIRAL:
            applySpiral();
            break;
        case TRANSITION_RIPPLE:
            applyRipple();
            break;
    }
    
    return true;
}

inline void TransitionEngine::applyFade() {
    if (m_dualStripMode) {
        // CENTER ORIGIN FADE optimized with integer math
        uint16_t stripLength = m_numLeds / 2;
        
        // Pre-calculate fade parameters using integer math
        uint16_t fadeEdge = (m_easedProgress * m_centerPoint * 130) >> 8;  // 1.3x multiplier
        uint16_t fadeTrailWidth = m_centerPoint * 30 / 100;  // 30% trail width
        
        // Process both strips in parallel-friendly blocks
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            CRGB* srcPtr = &m_sourceBuffer[offset];
            CRGB* tgtPtr = &m_targetBuffer[offset];
            CRGB* outPtr = &m_outputBuffer[offset];
            
            // Use external distance lookup from MegaLUTs
            extern uint8_t* distanceFromCenterLUT;
            uint8_t* distPtr = &distanceFromCenterLUT[offset];
            
            // Simple distance-based fade with smooth trailing edge
            for (uint16_t i = 0; i < stripLength; i++) {
                uint8_t dist = distPtr[i];
                uint8_t blendAmount = m_easedProgress;
                
                // Apply distance-based fading
                if (dist < fadeEdge - fadeTrailWidth) {
                    blendAmount = 255;  // Full new effect
                } else if (dist < fadeEdge) {
                    // Smooth fade in trail
                    uint16_t fadePos = dist - (fadeEdge - fadeTrailWidth);
                    blendAmount = 255 - ((fadePos * 255) / fadeTrailWidth);
                } else {
                    blendAmount = 0;  // Full old effect
                }
                
                // Mix based on progress
                blendAmount = scale8(blendAmount, m_easedProgress);
                
                // Use FastLED's optimized blend function
                outPtr[i] = blend(srcPtr[i], tgtPtr[i], blendAmount);
            }
        }
    } else {
        // Standard uniform fade - process in blocks for better cache usage
        uint8_t blendAmount = m_easedProgress;
        
        // Process 4 pixels at a time for better performance
        uint16_t i = 0;
        for (; i < m_numLeds - 3; i += 4) {
            m_outputBuffer[i] = blend(m_sourceBuffer[i], m_targetBuffer[i], blendAmount);
            m_outputBuffer[i+1] = blend(m_sourceBuffer[i+1], m_targetBuffer[i+1], blendAmount);
            m_outputBuffer[i+2] = blend(m_sourceBuffer[i+2], m_targetBuffer[i+2], blendAmount);
            m_outputBuffer[i+3] = blend(m_sourceBuffer[i+3], m_targetBuffer[i+3], blendAmount);
        }
        // Handle remaining pixels
        for (; i < m_numLeds; i++) {
            m_outputBuffer[i] = blend(m_sourceBuffer[i], m_targetBuffer[i], blendAmount);
        }
    }
}

inline void TransitionEngine::applyWipe(bool leftToRight, bool fromCenter) {
    if (fromCenter && m_dualStripMode) {
        // CENTER ORIGIN wipe optimized with lookup table
        uint16_t stripLength = m_numLeds / 2;
        uint8_t radius = (m_easedProgress * m_centerPoint) >> 8;
        
        // Use external distance lookup from MegaLUTs
        extern uint8_t* distanceFromCenterLUT;
        
        // Process both strips with optimized pointer arithmetic
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            CRGB* srcPtr = &m_sourceBuffer[offset];
            CRGB* tgtPtr = &m_targetBuffer[offset];
            CRGB* outPtr = &m_outputBuffer[offset];
            uint8_t* distPtr = &distanceFromCenterLUT[offset];
            
            if (leftToRight) {
                for (uint16_t i = 0; i < stripLength; i++) {
                    outPtr[i] = (distPtr[i] <= radius) ? tgtPtr[i] : srcPtr[i];
                }
            } else {
                uint8_t invRadius = m_centerPoint - radius;
                for (uint16_t i = 0; i < stripLength; i++) {
                    outPtr[i] = (distPtr[i] >= invRadius) ? tgtPtr[i] : srcPtr[i];
                }
            }
        }
    } else {
        // Standard linear wipe - process in blocks
        uint16_t threshold = (m_easedProgress * m_numLeds) >> 8;
        
        if (leftToRight) {
            // Copy target up to threshold
            memcpy(m_outputBuffer, m_targetBuffer, threshold * sizeof(CRGB));
            // Copy source for the rest
            memcpy(&m_outputBuffer[threshold], &m_sourceBuffer[threshold], 
                   (m_numLeds - threshold) * sizeof(CRGB));
        } else {
            uint16_t sourceEnd = m_numLeds - threshold;
            // Copy source up to threshold
            memcpy(m_outputBuffer, m_sourceBuffer, sourceEnd * sizeof(CRGB));
            // Copy target for the rest
            memcpy(&m_outputBuffer[sourceEnd], &m_targetBuffer[sourceEnd], 
                   threshold * sizeof(CRGB));
        }
    }
}




inline void TransitionEngine::applyPhaseShift() {
    // Frequency-based morphing with LUT
    extern uint8_t (*wavePatternLUT)[HardwareConfig::NUM_LEDS];
    
    // Update phase with integer math
    static uint8_t phaseAccumulator = 0;
    phaseAccumulator += (m_easedProgress >> 2);  // Slower phase progression
    
    // Process with optimized wave LUT
    uint8_t patternIndex = phaseAccumulator >> 3;  // Select pattern based on phase
    
    for (uint16_t i = 0; i < m_numLeds; i++) {
        // Use pre-calculated wave pattern
        uint8_t wave = wavePatternLUT[patternIndex & 255][i];
        
        // Add phase animation
        uint8_t phaseWave = sin8(i + phaseAccumulator * 4);
        wave = scale8(wave, phaseWave);
        
        // Scale wave by progress
        uint8_t blendAmount = scale8(wave, m_easedProgress);
        
        m_outputBuffer[i] = blend(m_sourceBuffer[i], m_targetBuffer[i], blendAmount);
    }
}

inline float TransitionEngine::applyEasing(float t, EasingCurve curve) {
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
            return t == 0 ? 0 : t == 1 ? 1 : -pow(2, 10 * (t - 1)) * sin((t - 1.1f) * 5 * PI);
            
        case EASE_OUT_ELASTIC:
            return t == 0 ? 0 : t == 1 ? 1 : pow(2, -10 * t) * sin((t - 0.1f) * 5 * PI) + 1;
            
        case EASE_IN_OUT_ELASTIC:
            if (t == 0) return 0;
            if (t == 1) return 1;
            t *= 2;
            if (t < 1) return -0.5f * pow(2, 10 * (t - 1)) * sin((t - 1.1f) * 5 * PI);
            return 0.5f * pow(2, -10 * (t - 1)) * sin((t - 1.1f) * 5 * PI) + 1;
            
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
            
        case EASE_IN_BACK:
            return t * t * (2.70158f * t - 1.70158f);
            
        case EASE_OUT_BACK:
            return 1 + (--t) * t * (2.70158f * t + 1.70158f);
            
        case EASE_IN_OUT_BACK:
            t *= 2;
            if (t < 1) return 0.5f * t * t * (3.5949095f * t - 2.5949095f);
            t -= 2;
            return 0.5f * (t * t * (3.5949095f * t + 2.5949095f) + 2);
            
        default:
            return t;
    }
}

inline CRGB TransitionEngine::lerpColor(CRGB from, CRGB to, uint8_t progress) {
    // Use FastLED's optimized blend function
    return blend(from, to, progress);
}

inline void TransitionEngine::resetState() {
    // Clear state
    memset(&m_state, 0, sizeof(m_state));
}




inline TransitionType TransitionEngine::getRandomTransition() {
    // Weighted random selection for variety
    uint8_t weights[] = {
        35,  // FADE - increased weight for smooth trailing fades
        15,  // WIPE_OUT
        15,  // WIPE_IN
        15,  // PHASE_SHIFT
        10,  // SPIRAL
        10   // RIPPLE
    };
    
    uint8_t total = 0;
    for (uint8_t w : weights) total += w;
    
    uint8_t r = random8(total);
    uint8_t sum = 0;
    
    for (uint8_t i = 0; i < TRANSITION_COUNT; i++) {
        sum += weights[i];
        if (r < sum) {
            return (TransitionType)i;
        }
    }
    
    return TRANSITION_FADE;  // Fallback
}

// ============== NEW SPECTACULAR TRANSITIONS ==============

inline void TransitionEngine::initializeSpiral() {
    m_state.spiralAngle = 0.0f;
    m_state.spiralSpeed = random8(3, 8) * 0.1f;  // Random spiral speed
}

inline void TransitionEngine::applySpiral() {
    // Update spiral rotation
    m_state.spiralAngle += m_state.spiralSpeed;
    
    if (m_dualStripMode) {
        // CENTER ORIGIN spiral for dual strips
        uint16_t stripLength = m_numLeds / 2;
        
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            
            for (uint16_t i = 0; i < stripLength; i++) {
                // Calculate distance from center (LED 79)
                float distFromCenter = abs((int)i - (int)m_centerPoint) / (float)m_centerPoint;
                
                // Use pre-calculated LUTs from MegaLUTs
                extern uint8_t* spiralAngleLUT;
                extern uint8_t (*wavePatternLUT)[HardwareConfig::NUM_LEDS];
                
                // Spiral using LUT - much faster
                uint8_t spiralIndex = spiralAngleLUT[i] + (uint8_t)(m_state.spiralAngle * 40);
                uint8_t spiralProgress = wavePatternLUT[0][i] >> 1;  // Use first pattern
                
                // Combine with main transition progress
                float combinedProgress = spiralProgress * m_progress;
                combinedProgress = constrain(combinedProgress, 0.0f, 1.0f);
                
                uint8_t blend = combinedProgress * 255;
                m_outputBuffer[offset + i] = lerpColor(
                    m_sourceBuffer[offset + i], 
                    m_targetBuffer[offset + i], 
                    blend
                );
            }
        }
    } else {
        // Single buffer spiral
        for (uint16_t i = 0; i < m_numLeds; i++) {
            float distFromCenter = abs((int)i - (int)m_centerPoint) / (float)m_centerPoint;
            float angle = m_state.spiralAngle + distFromCenter * TWO_PI * 2.0f;
            float spiralProgress = (sin(angle) + 1.0f) * 0.5f;
            float combinedProgress = spiralProgress * m_progress;
            
            uint8_t blend = constrain(combinedProgress * 255, 0, 255);
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], blend);
        }
    }
}

inline void TransitionEngine::initializeRipple() {
    m_state.rippleCount = 3;  // Start with 3 ripples
    
    for (uint8_t i = 0; i < m_state.rippleCount; i++) {
        m_state.ripples[i].radius = 0.0f;  // Start from center
        m_state.ripples[i].amplitude = 1.0f - (i * 0.3f);  // Decreasing amplitude
        m_state.ripples[i].birthTime = millis() + (i * 300);  // Staggered birth times
        m_state.ripples[i].active = true;
    }
}

inline void TransitionEngine::applyRipple() {
    uint32_t now = millis();
    
    // Start with source buffer
    memcpy(m_outputBuffer, m_sourceBuffer, m_numLeds * sizeof(CRGB));
    
    if (m_dualStripMode) {
        uint16_t stripLength = m_numLeds / 2;
        
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            
            for (uint16_t i = 0; i < stripLength; i++) {
                float distFromCenter = abs((int)i - (int)m_centerPoint);
                float totalRipple = 0.0f;
                
                // Combine multiple CENTER ORIGIN ripples
                for (uint8_t r = 0; r < m_state.rippleCount; r++) {
                    if (!m_state.ripples[r].active) continue;
                    
                    float rippleAge = (now - m_state.ripples[r].birthTime) / 1000.0f;
                    if (rippleAge < 0) continue;  // Not born yet
                    
                    // Ripple expands from center outward
                    float currentRadius = rippleAge * m_centerPoint * 0.8f;  // Ripple speed
                    float rippleDist = abs(distFromCenter - currentRadius);
                    
                    if (rippleDist < 8.0f) {  // Ripple width
                        float rippleIntensity = m_state.ripples[r].amplitude * 
                                              cos(rippleDist * PI / 4.0f) *
                                              exp(-rippleAge * 1.5f);  // Decay over time
                        totalRipple += rippleIntensity;
                    }
                }
                
                // Apply ripple effect with transition progress
                float rippleBlend = constrain((totalRipple + 1.0f) * 0.5f * m_progress, 0.0f, 1.0f);
                uint8_t blend = rippleBlend * 255;
                
                m_outputBuffer[offset + i] = lerpColor(
                    m_outputBuffer[offset + i], 
                    m_targetBuffer[offset + i], 
                    blend
                );
            }
        }
    }
}


#endif // TRANSITION_ENGINE_H