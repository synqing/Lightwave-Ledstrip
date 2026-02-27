// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ManifestCodec.h
 * @brief JSON codec for plugin manifest parsing and validation
 *
 * Single canonical location for parsing plugin manifest JSON into typed C++ structs.
 * Enforces schema versioning, type checking, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from manifests.
 * All other code consumes typed ManifestConfig structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum number of effect IDs in a manifest
 */
static constexpr uint8_t MAX_MANIFEST_EFFECTS = 128;

/**
 * @brief Maximum length for plugin name
 */
static constexpr size_t MAX_PLUGIN_NAME = 64;

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

/**
 * @brief Decoded manifest configuration
 */
struct ManifestConfig {
    uint8_t schemaVersion;              // Schema version (1 or 2)
    char pluginName[MAX_PLUGIN_NAME];   // Plugin name (required)
    bool overrideMode;                  // Override mode (default: false)
    uint8_t effectIds[MAX_MANIFEST_EFFECTS]; // Effect IDs
    uint8_t effectCount;                // Number of effects

    ManifestConfig() : schemaVersion(1), overrideMode(false), effectCount(0) {
        memset(pluginName, 0, sizeof(pluginName));
        memset(effectIds, 0, sizeof(effectIds));
    }
};

/**
 * @brief Decode result with typed config and error
 */
struct ManifestDecodeResult {
    bool success;
    ManifestConfig config;
    char errorMsg[MAX_ERROR_MSG];

    ManifestDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Manifest JSON Codec
 *
 * Single canonical parser for plugin manifest JSON. Enforces:
 * - Schema versioning (missing → v1, >2 → reject)
 * - Type checking using is<T>()
 * - Defaults using operator|
 * - Unknown-key rejection (schema 2 only)
 */
class ManifestCodec {
public:
    /**
     * @brief Decode manifest JSON into typed config
     *
     * @param root JSON root object (const, prevents mutation)
     * @return ManifestDecodeResult with config or error
     */
    static ManifestDecodeResult decode(JsonObjectConst root);

private:
    /**
     * @brief Check for unknown keys at root level (schema 2 only)
     *
     * @param root JSON root object
     * @param schemaVersion Schema version (only check if >= 2)
     * @param errorMsg Output buffer for error message
     * @return true if valid, false if unknown key found
     */
    static bool checkUnknownRootKeys(JsonObjectConst root, uint8_t schemaVersion, char* errorMsg);

    /**
     * @brief Check for unknown keys in plugin object (schema 2 only)
     *
     * @param plugin JSON plugin object
     * @param schemaVersion Schema version (only check if >= 2)
     * @param errorMsg Output buffer for error message
     * @return true if valid, false if unknown key found
     */
    static bool checkUnknownPluginKeys(JsonObjectConst plugin, uint8_t schemaVersion, char* errorMsg);

    /**
     * @brief Check for unknown keys in effects array element (schema 2 only)
     *
     * @param effect JSON effect object
     * @param schemaVersion Schema version (only check if >= 2)
     * @param errorMsg Output buffer for error message
     * @return true if valid, false if unknown key found
     */
    static bool checkUnknownEffectKeys(JsonObjectConst effect, uint8_t schemaVersion, char* errorMsg);
};

} // namespace codec
} // namespace lightwaveos
