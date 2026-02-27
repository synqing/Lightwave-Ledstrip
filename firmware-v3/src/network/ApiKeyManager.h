// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ApiKeyManager.h
 * @brief NVS-based storage for API authentication key
 *
 * Manages a single API key stored in NVS. Provides methods to get, set,
 * generate, and clear the key. Falls back to compile-time API_KEY if
 * NVS is empty.
 *
 * Storage format:
 * - Namespace: "auth"
 * - Key: "api_key" (plain string, max 64 chars)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#include <Arduino.h>
#include <Preferences.h>

namespace lightwaveos {
namespace network {

/**
 * @brief API key manager with NVS persistence
 */
class ApiKeyManager {
public:
    /**
     * @brief Maximum API key length
     */
    static constexpr size_t MAX_KEY_LENGTH = 64;

    /**
     * @brief Generated key length (alphanumeric)
     */
    static constexpr size_t GENERATED_KEY_LENGTH = 32;

    /**
     * @brief Constructor
     */
    ApiKeyManager();

    /**
     * @brief Destructor
     */
    ~ApiKeyManager();

    // Prevent copying
    ApiKeyManager(const ApiKeyManager&) = delete;
    ApiKeyManager& operator=(const ApiKeyManager&) = delete;

    /**
     * @brief Initialize NVS storage and load key
     * @return true if initialization succeeded
     */
    bool begin();

    /**
     * @brief Close NVS storage
     */
    void end();

    /**
     * @brief Get the current API key
     *
     * Returns NVS key if set, otherwise returns compile-time API_KEY.
     *
     * @return Current API key (never empty if auth is enabled)
     */
    String getKey() const;

    /**
     * @brief Set a new API key in NVS
     * @param key New API key (1-64 chars)
     * @return true if saved successfully, false if invalid or error
     */
    bool setKey(const String& key);

    /**
     * @brief Generate a new random API key and save to NVS
     *
     * Generates a 32-character alphanumeric key with "LW-" prefix.
     * Format: LW-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX (35 chars total)
     *
     * @return The newly generated key, or empty string on error
     */
    String generateKey();

    /**
     * @brief Clear the NVS key (reverts to compile-time default)
     * @return true if cleared successfully
     */
    bool clearKey();

    /**
     * @brief Check if a custom key is configured in NVS
     * @return true if NVS has a key, false if using compile-time default
     */
    bool hasCustomKey() const;

    /**
     * @brief Validate an API key against the stored key
     * @param key Key to validate
     * @return true if key matches, false otherwise
     */
    bool validateKey(const String& key) const;

private:
    Preferences m_prefs;
    bool m_initialized;
    String m_cachedKey;      // Cached NVS key (empty if not set)
    bool m_hasNvsKey;        // True if NVS has a custom key

    /**
     * @brief Load key from NVS into cache
     */
    void loadFromNvs();

    /**
     * @brief Get the compile-time default key
     * @return Compile-time API_KEY value
     */
    String getDefaultKey() const;
};

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
