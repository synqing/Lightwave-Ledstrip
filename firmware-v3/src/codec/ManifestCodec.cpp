/**
 * @file ManifestCodec.cpp
 * @brief Manifest codec implementation
 *
 * Single canonical JSON parser for plugin manifests. Enforces strict schema validation
 * and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "ManifestCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Allowed Keys (Schema 2)
// ============================================================================

// Root level allowed keys
static constexpr const char* ALLOWED_ROOT_KEYS[] = {
    "schema",
    "version",
    "plugin",
    "mode",
    "effects"
};
static constexpr size_t ALLOWED_ROOT_KEYS_COUNT = sizeof(ALLOWED_ROOT_KEYS) / sizeof(ALLOWED_ROOT_KEYS[0]);

// Plugin object allowed keys
static constexpr const char* ALLOWED_PLUGIN_KEYS[] = {
    "name",
    "version",
    "author",
    "description"
};
static constexpr size_t ALLOWED_PLUGIN_KEYS_COUNT = sizeof(ALLOWED_PLUGIN_KEYS) / sizeof(ALLOWED_PLUGIN_KEYS[0]);

// Effects array element allowed keys
static constexpr const char* ALLOWED_EFFECT_KEYS[] = {
    "id",
    "name"
};
static constexpr size_t ALLOWED_EFFECT_KEYS_COUNT = sizeof(ALLOWED_EFFECT_KEYS) / sizeof(ALLOWED_EFFECT_KEYS[0]);

// ============================================================================
// Unknown Key Checking (Schema 2)
// ============================================================================

static bool isAllowedKey(const char* key, const char* const* allowedKeys, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(key, allowedKeys[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool ManifestCodec::checkUnknownRootKeys(JsonObjectConst root, uint8_t schemaVersion, char* errorMsg) {
    if (schemaVersion < 2) {
        return true; // Skip check for schema 1
    }

    for (JsonPairConst kv : root) {
        const char* key = kv.key().c_str();
        if (!isAllowedKey(key, ALLOWED_ROOT_KEYS, ALLOWED_ROOT_KEYS_COUNT)) {
            snprintf(errorMsg, MAX_ERROR_MSG, "Unknown key '%s' at root level", key);
            return false;
        }
    }
    return true;
}

bool ManifestCodec::checkUnknownPluginKeys(JsonObjectConst plugin, uint8_t schemaVersion, char* errorMsg) {
    if (schemaVersion < 2) {
        return true; // Skip check for schema 1
    }

    for (JsonPairConst kv : plugin) {
        const char* key = kv.key().c_str();
        if (!isAllowedKey(key, ALLOWED_PLUGIN_KEYS, ALLOWED_PLUGIN_KEYS_COUNT)) {
            snprintf(errorMsg, MAX_ERROR_MSG, "Unknown key '%s' in plugin object", key);
            return false;
        }
    }
    return true;
}

bool ManifestCodec::checkUnknownEffectKeys(JsonObjectConst effect, uint8_t schemaVersion, char* errorMsg) {
    if (schemaVersion < 2) {
        return true; // Skip check for schema 1
    }

    for (JsonPairConst kv : effect) {
        const char* key = kv.key().c_str();
        if (!isAllowedKey(key, ALLOWED_EFFECT_KEYS, ALLOWED_EFFECT_KEYS_COUNT)) {
            snprintf(errorMsg, MAX_ERROR_MSG, "Unknown key '%s' in effects array element", key);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Main Decode Function
// ============================================================================

ManifestDecodeResult ManifestCodec::decode(JsonObjectConst root) {
    ManifestDecodeResult result;
    result.config = ManifestConfig();

    // Step 1: Extract schema version (optional, defaults to 1)
    if (root["schema"].is<int>()) {
        int schema = root["schema"].as<int>();
        if (schema < 1 || schema > 2) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Unsupported schema version: %d", schema);
            return result;
        }
        result.config.schemaVersion = static_cast<uint8_t>(schema);
    } else {
        result.config.schemaVersion = 1; // Default to v1 if missing
    }

    // Step 2: Check for unknown keys at root level (schema 2 only)
    if (!checkUnknownRootKeys(root, result.config.schemaVersion, result.errorMsg)) {
        return result;
    }

    // Step 3: Extract version (required string)
    if (!root["version"].is<const char*>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'version'");
        return result;
    }
    const char* version = root["version"].as<const char*>();
    if (!version || strcmp(version, "1.0") != 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Unsupported version: %s", version ? version : "(null)");
        return result;
    }

    // Step 4: Extract plugin object (required)
    if (!root["plugin"].is<JsonObjectConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'plugin'");
        return result;
    }
    JsonObjectConst plugin = root["plugin"].as<JsonObjectConst>();
    if (!plugin) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'plugin' must be an object");
        return result;
    }

    // Step 5: Check for unknown keys in plugin object (schema 2 only)
    if (!checkUnknownPluginKeys(plugin, result.config.schemaVersion, result.errorMsg)) {
        return result;
    }

    // Step 6: Extract plugin.name (required string)
    if (!plugin["name"].is<const char*>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'plugin.name'");
        return result;
    }
    const char* pluginName = plugin["name"].as<const char*>();
    if (!pluginName || strlen(pluginName) == 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Plugin name must not be empty");
        return result;
    }
    if (strlen(pluginName) >= MAX_PLUGIN_NAME) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Plugin name too long (max %u chars)", MAX_PLUGIN_NAME - 1);
        return result;
    }
    strncpy(result.config.pluginName, pluginName, sizeof(result.config.pluginName) - 1);
    result.config.pluginName[sizeof(result.config.pluginName) - 1] = '\0';

    // Step 7: Extract mode (optional string, default: "additive")
    const char* mode = root["mode"] | "additive";
    result.config.overrideMode = (strcmp(mode, "override") == 0);

    // Step 8: Extract effects array (required)
    if (!root["effects"].is<JsonArrayConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effects'");
        return result;
    }
    JsonArrayConst effects = root["effects"].as<JsonArrayConst>();
    if (!effects) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'effects' must be an array");
        return result;
    }

    if (effects.size() == 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Effects array must not be empty");
        return result;
    }

    if (effects.size() > MAX_MANIFEST_EFFECTS) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Too many effects (max %u)", MAX_MANIFEST_EFFECTS);
        return result;
    }

    // Step 9: Parse effect IDs
    result.config.effectCount = 0;
    for (JsonObjectConst effect : effects) {
        // Check for unknown keys in effect object (schema 2 only)
        if (!checkUnknownEffectKeys(effect, result.config.schemaVersion, result.errorMsg)) {
            return result;
        }

        // Extract effect.id (required integer)
        if (!effect["id"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effects[%u].id'", result.config.effectCount);
            return result;
        }
        int id = effect["id"].as<int>();

        if (id < 0 || id >= static_cast<int>(MAX_MANIFEST_EFFECTS)) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Invalid effect ID: %d (must be 0-%u)", id, MAX_MANIFEST_EFFECTS - 1);
            return result;
        }

        result.config.effectIds[result.config.effectCount++] = static_cast<uint8_t>(id);
    }

    // Success
    result.success = true;
    return result;
}

} // namespace codec
} // namespace lightwaveos
