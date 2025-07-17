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
    TRANSITION_FADE,          // CENTER ORIGIN crossfade - radiates from center
    TRANSITION_WIPE_OUT,      // Wipe from center outward
    TRANSITION_WIPE_IN,       // Wipe from edges inward  
    TRANSITION_DISSOLVE,      // Random pixel transition
    TRANSITION_SHATTER,       // Particle explosion from center
    TRANSITION_GLITCH,        // Digital glitch effect
    TRANSITION_PHASE_SHIFT,   // Frequency-based morph
    TRANSITION_COUNT
    // REMOVED: WIPE_LR, WIPE_RL, ZOOM_IN, ZOOM_OUT, MELT - these violate CENTER ORIGIN
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
    
    // Effect-specific state
    struct TransitionState {
        // Dissolve effect
        uint8_t pixelOrder[HardwareConfig::NUM_LEDS];
        uint16_t dissolveIndex;
        
        // Shatter effect
        struct Particle {
            float x;
            float vx;
            float vy;
            uint8_t hue;
            uint8_t lifetime;
        } particles[20];
        uint8_t particleCount;
        
        // Melt effect - REMOVED (violated CENTER ORIGIN)
        
        // Glitch effect
        uint16_t glitchSegments[10];
        uint8_t glitchCount;
        uint32_t lastGlitch;
        
        // Phase shift
        float phaseOffset;
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
    
    // Transition implementations - CENTER ORIGIN ONLY
    void applyFade();
    void applyWipe(bool outward, bool fromCenter);  // fromCenter always true now
    void applyDissolve();
    void applyShatter();
    void applyGlitch();
    void applyPhaseShift();
    // REMOVED: applyZoom, applyMelt
    
    // Helper functions
    void initializeDissolve();
    void initializeShatter();
    void initializeGlitch();
    // REMOVED: initializeMelt
    
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
        case TRANSITION_DISSOLVE:
            initializeDissolve();
            break;
        case TRANSITION_SHATTER:
            initializeShatter();
            break;
        case TRANSITION_GLITCH:
            initializeGlitch();
            break;
        // REMOVED: MELT initialization
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
    
    // Apply transition effect - CENTER ORIGIN ONLY
    switch (m_type) {
        case TRANSITION_FADE:
            applyFade();  // CENTER ORIGIN fade
            break;
        case TRANSITION_WIPE_OUT:
            applyWipe(true, true);  // Always from center
            break;
        case TRANSITION_WIPE_IN:
            applyWipe(false, true);  // Always to center
            break;
        case TRANSITION_DISSOLVE:
            applyDissolve();
            break;
        case TRANSITION_SHATTER:
            applyShatter();  // Particles explode from center
            break;
        case TRANSITION_GLITCH:
            applyGlitch();
            break;
        case TRANSITION_PHASE_SHIFT:
            applyPhaseShift();
            break;
        // REMOVED: WIPE_LR, WIPE_RL, ZOOM_IN, ZOOM_OUT, MELT
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

inline void TransitionEngine::applyDissolve() {
    uint16_t pixelsToShow = m_progress * m_numLeds;
    
    for (uint16_t i = 0; i < m_numLeds; i++) {
        uint16_t pixelIndex = m_state.pixelOrder[i];
        if (i < pixelsToShow) {
            m_outputBuffer[pixelIndex] = m_targetBuffer[pixelIndex];
        } else {
            m_outputBuffer[pixelIndex] = m_sourceBuffer[pixelIndex];
        }
    }
}

// REMOVED: applyZoom and applyMelt - violated CENTER ORIGIN principle

inline void TransitionEngine::applyShatter() {
    // Start with source
    memcpy(m_outputBuffer, m_sourceBuffer, m_numLeds * sizeof(CRGB));
    
    // Update and render particles FROM CENTER
    for (uint8_t i = 0; i < m_state.particleCount; i++) {
        auto& p = m_state.particles[i];
        
        // Update physics - particles move away from center
        p.x += p.vx * (1.0f + m_progress);  // Accelerate outward
        p.vy += 0.5f;  // Gravity
        
        // Update lifetime
        if (p.lifetime > 0) {
            p.lifetime--;
            
            // Render particle on both strips if in dual mode
            if (m_dualStripMode) {
                uint16_t stripLength = m_numLeds / 2;
                // Render on strip 1
                int16_t pos1 = (int16_t)p.x;
                if (pos1 >= 0 && pos1 < stripLength) {
                    CRGB particleColor = CHSV(p.hue, 255, p.lifetime * 2);
                    m_outputBuffer[pos1] = lerpColor(m_outputBuffer[pos1], particleColor, 200);
                }
                // Mirror on strip 2
                int16_t pos2 = stripLength + (stripLength - 1 - pos1);
                if (pos2 >= stripLength && pos2 < m_numLeds) {
                    CRGB particleColor = CHSV(p.hue, 255, p.lifetime * 2);
                    m_outputBuffer[pos2] = lerpColor(m_outputBuffer[pos2], particleColor, 200);
                }
            } else {
                int16_t pos = (int16_t)p.x;
                if (pos >= 0 && pos < m_numLeds) {
                    CRGB particleColor = CHSV(p.hue, 255, p.lifetime * 2);
                    m_outputBuffer[pos] = lerpColor(m_outputBuffer[pos], particleColor, 200);
                }
            }
        }
    }
    
    // Fade in target from CENTER ORIGIN
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float distFromCenter = getDistanceFromCenter(i);
        float centerProgress = m_progress * 2.0f - distFromCenter;
        centerProgress = constrain(centerProgress, 0.0f, 1.0f);
        uint8_t targetAlpha = centerProgress * centerProgress * 255;  // Quadratic fade from center
        m_outputBuffer[i] = lerpColor(m_outputBuffer[i], m_targetBuffer[i], targetAlpha);
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

inline void TransitionEngine::initializeDissolve() {
    // Initialize random pixel order
    for (uint16_t i = 0; i < m_numLeds; i++) {
        m_state.pixelOrder[i] = i;
    }
    
    // Fisher-Yates shuffle
    for (uint16_t i = m_numLeds - 1; i > 0; i--) {
        uint16_t j = random16(i + 1);
        uint8_t temp = m_state.pixelOrder[i];
        m_state.pixelOrder[i] = m_state.pixelOrder[j];
        m_state.pixelOrder[j] = temp;
    }
}

inline void TransitionEngine::initializeShatter() {
    // Create particles at CENTER ORIGIN
    m_state.particleCount = 20;
    for (uint8_t i = 0; i < m_state.particleCount; i++) {
        auto& p = m_state.particles[i];
        // Start at center with slight random offset
        p.x = m_centerPoint + random8(10) - 5;
        // Velocity away from center
        if (i < m_state.particleCount / 2) {
            p.vx = -random8(3, 8);  // Left direction
        } else {
            p.vx = random8(3, 8);   // Right direction
        }
        p.vy = -random8(5, 15);  // Upward velocity
        p.hue = random8();
        p.lifetime = 128;
    }
}

// REMOVED: initializeMelt - violated CENTER ORIGIN principle

inline void TransitionEngine::initializeGlitch() {
    m_state.glitchCount = 0;
    m_state.lastGlitch = 0;
}

inline TransitionType TransitionEngine::getRandomTransition() {
    // Weighted random selection for variety - CENTER ORIGIN ONLY
    uint8_t weights[] = {
        30,  // FADE - increased weight
        20,  // WIPE_OUT - from center
        20,  // WIPE_IN - to center
        15,  // DISSOLVE
        8,   // SHATTER - from center
        5,   // GLITCH
        2    // PHASE_SHIFT
        // REMOVED: WIPE_LR, WIPE_RL, ZOOM_IN, ZOOM_OUT, MELT
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

inline uint16_t TransitionEngine::getCenterPoint(uint16_t index) {
    // For dual strips, both use LED 79 as center
    if (m_dualStripMode) {
        return m_centerPoint;  // Always LED 79
    } else {
        return m_numLeds / 2;  // Middle of single strip
    }
}

inline float TransitionEngine::getDistanceFromCenter(uint16_t index) {
    if (m_dualStripMode) {
        uint16_t stripLength = m_numLeds / 2;
        uint16_t strip = index / stripLength;
        uint16_t localIndex = index % stripLength;
        // For dual strips, both use LED 79 as center
        uint16_t distFromCenter = abs((int)localIndex - (int)m_centerPoint);
        return (float)distFromCenter / m_centerPoint;
    } else {
        // Single strip mode - center is middle of strip
        uint16_t distFromCenter = abs((int)index - (int)m_centerPoint);
        return (float)distFromCenter / m_centerPoint;
    }
}

#endif // TRANSITION_ENGINE_H