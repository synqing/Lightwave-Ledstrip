/**
 * @file monotonic.h
 * @brief Monotonic Time Helpers (Platform Shim)
 * 
 * Provides monotonic time access across ESP32 platforms.
 */

#pragma once

#include <stdint.h>

// Platform-specific includes at file scope
#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#elif defined(__linux__) || defined(__APPLE__)
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get monotonic time in microseconds
 * @return Monotonic time in microseconds
 * 
 * Platform implementations:
 * - ESP32/ESP32-S3/ESP32-P4: esp_timer_get_time()
 * - Arduino (non-ESP): micros()
 * - Host/Test: clock_gettime(CLOCK_MONOTONIC)
 */
static inline uint64_t lw_monotonic_us(void) {
#if defined(ESP_PLATFORM)
    // ESP-IDF platforms - esp_timer_get_time returns int64_t
    return (uint64_t)esp_timer_get_time();
#elif defined(ARDUINO)
    // Arduino platforms (micros() wraps at ~70 minutes, handle carefully)
    return micros();
#elif defined(__linux__) || defined(__APPLE__)
    // Host platforms
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#else
    #error "Unsupported platform for lw_monotonic_us()"
#endif
}

/**
 * @brief Get monotonic time in milliseconds
 * @return Monotonic time in milliseconds
 */
static inline uint64_t lw_monotonic_ms(void) {
    return lw_monotonic_us() / 1000ULL;
}

#ifdef __cplusplus
}
#endif
