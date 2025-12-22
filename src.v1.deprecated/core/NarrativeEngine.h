#ifndef NARRATIVE_ENGINE_H
#define NARRATIVE_ENGINE_H

#include <Arduino.h>
#include "../config/features.h"

#if FEATURE_NARRATIVE_ENGINE

#include "EffectTypes.h"
#include "../config/hardware_config.h"

/**
 * NarrativeEngine - Global temporal conductor for visual drama
 *
 * Manages dramatic timing (BUILD -> HOLD -> RELEASE -> REST) that all effects
 * can query. Supports per-zone phase offsets for spatial choreography.
 *
 * Architecture:
 *   main.cpp loop()
 *       ├── NarrativeEngine::getInstance().update()  <- TOP LAYER
 *       ├── MotionEngine::getInstance().update()
 *       └── effects query NarrativeEngine::getInstance().getIntensity()
 *
 * Usage in effects:
 *   float intensity = NarrativeEngine::getInstance().getIntensity();
 *
 * Usage with zone offset:
 *   float intensity = NarrativeEngine::getInstance().getIntensity(zoneId);
 */
class NarrativeEngine {
public:
    // Singleton access
    static NarrativeEngine& getInstance() {
        static NarrativeEngine instance;
        return instance;
    }

    // === Core update (call once per frame in main.cpp) ===
    void update();

    // === Enable/disable ===
    void enable();
    void disable();
    bool isEnabled() const { return m_enabled; }

    // === Configuration - Phase durations ===
    void setBuildDuration(float seconds);
    void setHoldDuration(float seconds);
    void setReleaseDuration(float seconds);
    void setRestDuration(float seconds);

    // Scale all phases proportionally to hit target total cycle duration
    void setTempo(float totalCycleDuration);

    // Get current phase durations
    float getBuildDuration() const { return m_cycle.buildDuration; }
    float getHoldDuration() const { return m_cycle.holdDuration; }
    float getReleaseDuration() const { return m_cycle.releaseDuration; }
    float getRestDuration() const { return m_cycle.restDuration; }
    float getTotalDuration() const { return m_cycle.getTotalDuration(); }

    // === Configuration - Curve behavior ===
    void setBuildCurve(EasingCurve curve);
    void setReleaseCurve(EasingCurve curve);
    void setHoldBreathe(float amount);      // 0-1: oscillation during hold
    void setSnapAmount(float amount);       // 0-1: tanh punch at transitions
    void setDurationVariance(float amount); // 0-1: randomizes cycle length

    // === Per-zone phase offsets (0-1, fraction of cycle) ===
    void setZonePhaseOffset(uint8_t zoneId, float offsetRatio);
    float getZonePhaseOffset(uint8_t zoneId) const;

    // === Query methods (what effects call) ===

    // Global intensity (no zone offset) - returns 0-1
    float getIntensity() const;

    // Zone-specific intensity (applies phase offset) - returns 0-1
    float getIntensity(uint8_t zoneId) const;

    // Current phase
    NarrativePhase getPhase() const;
    NarrativePhase getPhase(uint8_t zoneId) const;

    // Progress within current phase (0-1)
    float getPhaseT() const;
    float getPhaseT(uint8_t zoneId) const;

    // Progress through entire cycle (0-1)
    float getCycleT() const;
    float getCycleT(uint8_t zoneId) const;

    // Edge detection - true for one frame when entering phase
    bool justEntered(NarrativePhase phase) const;

    // === Manual control ===
    void trigger();   // Force restart from BUILD
    void pause();
    void resume();
    void reset();

    // === Debug ===
    void printStatus() const;

private:
    NarrativeEngine();
    NarrativeEngine(const NarrativeEngine&) = delete;
    NarrativeEngine& operator=(const NarrativeEngine&) = delete;

    // Internal: get intensity at arbitrary cycle position (for zone offsets)
    float getIntensityAtCycleT(float cycleT) const;
    NarrativePhase getPhaseAtCycleT(float cycleT) const;
    float getPhaseTAtCycleT(float cycleT) const;

    // Global cycle state
    NarrativeCycle m_cycle;

    // Per-zone phase offsets (0-1, fraction of cycle)
    float m_zoneOffsets[HardwareConfig::MAX_ZONES] = {0};

    // Edge detection
    NarrativePhase m_lastPhase = PHASE_REST;
    NarrativePhase m_justEnteredPhase = PHASE_REST;
    bool m_phaseJustChanged = false;

    // Control
    bool m_enabled = false;
    bool m_paused = false;
    uint32_t m_pauseStartMs = 0;
    uint32_t m_totalPausedMs = 0;
};

#endif // FEATURE_NARRATIVE_ENGINE

#endif // NARRATIVE_ENGINE_H
