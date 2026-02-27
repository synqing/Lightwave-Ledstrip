// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file MotionEngine.cpp
 * @brief Implementation of advanced motion control engine
 *
 * Ported from LightwaveOS v1 to v2 architecture.
 */

#include "MotionEngine.h"

#if FEATURE_MOTION_ENGINE

namespace lightwaveos {
namespace enhancement {

// ============================================================================
// PHASE CONTROLLER
// ============================================================================

void PhaseController::update(float deltaTime) {
    if (autoRotate) {
        stripPhaseOffset += phaseVelocity * deltaTime;

        // Wrap phase to 0-360 range
        while (stripPhaseOffset >= 360.0f) {
            stripPhaseOffset -= 360.0f;
        }
        while (stripPhaseOffset < 0.0f) {
            stripPhaseOffset += 360.0f;
        }
    }
}

void PhaseController::setStripPhaseOffset(float degrees) {
    stripPhaseOffset = degrees;
    // Normalize to 0-360 range
    while (stripPhaseOffset >= 360.0f) {
        stripPhaseOffset -= 360.0f;
    }
    while (stripPhaseOffset < 0.0f) {
        stripPhaseOffset += 360.0f;
    }
}

float PhaseController::getStripPhaseRadians() const {
    return stripPhaseOffset * DEG_TO_RAD;
}

void PhaseController::enableAutoRotate(float degreesPerSecond) {
    autoRotate = true;
    phaseVelocity = degreesPerSecond;
}

uint16_t PhaseController::applyPhaseOffset(uint16_t index, uint16_t ledCount) const {
    if (ledCount == 0) return 0;

    // Convert phase offset to LED index offset
    // 360 degrees = full strip length
    float offsetFraction = stripPhaseOffset / 360.0f;
    int16_t ledOffset = (int16_t)(offsetFraction * ledCount);

    int16_t newIndex = ((int16_t)index + ledOffset) % (int16_t)ledCount;
    if (newIndex < 0) {
        newIndex += ledCount;
    }

    return (uint16_t)newIndex;
}

uint32_t PhaseController::applyAutoRotation(uint32_t timeMs) const {
    if (!autoRotate || phaseVelocity == 0.0f) {
        return timeMs;
    }

    // Apply phase offset as time shift
    // phaseVelocity is in degrees/second
    // Convert to time offset (360 degrees = 1 second at 360 deg/sec)
    float timeShiftMs = (stripPhaseOffset / phaseVelocity) * 1000.0f;

    return timeMs + (uint32_t)timeShiftMs;
}

// ============================================================================
// MOMENTUM ENGINE
// ============================================================================

void MomentumEngine::reset() {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        m_particles[i].active = false;
    }
    m_activeParticleCount = 0;
}

int MomentumEngine::addParticle(float pos, float vel, float mass,
                                 CRGB color, BoundaryMode boundary) {
    // Find free particle slot
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!m_particles[i].active) {
            m_particles[i].position = pos;
            m_particles[i].velocity = vel;
            m_particles[i].acceleration = 0.0f;
            m_particles[i].mass = mass;
            m_particles[i].drag = 0.98f;  // Default drag
            m_particles[i].color = color;
            m_particles[i].active = true;
            m_particles[i].boundaryMode = boundary;
            m_activeParticleCount++;
            return (int)i;
        }
    }
    return -1;  // No free slots
}

void MomentumEngine::applyForce(int particleId, float force) {
    if (particleId < 0 || particleId >= MAX_PARTICLES) return;
    if (!m_particles[particleId].active) return;

    // F = ma, so a = F/m
    m_particles[particleId].acceleration += force / m_particles[particleId].mass;
}

