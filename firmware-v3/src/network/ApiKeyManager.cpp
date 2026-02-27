// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ApiKeyManager.cpp
 * @brief NVS-based storage implementation for API authentication key
 */

#include "ApiKeyManager.h"

#if FEATURE_WEB_SERVER && FEATURE_API_AUTH

#include "../config/network_config.h"
#include <esp_random.h>

#define LW_LOG_TAG "ApiKey"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {

// NVS namespace and key names
static constexpr const char* NVS_NAMESPACE = "auth";
static constexpr const char* NVS_KEY_NAME = "api_key";

// Character set for generated keys (alphanumeric)
static constexpr const char* KEY_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static constexpr size_t KEY_CHARS_LEN = 62;

ApiKeyManager::ApiKeyManager()
    : m_initialized(false)
    , m_hasNvsKey(false)
{
}

ApiKeyManager::~ApiKeyManager() {
    end();
}

bool ApiKeyManager::begin() {
    if (m_initialized) {
        LW_LOGW("ApiKeyManager already initialized");
        return true;
    }

    // Open NVS namespace (read-write mode)
    if (!m_prefs.begin(NVS_NAMESPACE, false)) {
        LW_LOGE("Failed to open NVS namespace '%s'", NVS_NAMESPACE);
        return false;
    }

    m_initialized = true;

    // Load key from NVS
    loadFromNvs();

    if (m_hasNvsKey) {
        LW_LOGI("ApiKeyManager initialized - custom key configured");
    } else {
        LW_LOGI("ApiKeyManager initialized - using compile-time default key");
    }

    return true;
}

void ApiKeyManager::end() {
    if (m_initialized) {
        m_prefs.end();
        m_initialized = false;
        m_cachedKey = "";
        m_hasNvsKey = false;
    }
}

void ApiKeyManager::loadFromNvs() {
    if (!m_initialized) {
        return;
    }

    m_cachedKey = m_prefs.getString(NVS_KEY_NAME, "");
    m_hasNvsKey = (m_cachedKey.length() > 0);
}

String ApiKeyManager::getDefaultKey() const {
    return String(config::NetworkConfig::API_KEY_VALUE);
}

String ApiKeyManager::getKey() const {
    if (m_hasNvsKey && m_cachedKey.length() > 0) {
        return m_cachedKey;
    }
    return getDefaultKey();
}

bool ApiKeyManager::setKey(const String& key) {
    if (!m_initialized) {
        LW_LOGE("ApiKeyManager not initialized");
        return false;
    }

    // Validate key length
    if (key.length() == 0) {
        LW_LOGE("API key cannot be empty");
        return false;
    }
    if (key.length() > MAX_KEY_LENGTH) {
        LW_LOGE("API key too long (%d chars, max %d)", key.length(), MAX_KEY_LENGTH);
        return false;
    }

    // Save to NVS
    if (m_prefs.putString(NVS_KEY_NAME, key)) {
        m_cachedKey = key;
        m_hasNvsKey = true;
        LW_LOGI("API key updated successfully");
        return true;
    } else {
        LW_LOGE("Failed to save API key to NVS");
        return false;
    }
}

String ApiKeyManager::generateKey() {
    if (!m_initialized) {
        LW_LOGE("ApiKeyManager not initialized");
        return "";
    }

    // Generate key with format: LW-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX
    // 7 groups of 4 chars = 28 chars + 6 dashes + "LW-" prefix = 37 chars total
    String newKey = "LW-";

    for (int group = 0; group < 7; group++) {
        if (group > 0) {
            newKey += "-";
        }
        for (int i = 0; i < 4; i++) {
            uint32_t randomIndex = esp_random() % KEY_CHARS_LEN;
            newKey += KEY_CHARS[randomIndex];
        }
    }

    // Save the generated key
    if (setKey(newKey)) {
        LW_LOGI("Generated new API key: %s", newKey.c_str());
        return newKey;
    }

    LW_LOGE("Failed to save generated key");
    return "";
}

bool ApiKeyManager::clearKey() {
    if (!m_initialized) {
        LW_LOGE("ApiKeyManager not initialized");
        return false;
    }

    if (m_prefs.remove(NVS_KEY_NAME)) {
        m_cachedKey = "";
        m_hasNvsKey = false;
        LW_LOGI("API key cleared - reverting to compile-time default");
        return true;
    } else {
        LW_LOGE("Failed to clear API key from NVS");
        return false;
    }
}

bool ApiKeyManager::hasCustomKey() const {
    return m_hasNvsKey;
}

bool ApiKeyManager::validateKey(const String& key) const {
    if (key.length() == 0) {
        return false;
    }

    String currentKey = getKey();
    if (currentKey.length() == 0) {
        // Auth is enabled but no key configured - this shouldn't happen
        // but we'll deny access for safety
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    if (key.length() != currentKey.length()) {
        return false;
    }

    uint8_t result = 0;
    for (size_t i = 0; i < key.length(); i++) {
        result |= key[i] ^ currentKey[i];
    }
    return result == 0;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER && FEATURE_API_AUTH
