/**
 * @file MotionEngine.h
 * @brief Advanced motion control for visual enhancements
 *
 * MotionEngine provides:
 * - Phase Controller: Strip phase offset and auto-rotation for dual-strip sync
 * - Momentum Engine: Physics-based particle system with boundaries
 * - Speed Modulator: Time-varying speed with sine wave and decay modes
 *
 * Ported from LightwaveOS v1 to v2 architecture with namespace isolation.
 *
 * Usage:
 * @code
 * auto& motionEngine = lightwaveos::enhancement::MotionEngine::getInstance();
 * motionEngine.enable();
 *
 * // Phase offset between strips
 * motionEngine.getPhaseController().setStripPhaseOffset(45.0f);  // 45 degrees
 *
 * // Auto-rotating phase
 * motionEngine.getPhaseController().enableAutoRotate(30.0f);  // 30 deg/sec
 *
 * // In render loop:
 * motionEngine.update();
 * float phase = motionEngine.getPhaseController().getStripPhaseRadians();
 * @endcode
 */

#pragma once

#include <FastLED.h>
#include <math.h>
#include "../../config/features.h"

// Default feature flag if not defined
#ifndef FEATURE_MOTION_ENGINE
#define FEATURE_MOTION_ENGINE 1
#endif

#if FEATURE_MOTION_ENGINE

namespace lightwaveos {
namespace enhancement {

// ============================================================================
// PHASE CONTROLLER
// ============================================================================

/**
 * @struct PhaseController
 * @brief Controls phase offset between strips and auto-rotation
 *
 * Use for creating interference patterns and synchronized movement
 * between Strip 1 and Strip 2.
 */
struct PhaseController {
    float stripPhaseOffset;      ///< Phase offset between strips (0-360 degrees)
    float phaseVelocity;         ///< Auto-rotation speed (degrees per second)
    bool autoRotate;             ///< Enable automatic phase rotation

    PhaseController()
        : stripPhaseOffset(0.0f)
        , phaseVelocity(0.0f)
        , autoRotate(false)
    {}

    /**
     * @brief Update phase based on delta time
     * @param deltaTime Time since last update (seconds)
     */
    void update(float deltaTime);

    /**
     * @brief Set phase offset between strips
     * @param degrees Offset in degrees (0-360)
     */
    void setStripPhaseOffset(float degrees);

    /**
     * @brief Get phase offset in radians
     * @return Phase offset in radians for use with sin/cos
     */
    float getStripPhaseRadians() const;

    /**
     * @brief Enable auto-rotation with specified speed
     * @param degreesPerSecond Rotation speed
     */
    void enableAutoRotate(float degreesPerSecond);

    /**
     * @brief Apply phase offset to LED index
     * @param index Original LED index
     * @param ledCount Total LED count
     * @return Phase-shifted index
     */
    uint16_t applyPhaseOffset(uint16_t index, uint16_t ledCount) const;

    /**
     * @brief Apply auto-rotation to animation time
     * @param timeMs Current time in milliseconds
     * @return Rotated time value
     */
    uint32_t applyAutoRotation(uint32_t timeMs) const;
};

// ============================================================================
// MOMENTUM ENGINE (PARTICLE PHYSICS)
// ============================================================================

/**
 * @enum BoundaryMode
 * @brief How particles behave at edges
 */
enum BoundaryMode : uint8_t {
    BOUNDARY_WRAP,    ///< Wrap around to other side
    BOUNDARY_BOUNCE,  ///< Bounce back (reverse velocity)
    BOUNDARY_CLAMP,   ///< Stop at edge
    BOUNDARY_DIE      ///< Deactivate particle
};

/**
 * @struct Particle
 * @brief Single particle with physics properties
 */
struct Particle {
    float position;              ///< Current position (0.0-1.0 normalized)
    float velocity;              ///< Current velocity (units per second)
    float acceleration;          ///< Current acceleration (units per second^2)
    float mass;                  ///< Particle mass (affects inertia)
    float drag;                  ///< Air resistance coefficient (0-1)
    bool active;                 ///< Is particle active?
    CRGB color;                  ///< Particle color
    BoundaryMode boundaryMode;   ///< How to handle edges

    Particle()
        : position(0.0f)
        , velocity(0.0f)
        , acceleration(0.0f)
        , mass(1.0f)
        , drag(0.98f)
        , active(false)
        , color(CRGB::White)
        , boundaryMode(BOUNDARY_WRAP)
    {}
};

/**
 * @class MomentumEngine
 * @brief Physics-based particle system
 *
 * Manages up to 32 particles with position, velocity, acceleration,
 * mass, drag, and boundary handling.
 */
class MomentumEngine {
public:
    static constexpr uint8_t MAX_PARTICLES = 32;

    MomentumEngine() : m_activeParticleCount(0) {}

    /**
     * @brief Reset all particles
     */
    void reset();

