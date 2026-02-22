/**
 * @file WsPluginsCodec.cpp
 * @brief WebSocket plugins codec implementation
 *
 * Single canonical JSON parser for plugins WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsPluginsCodec.h"
#include "../plugins/PluginManagerActor.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Unknown Key Checking Helper
// ============================================================================

static bool isAllowedKey(const char* key, const char* const* allowedKeys, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(key, allowedKeys[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool WsPluginsCodec::checkUnknownKeys(JsonObjectConst root, const char* allowedKeys[], size_t allowedCount, char* errorMsg) {
    for (JsonPairConst kv : root) {
        const char* key = kv.key().c_str();
        if (!isAllowedKey(key, allowedKeys, allowedCount)) {
            snprintf(errorMsg, MAX_ERROR_MSG, "Unknown key '%s'", key);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Plugins List Decode
// ============================================================================

PluginsDecodeResult WsPluginsCodec::decodePluginsList(JsonObjectConst root) {
    PluginsDecodeResult result;
    result.request = PluginsRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract requestId (optional string, default: "")
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Plugins Stats Decode
// ============================================================================

PluginsDecodeResult WsPluginsCodec::decodePluginsStats(JsonObjectConst root) {
    PluginsDecodeResult result;
    result.request = PluginsRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract requestId (optional string, default: "")
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Plugins Reload Decode
// ============================================================================

PluginsDecodeResult WsPluginsCodec::decodePluginsReload(JsonObjectConst root) {
    PluginsDecodeResult result;
    result.request = PluginsRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract requestId (optional string, default: "")
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsPluginsCodec::encodePluginsList(const ::lightwaveos::plugins::PluginManagerActor& manager, const ::lightwaveos::plugins::PluginStats& stats, JsonObject& data) {
    data["registeredCount"] = stats.registeredCount;
    data["overrideModeEnabled"] = stats.overrideModeEnabled;
    
    // List registered effect IDs
    JsonArray effects = data["effects"].to<JsonArray>();
    for (uint16_t i = 0; i < plugins::PluginConfig::MAX_EFFECTS; i++) {
        if (manager.isEffectRegistered(i)) {
            effects.add(i);
        }
    }
}

void WsPluginsCodec::encodePluginsStats(const ::lightwaveos::plugins::PluginStats& stats, JsonObject& data) {
    // Core stats
    data["registeredCount"] = stats.registeredCount;
    data["loadedFromLittleFS"] = stats.loadedFromLittleFS;
    data["overrideModeEnabled"] = stats.overrideModeEnabled;
    data["disabledByOverride"] = stats.disabledByOverride;
    data["registrationsFailed"] = stats.registrationsFailed;
    data["unregistrations"] = stats.unregistrations;
    
    // Reload status
    data["lastReloadOk"] = stats.lastReloadOk;
    data["lastReloadMillis"] = stats.lastReloadMillis;
    data["manifestCount"] = stats.manifestCount;
    data["errorCount"] = stats.errorCount;
    
    // Include error summary if present
    if (stats.lastErrorSummary[0] != '\0') {
        data["lastErrorSummary"] = stats.lastErrorSummary;
    }
}

void WsPluginsCodec::encodePluginsReload(bool reloadSuccess, const ::lightwaveos::plugins::PluginStats& stats, uint8_t manifestCount, const ::lightwaveos::plugins::ParsedManifest* manifests, JsonObject& data) {
    data["reloadSuccess"] = reloadSuccess;
    
    // Stats
    JsonObject statsObj = data["stats"].to<JsonObject>();
    statsObj["registeredCount"] = stats.registeredCount;
    statsObj["loadedFromLittleFS"] = stats.loadedFromLittleFS;
    statsObj["overrideModeEnabled"] = stats.overrideModeEnabled;
    statsObj["disabledByOverride"] = stats.disabledByOverride;
    statsObj["lastReloadOk"] = stats.lastReloadOk;
    statsObj["lastReloadMillis"] = stats.lastReloadMillis;
    statsObj["manifestCount"] = stats.manifestCount;
    statsObj["errorCount"] = stats.errorCount;
    
    // Errors array (only include manifests with errors)
    JsonArray errors = data["errors"].to<JsonArray>();
    for (uint8_t i = 0; i < manifestCount; i++) {
        if (!manifests[i].valid) {
            JsonObject errObj = errors.add<JsonObject>();
            errObj["file"] = manifests[i].filePath;
            errObj["error"] = manifests[i].errorMsg;
        }
    }
}

} // namespace codec
} // namespace lightwaveos
