/**
 * @file GoertzelKernel.cpp
 * @brief Goertzel kernel implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "GoertzelKernel.h"
#include <cmath>

namespace lightwaveos {
namespace audio {
namespace k1 {

float GoertzelKernel::process(const int16_t* samples_i16, size_t N, int16_t coeff_q14) {
    if (samples_i16 == nullptr || N == 0) {
        return 0.0f;
    }

    // Convert Q14 coefficient to float
    float coeff = static_cast<float>(coeff_q14) / 16384.0f;

    // Goertzel recurrence: Q[n] = coeff * Q[n-1] - Q[n-2] + x[n]
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;

    for (size_t n = 0; n < N; n++) {
        // Convert int16 sample to float [-1.0, 1.0]
        float x = static_cast<float>(samples_i16[n]) / 32768.0f;

        // Recurrence relation
        q0 = coeff * q1 - q2 + x;
        q2 = q1;
        q1 = q0;
    }

    // Final magnitude: sqrt(Q1^2 + Q2^2 - coeff*Q1*Q2)
    float magnitude = std::sqrt(q1 * q1 + q2 * q2 - coeff * q1 * q2);

    return magnitude;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

