/**
 * @file OtaTokenManager.cpp
 * @brief Per-device OTA token management with NVS persistence
 *
 * @see OtaTokenManager.h for full documentation
 */

#include "OtaTokenManager.h"

#if FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER && !defined(NATIVE_BUILD)

#include "../../config/network_config.h"

#define LW_LOG_TAG "OtaToken"
#include "../../utils/Log.h"

namespace lightwaveos {
namespace core {
namespace system {

// ============================================================================
// Static member definitions (declared inline in header, linked here)
// ============================================================================

bool OtaTokenManager::init() {
    if (s_initialized) {
        return true;
    }

    Preferences prefs;

    // Open NVS namespace in read-write mode
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LW_LOGW("NVS namespace '%s' open failed -- using compile-time fallback token", NVS_NAMESPACE);
        s_cachedToken = config::NetworkConfig::OTA_UPDATE_TOKEN;
        s_usingNvs = false;
        s_initialized = true;

        // Emit telemetry (no token value)
        Serial.printf("{\"event\":\"ota.token.init\",\"ts_mono_ms\":%lu,\"source\":\"compile_time\"}\n",
                      static_cast<unsigned long>(millis()));
        return true;
    }

    // Try to read existing token from NVS
    String storedToken = prefs.getString(NVS_KEY, "");

    if (storedToken.length() > 0) {
        // Token exists in NVS -- use it
        s_cachedToken = storedToken;
        s_usingNvs = true;
        s_initialized = true;
        prefs.end();

        LW_LOGI("OTA token loaded from NVS (%u chars)", storedToken.length());
        Serial.printf("{\"event\":\"ota.token.init\",\"ts_mono_ms\":%lu,\"source\":\"nvs\",\"tokenLen\":%u}\n",
                      static_cast<unsigned long>(millis()),
                      storedToken.length());
        return true;
    }

    // No token in NVS -- generate one using hardware RNG
    char tokenBuf[TOKEN_LENGTH + 1];
    generateRandomHexToken(tokenBuf);

    // Store in NVS
    if (prefs.putString(NVS_KEY, tokenBuf) == 0) {
        // Write failed -- fall back to compile-time token
        LW_LOGW("NVS write failed -- using compile-time fallback token");
        s_cachedToken = config::NetworkConfig::OTA_UPDATE_TOKEN;
        s_usingNvs = false;
        s_initialized = true;
        prefs.end();

        Serial.printf("{\"event\":\"ota.token.init\",\"ts_mono_ms\":%lu,\"source\":\"compile_time\",\"reason\":\"nvs_write_failed\"}\n",
                      static_cast<unsigned long>(millis()));
        return true;
    }

    s_cachedToken = tokenBuf;
    s_usingNvs = true;
    s_initialized = true;
    prefs.end();

    LW_LOGI("Generated new per-device OTA token (first boot)");
    Serial.printf("{\"event\":\"ota.token.generated\",\"ts_mono_ms\":%lu,\"tokenLen\":%u}\n",
                  static_cast<unsigned long>(millis()),
                  TOKEN_LENGTH);

    return true;
}

const String& OtaTokenManager::getToken() {
    if (!s_initialized) {
        // Safety: if someone calls getToken() before init(), return compile-time token
        static String s_fallback(config::NetworkConfig::OTA_UPDATE_TOKEN);
        return s_fallback;
    }
    return s_cachedToken;
}

bool OtaTokenManager::regenerateToken() {
    // Generate new random token
    char tokenBuf[TOKEN_LENGTH + 1];
    generateRandomHexToken(tokenBuf);

    // Store in NVS
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LW_LOGW("NVS open failed during token regeneration");
        return false;
    }

    if (prefs.putString(NVS_KEY, tokenBuf) == 0) {
        LW_LOGW("NVS write failed during token regeneration");
        prefs.end();
        return false;
    }

    prefs.end();

    // Update in-memory cache
    s_cachedToken = tokenBuf;
    s_usingNvs = true;

    LW_LOGI("OTA token regenerated");
    Serial.printf("{\"event\":\"ota.token.regenerated\",\"ts_mono_ms\":%lu}\n",
                  static_cast<unsigned long>(millis()));

    return true;
}

bool OtaTokenManager::setToken(const char* token) {
    if (token == nullptr || strlen(token) == 0) {
        LW_LOGW("setToken() called with empty token");
        return false;
    }

    // Store in NVS
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        LW_LOGW("NVS open failed during setToken()");
        return false;
    }

    if (prefs.putString(NVS_KEY, token) == 0) {
        LW_LOGW("NVS write failed during setToken()");
        prefs.end();
        return false;
    }

    prefs.end();

    // Update in-memory cache
    s_cachedToken = token;
    s_usingNvs = true;

    LW_LOGI("OTA token set manually (%u chars)", strlen(token));
    Serial.printf("{\"event\":\"ota.token.set\",\"ts_mono_ms\":%lu,\"tokenLen\":%u}\n",
                  static_cast<unsigned long>(millis()),
                  static_cast<unsigned>(strlen(token)));

    return true;
}

void OtaTokenManager::generateRandomHexToken(char* outBuf) {
    // Generate 16 random bytes (128 bits) using ESP32 hardware TRNG
    // esp_random() returns a true random 32-bit value from the hardware RNG
    uint32_t randomWords[4];
    for (int i = 0; i < 4; i++) {
        randomWords[i] = esp_random();
    }

    // Convert to lowercase hex string
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(randomWords);
    for (int i = 0; i < 16; i++) {
        snprintf(&outBuf[i * 2], 3, "%02x", bytes[i]);
    }
    outBuf[TOKEN_LENGTH] = '\0';
}

} // namespace system
} // namespace core
} // namespace lightwaveos

#endif  // FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER && !NATIVE_BUILD
