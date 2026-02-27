// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PluginManagerActor.cpp
 * @brief Plugin manager implementation
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "PluginManagerActor.h"
#include "BuiltinEffectRegistry.h"
#include "../codec/ManifestCodec.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#ifndef NATIVE_BUILD
#include <LittleFS.h>
#endif

#define LW_LOG_TAG "PluginMgr"
#include "../utils/Log.h"

namespace lightwaveos {
namespace plugins {

// ============================================================================
// Constructor
// ============================================================================

PluginManagerActor::PluginManagerActor()
    : m_targetRegistry(nullptr)
    , m_overrideMode(false)
    , m_manifestCount(0)
{
    memset(m_effects, 0, sizeof(m_effects));
    memset(&m_stats, 0, sizeof(m_stats));
    memset(m_manifests, 0, sizeof(m_manifests));
    memset(m_allowedEffects, 0, sizeof(m_allowedEffects));

    LW_LOGD("PluginManagerActor constructed");
}

// ============================================================================
// Lifecycle
// ============================================================================

void PluginManagerActor::onStart() {
    LW_LOGI("PluginManagerActor starting...");
    loadPluginsFromLittleFS();
}

// ============================================================================
// IEffectRegistry Implementation
// ============================================================================

bool PluginManagerActor::registerEffect(uint8_t id, IEffect* effect) {
    if (id >= PluginConfig::MAX_EFFECTS || effect == nullptr) {
        m_stats.registrationsFailed++;
        return false;
    }

    // In override mode, only allow effects in the allowed list
    if (m_overrideMode && !m_allowedEffects[id]) {
        m_stats.disabledByOverride++;
        return false;
    }

    m_effects[id] = effect;
    m_stats.registeredCount++;

    // Forward to target registry if set
    if (m_targetRegistry) {
        return m_targetRegistry->registerEffect(id, effect);
    }

    return true;
}

bool PluginManagerActor::unregisterEffect(uint8_t id) {
    if (id >= PluginConfig::MAX_EFFECTS) {
        return false;
    }

    if (m_effects[id] != nullptr) {
        m_effects[id] = nullptr;
        m_stats.registeredCount--;
        m_stats.unregistrations++;

        if (m_targetRegistry) {
            return m_targetRegistry->unregisterEffect(id);
        }
        return true;
    }

    return false;
}

bool PluginManagerActor::isEffectRegistered(uint8_t id) const {
    if (id >= PluginConfig::MAX_EFFECTS) {
        return false;
    }
    return m_effects[id] != nullptr;
}

uint8_t PluginManagerActor::getRegisteredCount() const {
    return m_stats.registeredCount;
}

// ============================================================================
// Plugin Loading
// ============================================================================

void PluginManagerActor::loadPluginsFromLittleFS() {
#ifndef NATIVE_BUILD
    LW_LOGI("Loading plugins from LittleFS...");

    // Mount LittleFS
    if (!LittleFS.begin(false)) {
        LW_LOGW("LittleFS mount failed - plugin loading skipped");
        snprintf(m_stats.lastErrorSummary, sizeof(m_stats.lastErrorSummary),
                 "LittleFS mount failed");
        m_stats.lastReloadOk = false;
        m_stats.lastReloadMillis = millis();
        return;
    }

    // Scan and parse manifests
    m_manifestCount = scanManifestFiles();
    m_stats.manifestCount = m_manifestCount;

    if (m_manifestCount == 0) {
        LW_LOGI("No plugin manifests found");
        m_stats.lastReloadOk = true;
        m_stats.lastReloadMillis = millis();
        m_stats.errorCount = 0;
        return;
    }

    // Apply manifests
    if (applyManifests()) {
        m_stats.lastReloadOk = true;
        LW_LOGI("Plugin manifests applied: %u effects from %u manifests",
                m_stats.loadedFromLittleFS, m_manifestCount);
    } else {
        m_stats.lastReloadOk = false;
        LW_LOGW("Plugin manifest application failed");
    }

    m_stats.lastReloadMillis = millis();
#else
    LW_LOGD("LittleFS plugin loading not available in native build");
#endif
}

bool PluginManagerActor::reloadFromLittleFS() {
#ifndef NATIVE_BUILD
    LW_LOGI("Reloading plugins from LittleFS (atomic)...");

    // Mount LittleFS
    if (!LittleFS.begin(false)) {
        LW_LOGW("LittleFS mount failed during reload");
        snprintf(m_stats.lastErrorSummary, sizeof(m_stats.lastErrorSummary),
                 "LittleFS mount failed");
        m_stats.lastReloadOk = false;
        m_stats.lastReloadMillis = millis();
        return false;
    }

    // Store previous state for rollback
    ParsedManifest prevManifests[PluginConfig::MAX_MANIFESTS];
    uint8_t prevManifestCount = m_manifestCount;
    bool prevOverrideMode = m_overrideMode;
    memcpy(prevManifests, m_manifests, sizeof(m_manifests));

    // Scan and parse all manifests
    m_manifestCount = scanManifestFiles();
    m_stats.manifestCount = m_manifestCount;

    // Count errors
    uint8_t errorCount = 0;
    for (uint8_t i = 0; i < m_manifestCount; i++) {
        if (!m_manifests[i].valid) {
            errorCount++;
            if (errorCount == 1) {
                // Store first error as summary
                snprintf(m_stats.lastErrorSummary, sizeof(m_stats.lastErrorSummary),
                         "%s: %s", m_manifests[i].filePath, m_manifests[i].errorMsg);
            }
        }
    }
    m_stats.errorCount = errorCount;

    // If any manifest is invalid, rollback and fail
    if (errorCount > 0) {
        LW_LOGW("Reload failed: %u manifest errors, keeping previous state", errorCount);
        memcpy(m_manifests, prevManifests, sizeof(m_manifests));
        m_manifestCount = prevManifestCount;
        m_overrideMode = prevOverrideMode;
        m_stats.lastReloadOk = false;
        m_stats.lastReloadMillis = millis();
        return false;
    }

    // All manifests valid - apply atomically
    // First, clear current registrations that came from manifests
    // (We don't clear built-in registrations)
    m_stats.loadedFromLittleFS = 0;

    // Reset override mode and allowed effects
    m_overrideMode = false;
    memset(m_allowedEffects, 0, sizeof(m_allowedEffects));

    // Apply manifests
    if (applyManifests()) {
        m_stats.lastReloadOk = true;
        m_stats.lastErrorSummary[0] = '\0';
        LW_LOGI("Reload succeeded: %u effects from %u manifests",
                m_stats.loadedFromLittleFS, m_manifestCount);
    } else {
        // This shouldn't happen if validation passed, but handle it
        m_stats.lastReloadOk = false;
        snprintf(m_stats.lastErrorSummary, sizeof(m_stats.lastErrorSummary),
                 "Application failed after validation");
        LW_LOGE("Reload application failed unexpectedly");
    }

    m_stats.lastReloadMillis = millis();
    return m_stats.lastReloadOk;
#else
    LW_LOGD("LittleFS reload not available in native build");
    m_stats.lastReloadOk = false;
    m_stats.lastReloadMillis = millis();
    snprintf(m_stats.lastErrorSummary, sizeof(m_stats.lastErrorSummary),
             "Not available in native build");
    return false;
#endif
}

// ============================================================================
// Internal Methods
// ============================================================================

uint8_t PluginManagerActor::scanManifestFiles() {
#ifndef NATIVE_BUILD
    static constexpr const char* kPluginSuffix = ".plugin.json";
    static constexpr size_t kPluginSuffixLen = 12;

    uint8_t count = 0;

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        LW_LOGW("LittleFS root not available");
        return 0;
    }

