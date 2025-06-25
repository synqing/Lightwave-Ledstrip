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
    TRANSITION_GLITCH,        // Digital glitch effect
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
    
    // Effect-specific state - OPTIMIZED FOR MEMORY
    struct TransitionState {
        // Glitch effect
        uint16_t glitchSegments[8];  // Reduced count
        uint8_t glitchCount;
        uint32_t lastGlitch;
        
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
    void applyGlitch();
    void applyPhaseShift();
    void applySpiral();
    void applyRipple();
    
    // Helper functions
    void initializeGlitch();
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
        case TRANSITION_GLITCH:
            initializeGlitch();
            break;
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
        m_active = false;
        
        // Copy final state
        memcpy(m_outputBuffer, m_targetBuffer, m_numLeds * sizeof(CRGB));
        return false;
    }
    
    // Apply easing
    float rawProgress = (float)elapsed / m_duration;
    m_progress = applyEasing(rawProgress, m_curve);
    
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
        case TRANSITION_GLITCH:
            applyGlitch();
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
        // CENTER ORIGIN FADE - fade radiates from center outward
        uint16_t stripLength = m_numLeds / 2;
        
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            
            for (uint16_t i = 0; i < stripLength; i++) {
                // Calculate distance from center (LED 79)
                float distFromCenter = abs((int)i - (int)m_centerPoint) / (float)m_centerPoint;
                
                // Fade progress based on distance from center
                float localProgress = m_progress * 2.0f - distFromCenter;
                localProgress = constrain(localProgress, 0.0f, 1.0f);
                
                uint8_t blend = localProgress * 255;
                m_outputBuffer[offset + i] = lerpColor(
                    m_sourceBuffer[offset + i], 
                    m_targetBuffer[offset + i], 
                    blend
                );
            }
        }
    } else {
        // Standard uniform fade
        uint8_t blend = m_progress * 255;
        for (uint16_t i = 0; i < m_numLeds; i++) {
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], blend);
        }
    }
}

inline void TransitionEngine::applyWipe(bool leftToRight, bool fromCenter) {
    if (fromCenter && m_dualStripMode) {
        // CENTER ORIGIN wipe for dual strips
        uint16_t stripLength = m_numLeds / 2;
        uint16_t radius = m_progress * m_centerPoint;
        
        // Process both strips
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            
            for (uint16_t i = 0; i < stripLength; i++) {
                uint16_t distFromCenter = abs((int)i - (int)m_centerPoint);
                bool showTarget = leftToRight ? 
                    (distFromCenter <= radius) : 
                    (distFromCenter >= m_centerPoint - radius);
                    
                m_outputBuffer[offset + i] = showTarget ? 
                    m_targetBuffer[offset + i] : 
                    m_sourceBuffer[offset + i];
            }
        }
    } else {
        // Standard linear wipe
        uint16_t threshold = m_progress * m_numLeds;
        
        for (uint16_t i = 0; i < m_numLeds; i++) {
            bool showTarget = leftToRight ? 
                (i < threshold) : 
                (i >= m_numLeds - threshold);
                
            m_outputBuffer[i] = showTarget ? 
                m_targetBuffer[i] : 
                m_sourceBuffer[i];
        }
    }
}


inline void TransitionEngine::applyMelt() {
    if (!m_state.heatMap) return;  // Safety check
    
    // CENTER ORIGIN melting - heat radiates from center outward
    if (m_dualStripMode) {
        uint16_t stripLength = m_numLeds / 2;
        float meltRadius = m_progress * m_centerPoint;
        
        for (uint16_t strip = 0; strip < 2; strip++) {
            uint16_t offset = strip * stripLength;
            
            for (uint16_t i = 0; i < stripLength; i++) {
                float distFromCenter = abs((int)i - (int)m_centerPoint);
                
                // Add heat based on distance from melting boundary
                uint8_t heatAdd = 0;
                if (distFromCenter <= meltRadius + 10) {
                    float heatIntensity = max(0.0f, 1.0f - abs(distFromCenter - meltRadius) / 10.0f);
                    heatAdd = heatIntensity * 60;
                }
                
                // Update heat (8-bit arithmetic)
                m_state.heatMap[offset + i] = min(255, (int)m_state.heatMap[offset + i] + heatAdd);
                m_state.heatMap[offset + i] = (m_state.heatMap[offset + i] * 240) >> 8;  // Cool down
                
                // Apply melting effect
                if (distFromCenter <= meltRadius || m_state.heatMap[offset + i] > 128) {
                    m_outputBuffer[offset + i] = m_targetBuffer[offset + i];
                } else {
                    // Blend based on heat
                    uint8_t blend = m_state.heatMap[offset + i];
                    m_outputBuffer[offset + i] = lerpColor(m_sourceBuffer[offset + i], m_targetBuffer[offset + i], blend);
                }
            }
        }
    }
}


