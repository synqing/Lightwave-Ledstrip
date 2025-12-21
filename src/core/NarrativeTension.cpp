/**
 * Narrative Tension Engine Implementation
 */

#include "NarrativeTension.h"
#include "../config/hardware_config.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

// Singleton instance
NarrativeTension* NarrativeTension::s_instance = nullptr;

// Global accessor (initialized in main.cpp)
NarrativeTension* g_narrativeTension = nullptr;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

NarrativeTension& NarrativeTension::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new NarrativeTension();
    }
    return *s_instance;
}

void NarrativeTension::setInstance(NarrativeTension* instance) {
    s_instance = instance;
}

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

NarrativeTension::NarrativeTension()
    : m_tension(0.0f)
    , m_phase(PHASE_BUILD)
    , m_phaseStartMs(0)
    , m_phaseDurationMs(15000)  // Default 15 second build
    , m_initialized(false)
    , m_enabled(true)
    , m_tensionOverride(-1.0f)
    , m_holdBreathe(0.2f)  // Default 20% breathe amplitude
{
    // Initialize will be called explicitly
}

void NarrativeTension::initialize() {
    m_phase = PHASE_BUILD;
    m_phaseStartMs = millis();
    m_phaseDurationMs = 15000;
    m_tension = 0.0f;
    m_initialized = true;
    m_enabled = true;
    m_tensionOverride = -1.0f;
}

void NarrativeTension::reset() {
    initialize();
}

// ============================================================================
// PHASE CONTROL
// ============================================================================

void NarrativeTension::setPhase(NarrativePhase phase, uint32_t durationMs) {
    // Validate phase
    if (phase > PHASE_REST) {
        phase = PHASE_BUILD;  // Clamp to valid range
    }
    
    // Clamp duration to valid range (100ms - 60000ms)
    if (durationMs < 100) {
        durationMs = 100;
    } else if (durationMs > 60000) {
        durationMs = 60000;
    }
    
    m_phase = phase;
    m_phaseStartMs = millis();
    m_phaseDurationMs = durationMs;
    m_initialized = true;
    
    // Immediately update tension for new phase
    update();
}

float NarrativeTension::getPhaseProgress() const {
    if (!m_initialized || m_phaseDurationMs == 0) {
        return 0.0f;
    }
    
    uint32_t now = millis();
    uint32_t elapsed;
    
    // Handle timer overflow
    if (now < m_phaseStartMs) {
        // Timer wrapped, use maximum duration
        elapsed = m_phaseDurationMs;
    } else {
        elapsed = now - m_phaseStartMs;
    }
    
    float progress = (float)elapsed / (float)m_phaseDurationMs;
    return clamp(progress, 0.0f, 1.0f);
}

// ============================================================================
// TENSION QUERIES
// ============================================================================

float NarrativeTension::getTension() const {
    // Manual override takes precedence
    if (m_tensionOverride >= 0.0f) {
        return clamp(m_tensionOverride, 0.0f, 1.0f);
    }
    
    if (!m_enabled || !m_initialized) {
        return 0.0f;
    }
    
    return clamp(m_tension, 0.0f, 1.0f);
}

float NarrativeTension::getTempoMultiplier() const {
    float tension = getTension();
    // Formula: multiplier = 1.0 + (tension * 0.5)
    return 1.0f + (tension * 0.5f);
}

float NarrativeTension::getComplexityScaling() const {
    float tension = getTension();
    // Formula: complexity = baseComplexity * (0.5 + tension * 0.5)
    return 0.5f + (tension * 0.5f);
}

// ============================================================================
// UPDATE & CONTROL
// ============================================================================