void MomentumEngine::update(float deltaTime) {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!m_particles[i].active) continue;

        Particle& p = m_particles[i];

        // Euler integration
        p.velocity += p.acceleration * deltaTime;
        p.velocity *= p.drag;  // Apply drag
        p.position += p.velocity * deltaTime;

        // Reset acceleration (forces are applied per frame)
        p.acceleration = 0.0f;

        // Boundary conditions
        switch (p.boundaryMode) {
            case BOUNDARY_WRAP:
                if (p.position > 1.0f) {
                    p.position = fmodf(p.position, 1.0f);
                } else if (p.position < 0.0f) {
                    p.position = 1.0f + fmodf(p.position, 1.0f);
                }
                break;

            case BOUNDARY_BOUNCE:
                if (p.position > 1.0f) {
                    p.position = 1.0f - (p.position - 1.0f);
                    p.velocity = -p.velocity;
                } else if (p.position < 0.0f) {
                    p.position = -p.position;
                    p.velocity = -p.velocity;
                }
                break;

            case BOUNDARY_CLAMP:
                if (p.position > 1.0f) {
                    p.position = 1.0f;
                    p.velocity = 0.0f;
                } else if (p.position < 0.0f) {
                    p.position = 0.0f;
                    p.velocity = 0.0f;
                }
                break;

            case BOUNDARY_DIE:
                if (p.position > 1.0f || p.position < 0.0f) {
                    p.active = false;
                    if (m_activeParticleCount > 0) {
                        m_activeParticleCount--;
                    }
                }
                break;
        }
    }
}

Particle* MomentumEngine::getParticle(int particleId) {
    if (particleId < 0 || particleId >= MAX_PARTICLES) return nullptr;
    return &m_particles[particleId];
}

// ============================================================================
// SPEED MODULATOR
// ============================================================================

SpeedModulator::SpeedModulator(float base)
    : m_type(MOD_CONSTANT)
    , m_baseSpeed(base)
    , m_modulationDepth(0.5f)
    , m_phase(0.0f)
{}

void SpeedModulator::setModulation(ModulationType mod, float depth) {
    m_type = mod;
    m_modulationDepth = depth;
    if (mod != MOD_CONSTANT) {
        m_phase = 0.0f;  // Reset phase when changing mode
    }
}

float SpeedModulator::getSpeed(float deltaTime) {
    m_phase += deltaTime;

    float modulation = 0.0f;
    switch (m_type) {
        case MOD_CONSTANT:
            return m_baseSpeed;

        case MOD_SINE_WAVE:
            // Oscillate between (1-depth) and (1+depth) of base speed
            modulation = sinf(m_phase * TWO_PI) * m_modulationDepth;
            break;

        case MOD_EXPONENTIAL_DECAY:
            // Decay from 1.0 toward (1-depth) over time
            modulation = expf(-m_phase * m_modulationDepth) - 0.5f;
            break;
    }

    return m_baseSpeed * (1.0f + modulation);
}

void SpeedModulator::setBaseSpeed(float speed) {
    m_baseSpeed = speed;
}

// ============================================================================
// MAIN MOTION ENGINE
// ============================================================================

MotionEngine::MotionEngine()
    : m_lastUpdateTime(0)
    , m_deltaTime(0.0f)
    , m_enabled(false)
{}

void MotionEngine::enable() {
    m_enabled = true;
    m_lastUpdateTime = millis();
}

void MotionEngine::disable() {
    m_enabled = false;
}

void MotionEngine::update() {
    if (!m_enabled) return;

    // Calculate delta time
    uint32_t now = millis();
    m_deltaTime = (now - m_lastUpdateTime) * 0.001f;  // Convert to seconds
    m_lastUpdateTime = now;

    // Clamp delta time to avoid physics explosions after long pauses
    if (m_deltaTime > 0.1f) {
        m_deltaTime = 0.1f;
    }

    // Update subsystems
    m_phaseCtrl.update(m_deltaTime);
    m_momentumEngine.update(m_deltaTime);
}

} // namespace enhancement
} // namespace lightwaveos

#endif // FEATURE_MOTION_ENGINE
