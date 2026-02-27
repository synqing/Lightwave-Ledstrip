/**
 * @file HttpPluginCodec.h
 * @brief JSON codec for HTTP plugin endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP plugin endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from plugin HTTP endpoints.
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

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Plugin list response data
 */
struct HttpPluginListData {
    uint8_t registeredCount;
    bool loadedFromLittleFS;
    bool overrideModeEnabled;
    
    HttpPluginListData() : registeredCount(0), loadedFromLittleFS(false), overrideModeEnabled(false) {}
};

/**
 * @brief Plugin stats response data
 */
struct HttpPluginStatsData {
    uint8_t registeredCount;
    bool loadedFromLittleFS;
    bool overrideModeEnabled;
    bool disabledByOverride;
    uint32_t registrationsFailed;
    uint32_t unregistrations;
    bool lastReloadOk;
    uint32_t lastReloadMillis;
    uint8_t manifestCount;
    uint8_t errorCount;
    const char* lastErrorSummary;
    
    HttpPluginStatsData()
        : registeredCount(0), loadedFromLittleFS(false), overrideModeEnabled(false),
          disabledByOverride(false), registrationsFailed(0), unregistrations(0),
          lastReloadOk(false), lastReloadMillis(0), manifestCount(0), errorCount(0),
          lastErrorSummary(nullptr) {}
};

/**
 * @brief Plugin manifest item data
 */
struct HttpPluginManifestItemData {
    const char* file;
    bool valid;
    const char* name;
    const char* mode;
    uint8_t effectCount;
    const char* error;
    
    HttpPluginManifestItemData()
        : file(""), valid(false), name(nullptr), mode(nullptr), effectCount(0), error(nullptr) {}
};

/**
 * @brief Plugin manifests response data
 */
struct HttpPluginManifestsData {
    uint8_t count;
    const HttpPluginManifestItemData* manifests;
    size_t manifestCount;
    
    HttpPluginManifestsData() : count(0), manifests(nullptr), manifestCount(0) {}
};

/**
 * @brief Plugin reload response data
 */
struct HttpPluginReloadData {
    bool reloadSuccess;
    HttpPluginStatsData stats;
    const HttpPluginManifestItemData* errors;
    size_t errorCount;
    
    HttpPluginReloadData() : reloadSuccess(false), errors(nullptr), errorCount(0) {}
};

/**
 * @brief HTTP Plugin Command JSON Codec
 *
 * Single canonical parser for plugin HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpPluginCodec {
public:
    // Encode functions (response building)
    static void encodeList(const HttpPluginListData& data, JsonObject& obj);
    static void encodeStats(const HttpPluginStatsData& data, JsonObject& obj);
    static void encodeManifests(const HttpPluginManifestsData& data, JsonObject& obj);
    static void encodeReload(const HttpPluginReloadData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
