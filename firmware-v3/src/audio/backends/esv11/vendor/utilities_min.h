#pragma once

#include <cstdint>
#include <cstring>
#include "EsV11Shim.h"

inline void shift_and_copy_arrays(float history_array[], size_t history_size, const float new_array[], size_t new_size) {
    profile_function([&]() {
        memmove(history_array, history_array + new_size, (history_size - new_size) * sizeof(float));
        memcpy(history_array + history_size - new_size, new_array, new_size * sizeof(float));
    }, __func__);
}

inline void shift_array_left(float* array, uint16_t array_size, uint16_t shift_amount) {
    if (shift_amount >= array_size) {
        memset(array, 0, array_size * sizeof(float));
    } else {
        memmove(array, array + shift_amount, (array_size - shift_amount) * sizeof(float));
        memset(array + array_size - shift_amount, 0, shift_amount * sizeof(float));
    }
}

inline float clip_float(float input) {
    if (input < 0.0f) return 0.0f;
    if (input > 1.0f) return 1.0f;
    return input;
}
