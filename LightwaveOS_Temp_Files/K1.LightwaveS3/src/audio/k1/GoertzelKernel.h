/**
 * @file GoertzelKernel.h
 * @brief Goertzel algorithm kernel (single bin computation)
 *
 * Implements the Goertzel recurrence relation for efficient single-frequency
 * magnitude estimation.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Goertzel kernel for single bin computation
 *
 * Processes windowed samples through Goertzel recurrence to produce
 * raw magnitude/power (pre-normalization).
 */
class GoertzelKernel {
public:
    /**
     * @brief Process samples through Goertzel algorithm
     *
     * @param samples_i16 Windowed samples (length N)
     * @param N Window length (number of samples)
     * @param coeff_q14 Q14 coefficient: 2*cos(2πk/N) * 16384
     * @return Raw magnitude/power (pre-normalization)
     */
    static float process(const int16_t* samples_i16, size_t N, int16_t coeff_q14);
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

