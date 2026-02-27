// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaTokenManager.h
 * @brief Per-device OTA token management with NVS persistence
 *
 * Generates and stores a unique per-device OTA authentication token in NVS.
 * On first boot (no token in NVS), a 128-bit random token is generated using
 * the ESP32-S3 hardware RNG (esp_random()) and stored as a 32-character
 * lowercase hex string.
 *
 * The token persists across firmware updates (NVS survives OTA).
 * If NVS access fails, falls back to the compile-time OTA_UPDATE_TOKEN.
 *
 * Storage:
 *   - NVS Namespace: "ota"
 *   - NVS Key: "token"
 *   - Format: 32-char lowercase hex string (128-bit entropy)
 *
 * Security:
 *   - Token values are NEVER logged to serial
 *   - Only token lifecycle events (generated, regenerated, set) are logged
 *   - Constant-time comparison is handled by callers (FirmwareHandlers, WsOtaCommands)
 *
 * Thread safety:
 *   - init() must be called from setup() before WebServer starts (single-threaded)
 *   - getToken() returns a cached copy, safe to call from any core
 *   - regenerateToken()/setToken() should only be called from Core 0 (HTTP handlers)
 *
 * @see FirmwareHandlers.cpp - REST OTA auth
 * @see WsOtaCommands.cpp - WebSocket OTA auth
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER && !defined(NATIVE_BUILD)

#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

namespace lightwaveos {
namespace core {
namespace system {

class OtaTokenManager {
public:
    /// Token length: 32 hex chars = 128 bits of entropy
    static constexpr size_t TOKEN_LENGTH = 32;

    /// NVS namespace for OTA token storage
    static constexpr const char* NVS_NAMESPACE = "ota";

    /// NVS key for the token
    static constexpr const char* NVS_KEY = "token";

    /**
     * @brief Initialize the OTA token manager
     *
     * Opens NVS namespace "ota" and loads the stored token.
     * If no token exists (first boot), generates a new one via hardware RNG.
     * If NVS fails entirely, falls back to compile-time OTA_UPDATE_TOKEN.
     *
     * Must be called once from setup() before WebServer starts.
     *
     * @return true if initialized successfully (NVS or fallback)
     */
    static bool init();

    /**
     * @brief Get the current OTA token
     *
     * Returns the cached token string. Safe to call from any core.
     * Never returns an empty string -- falls back to compile-time token.
     *
     * @return Current OTA token (32-char hex or compile-time fallback)
     */
    static const String& getToken();

    /**
     * @brief Generate a new random token and store in NVS
     *
     * Uses esp_random() for hardware RNG. The new token replaces the
     * previous one in both NVS and the in-memory cache.
     *
     * Logs the regeneration event (but NOT the token value).
     *
     * @return true if token was generated and stored successfully
     */
    static bool regenerateToken();

    /**
     * @brief Manually set a specific token
     *
     * Validates that the token is non-empty and stores it in NVS.
     * The token is cached in memory for fast access.
     *
     * Logs the set event (but NOT the token value).
     *
     * @param token The token string to set
     * @return true if token was stored successfully
     */
    static bool setToken(const char* token);

    /**
     * @brief Check if the manager is initialized
     * @return true if init() was called successfully
     */
    static bool isInitialized() { return s_initialized; }

    /**
     * @brief Check if using NVS token (vs compile-time fallback)
     * @return true if the active token came from NVS
     */
    static bool isUsingNvsToken() { return s_usingNvs; }

private:
    /**
     * @brief Generate a 32-char lowercase hex token from hardware RNG
     * @param outBuf Buffer of at least TOKEN_LENGTH+1 bytes
     */
    static void generateRandomHexToken(char* outBuf);

    static inline bool s_initialized = false;
    static inline bool s_usingNvs = false;
    static inline String s_cachedToken;
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#else  // Stub for native builds or disabled features

#include <cstddef>
#include <Arduino.h>

namespace lightwaveos {
namespace core {
namespace system {

class OtaTokenManager {
public:
    static constexpr size_t TOKEN_LENGTH = 32;
    static bool init() { return true; }
    static const String& getToken() {
        static String s_fallback = "stub-token";
        return s_fallback;
    }
    static bool regenerateToken() { return false; }
    static bool setToken(const char*) { return false; }
    static bool isInitialized() { return false; }
    static bool isUsingNvsToken() { return false; }
};

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif  // FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER && !NATIVE_BUILD