    File file = root.openNextFile();
    while (file && count < PluginConfig::MAX_MANIFESTS) {
        const char* name = file.name();
        size_t nameLen = strlen(name);

        // Check for .plugin.json suffix
        if (nameLen > kPluginSuffixLen &&
            strcmp(name + nameLen - kPluginSuffixLen, kPluginSuffix) == 0) {

            // Build full path
            char path[PluginConfig::LITTLEFS_PLUGIN_PATH_MAX];
            snprintf(path, sizeof(path), "/%s", name);

            LW_LOGD("Found manifest: %s", path);

            // Parse the manifest
            if (parseManifest(path, m_manifests[count])) {
                // Validate the manifest
                validateManifest(m_manifests[count]);
            }

            count++;
        }

        file = root.openNextFile();
    }

    return count;
#else
    return 0;
#endif
}

bool PluginManagerActor::parseManifest(const char* path, ParsedManifest& manifest) {
#ifndef NATIVE_BUILD
    // Initialize manifest
    memset(&manifest, 0, sizeof(manifest));
    strncpy(manifest.filePath, path, sizeof(manifest.filePath) - 1);
    manifest.valid = false;

    // Open file
    File file = LittleFS.open(path, "r");
    if (!file) {
        snprintf(manifest.errorMsg, sizeof(manifest.errorMsg), "Failed to open file");
        return false;
    }

    // Check file size
    size_t fileSize = file.size();
    if (fileSize > PluginConfig::MANIFEST_CAPACITY) {
        snprintf(manifest.errorMsg, sizeof(manifest.errorMsg),
                 "File too large (%u > %u)", (unsigned)fileSize,
                 (unsigned)PluginConfig::MANIFEST_CAPACITY);
        file.close();
        return false;
    }

    // Parse JSON using codec (single canonical JSON parser)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        snprintf(manifest.errorMsg, sizeof(manifest.errorMsg),
                 "JSON parse error: %s", err.c_str());
        return false;
    }

    // Decode using ManifestCodec (only place JSON keys are read)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ManifestDecodeResult decodeResult = codec::ManifestCodec::decode(root);

    if (!decodeResult.success) {
        // Copy error message from codec
        strncpy(manifest.errorMsg, decodeResult.errorMsg, sizeof(manifest.errorMsg) - 1);
        manifest.errorMsg[sizeof(manifest.errorMsg) - 1] = '\0';
        return false;
    }

    // Copy decoded config to manifest struct
    strncpy(manifest.pluginName, decodeResult.config.pluginName, sizeof(manifest.pluginName) - 1);
    manifest.pluginName[sizeof(manifest.pluginName) - 1] = '\0';
    manifest.overrideMode = decodeResult.config.overrideMode;
    manifest.effectCount = decodeResult.config.effectCount;
    memcpy(manifest.effectIds, decodeResult.config.effectIds, 
           decodeResult.config.effectCount * sizeof(uint8_t));

    manifest.valid = true;
    return true;
