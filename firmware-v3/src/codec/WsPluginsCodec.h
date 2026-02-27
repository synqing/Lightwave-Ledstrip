// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsPluginsCodec.h
 * @brief JSON codec for WebSocket plugins commands parsing and validation
 *
 * Single canonical location for parsing WebSocket plugins command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from plugins WS commands.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>

// Forward declarations for encoder dependencies
namespace lightwaveos {
namespace plugins {
struct PluginStats;
struct ParsedManifest;
class PluginManagerActor;
} // namespace plugins
} // namespace lightwaveos

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Plugins Request (shared across all plugin commands)
// ============================================================================

/**
 * @brief Decoded plugins command request
 * 
 * All three plugin commands (list, stats, reload) share the same minimal structure:
 * - requestId (optional string for correlation)
 */
struct PluginsRequest {
    const char* requestId;  // Optional (for correlation)
    
    PluginsRequest() : requestId("") {}
};

/**
 * @brief Decode result with typed request and error
 */
struct PluginsDecodeResult {
    bool success;
    PluginsRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    PluginsDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// WebSocket Plugins Command JSON Codec
// ============================================================================

/**
 * @brief WebSocket Plugins Command JSON Codec
 *
 * Single canonical parser for plugins WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Optional field defaults using operator|
 * - Unknown-key rejection (always enforced)
 */
class WsPluginsCodec {
public:
    /**
     * @brief Decode plugins.list JSON into typed request
     *
     * @param root JSON root object (const, prevents mutation)
     * @return PluginsDecodeResult with request or error
     */
    static PluginsDecodeResult decodePluginsList(JsonObjectConst root);
    
    /**
     * @brief Decode plugins.stats JSON into typed request
     *
     * @param root JSON root object (const, prevents mutation)
     * @return PluginsDecodeResult with request or error
     */
    static PluginsDecodeResult decodePluginsStats(JsonObjectConst root);
    
    /**
     * @brief Decode plugins.reload JSON into typed request
     *
     * @param root JSON root object (const, prevents mutation)
     * @return PluginsDecodeResult with request or error
     */
    static PluginsDecodeResult decodePluginsReload(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    static void encodePluginsList(const ::lightwaveos::plugins::PluginManagerActor& manager, const ::lightwaveos::plugins::PluginStats& stats, JsonObject& data);
    static void encodePluginsStats(const ::lightwaveos::plugins::PluginStats& stats, JsonObject& data);
    static void encodePluginsReload(bool reloadSuccess, const ::lightwaveos::plugins::PluginStats& stats, uint8_t manifestCount, const ::lightwaveos::plugins::ParsedManifest* manifests, JsonObject& data);
    
private:
    /**
     * @brief Check for unknown keys in root object
     *
     * @param root JSON root object
     * @param allowedKeys Array of allowed key names
     * @param allowedCount Number of allowed keys
     * @param errorMsg Output buffer for error message
     * @return true if valid, false if unknown key found
     */
    static bool checkUnknownKeys(JsonObjectConst root, const char* allowedKeys[], size_t allowedCount, char* errorMsg);
};

} // namespace codec
} // namespace lightwaveos