    /**
     * @brief Add a new particle
     * @param pos Initial position (0.0-1.0)
     * @param vel Initial velocity
     * @param mass Particle mass (default 1.0)
     * @param color Particle color
     * @param boundary Boundary behavior
     * @return Particle ID (0-31), or -1 if no slots available
     */
    int addParticle(float pos, float vel, float mass = 1.0f,
                    CRGB color = CRGB::White,
                    BoundaryMode boundary = BOUNDARY_WRAP);

    /**
     * @brief Apply force to a particle
     * @param particleId Particle ID from addParticle()
     * @param force Force to apply (F = ma)
     */
    void applyForce(int particleId, float force);

    /**
     * @brief Update all particles
     * @param deltaTime Time since last update (seconds)
     */
    void update(float deltaTime);

    /**
     * @brief Get particle by ID
     * @param particleId Particle ID
     * @return Pointer to particle, or nullptr if invalid
     */
    Particle* getParticle(int particleId);

    /**
     * @brief Get active particle count
     */
    uint8_t getActiveCount() const { return m_activeParticleCount; }

private:
    Particle m_particles[MAX_PARTICLES];
    uint8_t m_activeParticleCount;
};

// ============================================================================
// SPEED MODULATOR
// ============================================================================

/**
 * @class SpeedModulator
 * @brief Time-varying speed control
 *
 * Provides modulation modes for dynamic speed changes:
 * - Constant: Fixed speed
 * - Sine Wave: Oscillating speed
 * - Exponential Decay: Slowing down over time
 */
class SpeedModulator {
public:
    enum ModulationType : uint8_t {
        MOD_CONSTANT,           ///< Fixed speed
        MOD_SINE_WAVE,          ///< Oscillating speed
        MOD_EXPONENTIAL_DECAY   ///< Decreasing speed
    };

    /**
     * @brief Construct with base speed
     * @param base Base speed value (default 25.0)
     */
    explicit SpeedModulator(float base = 25.0f);

    /**
     * @brief Set modulation mode
     * @param mod Modulation type
     * @param depth Modulation depth (0.0-1.0)
     */
    void setModulation(ModulationType mod, float depth = 0.5f);

    /**
     * @brief Get current modulated speed
     * @param deltaTime Time since last call (seconds)
     * @return Current speed value
     */
    float getSpeed(float deltaTime);

    /**
     * @brief Set base speed
     * @param speed New base speed
     */
    void setBaseSpeed(float speed);

    /**
     * @brief Get base speed
     */
    float getBaseSpeed() const { return m_baseSpeed; }

private:
    ModulationType m_type;
    float m_baseSpeed;
    float m_modulationDepth;
    float m_phase;
};

// ============================================================================
// MAIN MOTION ENGINE
// ============================================================================

/**
 * @class MotionEngine
 * @brief Singleton providing advanced motion control
 *
 * Thread Safety: NOT thread-safe. Call only from Core 1 (render loop).
 * Memory: ~1.5 KB static (32 particles + controllers), no heap allocation.
 */
class MotionEngine {
public:
    /**
     * @brief Get singleton instance
     */
    static MotionEngine& getInstance() {
        static MotionEngine instance;
        return instance;
    }

    // Engine control
    void enable();
    void disable();
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Update all subsystems (call once per frame)
     */
    void update();

    // Access subsystems
    PhaseController& getPhaseController() { return m_phaseCtrl; }
    const PhaseController& getPhaseController() const { return m_phaseCtrl; }

    MomentumEngine& getMomentumEngine() { return m_momentumEngine; }
    const MomentumEngine& getMomentumEngine() const { return m_momentumEngine; }

    SpeedModulator& getSpeedModulator() { return m_speedMod; }
    const SpeedModulator& getSpeedModulator() const { return m_speedMod; }

    /**
     * @brief Get time since last update
     * @return Delta time in seconds
     */
    float getDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Apply phase offset to LED index (convenience method)
     */
    uint16_t applyPhaseOffset(uint16_t index, uint16_t ledCount) const {
        return m_phaseCtrl.applyPhaseOffset(index, ledCount);
    }

    /**
     * @brief Apply auto-rotation to time value (convenience method)
     */
    uint32_t applyAutoRotation(uint32_t timeMs) const {
        return m_phaseCtrl.applyAutoRotation(timeMs);
    }

private:
    // Private constructor (singleton pattern)
    MotionEngine();
    MotionEngine(const MotionEngine&) = delete;
    MotionEngine& operator=(const MotionEngine&) = delete;

    // Subsystems
    PhaseController m_phaseCtrl;
    MomentumEngine m_momentumEngine;
    SpeedModulator m_speedMod;

    // Timing
    uint32_t m_lastUpdateTime;
    float m_deltaTime;
    bool m_enabled;
};

} // namespace enhancement
} // namespace lightwaveos

#endif // FEATURE_MOTION_ENGINE
