/**
 * Narrative Tension Engine
 * 
 * Provides BUILD/HOLD/RELEASE/REST narrative phases with tension curves
 * for automatic tempo modulation and parameter scaling in light shows.
 * 
 * Based on specifications from:
 * - docs/storytelling/LGP_STORYTELLING_FRAMEWORK.md
 * - docs/analysis/RESEARCH_ANALYSIS_REPORT.md
 * 
 * Usage:
 *   NarrativeTension& tension = NarrativeTension::getInstance();
 *   tension.setPhase(PHASE_BUILD, 15000);  // 15 second build
 *   tension.update();  // Call every frame
 *   float t = tension.getTension();  // 0.0-1.0
 */

#ifndef NARRATIVE_TENSION_H
#define NARRATIVE_TENSION_H

#include <Arduino.h>
#include "EffectTypes.h"

class NarrativeTension {
public:
    // Singleton access
    static NarrativeTension& getInstance();
    
    // Dependency injection for testing
    static void setInstance(NarrativeTension* instance);
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * Initialize tension system with default phase (BUILD)
     */
    NarrativeTension();
    
    /**
     * Initialize/reset the tension system
     */
    void initialize();
    
    // ========================================================================
    // PHASE CONTROL
    // ========================================================================
    
    /**
     * Transition to new narrative phase with specified duration
     * @param phase NarrativePhase enum (BUILD|HOLD|RELEASE|REST)
     * @param durationMs Phase duration in milliseconds (100-60000)
     */
    void setPhase(NarrativePhase phase, uint32_t durationMs);
    
    /**
     * Get current narrative phase
     * @return Current NarrativePhase
     */
    NarrativePhase getPhase() const { return m_phase; }
    
    /**
     * Check if currently in specific phase
     * @param phase Phase to check
     * @return true if in specified phase
     */
    bool isIn(NarrativePhase phase) const { return m_phase == phase; }
    
    /**
     * Get progress within current phase (0.0-1.0)
     * @return Phase progress
     */
    float getPhaseProgress() const;
    
    // ========================================================================
    // TENSION QUERIES
    // ========================================================================
    
    /**
     * Get current tension value (0.0-1.0)
     * @return Tension value based on current phase and progress
     */
    float getTension() const;
    
    /**
     * Get tempo multiplier based on tension (1.0-1.5x)
     * Formula: multiplier = 1.0 + (tension * 0.5)
     * @return Tempo multiplier
     */
    float getTempoMultiplier() const;
    
    /**
     * Get intensity scaling factor based on tension
     * Formula: intensity = baseIntensity * tension
     * @return Intensity scaling factor (0.0-1.0)
     */
    float getIntensityScaling() const { return getTension(); }
    
    /**
     * Get complexity scaling factor based on tension
     * Formula: complexity = baseComplexity * (0.5 + tension * 0.5)
     * @return Complexity scaling factor (0.5-1.0)
     */
    float getComplexityScaling() const;
    
    // ========================================================================
    // UPDATE & CONTROL
    // ========================================================================
    
    /**
     * Update tension system (call every frame)
     * Updates tension based on phase progress and triggers phase transitions
     */
    void update();
    
    /**
     * Enable/disable tension modulation
     * @param enabled true to enable, false to disable
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * Check if tension system is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * Set manual tension override (for testing/debugging)
     * @param tension Override value (0.0-1.0), or -1.0 to disable override
     */
    void setTensionOverride(float tension);
    
    /**
     * Manual trigger - skip REST and force BUILD
     */
    void trigger();
    
    /**
     * Reset tension system to initial state
     */
    void reset();
    
private:
    // Singleton instance
    static NarrativeTension* s_instance;
    
    // Runtime state
    float m_tension;              // Current tension value (0.0-1.0)
    NarrativePhase m_phase;       // Current narrative phase
    uint32_t m_phaseStartMs;      // Phase start time (millis())
    uint32_t m_phaseDurationMs;   // Phase duration (milliseconds)
    bool m_initialized;           // System initialized flag
    bool m_enabled;               // Tension modulation enabled
    float m_tensionOverride;      // Manual override value (-1.0 = disabled)
    
    // Phase-specific configuration
    float m_holdBreathe;          // Oscillation amplitude during HOLD (0.0-1.0)
    
    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================
    
    /**
     * Calculate tension for BUILD phase
     * Exponential rise: 0.0 -> 1.0 using EASE_IN_QUAD
     * @param phaseProgress Progress within phase (0.0-1.0)
     * @return Tension value (0.0-1.0)
     */
    float calculateBuildTension(float phaseProgress) const;
    
    /**
     * Calculate tension for HOLD phase
     * Plateau with micro-variations: 0.8-1.0
     * @param phaseProgress Progress within phase (0.0-1.0)
     * @return Tension value (0.8-1.0)
     */
    float calculateHoldTension(float phaseProgress) const;
    
    /**
     * Calculate tension for RELEASE phase
     * Exponential decay: 1.0 -> 0.2 using EASE_OUT_QUAD
     * @param phaseProgress Progress within phase (0.0-1.0)
     * @return Tension value (0.2-1.0)
     */
    float calculateReleaseTension(float phaseProgress) const;
    
    /**
     * Calculate tension for REST phase
     * Near-zero with subtle drift: 0.0-0.2
     * @param phaseProgress Progress within phase (0.0-1.0)
     * @return Tension value (0.0-0.2)
     */
    float calculateRestTension(float phaseProgress) const;
    
    /**
     * Clamp value to valid range
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    float clamp(float value, float min, float max) const;
    
    /**
     * Handle phase transition (called when phase duration expires)
     */
    void advancePhase();
    
    /**
     * Validate and recover from invalid state
     */
    void validateAndRecover();
};

// Global accessor for effects (optional, for convenience)
extern NarrativeTension* g_narrativeTension;

#endif // NARRATIVE_TENSION_H

