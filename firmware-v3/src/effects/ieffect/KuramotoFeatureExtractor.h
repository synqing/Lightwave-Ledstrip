/**
 * @file KuramotoFeatureExtractor.h
 * @brief Extract renderable events from invisible Kuramoto field
 *
 * Turns phase field into:
 * - velocity[i] in [-1,+1] (from phase gradient) - direction for transport
 * - coherence[i] in [0,1] (local order parameter) - cluster detection
 * - event[i] in [0,1] (injection strength) - where to inject light
 *
 * The events are: phase slips + coherence edges + curvature
 * These become injection points in the transport buffer.
 */

#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>

#include "KuramotoOscillatorField.h"

namespace lightwaveos::effects::ieffect {

/**
 * @brief KuramotoFeatureExtractor
 *
 * Converts an invisible phase field into *renderable* structure:
 *  - velocity[i] in [-1,+1] (from phase_gradient)
 *  - coherence[i] in [0,1] (local order)
 *  - event[i] in [0,1] (phase slips + coherence edges + curvature)
 */
class KuramotoFeatureExtractor {
public:
    static constexpr uint16_t N = KuramotoOscillatorField::N;

    /**
     * @param theta Current phases (length N)
     * @param prevTheta Previous phases (length N)
     * @param radius Nonlocal radius used by oscillator field
     * @param kernel Kernel weights array length (2*radius+1)
     * @param outVelocity [-1,1]
     * @param outCoherence [0,1]
     * @param outEvent [0,1]
     */
    static void extract(
        const float* theta,
        const float* prevTheta,
        uint16_t radius,
        const float* kernel,
        float* outVelocity,
        float* outCoherence,
        float* outEvent
    ) {
        if (!theta || !prevTheta || !kernel || !outVelocity || !outCoherence || !outEvent) return;
        if (radius < 1) radius = 1;
        if (radius > KuramotoOscillatorField::MAX_R) radius = KuramotoOscillatorField::MAX_R;

        // 1) Local coherence r_local[i] (complex average over neighbourhood)
        // r = sqrt( (sum cos)^2 + (sum sin)^2 ) / sum_w
        computeLocalCoherence(theta, radius, kernel, outCoherence);

        // 2) Phase gradient → velocity (use wrapped neighbour difference)
        for (uint16_t i = 0; i < N; ++i) {
            const uint16_t ip = (i + 1) % N;
            const uint16_t im = (i + N - 1) % N;
            const float d = KuramotoOscillatorField::wrapPi(theta[ip] - theta[im]);
            // gradient scale: map roughly [-pi,pi] to [-1,1]
            float v = d * (1.0f / KuramotoOscillatorField::PI_F);
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            outVelocity[i] = v;
        }

        // 3) Derived events: phase slip + coherence edges + curvature
        for (uint16_t i = 0; i < N; ++i) {
            const uint16_t ip = (i + 1) % N;
            const uint16_t im = (i + N - 1) % N;

            // Phase slip: big wrapped jump in time
            // Threshold lowered from 1.6 to 0.5 radians so typical kicks (0.65-2.0 rad) register as events
            const float dThetaT = fabsf(KuramotoOscillatorField::wrapPi(theta[i] - prevTheta[i]));
            float slip = (dThetaT - 0.5f); // threshold ~0.16*pi (was 0.5*pi - too high!)
            if (slip < 0.0f) slip = 0.0f;
            slip = fminf(1.0f, slip * 0.4f); // gentler scaling (was 0.8)

            // Coherence edge: |r_local[i] - r_local[i±1]|
            const float edge = fminf(1.0f, fabsf(outCoherence[i] - outCoherence[ip]) + fabsf(outCoherence[i] - outCoherence[im]));

            // Curvature: second difference magnitude (wrapped)
            const float curv = fabsf(KuramotoOscillatorField::wrapPi(theta[ip] - 2.0f * theta[i] + theta[im]));
            float curvN = fminf(1.0f, curv * (1.0f / KuramotoOscillatorField::PI_F));

            // Mix with weights. More curvature sensitivity for visible wavefronts.
            float e = 0.80f * slip + 0.60f * edge + 0.50f * curvN;

            // Soft threshold - lower to let more events through.
            // (0.12->0) so even subtle events create some light.
            e = (e - 0.12f) * 1.136f;
            if (e < 0.0f) e = 0.0f;
            if (e > 1.0f) e = 1.0f;

            outEvent[i] = e;
        }
    }

private:
    static void computeLocalCoherence(const float* theta, uint16_t radius, const float* kernel, float* outR) {
        const uint16_t R = radius;

        // sum_w
        float sumW = 0.0f;
        for (int d = -(int)R; d <= (int)R; ++d) sumW += kernel[d + R];
        if (sumW < 1e-6f) sumW = 1.0f;

        for (uint16_t i = 0; i < N; ++i) {
            float sumC = 0.0f;
            float sumS = 0.0f;

            for (int d = -(int)R; d <= (int)R; ++d) {
                const uint16_t j = (uint16_t)(( (int)i + d + (int)N ) % (int)N);
                const float th = theta[j];
                const float w  = kernel[d + R];
                sumC += w * cosf(th);
                sumS += w * sinf(th);
            }

            const float mag = sqrtf(sumC * sumC + sumS * sumS);
            float r = mag / sumW;
            if (r < 0.0f) r = 0.0f;
            if (r > 1.0f) r = 1.0f;
            outR[i] = r;
        }
    }
};

} // namespace lightwaveos::effects::ieffect
