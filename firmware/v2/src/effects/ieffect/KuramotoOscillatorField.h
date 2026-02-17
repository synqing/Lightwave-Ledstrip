/**
 * @file KuramotoOscillatorField.h
 * @brief Invisible Kuramoto oscillator field with nonlocal coupling
 *
 * This is the ENGINE layer. It does NOT render anything.
 * Audio steers K, spread, noise, kicks - the field evolves autonomously.
 *
 * Key properties:
 * - 80 oscillators (one per radial bin)
 * - Nonlocal coupling (cosine kernel) - required for chimera-like regimes
 * - Heun/RK2 integration for frame-rate independence
 * - Kicks create phase slips (visual events)
 */

#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <esp_heap_caps.h>

namespace lightwaveos::effects::ieffect {

/**
 * @brief Lightweight deterministic RNG (xorshift32) for embedded use.
 *
 * We avoid std::random or heavy dependencies.
 */
class XorShift32 {
public:
    explicit XorShift32(uint32_t seed = 0x12345678u) : m_state(seed ? seed : 0x12345678u) {}
    uint32_t nextU32() {
        uint32_t x = m_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        m_state = x;
        return x;
    }
    // [0,1)
    float next01() {
        // Take top 24 bits for a reasonably uniform mantissa.
        return (float)(nextU32() >> 8) * (1.0f / 16777216.0f);
    }
    // Approximate N(0,1) using sum of uniforms (CLT-ish). Cheap.
    float approxNormal() {
        float s = 0.0f;
        for (int i = 0; i < 6; ++i) s += next01();
        // Mean 3.0, var ~0.5; rescale to ~N(0,1)
        return (s - 3.0f) * 1.41421356f;
    }
private:
    uint32_t m_state;
};

/**
 * @brief KuramotoOscillatorField
 *
 * Invisible 1D ring of phase oscillators with **nonlocal coupling**.
 *
 * Key idea:
 *  - You do NOT render theta.
 *  - You evolve theta, then extract events (slips/edges/curvature).
 *
 * Numerics:
 *  - Uses Heun / RK2 for dt stability.
 */
class KuramotoOscillatorField {
public:
    static constexpr uint8_t  MAX_ZONES = 4;
    static constexpr uint16_t N         = 80;      // Oscillators (centre->edge samples)
    static constexpr uint16_t MAX_R     = 24;      // Max nonlocal radius (for kernel)
    static constexpr float    PI_F      = 3.14159265f;

    // PSRAM-ALLOCATED -- large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        float theta[MAX_ZONES][N];
        float prevTheta[MAX_ZONES][N];
        float omega[MAX_ZONES][N];
        float dTheta1[N];
        float dTheta2[N];
        float thetaPred[N];
    };

    KuramotoOscillatorField() { resetAll(0xA5A5A5A5u); }

    bool allocatePsram() {
        if (m_ps) return true;
        m_ps = static_cast<PsramData*>(heap_caps_malloc(sizeof(PsramData), MALLOC_CAP_SPIRAM));
        return m_ps != nullptr;
    }

    void freePsram() {
        if (m_ps) { heap_caps_free(m_ps); m_ps = nullptr; }
    }

    void resetAll(uint32_t seed) {
        for (uint8_t z = 0; z < MAX_ZONES; ++z) {
            m_rng[z] = XorShift32(seed ^ (0x9E3779B9u * (z + 1)));
            m_kickAcc[z] = 0.0f;
        }
        m_radius = 8;
        buildKernel();

        if (!m_ps) return;

        for (uint8_t z = 0; z < MAX_ZONES; ++z) {
            for (uint16_t i = 0; i < N; ++i) {
                // Random initial phases in [-pi, pi]
                m_ps->theta[z][i] = wrapPi((m_rng[z].next01() * 2.0f - 1.0f) * PI_F);
                m_ps->prevTheta[z][i] = m_ps->theta[z][i];
                m_ps->omega[z][i] = 0.0f;
            }
        }
    }

