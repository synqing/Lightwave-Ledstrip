#include "MotionEngine.h"

#if FEATURE_MOTION_ENGINE

// Week 1-2: Skeleton implementation with empty methods
// Week 4: Full implementation

// ====== PHASE CONTROLLER ======
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
}

float PhaseController::getStripPhaseRadians() const {
    return stripPhaseOffset * DEG_TO_RAD;
}

void PhaseController::enableAutoRotate(float degreesPerSecond) {
    autoRotate = true;
    phaseVelocity = degreesPerSecond;
}

// ====== MOMENTUM ENGINE ======
void MomentumEngine::reset() {
    for (auto& p : particles) {
        p.active = false;
    }
    activeParticleCount = 0;
}

int MomentumEngine::addParticle(float pos, float vel, float mass, CRGB color, BoundaryMode boundary) {
    // Find free particle slot
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].position = pos;
            particles[i].velocity = vel;
            particles[i].acceleration = 0;
            particles[i].mass = mass;
            particles[i].drag = 0.98f;  // Default drag
            particles[i].color = color;
            particles[i].active = true;
            particles[i].boundaryMode = boundary;
            activeParticleCount++;
            return i;
        }
    }
    return -1;  // No free slots
}

void MomentumEngine::applyForce(int particleId, float force) {
    if (particleId < 0 || particleId >= MAX_PARTICLES) return;
    if (!particles[particleId].active) return;

    // F = ma, so a = F/m
    particles[particleId].acceleration += force / particles[particleId].mass;
}

void MomentumEngine::update(float deltaTime) {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;

        // Euler integration
        particles[i].velocity += particles[i].acceleration * deltaTime;
        particles[i].velocity *= particles[i].drag;  // Apply drag
        particles[i].position += particles[i].velocity * deltaTime;

        // Reset acceleration (forces are applied per frame)
        particles[i].acceleration = 0;

        // Boundary conditions
        switch (particles[i].boundaryMode) {
            case BOUNDARY_WRAP:
                if (particles[i].position > 1.0f) {
                    particles[i].position = fmod(particles[i].position, 1.0f);
                } else if (particles[i].position < 0.0f) {
                    particles[i].position = 1.0f + fmod(particles[i].position, 1.0f);
                }
                break;

            case BOUNDARY_BOUNCE:
                if (particles[i].position > 1.0f) {
                    particles[i].position = 1.0f - (particles[i].position - 1.0f);
                    particles[i].velocity = -particles[i].velocity;
                } else if (particles[i].position < 0.0f) {
                    particles[i].position = -particles[i].position;
                    particles[i].velocity = -particles[i].velocity;
                }
                break;

            case BOUNDARY_CLAMP:
                if (particles[i].position > 1.0f) {
                    particles[i].position = 1.0f;
                    particles[i].velocity = 0;
                } else if (particles[i].position < 0.0f) {
                    particles[i].position = 0.0f;
                    particles[i].velocity = 0;
                }
                break;

            case BOUNDARY_DIE:
                if (particles[i].position > 1.0f || particles[i].position < 0.0f) {
                    particles[i].active = false;
                    if (activeParticleCount > 0) activeParticleCount--;
                }
                break;
        }
    }
}

Particle* MomentumEngine::getParticle(int particleId) {
    if (particleId < 0 || particleId >= MAX_PARTICLES) return nullptr;
    return &particles[particleId];
}

// ====== SPEED MODULATOR ======
SpeedModulator::SpeedModulator(float base)
    : type(MOD_CONSTANT)
    , baseSpeed(base)
    , modulationDepth(0.5f)
    , phase(0)
{}

void SpeedModulator::setModulation(ModulationType mod, float depth) {
    type = mod;
    modulationDepth = depth;
}

float SpeedModulator::getSpeed(float deltaTime) {
    phase += deltaTime;

    float modulation = 0;
    switch (type) {
        case MOD_CONSTANT:
            return baseSpeed;

        case MOD_SINE_WAVE:
            modulation = sin(phase * TWO_PI) * modulationDepth;
            break;

        case MOD_EXPONENTIAL_DECAY:
            modulation = exp(-phase * modulationDepth) - 0.5f;
            break;
    }

    return baseSpeed * (1.0f + modulation);
}

void SpeedModulator::setBaseSpeed(float speed) {
    baseSpeed = speed;
}

// ====== MAIN MOTION ENGINE ======
MotionEngine::MotionEngine()
    : m_lastUpdateTime(0)
    , m_deltaTime(0)
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
    m_deltaTime = (now - m_lastUpdateTime) * 0.001f; // Convert to seconds
    m_lastUpdateTime = now;

    // Update subsystems
    m_phaseCtrl.update(m_deltaTime);
    m_momentumEngine.update(m_deltaTime);
}

#endif // FEATURE_MOTION_ENGINE
