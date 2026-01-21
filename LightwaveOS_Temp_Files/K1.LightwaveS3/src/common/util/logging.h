/**
 * @file logging.h
 * @brief Lightweight Logging Macros
 * 
 * Provides platform-agnostic logging with minimal overhead.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
    LW_LOG_ERROR = 0,
    LW_LOG_WARN,
    LW_LOG_INFO,
    LW_LOG_DEBUG,
    LW_LOG_VERBOSE
} lw_log_level_t;

// Platform-specific log implementation
#if defined(ESP_PLATFORM)
    // ESP-IDF logging
    #include "esp_log.h"
    #define LW_LOG_TAG "LW"
    #define LW_LOGE(fmt, ...) ESP_LOGE(LW_LOG_TAG, fmt, ##__VA_ARGS__)
    #define LW_LOGW(fmt, ...) ESP_LOGW(LW_LOG_TAG, fmt, ##__VA_ARGS__)
    #define LW_LOGI(fmt, ...) ESP_LOGI(LW_LOG_TAG, fmt, ##__VA_ARGS__)
    #define LW_LOGD(fmt, ...) ESP_LOGD(LW_LOG_TAG, fmt, ##__VA_ARGS__)
    #define LW_LOGV(fmt, ...) ESP_LOGV(LW_LOG_TAG, fmt, ##__VA_ARGS__)
#elif defined(ARDUINO)
    // Arduino Serial logging
    #define LW_LOGE(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGW(fmt, ...) Serial.printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGI(fmt, ...) Serial.printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGD(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGV(fmt, ...) Serial.printf("[VERB]  " fmt "\n", ##__VA_ARGS__)
#else
    // Host/Test logging (printf)
    #include <stdio.h>
    #define LW_LOGE(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGW(fmt, ...) printf("[WARN]  " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGI(fmt, ...) printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGD(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define LW_LOGV(fmt, ...) printf("[VERB]  " fmt "\n", ##__VA_ARGS__)
#endif

// Conditional compilation based on log level
#ifndef LW_LOG_LEVEL
    #define LW_LOG_LEVEL LW_LOG_INFO
#endif

#if LW_LOG_LEVEL >= LW_LOG_VERBOSE
    #define LW_LOG_VERBOSE_ENABLED 1
#else
    #undef LW_LOGV
    #define LW_LOGV(fmt, ...)
#endif

#if LW_LOG_LEVEL >= LW_LOG_DEBUG
    #define LW_LOG_DEBUG_ENABLED 1
#else
    #undef LW_LOGD
    #define LW_LOGD(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif
