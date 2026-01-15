/**
 * @file utilities.h
 * @brief Emotiscope utility functions
 *
 * Ported from Emotiscope.HIL/v1.1_build/utilities.h
 * Contains helper functions for array manipulation and signal processing.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {
namespace emotiscope {

/**
 * @brief Clip a float to [0.0, 1.0] range
 */
inline float clipFloat(float input) {
    return std::min(1.0f, std::max(0.0f, input));
}

/**
 * @brief Shift history array left and append new data
 *
 * Efficiently shifts existing data to make room for new samples.
 *
 * @param history_array  Array to modify in-place
 * @param history_size   Total size of history array
 * @param new_array      New data to append
 * @param new_size       Size of new data
 */
inline void shiftAndCopyArrays(float* history_array, size_t history_size,
                                const float* new_array, size_t new_size) {
    // Shift existing data left
    memmove(history_array, history_array + new_size,
            (history_size - new_size) * sizeof(float));
    // Copy new data to end
    memcpy(history_array + history_size - new_size, new_array, new_size * sizeof(float));
}

/**
 * @brief Shift array contents left by N positions, zeroing vacated space
 *
 * @param array        Array to shift
 * @param array_size   Size of array
 * @param shift_amount Number of positions to shift
 */
inline void shiftArrayLeft(float* array, uint16_t array_size, uint16_t shift_amount) {
    if (shift_amount >= array_size) {
        memset(array, 0, array_size * sizeof(float));
    } else {
        memmove(array, array + shift_amount, (array_size - shift_amount) * sizeof(float));
        memset(array + array_size - shift_amount, 0, shift_amount * sizeof(float));
    }
}

/**
 * @brief Linear interpolation between array indices
 *
 * @param index       Normalized index (0.0 to 1.0)
 * @param array       Array to interpolate from
 * @param array_size  Size of array
 * @return            Interpolated value
 */
inline float interpolate(float index, const float* array, uint16_t array_size) {
    float index_f = index * (array_size - 1);
    uint16_t index_i = static_cast<uint16_t>(index_f);
    float frac = index_f - index_i;

    float left_val = array[index_i];
    float right_val = (index_i + 1 < array_size) ? array[index_i + 1] : left_val;

    return (1.0f - frac) * left_val + frac * right_val;
}

/**
 * @brief Apply single-pole low-pass filter to array
 *
 * @param input_array      Array to filter in-place
 * @param num_samples      Number of samples
 * @param sample_rate      Sample rate in Hz
 * @param cutoff_frequency Cutoff frequency in Hz
 * @param filter_order     Number of passes (higher = steeper rolloff)
 */
inline void lowPassFilter(float* input_array, uint16_t num_samples,
                          uint16_t sample_rate, float cutoff_frequency,
                          uint8_t filter_order) {
    float rc = 1.0f / (2.0f * M_PI * cutoff_frequency);
    float alpha = 1.0f / (1.0f + (sample_rate * rc));

    for (uint8_t order = 0; order < filter_order; ++order) {
        float filtered_value = input_array[0];
        for (uint16_t n = 1; n < num_samples; ++n) {
            filtered_value = alpha * input_array[n] + (1.0f - alpha) * filtered_value;
            input_array[n] = filtered_value;
        }
    }
}

/**
 * @brief Multiply array by scalar (in-place)
 *
 * Replacement for ESP-DSP dsps_mulc_f32 when not available.
 *
 * @param input   Source array
 * @param output  Destination array (can be same as input)
 * @param length  Number of elements
 * @param scalar  Multiplication factor
 */
inline void multiplyByScalar(const float* input, float* output,
                              uint16_t length, float scalar) {
    for (uint16_t i = 0; i < length; i++) {
        output[i] = input[i] * scalar;
    }
}

/**
 * @brief Fast tanh approximation
 */
inline float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
 * @brief Convert linear [0,1] to triangle wave [0,1,0]
 */
inline float linearToTri(float input) {
    if (input < 0.0f || input > 1.0f) return 0.0f;
    return (input <= 0.5f) ? (2.0f * input) : (2.0f * (1.0f - input));
}

/**
 * @brief Sum a range of array elements
 */
inline float sumRange(const float* array, uint16_t start, uint16_t end) {
    float sum = 0.0f;
    for (uint16_t i = start; i <= end; i++) {
        sum += array[i];
    }
    return sum;
}

} // namespace emotiscope
} // namespace audio
} // namespace lightwaveos
