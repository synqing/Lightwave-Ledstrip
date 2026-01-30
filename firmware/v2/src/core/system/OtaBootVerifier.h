/**
 * @file OtaBootVerifier.h
 * @brief OTA boot verification and automatic rollback for K1 prototype
 *
 * The K1 prototype has NO USB access after assembly -- it is 100% OTA-dependent.
 * A bad firmware push without rollback protection bricks the device permanently.
 *
 * This module uses ESP-IDF's app rollback feature to verify that a newly OTA'd
 * firmware boots successfully. The bootloader marks a new OTA app as
 * ESP_OTA_IMG_PENDING_VERIFY. If the app does not call
 * esp_ota_mark_app_valid_cancel_rollback() before the next reboot, the bootloader
 * rolls back to the previous working partition automatically.
 *
 * Boot sequence:
 *   1. init()                    - Early in setup(), logs rollback status
 *   2. markAppValidIfHealthy()   - After WiFi + WebServer init, validates or rolls back
 *
 * Health checks before marking valid:
 *   - Free heap > 100 KB
 *   - WiFi connected or AP mode active
 *   - WebServer instance created and started
 *
 * If health checks fail within the validation window (30 seconds), the firmware
 * rolls back to the previous partition and reboots.
 *
 * All state transitions emit structured JSON telemetry on Serial for trace capture.
 *
 * @note Requires CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y in sdkconfig.
 *       This is already enabled by default in ESP32 Arduino framework for S3.
 *
 * @see esp_ota_ops.h
 * @see WsOtaCommands.cpp (WebSocket OTA upload protocol)
 */

#pragma once

#include "../../config/features.h"

// Only compile on real ESP32 targets with OTA enabled
#if FEATURE_OTA_UPDATE && !defined(NATIVE_BUILD)

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define LW_LOG_TAG_OTA_BOOT "OtaBoot"

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief OTA boot verification and automatic rollback
 *
 * Static utility class -- no instances needed.
 * Thread-safe: init() and markAppValidIfHealthy() are called sequentially
 * from setup() on the main core, so no mutex is required.
 */
class OtaBootVerifier {
public:
    // ========================================================================
    // Minimum heap required to consider the firmware healthy (bytes)
    // ========================================================================
    static constexpr size_t MIN_HEALTHY_HEAP = 100 * 1024;  // 100 KB

    // ========================================================================
    // Maximum time to wait for health checks before forced rollback (ms)
    // ========================================================================
    static constexpr uint32_t VALIDATION_TIMEOUT_MS = 30000;  // 30 seconds

    /**
     * @brief Initialize OTA boot verification (call early in setup())
     *
     * Checks whether rollback is possible (i.e., this is a freshly OTA'd app
     * that has not yet been validated). Logs the current boot partition and
     * rollback status via structured JSON telemetry.
     *
     * This does NOT mark the app as valid -- that happens later in
     * markAppValidIfHealthy() after all critical subsystems are confirmed.
     */
    static void init() {
        s_bootTimeMs = millis();

        // Check if rollback is possible (new OTA app pending verification)
        s_rollbackPossible = esp_ota_check_rollback_is_possible();

        // Get current running partition info
        const esp_partition_t* running = esp_ota_get_running_partition();
        const char* partLabel = running ? running->label : "unknown";

        // Emit boot check telemetry
        char buf[256];
        int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.boot.check\",\"ts_mono_ms\":%lu,"
            "\"rollbackPossible\":%s,\"partition\":\"%s\"}",
            static_cast<unsigned long>(s_bootTimeMs),
            s_rollbackPossible ? "true" : "false",
            partLabel
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }

        // Log human-readable status
        if (s_rollbackPossible) {
            Serial.printf("[%lu][INFO][%s] NEW OTA firmware detected on '%s' -- "
                          "awaiting health validation\n",
                          static_cast<unsigned long>(s_bootTimeMs),
                          LW_LOG_TAG_OTA_BOOT, partLabel);
        } else {
            Serial.printf("[%lu][INFO][%s] Boot partition '%s' -- "
                          "already validated (no rollback pending)\n",
                          static_cast<unsigned long>(s_bootTimeMs),
                          LW_LOG_TAG_OTA_BOOT, partLabel);
        }

