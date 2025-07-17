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
    TRANSITION_PULSEWAVE,     // Concentric energy pulses from center
    TRANSITION_IMPLOSION,     // Particles converge and collapse to center
    TRANSITION_IRIS,          // Mechanical aperture open/close from center
    TRANSITION_NUCLEAR,       // Chain reaction explosion from center
    TRANSITION_STARGATE,      // Event horizon portal effect at center
    TRANSITION_KALEIDOSCOPE,  // Symmetric crystal patterns from center
    TRANSITION_MANDALA,       // Sacred geometry radiating from center
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
        
        // Pulsewave effect
        struct Pulse {
            float radius;
            float intensity;
            float velocity;
        } pulses[5];
        uint8_t pulseCount;
        uint32_t lastPulse;
        
        // Implosion effect
        struct ImplodeParticle {
            float radius;
            float angle;
            float velocity;
            uint8_t hue;
            uint8_t brightness;
        } implodeParticles[30];
        
        // Iris effect
        float irisRadius;
        uint8_t bladeCount;
        float bladeAngle;
        
        // Nuclear effect
        float shockwaveRadius;
        float radiationIntensity;
        uint8_t chainReactions[20];
        uint8_t reactionCount;
        
        // Stargate effect
        float eventHorizonRadius;
        float chevronAngle;
        uint8_t activeChevrons;
        float wormholePhase;
        
        // Kaleidoscope effect
        uint8_t symmetryFold;
        float rotationAngle;
        
        // Mandala effect
        float mandalaPhase;
        uint8_t ringCount;
        float ringRadii[8];
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
    void applyPulsewave();
    void applyImplosion();
    void applyIris();
    void applyNuclear();
    void applyStargate();
    void applyKaleidoscope();
    void applyMandala();
    // REMOVED: applyZoom, applyMelt
    
    // Helper functions
    void initializeDissolve();
    void initializeShatter();
    void initializeGlitch();
    void initializePulsewave();
    void initializeImplosion();
    void initializeIris();
    void initializeNuclear();
    void initializeStargate();
    void initializeKaleidoscope();
    void initializeMandala();
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
        case TRANSITION_PULSEWAVE:
            initializePulsewave();
            break;
        case TRANSITION_IMPLOSION:
            initializeImplosion();
            break;
        case TRANSITION_IRIS:
            initializeIris();
            break;
        case TRANSITION_NUCLEAR:
            initializeNuclear();
            break;
        case TRANSITION_STARGATE:
            initializeStargate();
            break;
        case TRANSITION_KALEIDOSCOPE:
            initializeKaleidoscope();
            break;
        case TRANSITION_MANDALA:
            initializeMandala();
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
        case TRANSITION_PULSEWAVE:
            applyPulsewave();
            break;
        case TRANSITION_IMPLOSION:
            applyImplosion();
            break;
        case TRANSITION_IRIS:
            applyIris();
            break;
        case TRANSITION_NUCLEAR:
            applyNuclear();
            break;
        case TRANSITION_STARGATE:
            applyStargate();
            break;
        case TRANSITION_KALEIDOSCOPE:
            applyKaleidoscope();
            break;
        case TRANSITION_MANDALA:
            applyMandala();
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

inline void TransitionEngine::applyPulsewave() {
    // Copy source as base
    memcpy(m_outputBuffer, m_sourceBuffer, m_numLeds * sizeof(CRGB));
    
    // Generate new pulses periodically
    uint32_t now = millis();
    if (now - m_state.lastPulse > 200 && m_state.pulseCount < 5) {
        m_state.pulses[m_state.pulseCount].radius = 0;
        m_state.pulses[m_state.pulseCount].intensity = 1.0f;
        m_state.pulses[m_state.pulseCount].velocity = 2.0f + m_progress * 3.0f;
        m_state.pulseCount++;
        m_state.lastPulse = now;
    }
    
    // Update and render pulses
    for (uint8_t p = 0; p < m_state.pulseCount; p++) {
        auto& pulse = m_state.pulses[p];
        pulse.radius += pulse.velocity;
        pulse.intensity *= 0.98f; // Decay
        
        // Render pulse ring
        for (uint16_t i = 0; i < m_numLeds; i++) {
            float dist = getDistanceFromCenter(i) * m_centerPoint;
            float ringDist = abs(dist - pulse.radius);
            
            if (ringDist < 5.0f) {
                float ringIntensity = (1.0f - ringDist / 5.0f) * pulse.intensity;
                uint8_t blendAmount = ringIntensity * m_progress * 255;
                m_outputBuffer[i] = lerpColor(m_outputBuffer[i], m_targetBuffer[i], blendAmount);
            }
        }
    }
}

