/**
 * @file HttpPluginCodec.cpp
 * @brief HTTP plugin codec implementation
 *
 * Single canonical JSON parser for plugin HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpPluginCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Encode Functions
// ============================================================================

void HttpPluginCodec::encodeList(const HttpPluginListData& data, JsonObject& obj) {
    obj["registeredCount"] = data.registeredCount;
    obj["loadedFromLittleFS"] = data.loadedFromLittleFS;
    obj["overrideModeEnabled"] = data.overrideModeEnabled;
}

void HttpPluginCodec::encodeStats(const HttpPluginStatsData& data, JsonObject& obj) {
    obj["registeredCount"] = data.registeredCount;
    obj["loadedFromLittleFS"] = data.loadedFromLittleFS;
    obj["overrideModeEnabled"] = data.overrideModeEnabled;
    obj["disabledByOverride"] = data.disabledByOverride;
    obj["registrationsFailed"] = data.registrationsFailed;
    obj["unregistrations"] = data.unregistrations;
    obj["lastReloadOk"] = data.lastReloadOk;
    obj["lastReloadMillis"] = data.lastReloadMillis;
    obj["manifestCount"] = data.manifestCount;
    obj["errorCount"] = data.errorCount;
    if (data.lastErrorSummary && data.lastErrorSummary[0] != '\0') {
        obj["lastErrorSummary"] = data.lastErrorSummary;
    }
}

void HttpPluginCodec::encodeManifests(const HttpPluginManifestsData& data, JsonObject& obj) {
    obj["count"] = data.count;
    JsonArray files = obj["files"].to<JsonArray>();
    for (size_t i = 0; i < data.manifestCount; ++i) {
        const HttpPluginManifestItemData& item = data.manifests[i];
        JsonObject fileObj = files.add<JsonObject>();
        fileObj["file"] = item.file;
        fileObj["valid"] = item.valid;
        if (item.valid) {
            fileObj["name"] = item.name;
            fileObj["mode"] = item.mode;
            fileObj["effectCount"] = item.effectCount;
        } else if (item.error) {
            fileObj["error"] = item.error;
        }
    }
}

void HttpPluginCodec::encodeReload(const HttpPluginReloadData& data, JsonObject& obj) {
    obj["reloadSuccess"] = data.reloadSuccess;

    JsonObject statsObj = obj["stats"].to<JsonObject>();
    encodeStats(data.stats, statsObj);

    JsonArray errors = obj["errors"].to<JsonArray>();
    for (size_t i = 0; i < data.errorCount; ++i) {
        const HttpPluginManifestItemData& item = data.errors[i];
        JsonObject errObj = errors.add<JsonObject>();
        errObj["file"] = item.file;
        errObj["error"] = item.error ? item.error : "";
    }
}

} // namespace codec
} // namespace lightwaveos