        s_initialized = true;
    }

    /**
     * @brief Validate health and mark app as valid, or rollback
     *
     * Call this AFTER WiFi and WebServer initialization completes.
     * Runs the health check suite and either:
     *   - Marks the app valid (cancels rollback) if all checks pass
     *   - Triggers rollback + reboot if checks fail
     *
     * If rollback is not possible (app already validated or first flash),
     * this function logs the status and returns immediately.
     *
     * @param wifiConnectedOrAP  true if WiFi STA connected or AP mode active
     * @param webServerStarted   true if WebServer::begin() succeeded
     */
    static void markAppValidIfHealthy(bool wifiConnectedOrAP, bool webServerStarted) {
        if (!s_initialized) {
            return;
        }

        uint32_t now = millis();
        size_t freeHeap = ESP.getFreeHeap();
        bool heapOk = freeHeap >= MIN_HEALTHY_HEAP;

        // Determine WiFi state string for telemetry
        const char* wifiStateStr = wifiConnectedOrAP ? "connected_or_ap" : "no_network";

        // --- Non-OTA boot: just log and return ---
        if (!s_rollbackPossible) {
            char buf[256];
            int n = snprintf(buf, sizeof(buf),
                "{\"event\":\"ota.boot.validated\",\"ts_mono_ms\":%lu,"
                "\"heap\":%lu,\"wifiState\":\"%s\","
                "\"webServer\":%s,\"note\":\"already_validated\"}",
                static_cast<unsigned long>(now),
                static_cast<unsigned long>(freeHeap),
                wifiStateStr,
                webServerStarted ? "true" : "false"
            );
            if (n > 0 && n < static_cast<int>(sizeof(buf))) {
                Serial.println(buf);
            }
            return;
        }

        // --- OTA boot: run health checks ---
        bool allHealthy = heapOk && wifiConnectedOrAP && webServerStarted;

        if (allHealthy) {
            // All checks pass -- mark app as valid, cancel rollback
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

            char buf[320];
            if (err == ESP_OK) {
                int n = snprintf(buf, sizeof(buf),
                    "{\"event\":\"ota.boot.validated\",\"ts_mono_ms\":%lu,"
                    "\"heap\":%lu,\"wifiState\":\"%s\","
                    "\"webServer\":true,\"uptimeMs\":%lu}",
                    static_cast<unsigned long>(now),
                    static_cast<unsigned long>(freeHeap),
                    wifiStateStr,
                    static_cast<unsigned long>(now - s_bootTimeMs)
                );
                if (n > 0 && n < static_cast<int>(sizeof(buf))) {
                    Serial.println(buf);
                }

                Serial.printf("[%lu][INFO][%s] OTA firmware VALIDATED -- "
                              "rollback cancelled (heap=%lu, wifi=%s, ws=ok)\n",
                              static_cast<unsigned long>(now),
                              LW_LOG_TAG_OTA_BOOT,
                              static_cast<unsigned long>(freeHeap),
                              wifiStateStr);
            } else {
                // esp_ota_mark_app_valid failed -- unusual but not fatal
                // The app continues running; rollback will trigger on next
                // reboot if not resolved.
                int n = snprintf(buf, sizeof(buf),
                    "{\"event\":\"ota.boot.validated\",\"ts_mono_ms\":%lu,"
                    "\"heap\":%lu,\"wifiState\":\"%s\","
                    "\"webServer\":true,\"espErr\":%d,"
                    "\"note\":\"mark_valid_failed\"}",
                    static_cast<unsigned long>(now),
                    static_cast<unsigned long>(freeHeap),
                    wifiStateStr,
                    static_cast<int>(err)
                );
                if (n > 0 && n < static_cast<int>(sizeof(buf))) {
                    Serial.println(buf);
                }

                Serial.printf("[%lu][WARN][%s] esp_ota_mark_app_valid returned %d -- "
                              "rollback still pending!\n",
                              static_cast<unsigned long>(now),
                              LW_LOG_TAG_OTA_BOOT,
                              static_cast<int>(err));
            }

            s_validated = true;
        } else {
            // Health check failed -- build reason string and rollback
            char reason[128];
            int rn = snprintf(reason, sizeof(reason),
                "heap_%s(%luKB),wifi_%s,webserver_%s",
                heapOk ? "ok" : "LOW",
                static_cast<unsigned long>(freeHeap / 1024),
                wifiConnectedOrAP ? "ok" : "FAIL",
                webServerStarted ? "ok" : "FAIL"
            );
            if (rn < 0 || rn >= static_cast<int>(sizeof(reason))) {
                reason[sizeof(reason) - 1] = '\0';
            }

            Serial.printf("[%lu][ERROR][%s] Health check FAILED: %s\n",
                          static_cast<unsigned long>(now),
                          LW_LOG_TAG_OTA_BOOT, reason);

            // Check if we have exceeded the validation timeout
            uint32_t elapsed = now - s_bootTimeMs;
            if (elapsed < VALIDATION_TIMEOUT_MS) {
                // Still within timeout -- log warning but do NOT rollback yet.
                // The caller may retry after more subsystems initialize.
                Serial.printf("[%lu][WARN][%s] Validation deferred -- "
                              "%lu ms remaining in window\n",
                              static_cast<unsigned long>(now),
                              LW_LOG_TAG_OTA_BOOT,
                              static_cast<unsigned long>(VALIDATION_TIMEOUT_MS - elapsed));
                return;
            }

            // Timeout exceeded -- rollback now
            rollback(reason);
        }
    }

    /**
     * @brief Check if this is the first boot after an OTA update
     * @return true if rollback is possible (new OTA app pending verification)
     */
    static bool isFirstBootAfterOta() {
        return s_rollbackPossible;
    }

    /**
     * @brief Check if the app has been validated this boot
     * @return true if markAppValidIfHealthy() succeeded
     */
    static bool isValidated() {
        return s_validated;
    }

    /**
     * @brief Force rollback to previous firmware partition
     *
     * Emits telemetry, marks current app invalid, and reboots into the
     * previous OTA partition. This function does NOT return.
     *
     * @param reason  Human-readable reason string for telemetry
     */
    static void rollback(const char* reason) {
        uint32_t now = millis();

        // Emit rollback telemetry
        char buf[320];
        // Escape reason for JSON safety
        char reasonEscaped[128] = {0};
        size_t outIdx = 0;
        if (reason) {
            for (size_t i = 0; reason[i] != '\0' && outIdx < sizeof(reasonEscaped) - 1; i++) {
                char c = reason[i];
                if (c == '"' || c == '\\') {
                    if (outIdx < sizeof(reasonEscaped) - 2) {
                        reasonEscaped[outIdx++] = '\\';
                        reasonEscaped[outIdx++] = c;
                    }
                } else if (c >= 32 && c < 127) {
                    reasonEscaped[outIdx++] = c;
                }
            }
        }
        reasonEscaped[outIdx] = '\0';

        int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ota.boot.rollback\",\"ts_mono_ms\":%lu,"
            "\"reason\":\"%s\",\"uptimeMs\":%lu}",
            static_cast<unsigned long>(now),
            reasonEscaped,
            static_cast<unsigned long>(now - s_bootTimeMs)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }

        Serial.printf("[%lu][ERROR][%s] ROLLING BACK to previous firmware: %s\n",
                      static_cast<unsigned long>(now),
                      LW_LOG_TAG_OTA_BOOT, reason);

        // Flush serial output before reboot
        Serial.flush();
        delay(100);

        // Mark current app invalid and reboot into previous partition
        esp_ota_mark_app_invalid_rollback_and_reboot();

        // Should never reach here -- but if it does, halt
        Serial.printf("[%lu][ERROR][%s] Rollback failed! System halted.\n",
                      static_cast<unsigned long>(millis()),
                      LW_LOG_TAG_OTA_BOOT);
        while (true) {
            delay(1000);
        }
    }

private:
    static inline bool s_initialized = false;
    static inline bool s_rollbackPossible = false;
    static inline bool s_validated = false;
    static inline uint32_t s_bootTimeMs = 0;
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#else  // !FEATURE_OTA_UPDATE || NATIVE_BUILD

// Stub implementation for native builds and non-OTA configurations
namespace lightwaveos {
namespace core {
namespace system {

class OtaBootVerifier {
public:
    static void init() {}
    static void markAppValidIfHealthy(bool, bool) {}
    static bool isFirstBootAfterOta() { return false; }
    static bool isValidated() { return true; }
    static void rollback(const char*) {}
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif  // FEATURE_OTA_UPDATE && !NATIVE_BUILD