inline void TransitionEngine::applyImplosion() {
    // Start with target
    memcpy(m_outputBuffer, m_targetBuffer, m_numLeds * sizeof(CRGB));
    
    // Update particles converging to center
    for (uint8_t i = 0; i < 30; i++) {
        auto& p = m_state.implodeParticles[i];
        
        // Move toward center with acceleration
        p.radius *= (0.95f - m_progress * 0.1f);
        p.velocity *= 1.05f;
        
        // Render particle
        if (p.radius > 1.0f) {
            for (uint16_t led = 0; led < m_numLeds; led++) {
                float dist = getDistanceFromCenter(led) * m_centerPoint;
                if (abs(dist - p.radius) < 2.0f) {
                    CRGB particleColor = CHSV(p.hue, 255, p.brightness * (1.0f - m_progress));
                    m_outputBuffer[led] = lerpColor(m_outputBuffer[led], particleColor, 200);
                }
            }
        }
    }
    
    // Flash at center on impact
    if (m_progress > 0.8f) {
        float flash = (m_progress - 0.8f) * 5.0f;
        uint16_t flashRadius = flash * m_centerPoint;
        for (uint16_t i = 0; i < m_numLeds; i++) {
            if (getDistanceFromCenter(i) * m_centerPoint < flashRadius) {
                m_outputBuffer[i] = lerpColor(m_outputBuffer[i], CRGB::White, 255 * (1.0f - flash));
            }
        }
    }
}

inline void TransitionEngine::applyIris() {
    // Mechanical aperture effect
    float targetRadius = m_progress * m_centerPoint;
    
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float dist = getDistanceFromCenter(i) * m_centerPoint;
        
        // Create hexagonal iris shape
        float angle = atan2(i - m_centerPoint, 1);
        float bladeDist = dist * (1.0f + 0.1f * sin(angle * m_state.bladeCount + m_state.bladeAngle));
        
        bool showTarget = bladeDist < targetRadius;
        
        // Smooth edge
        if (abs(bladeDist - targetRadius) < 2.0f) {
            uint8_t blend = (1.0f - abs(bladeDist - targetRadius) / 2.0f) * 255;
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], showTarget ? blend : 255 - blend);
        } else {
            m_outputBuffer[i] = showTarget ? m_targetBuffer[i] : m_sourceBuffer[i];
        }
    }
}

inline void TransitionEngine::applyNuclear() {
    // Copy source
    memcpy(m_outputBuffer, m_sourceBuffer, m_numLeds * sizeof(CRGB));
    
    // Expanding shockwave
    m_state.shockwaveRadius = m_progress * m_centerPoint * 1.5f;
    
    // Chain reactions
    for (uint8_t i = 0; i < m_state.reactionCount; i++) {
        uint16_t pos = m_state.chainReactions[i];
        float localRadius = (m_progress - i * 0.05f) * 20.0f;
        
        if (localRadius > 0) {
            for (uint16_t led = 0; led < m_numLeds; led++) {
                float dist = abs((int)led - (int)pos);
                if (dist < localRadius) {
                    float intensity = (1.0f - dist / localRadius) * (1.0f - m_progress);
                    CRGB flash = CRGB(255, 200, 100);
                    m_outputBuffer[led] = lerpColor(m_outputBuffer[led], flash, intensity * 255);
                }
            }
        }
    }
    
    // Main shockwave
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float dist = getDistanceFromCenter(i) * m_centerPoint;
        
        if (dist < m_state.shockwaveRadius) {
            // Inside shockwave - show target with radiation glow
            float radiation = sin(dist * 0.5f + m_progress * 10.0f) * 0.3f + 0.7f;
            CRGB glowColor = lerpColor(m_targetBuffer[i], CRGB(255, 100, 0), radiation * 100);
            m_outputBuffer[i] = glowColor;
        } else if (dist < m_state.shockwaveRadius + 5) {
            // Shockwave edge
            float edge = 1.0f - (dist - m_state.shockwaveRadius) / 5.0f;
            CRGB edgeColor = CRGB(255, 255, 200);
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], edgeColor, edge * 255);
        }
    }
}