void NarrativeTension::update() {
    if (!m_enabled) {
        return;
    }
    
    // Validate and recover from invalid state
    validateAndRecover();
    
    if (!m_initialized) {
        initialize();
        return;
    }
    
    float phaseProgress = getPhaseProgress();
    
    // Calculate tension based on current phase
    switch (m_phase) {
        case PHASE_BUILD:
            m_tension = calculateBuildTension(phaseProgress);
            break;
            
        case PHASE_HOLD:
            m_tension = calculateHoldTension(phaseProgress);
            break;
            
        case PHASE_RELEASE:
            m_tension = calculateReleaseTension(phaseProgress);
            break;
            
        case PHASE_REST:
            m_tension = calculateRestTension(phaseProgress);
            break;
    }
    
    // Clamp tension to valid range
    m_tension = clamp(m_tension, 0.0f, 1.0f);
    
    // Check for phase transition
    if (phaseProgress >= 1.0f) {
        advancePhase();
    }
}

void NarrativeTension::setTensionOverride(float tension) {
    if (tension < 0.0f) {
        m_tensionOverride = -1.0f;  // Disable override
    } else {
        m_tensionOverride = clamp(tension, 0.0f, 1.0f);
    }
}

void NarrativeTension::trigger() {
    setPhase(PHASE_BUILD, m_phaseDurationMs);
}

// ============================================================================
// INTERNAL METHODS - TENSION CALCULATIONS
// ============================================================================

float NarrativeTension::calculateBuildTension(float phaseProgress) const {
    // Exponential rise: 0.0 -> 1.0
    // Using EASE_IN_QUAD: t²
    float t = clamp(phaseProgress, 0.0f, 1.0f);
    return t * t;
}

float NarrativeTension::calculateHoldTension(float phaseProgress) const {
    // Plateau with micro-variations: 0.8-1.0
    float baseTension = 0.9f;  // Slight below peak for visual interest
    
    if (m_holdBreathe > 0.0f) {
        // Sine wave oscillation during hold
        float breathe = sin(phaseProgress * TWO_PI) * m_holdBreathe * 0.1f;
        return clamp(baseTension + breathe, 0.8f, 1.0f);
    }
    
    return baseTension;
}

float NarrativeTension::calculateReleaseTension(float phaseProgress) const {
    // Exponential decay: 1.0 -> 0.2
    // Using EASE_OUT_QUAD: 1 - (1-t)²
    float t = clamp(phaseProgress, 0.0f, 1.0f);
    float eased = 1.0f - (1.0f - t) * (1.0f - t);
    
    // Scale to 0.2-1.0 range
    return 0.2f + (eased * 0.8f);
}

float NarrativeTension::calculateRestTension(float phaseProgress) const {
    // Near-zero with subtle drift: 0.0-0.2
    float baseTension = 0.1f;
    float drift = sin(phaseProgress * TWO_PI * 0.5f) * 0.1f;  // Slow oscillation
    return clamp(baseTension + drift, 0.0f, 0.2f);
}

// ============================================================================
// INTERNAL METHODS - UTILITIES
// ============================================================================

float NarrativeTension::clamp(float value, float min, float max) const {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void NarrativeTension::advancePhase() {
    // Advance to next phase in cycle
    switch (m_phase) {
        case PHASE_BUILD:
            m_phase = PHASE_HOLD;
            break;
            
        case PHASE_HOLD:
            m_phase = PHASE_RELEASE;
            break;
            
        case PHASE_RELEASE:
            m_phase = PHASE_REST;
            break;
            
        case PHASE_REST:
            // Cycle complete - restart BUILD
            m_phase = PHASE_BUILD;
            break;
    }
    
    // Reset phase timing
    m_phaseStartMs = millis();
}

void NarrativeTension::validateAndRecover() {
    // Validate phase
    if (m_phase > PHASE_REST) {
        // Invalid phase - reset to safe state
        m_phase = PHASE_BUILD;
        reset();
        return;
    }
    
    // Handle timer overflow
    uint32_t now = millis();
    if (now < m_phaseStartMs && m_phaseStartMs > 0x7FFFFFFF) {
        // Timer wrapped - reset phase timing
        m_phaseStartMs = now;
    }
    
    // Validate duration
    if (m_phaseDurationMs == 0) {
        m_phaseDurationMs = 15000;  // Default duration
    }
}

