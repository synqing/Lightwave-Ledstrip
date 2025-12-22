#ifndef MOTION_ENGINE_H
#define MOTION_ENGINE_H

#include <FastLED.h>
#include <math.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

#if FEATURE_MOTION_ENGINE

// MotionEngine - Advanced motion control for visual enhancements
// Week 1-2: Skeleton implementation (empty methods)
// Week 4: Full implementation with phase control, easing, momentum physics

// ====== PHASE CONTROLLER ======
struct PhaseController {
    float stripPhaseOffset;      // Phase offset between strip1 and strip2 (0-360°)
    float phaseVelocity;         // Auto-rotating phase speed (degrees per frame)
    bool autoRotate;             // Enable automatic phase rotation

    PhaseController()
        : stripPhaseOffset(0)
        , phaseVelocity(0)
        , autoRotate(false)
    {}

    void update(float deltaTime);
    void setStripPhaseOffset(float degrees);
    float getStripPhaseRadians() const;
    void enableAutoRotate(float degreesPerSecond);
};

// ====== MOMENTUM ENGINE ======
enum BoundaryMode {
    BOUNDARY_WRAP,
    BOUNDARY_BOUNCE,
    BOUNDARY_CLAMP,
    BOUNDARY_DIE
};

struct Particle {
    float position;              // Current position (0.0-1.0 normalized)
    float velocity;              // Current velocity
    float acceleration;          // Current acceleration
    float mass;                  // Particle mass (affects inertia)
    float drag;                  // Air resistance coefficient (0-1)
    bool active;                 // Is particle active?
    CRGB color;                  // Particle color
    BoundaryMode boundaryMode;   // How to handle edges

    Particle()
        : position(0)
        , velocity(0)
        , acceleration(0)
        , mass(1.0f)
        , drag(0.98f)
        , active(false)
        , color(CRGB::White)
        , boundaryMode(BOUNDARY_WRAP)
    {}
};

class MomentumEngine {
private:
    static constexpr uint8_t MAX_PARTICLES = 32;
    Particle particles[MAX_PARTICLES];
    uint8_t activeParticleCount;

public:
    MomentumEngine() : activeParticleCount(0) {}

    void reset();
    int addParticle(float pos, float vel, float mass = 1.0f, CRGB color = CRGB::White, BoundaryMode boundary = BOUNDARY_WRAP);
    void applyForce(int particleId, float force);
    void update(float deltaTime);
    Particle* getParticle(int particleId);
    uint8_t getActiveCount() const { return activeParticleCount; }
};

// ====== SPEED MODULATOR ======
class SpeedModulator {
public:
    enum ModulationType {
        MOD_CONSTANT,
        MOD_SINE_WAVE,
        MOD_EXPONENTIAL_DECAY
    };

private:
    ModulationType type;
    float baseSpeed;
    float modulationDepth;
    float phase;

public:
    SpeedModulator(float base = 25.0f);
    void setModulation(ModulationType mod, float depth = 0.5f);
    float getSpeed(float deltaTime);
    void setBaseSpeed(float speed);
};

// ====== MAIN MOTION ENGINE ======
class MotionEngine {
public:
    // Singleton access pattern
    static MotionEngine& getInstance() {
        static MotionEngine instance;
        return instance;
    }

    // Engine control
    void enable();
    void disable();
    bool isEnabled() const { return m_enabled; }

    // API v2 "warp" parameters (stored configuration; effects may consume later)
    uint8_t getWarpStrength() const { return m_warpStrength; }
    uint8_t getWarpFrequency() const { return m_warpFrequency; }
    void setWarpStrength(uint8_t v) { m_warpStrength = v; }
    void setWarpFrequency(uint8_t v) { m_warpFrequency = v; }

    // Update all subsystems
    void update();

    // Access subsystems
    PhaseController& getPhaseController() { return m_phaseCtrl; }
    MomentumEngine& getMomentumEngine() { return m_momentumEngine; }
    SpeedModulator& getSpeedModulator() { return m_speedMod; }

    float getDeltaTime() const { return m_deltaTime; }

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

    // API v2 configuration
    uint8_t m_warpStrength;
    uint8_t m_warpFrequency;
};

#endif // FEATURE_MOTION_ENGINE
#endif // MOTION_ENGINE_H