#else
    memset(&manifest, 0, sizeof(manifest));
    strncpy(manifest.filePath, path, sizeof(manifest.filePath) - 1);
    snprintf(manifest.errorMsg, sizeof(manifest.errorMsg), "Native build - no LittleFS");
    return false;
#endif
}

bool PluginManagerActor::validateManifest(ParsedManifest& manifest) {
    if (!manifest.valid) {
        return false;
    }

    // Validate all effect IDs exist in built-in registry
    for (uint8_t i = 0; i < manifest.effectCount; i++) {
        uint8_t id = manifest.effectIds[i];
        if (!BuiltinEffectRegistry::hasBuiltin(id)) {
            snprintf(manifest.errorMsg, sizeof(manifest.errorMsg),
                     "Effect ID %u not found in built-in registry", id);
            manifest.valid = false;
            return false;
        }
    }

    return true;
}

bool PluginManagerActor::applyManifests() {
    // First pass: check for override mode
    for (uint8_t i = 0; i < m_manifestCount; i++) {
        if (m_manifests[i].valid && m_manifests[i].overrideMode) {
            m_overrideMode = true;
            LW_LOGI("Override mode enabled by manifest: %s", m_manifests[i].pluginName);
            break;
        }
    }

    // If override mode, build allowed effects list
    if (m_overrideMode) {
        memset(m_allowedEffects, 0, sizeof(m_allowedEffects));

        for (uint8_t i = 0; i < m_manifestCount; i++) {
            if (!m_manifests[i].valid) continue;

            for (uint8_t j = 0; j < m_manifests[i].effectCount; j++) {
                uint8_t id = m_manifests[i].effectIds[j];
                m_allowedEffects[id] = true;
            }
        }

        // Count disabled effects
        uint8_t builtinCount = BuiltinEffectRegistry::getBuiltinCount();
        uint8_t allowedCount = 0;
        for (uint8_t i = 0; i < PluginConfig::MAX_EFFECTS; i++) {
            if (m_allowedEffects[i]) allowedCount++;
        }
        m_stats.disabledByOverride = builtinCount > allowedCount ?
                                      builtinCount - allowedCount : 0;
        m_stats.overrideModeEnabled = true;

        LW_LOGI("Override mode: %u effects allowed, %u disabled",
                allowedCount, m_stats.disabledByOverride);
    } else {
        m_stats.overrideModeEnabled = false;
        m_stats.disabledByOverride = 0;
    }

    // Second pass: count loaded effects
    uint8_t loadedCount = 0;
    for (uint8_t i = 0; i < m_manifestCount; i++) {
        if (!m_manifests[i].valid) continue;
        loadedCount += m_manifests[i].effectCount;
    }
    m_stats.loadedFromLittleFS = loadedCount;

    return true;
}

void PluginManagerActor::clearRegistrations() {
    for (uint8_t i = 0; i < PluginConfig::MAX_EFFECTS; i++) {
        if (m_effects[i] != nullptr) {
            unregisterEffect(i);
        }
    }
}

const ParsedManifest* PluginManagerActor::getManifest(uint8_t index) const {
    if (index >= m_manifestCount) {
        return nullptr;
    }
    return &m_manifests[index];
}

} // namespace plugins
} // namespace lightwaveos