inline void TransitionEngine::applyGlitch() {
    // Copy source as base
    memcpy(m_outputBuffer, m_sourceBuffer, m_numLeds * sizeof(CRGB));
    
    // Apply glitch segments
    uint32_t now = millis();
    if (now - m_state.lastGlitch > 50) {  // New glitch every 50ms
        m_state.lastGlitch = now;
        
        // Random glitch segments
        m_state.glitchCount = random8(3, 8);
        for (uint8_t i = 0; i < m_state.glitchCount; i++) {
            m_state.glitchSegments[i] = random16(m_numLeds);
        }
    }
    
    // Apply glitches
    for (uint8_t i = 0; i < m_state.glitchCount; i++) {
        uint16_t pos = m_state.glitchSegments[i];
        uint16_t len = random8(5, 20);
        
        for (uint16_t j = 0; j < len && pos + j < m_numLeds; j++) {
            if (random8() < (m_progress * 255)) {
                // Glitch to target
                m_outputBuffer[pos + j] = m_targetBuffer[pos + j];
            } else if (random8() < 30) {
                // Corruption
                m_outputBuffer[pos + j] = CRGB(random8(), random8(), random8());
            }
        }
    }
}

inline void TransitionEngine::applyPhaseShift() {
    // Frequency-based morphing
    m_state.phaseOffset += m_progress * 0.2f;
    
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float position = (float)i / m_numLeds;
        float wave = sin(position * TWO_PI * 3 + m_state.phaseOffset);
        float blend = (wave + 1.0f) * 0.5f * m_progress;
        
        m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], blend * 255);
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


inline void TransitionEngine::initializeMelt() {
    // Allocate heat map if needed
    if (!m_state.heatMap) {
        m_state.heatMap = new uint8_t[m_numLeds];
    }
    // Clear heat map
    memset(m_state.heatMap, 0, m_numLeds);
}

inline void TransitionEngine::initializeGlitch() {
    m_state.glitchCount = 0;
    m_state.lastGlitch = 0;
}

inline TransitionType TransitionEngine::getRandomTransition() {
    // Weighted random selection for variety
    uint8_t weights[] = {
        20,  // FADE
        10,  // WIPE_OUT
        10,  // WIPE_IN
        10,  // MELT
        8,   // GLITCH
        10,  // PHASE_SHIFT
        12,  // SPIRAL - NEW!
        12,  // RIPPLE - NEW!
        8    // LIGHTNING - NEW!
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
                
                // Spiral equation: angle varies with distance and time
                float angle = m_state.spiralAngle + distFromCenter * TWO_PI * 2.0f;
                float spiralProgress = (sin(angle) + 1.0f) * 0.5f;
                
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

inline void TransitionEngine::initializeLightning() {
    m_state.lightning.segmentCount = 0;
    m_state.lightning.lastStrike = 0;
    m_state.lightning.intensity = 255;
    m_state.lightning.active = false;
}

inline void TransitionEngine::applyLightning() {
    uint32_t now = millis();
    
    // Start with current blend state
    uint8_t baseBlend = m_progress * 255;
    for (uint16_t i = 0; i < m_numLeds; i++) {
        m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], baseBlend);
    }
    
    // Generate CENTER ORIGIN lightning strikes
    if (now - m_state.lightning.lastStrike > 150 + random8(200)) {  // Random strike interval
        m_state.lightning.lastStrike = now;
        m_state.lightning.active = true;
        m_state.lightning.intensity = 255;
        
        // Generate CENTER ORIGIN lightning path - radiates outward
        m_state.lightning.segmentCount = random8(4, 8);
        
        if (m_dualStripMode) {
            uint16_t stripLength = m_numLeds / 2;
            
            // Create lightning on both strips radiating from center
            for (uint8_t i = 0; i < m_state.lightning.segmentCount; i++) {
                // Distance from center increases with each segment
                float distance = (i + 1) * (m_centerPoint / (float)m_state.lightning.segmentCount);
                distance += random8(10) - 5;  // Add some jitter
                
                // Place on both strips
                uint16_t pos1 = m_centerPoint + constrain(distance, 0, m_centerPoint);
                uint16_t pos2 = stripLength + (m_centerPoint - constrain(distance, 0, m_centerPoint));
                
                m_state.lightning.segments[i] = pos1;  // First strip
                if (i + m_state.lightning.segmentCount < 10) {
                    m_state.lightning.segments[i + m_state.lightning.segmentCount] = pos2;  // Second strip
                }
            }
            m_state.lightning.segmentCount *= 2;  // Both strips
        }
    }
    
    // Render lightning bolt
    if (m_state.lightning.active) {
        // Fade lightning intensity
        m_state.lightning.intensity = max(0, (int)m_state.lightning.intensity - 12);
        if (m_state.lightning.intensity == 0) {
            m_state.lightning.active = false;
        }
        
        // Draw lightning segments
        for (uint8_t i = 0; i < m_state.lightning.segmentCount; i++) {
            uint16_t pos = m_state.lightning.segments[i];
            if (pos >= m_numLeds) continue;
            
            // Main bolt
            CRGB lightningColor = CRGB::White;
            lightningColor.nscale8(m_state.lightning.intensity);
            m_outputBuffer[pos] = lightningColor;
            
            // Glow around bolt
            if (pos > 0) {
                m_outputBuffer[pos - 1] = lerpColor(m_outputBuffer[pos - 1], lightningColor, 128);
            }
            if (pos < m_numLeds - 1) {
                m_outputBuffer[pos + 1] = lerpColor(m_outputBuffer[pos + 1], lightningColor, 128);
            }
        }
    }
}

#endif // TRANSITION_ENGINE_H