inline void TransitionEngine::applyStargate() {
    // Event horizon effect
    float horizonRadius = m_state.eventHorizonRadius * (1.0f + 0.1f * sin(m_state.wormholePhase));
    
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float dist = getDistanceFromCenter(i) * m_centerPoint;
        
        if (dist < horizonRadius) {
            // Inside event horizon - swirling wormhole
            float swirl = sin(dist * 0.2f + m_state.wormholePhase + m_state.chevronAngle);
            uint8_t hue = (swirl * 30 + 160 + m_progress * 100); // Blue-purple
            CRGB wormholeColor = CHSV(hue, 255, 255);
            m_outputBuffer[i] = lerpColor(m_targetBuffer[i], wormholeColor, 128);
        } else if (dist < horizonRadius + 10) {
            // Event horizon edge with chevron indicators
            float edgeDist = dist - horizonRadius;
            float chevronGlow = 0;
            
            // Calculate chevron positions
            for (uint8_t c = 0; c < m_state.activeChevrons; c++) {
                float chevAngle = (c * TWO_PI / 7) + m_state.chevronAngle;
                float chevDist = sin(chevAngle) * edgeDist;
                if (abs(chevDist) < 2.0f) {
                    chevronGlow = max(chevronGlow, 1.0f - abs(chevDist) / 2.0f);
                }
            }
            
            CRGB edgeColor = lerpColor(CRGB(0, 50, 100), CRGB(100, 200, 255), chevronGlow);
            uint8_t blend = (1.0f - edgeDist / 10.0f) * 255;
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], edgeColor, blend);
        } else {
            m_outputBuffer[i] = m_sourceBuffer[i];
        }
    }
    
    // Update animation
    m_state.wormholePhase += 0.1f;
    m_state.chevronAngle += 0.02f;
}

inline void TransitionEngine::applyKaleidoscope() {
    // Crystal-like symmetric patterns
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float dist = getDistanceFromCenter(i) * m_centerPoint;
        float angle = atan2(i - m_centerPoint, dist + 1);
        
        // Apply symmetry fold
        float foldedAngle = fmod(abs(angle + m_state.rotationAngle), TWO_PI / m_state.symmetryFold);
        
        // Create kaleidoscope pattern
        float pattern = sin(dist * 0.1f + foldedAngle * 10) * 
                       cos(foldedAngle * m_state.symmetryFold);
        
        // Blend based on pattern and progress
        uint8_t blendAmount = (pattern * 0.5f + 0.5f) * m_progress * 255;
        
        // Add crystalline color shift
        CRGB crystalColor = m_targetBuffer[i];
        crystalColor.r = scale8(crystalColor.r, 200 + pattern * 55);
        crystalColor.g = scale8(crystalColor.g, 200 + pattern * 55);
        crystalColor.b = scale8(crystalColor.b, 255);
        
        m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], crystalColor, blendAmount);
    }
    
    m_state.rotationAngle += 0.02f;
}