    /**
     * @brief Step the oscillator field for a given zone.
     *
     * @param zoneId 0..MAX_ZONES-1
     * @param dt Seconds
     * @param K Coupling strength
     * @param freqSpread Spread of natural frequencies (rad/s)
     * @param radius Nonlocal radius in indices (1..MAX_R)
     * @param noiseSigma Continuous phase noise (rad/sqrt(s))
     * @param kickRateHz Poisson-ish kick rate (events/sec)
     * @param kickStrength Kick amplitude (radians)
     */
    void step(
        uint8_t zoneId,
        float dt,
        float K,
        float freqSpread,
        uint16_t radius,
        float noiseSigma,
        float kickRateHz,
        float kickStrength
    ) {
        if (!m_ps) return;
        if (zoneId >= MAX_ZONES) return;
        if (dt <= 0.0f) return;

        // Clamp dt hard: this system is sensitive to huge pauses.
        if (dt > 0.05f) dt = 0.05f; // 50ms

        // Update radius and kernel if needed.
        if (radius < 1) radius = 1;
        if (radius > MAX_R) radius = MAX_R;
        if (radius != m_radius) {
            m_radius = radius;
            buildKernel();
        }

        // Refresh natural frequencies each frame (cheap) to implement spread steering.
        // We keep mean ~0 so the field is not a constant uniform drift.
        for (uint16_t i = 0; i < N; ++i) {
            // Uniform approx in [-1,1]
            const float u = (m_rng[zoneId].next01() * 2.0f - 1.0f);
            m_ps->omega[zoneId][i] = u * freqSpread;
        }

        // Save previous theta for slip detection downstream.
        memcpy(m_ps->prevTheta[zoneId], m_ps->theta[zoneId], sizeof(float) * N);

        // Heun / RK2 integration: theta_{n+1} = theta_n + dt * 0.5*(f(theta)+f(theta_pred))
        computeDerivative(zoneId, m_ps->theta[zoneId], K, m_ps->dTheta1);

        for (uint16_t i = 0; i < N; ++i) {
            m_ps->thetaPred[i] = wrapPi(m_ps->theta[zoneId][i] + dt * m_ps->dTheta1[i]);
        }

        computeDerivative(zoneId, m_ps->thetaPred, K, m_ps->dTheta2);

        // Kicks: model as a rate process (accumulate expected kicks and fire when >=1).
        m_kickAcc[zoneId] += kickRateHz * dt;
        while (m_kickAcc[zoneId] >= 1.0f) {
            m_kickAcc[zoneId] -= 1.0f;

            // Kick a handful of oscillators: causes phase slips/edges later.
            // Use 3.999f to ensure count stays in [1,4] range (next01 returns [0,1))
            const uint8_t count = 1 + (uint8_t)(m_rng[zoneId].next01() * 3.999f); // 1..4
            for (uint8_t k = 0; k < count; ++k) {
                const uint16_t idx = (uint16_t)(m_rng[zoneId].next01() * (float)N) % N;
                const float dir = (m_rng[zoneId].next01() < 0.5f) ? -1.0f : 1.0f;
                m_ps->theta[zoneId][idx] = wrapPi(m_ps->theta[zoneId][idx] + dir * kickStrength);
            }
        }

        // Final update
        for (uint16_t i = 0; i < N; ++i) {
            // Add continuous noise as sigma*sqrt(dt)*N(0,1)
            const float noise = noiseSigma * sqrtf(dt) * m_rng[zoneId].approxNormal();
            const float dTheta = 0.5f * (m_ps->dTheta1[i] + m_ps->dTheta2[i]);
            m_ps->theta[zoneId][i] = wrapPi(m_ps->theta[zoneId][i] + dt * dTheta + noise);
        }
    }

    const float* theta(uint8_t zoneId) const {
        if (!m_ps) return nullptr;
        return (zoneId < MAX_ZONES) ? m_ps->theta[zoneId] : nullptr;
    }

    const float* prevTheta(uint8_t zoneId) const {
        if (!m_ps) return nullptr;
        return (zoneId < MAX_ZONES) ? m_ps->prevTheta[zoneId] : nullptr;
    }

    uint16_t radius() const { return m_radius; }
    const float* kernel() const { return m_kernel; } // length = 2*radius+1

    static float wrapPi(float x) {
        // Map to [-pi, pi] using bounded fmodf (prevents infinite loop on NaN/large values)
        x = fmodf(x + PI_F, 2.0f * PI_F);
        if (x < 0.0f) x += 2.0f * PI_F;
        return x - PI_F;
    }

private:
    // Compute f(theta) for each oscillator i.
    // f(theta_i) = omega_i + K * sum_j w(d)*sin(theta_j - theta_i) / sum_w
    void computeDerivative(uint8_t zoneId, const float* thetaIn, float K, float* out) {
        const uint16_t R = m_radius;
        const float* w = m_kernel; // length 2R+1, indexed by (d+R)

        // Precompute sum_w (kernel is symmetric and built for the current R)
        float sumW = 0.0f;
        for (int d = -(int)R; d <= (int)R; ++d) sumW += w[d + R];
        if (sumW < 1e-6f) sumW = 1.0f;

        for (uint16_t i = 0; i < N; ++i) {
            float coup = 0.0f;
            const float theta_i = thetaIn[i];

            for (int d = -(int)R; d <= (int)R; ++d) {
                const uint16_t j = (uint16_t)(( (int)i + d + (int)N ) % (int)N);
                const float theta_j = thetaIn[j];
                coup += w[d + R] * sinf(theta_j - theta_i);
            }

            out[i] = m_ps->omega[zoneId][i] + (K / sumW) * coup;
        }
    }

    void buildKernel() {
        // Cosine-like kernel (Abrams/Strogatz chimera papers often use cosine kernel).
        // We approximate with a raised cosine within radius.
        const uint16_t R = m_radius;
        for (int d = -(int)R; d <= (int)R; ++d) {
            const float x = (float)d / (float)R; // -1..1
            // Raised cosine: 0.5*(1+cos(pi*x))
            m_kernel[d + R] = 0.5f * (1.0f + cosf(PI_F * x));
        }
        // Ensure centre weight is non-zero
        m_kernel[R] = 1.0f;

        // For safety, clear remaining unused entries (when radius shrinks)
        for (uint16_t i = 2 * R + 1; i < 2 * MAX_R + 1; ++i) {
            m_kernel[i] = 0.0f;
        }
    }

    uint16_t m_radius = 8;

    // PSRAM-allocated large buffers
    PsramData* m_ps = nullptr;

    // Per-zone kick accumulator (expected kicks) -- small, stays in DRAM
    float m_kickAcc[MAX_ZONES];

    // Kernel weights: size max 2*MAX_R + 1 -- small (196 bytes), stays in DRAM
    float m_kernel[2 * MAX_R + 1];

    XorShift32 m_rng[MAX_ZONES];
};

} // namespace lightwaveos::effects::ieffect
