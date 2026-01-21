/**
 * @file NarrativeEngine.h
 * @brief Global temporal conductor for visual drama
 *
 * LightwaveOS v2 - Narrative Engine
 *
 * Manages dramatic timing (BUILD -> HOLD -> RELEASE -> REST) that all effects
 * can query. Supports per-zone phase offsets for spatial choreography.
 *
 * Usage in effects:
 *   float intensity = NARRATIVE.getIntensity();
 *
 * Usage with zone offset:
 *   float intensity = NARRATIVE.getIntensity(zoneId);
 */

#pragma once

#include <Arduino.h>
#include "../EffectTypes.h"

namespace lightwaveos {
namespace narrative {

using namespace lightwaveos::effects;

/**
 * @brief Global narrative timing engine
 *
 * Singleton that provides dramatic timing arc for visual effects.
 * Can be enabled/disabled at runtime.
 */
class NarrativeEngine {
public:
    // Singleton access
    static NarrativeEngine& getInstance() {
        static NarrativeEngine instance;
        return instance;
    }

    // ==================== Core Update ====================

    /**
     * @brief Update the narrative state machine
     * Call once per frame in main loop
     */
    void update();

    // ==================== Enable/Disable ====================

    void enable();
    void disable();
    bool isEnabled() const { return m_enabled; }

    // ==================== Configuration - Durations ====================

    void setBuildDuration(float seconds);
    void setHoldDuration(float seconds);
    void setReleaseDuration(float seconds);
    void setRestDuration(float seconds);

    /**
     * @brief Scale all phases to hit target total cycle duration
     */
    void setTempo(float totalCycleDuration);

    // Getters
    float getBuildDuration() const { return m_cycle.buildDuration; }
    float getHoldDuration() const { return m_cycle.holdDuration; }
    float getReleaseDuration() const { return m_cycle.releaseDuration; }
    float getRestDuration() const { return m_cycle.restDuration; }
    float getTotalDuration() const { return m_cycle.getTotalDuration(); }
    EasingCurve getBuildCurve() const { return m_cycle.buildCurve; }
    EasingCurve getReleaseCurve() const { return m_cycle.releaseCurve; }
    float getHoldBreathe() const { return m_cycle.holdBreathe; }
    float getSnapAmount() const { return m_cycle.snapAmount; }
    float getDurationVariance() const { return m_cycle.durationVariance; }

    // ==================== Configuration - Curves ====================

    void setBuildCurve(EasingCurve curve);
    void setReleaseCurve(EasingCurve curve);
    void setHoldBreathe(float amount);      // 0-1: oscillation during hold
    void setSnapAmount(float amount);       // 0-1: tanh punch at transitions
    void setDurationVariance(float amount); // 0-1: randomizes cycle length

    // ==================== Per-Zone Offsets ====================

    /**
     * @brief Set phase offset for a zone (0-1, fraction of cycle)
     */
    void setZonePhaseOffset(uint8_t zoneId, float offsetRatio);
    float getZonePhaseOffset(uint8_t zoneId) const;

    // ==================== Query Methods ====================

    /**
     * @brief Get global intensity (no zone offset) - returns 0-1
     * Alias: getTension() for v1 compatibility
     */
    float getIntensity() const;
    float getTension() const { return getIntensity(); }  // v1 compatibility alias

    /**
     * @brief Get zone-specific intensity (applies phase offset) - returns 0-1
     */
    float getIntensity(uint8_t zoneId) const;

    /**
     * @brief Get tempo multiplier based on tension (1.0-1.5x)
     * Formula: multiplier = 1.0 + (tension * 0.5)
     * @return Tempo multiplier
     */
    float getTempoMultiplier() const;

    /**
     * @brief Get complexity scaling factor based on tension (0.5-1.0)
     * Formula: complexity = baseComplexity * (0.5 + tension * 0.5)
     * @return Complexity scaling factor
     */
    float getComplexityScaling() const;

    /**
     * @brief Get current phase
     */
    NarrativePhase getPhase() const;
    NarrativePhase getPhase(uint8_t zoneId) const;

    /**
     * @brief Get progress within current phase (0-1)
     * Alias: getPhaseProgress() for v1 compatibility
     */
    float getPhaseT() const;
    float getPhaseProgress() const { return getPhaseT(); }  // v1 compatibility alias
    float getPhaseT(uint8_t zoneId) const;

    /**
     * @brief Get progress through entire cycle (0-1)
     */
    float getCycleT() const;
    float getCycleT(uint8_t zoneId) const;

    /**
     * @brief Edge detection - true for one frame when entering phase
     */
    bool justEntered(NarrativePhase phase) const;

    /**
     * @brief Check if currently in specific phase
     * @param phase Phase to check
     * @return true if in specified phase
     */
    bool isIn(NarrativePhase phase) const { return getPhase() == phase; }

    // ==================== Manual Control ====================

    void trigger();   // Force restart from BUILD
    void pause();
    void resume();
    void reset();

    /**
     * @brief Set manual tension override (for testing/debugging)
     * @param tension Override value (0.0-1.0), or -1.0 to disable override
     */
    void setTensionOverride(float tension);

    /**
     * @brief Transition to new narrative phase with specified duration (v1 compatibility)
     * @param phase NarrativePhase enum (BUILD|HOLD|RELEASE|REST)
     * @param durationMs Phase duration in milliseconds (100-60000)
     */
    void setPhase(NarrativePhase phase, uint32_t durationMs);

    // ==================== Debug ====================

    void printStatus() const;

private:
    NarrativeEngine();
    NarrativeEngine(const NarrativeEngine&) = delete;
    NarrativeEngine& operator=(const NarrativeEngine&) = delete;

    // Internal calculations
    float getIntensityAtCycleT(float cycleT) const;
    NarrativePhase getPhaseAtCycleT(float cycleT) const;
    float getPhaseTAtCycleT(float cycleT) const;

    // Global cycle state
    NarrativeCycle m_cycle;

    // Per-zone phase offsets (0-1)
    static constexpr uint8_t MAX_ZONES = 4;
    float m_zoneOffsets[MAX_ZONES] = {0};

    // Edge detection
    NarrativePhase m_lastPhase = PHASE_REST;
    NarrativePhase m_justEnteredPhase = PHASE_REST;
    bool m_phaseJustChanged = false;

    // Control state
    bool m_enabled = false;
    bool m_paused = false;
    uint32_t m_pauseStartMs = 0;
    uint32_t m_totalPausedMs = 0;

    // v1 NarrativeTension compatibility
    float m_tensionOverride = -1.0f;  // Manual override value (-1.0 = disabled)
    bool m_manualPhaseControl = false; // True when using manual phase control
};

} // namespace narrative
} // namespace lightwaveos

// Convenience macro
#define NARRATIVE lightwaveos::narrative::NarrativeEngine::getInstance()
