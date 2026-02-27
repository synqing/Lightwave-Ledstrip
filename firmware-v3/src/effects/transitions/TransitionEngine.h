// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file TransitionEngine.h
 * @brief Smooth transitions between effects using CENTER ORIGIN animations
 *
 * LightwaveOS v2 - Transition System
 *
 * The TransitionEngine manages animated crossfades between effects using
 * a three-buffer model (source + target → output) and 12 transition types.
 *
 * State Machine:
 *   IDLE → startTransition() → ACTIVE → update() until complete → IDLE
 *
 * Buffer Architecture:
 *   Source buffer: Old effect (frozen at transition start)
 *   Target buffer: New effect (rendered fresh)
 *   Output buffer: Interpolated result (written to LED buffer)
 */

#pragma once

#include <FastLED.h>
#include "Easing.h"
#include "TransitionTypes.h"

namespace lightwaveos {
namespace transitions {

// ==================== Constants ====================

static constexpr uint16_t STRIP_LENGTH = 160;
static constexpr uint16_t TOTAL_LEDS = 320;
static constexpr uint8_t CENTER_POINT = 79;

// Dissolve configuration
static constexpr uint16_t MAX_DISSOLVE_PIXELS = TOTAL_LEDS;

// Implosion particles
static constexpr uint8_t MAX_PARTICLES = 30;

// Pulsewave rings
static constexpr uint8_t MAX_PULSES = 5;

// ==================== Particle for Implosion ====================

struct Particle {
    float position;       // 0-159 position on strip
    float velocity;       // Speed toward center
    uint8_t strip;        // 0 or 1
    bool active;
};

// ==================== Pulse for Pulsewave ====================

struct Pulse {
    float radius;         // Distance from center (0-79)
    float intensity;      // Brightness (0-1)
    bool active;
};

// ==================== TransitionEngine Class ====================

class TransitionEngine {
public:
    TransitionEngine();
    ~TransitionEngine() = default;

    // ==================== Transition Control ====================

    /**
     * @brief Start a transition between two effects
     * @param sourceBuffer Buffer containing old effect (will be copied)
     * @param targetBuffer Buffer containing new effect
     * @param outputBuffer Buffer for interpolated output
     * @param type Transition type
     * @param durationMs Transition duration in milliseconds
     * @param curve Easing curve to use
     */
    void startTransition(const CRGB* sourceBuffer,
                         const CRGB* targetBuffer,
                         CRGB* outputBuffer,
                         TransitionType type,
                         uint16_t durationMs,
                         EasingCurve curve);

    /**
     * @brief Start transition with default duration/easing
     */
    void startTransition(const CRGB* sourceBuffer,
                         const CRGB* targetBuffer,
                         CRGB* outputBuffer,
                         TransitionType type);

    /**
     * @brief Update transition state (call each frame)
     * @return true if transition still active, false when complete
     */
    bool update();

    /**
     * @brief Cancel current transition
     */
    void cancel();

    // ==================== State Queries ====================

    bool isActive() const { return m_active; }
    float getProgress() const { return m_progress; }
    TransitionType getType() const { return m_type; }
    uint32_t getElapsedMs() const;
    uint32_t getRemainingMs() const;

    // ==================== Random Transition ====================

    /**
     * @brief Get a random transition type (weighted distribution)
     */
    static TransitionType getRandomTransition();

private:
    // ==================== State Machine ====================

    bool m_active;
    float m_progress;         // Eased progress (0.0 - 1.0)
    float m_rawProgress;      // Linear progress (0.0 - 1.0)
    TransitionType m_type;
    EasingCurve m_curve;
    uint32_t m_startTime;
    uint16_t m_durationMs;

    // ==================== Buffers ====================

    CRGB m_sourceBuffer[TOTAL_LEDS];  // Copy of old effect
    CRGB m_targetBuffer[TOTAL_LEDS];  // Copy of new effect (NOT pointer - prevents aliasing!)
    CRGB* m_outputBuffer;              // Pointer to output

    // ==================== Effect-Specific State ====================

    // Dissolve
    uint16_t m_dissolveOrder[MAX_DISSOLVE_PIXELS];

    // Implosion
    Particle m_particles[MAX_PARTICLES];

    // Pulsewave
    Pulse m_pulses[MAX_PULSES];
    uint8_t m_activePulses;

    // Iris
    float m_irisRadius;

    // Nuclear
    float m_shockwaveRadius;
    float m_radiationIntensity;

    // Stargate
    float m_eventHorizonRadius;
    float m_chevronAngle;

    // Kaleidoscope
    uint8_t m_foldCount;
    float m_rotationAngle;

    // Mandala
    float m_ringPhases[5];

    // ==================== Helper Methods ====================

    float getDistanceFromCenter(uint16_t index) const;
    CRGB lerpColor(const CRGB& from, const CRGB& to, uint8_t blend) const;

    // ==================== Initialization ====================

    void initDissolve();
    void initImplosion();
    void initPulsewave();
    void initNuclear();
    void initStargate();
    void initKaleidoscope();
    void initMandala();

    // ==================== Transition Implementations ====================

    void applyFade();
    void applyWipeOut();
    void applyWipeIn();
    void applyDissolve();
    void applyPhaseShift();
    void applyPulsewave();
    void applyImplosion();
    void applyIris();
    void applyNuclear();
    void applyStargate();
    void applyKaleidoscope();
    void applyMandala();
};

} // namespace transitions
} // namespace lightwaveos