inline void TransitionEngine::applyMandala() {
    // Sacred geometry patterns
    for (uint16_t i = 0; i < m_numLeds; i++) {
        float dist = getDistanceFromCenter(i) * m_centerPoint;
        
        // Calculate which ring this LED belongs to
        uint8_t ring = 0;
        float ringIntensity = 0;
        
        for (uint8_t r = 0; r < m_state.ringCount; r++) {
            float ringDist = abs(dist - m_state.ringRadii[r]);
            if (ringDist < 3.0f) {
                ring = r;
                ringIntensity = 1.0f - ringDist / 3.0f;
                break;
            }
        }
        
        // Apply mandala pattern
        if (ringIntensity > 0) {
            float angle = atan2(i - m_centerPoint, 1);
            float pattern = sin(angle * (ring + 3) + m_state.mandalaPhase * (ring + 1));
            
            uint8_t hue = (ring * 30 + pattern * 20 + m_progress * 100);
            CRGB mandalaColor = CHSV(hue, 200, 255 * ringIntensity);
            
            uint8_t blend = m_progress * 255 * ringIntensity;
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], 
                                        lerpColor(m_targetBuffer[i], mandalaColor, 128), 
                                        blend);
        } else {
            // Fade between source and target
            uint8_t fadeBlend = m_progress * m_progress * 255;
            m_outputBuffer[i] = lerpColor(m_sourceBuffer[i], m_targetBuffer[i], fadeBlend);
        }
    }
    
    m_state.mandalaPhase += 0.05f;
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

inline void TransitionEngine::initializePulsewave() {
    m_state.pulseCount = 0;
    m_state.lastPulse = 0;
    // Initial pulse from center
    m_state.pulses[0].radius = 0;
    m_state.pulses[0].intensity = 1.0f;
    m_state.pulses[0].velocity = 3.0f;
    m_state.pulseCount = 1;
}

inline void TransitionEngine::initializeImplosion() {
    // Create particles at edges converging to center
    for (uint8_t i = 0; i < 30; i++) {
        auto& p = m_state.implodeParticles[i];
        p.radius = m_centerPoint + random8(20, 40);
        p.angle = (i * TWO_PI / 30) + random8() * 0.1f;
        p.velocity = 1.0f + random8() * 0.02f;
        p.hue = random8();
        p.brightness = 200 + random8(55);
    }
}

inline void TransitionEngine::initializeIris() {
    m_state.irisRadius = 0;
    m_state.bladeCount = 6; // Hexagonal iris
    m_state.bladeAngle = 0;
}

inline void TransitionEngine::initializeNuclear() {
    m_state.shockwaveRadius = 0;
    m_state.radiationIntensity = 1.0f;
    m_state.reactionCount = 0;
    
    // Generate chain reaction points
    for (uint8_t i = 0; i < 5; i++) {
        m_state.chainReactions[i] = m_centerPoint + random16(40) - 20;
        m_state.reactionCount++;
    }
}

inline void TransitionEngine::initializeStargate() {
    m_state.eventHorizonRadius = 0;
    m_state.chevronAngle = 0;
    m_state.activeChevrons = 0;
    m_state.wormholePhase = 0;
}

inline void TransitionEngine::initializeKaleidoscope() {
    m_state.symmetryFold = 6; // 6-fold symmetry
    m_state.rotationAngle = 0;
}

inline void TransitionEngine::initializeMandala() {
    m_state.mandalaPhase = 0;
    m_state.ringCount = 5;
    
    // Create concentric rings
    for (uint8_t i = 0; i < m_state.ringCount; i++) {
        m_state.ringRadii[i] = (i + 1) * m_centerPoint / (m_state.ringCount + 1);
    }
}

inline TransitionType TransitionEngine::getRandomTransition() {
    // Weighted random selection for variety - CENTER ORIGIN ONLY
    uint8_t weights[] = {
        20,  // FADE
        15,  // WIPE_OUT - from center
        15,  // WIPE_IN - to center
        10,  // DISSOLVE
        6,   // SHATTER - from center
        4,   // GLITCH
        2,   // PHASE_SHIFT
        8,   // PULSEWAVE - energy rings
        7,   // IMPLOSION - particles collapse
        6,   // IRIS - mechanical aperture
        5,   // NUCLEAR - chain reaction
        4,   // STARGATE - wormhole portal
        3,   // KALEIDOSCOPE - crystal patterns
        2    // MANDALA - sacred geometry
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