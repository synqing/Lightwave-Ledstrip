/**
 * @file CascadedSpringEffect.h
 * @brief Ultra-smooth audio-reactive wave using cascaded spring physics
 *
 * Effect ID: TBD (audio demo)
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE | PHYSICS
 *
 * MATHEMATICAL BASIS:
 * Two cascaded spring followers create ultra-smooth momentum:
 * - Spring 1 (fast, k=100): Tracks raw audio energy with ~100ms response
 * - Spring 2 (slow, k=25): Smooths Spring 1's output with ~400ms response
 *
 * Spring Physics: F = -kx - bv where b = 2*sqrt(k*m) for critical damping
 *
 * The cascade produces:
 * - Natural momentum and overshoot on audio transients
 * - Silky smooth decay without jitter
 * - Frame-rate independent behavior via dt integration
 *
 * Instance State:
 * - m_spring1, m_spring2: Cascaded spring followers
 * - m_phase: Wave animation phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

/**
 * @brief Critically damped spring follower for smooth value tracking
 *
 * Implements F = -kx - bv with critical damping b = 2*sqrt(k*m)
 * This provides the fastest approach to target WITHOUT overshoot.
 */
class SpringFollower {
private:
    float m_position = 0.0f;
    float m_velocity = 0.0f;
    float m_stiffness;
    float m_mass;
    float m_damping;

public:
    /**
     * @brief Construct a critically-damped spring
     * @param stiffness Spring stiffness (higher = faster response)
     * @param mass Spring mass (higher = more inertia/momentum)
     *
     * Response time ~= 4 * sqrt(m/k) seconds to settle within 2%
     * - k=100, m=1 -> ~400ms settle time
     * - k=25, m=1 -> ~800ms settle time
     */
    SpringFollower(float stiffness = 50.0f, float mass = 1.0f)
        : m_stiffness(stiffness), m_mass(mass) {
        // Critical damping: b = 2*sqrt(k*m) - fastest without overshoot
        m_damping = 2.0f * sqrtf(m_stiffness * m_mass);
    }

    /**
     * @brief Update spring physics toward target
     * @param target Target value to approach
     * @param dt Delta time in seconds (use ctx.getSafeDeltaSeconds())
     * @return Current smoothed position
     */
    float update(float target, float dt) {
        // Spring force: F = -k*displacement
        float displacement = m_position - target;
        float springForce = -m_stiffness * displacement;

        // Damping force: F = -b*velocity
        float dampingForce = -m_damping * m_velocity;

        // Newton: F = ma -> a = F/m
        float acceleration = (springForce + dampingForce) / m_mass;

        // Euler integration (stable for critical damping)
        m_velocity += acceleration * dt;
        m_position += m_velocity * dt;

        return m_position;
    }

    /**
     * @brief Reset spring to position with zero velocity
     */
    void reset(float newPosition = 0.0f) {
        m_position = newPosition;
        m_velocity = 0.0f;
    }

    /**
     * @brief Get current position
     */
    float getPosition() const { return m_position; }

    /**
     * @brief Get current velocity (useful for motion blur effects)
     */
    float getVelocity() const { return m_velocity; }
};

/**
 * @brief Cascaded spring effect for ultra-smooth audio reactivity
 *
 * Two springs in series create a second-order filter with natural
 * momentum and smooth decay. The visual result is:
 * - Quick response to audio transients (via spring 1)
 * - Buttery smooth overall motion (via spring 2)
 * - No jitter even with noisy audio input
 */
class CascadedSpringEffect : public plugins::IEffect {
public:
    CascadedSpringEffect() = default;
    ~CascadedSpringEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Cascaded springs: raw audio -> spring1 -> spring2 -> visual
    // Spring 1: Fast response (k=100, ~100ms settle)
    SpringFollower m_spring1{100.0f, 1.0f};

    // Spring 2: Slower response (k=25, ~400ms settle)
    // This smooths spring1's output for ultra-smooth motion
    SpringFollower m_spring2{25.0f, 1.0f};

    // Wave animation phase accumulator
    float m_phase = 0.0f;

    // Track hop sequence for audio updates
    uint32_t m_lastHopSeq = 0;
    float m_targetEnergy = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
